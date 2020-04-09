// max test size (in Bytes)
#define MAX_PBMEM_SIZE 500*1000*1000

#ifdef NCSIM
#error testmode not implemented for ncsim
#endif

#include "testmode.h"
#include "common.h"
#include "s2comm.h"
#include "s2ctrl.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga_arq.h"

extern "C" {
// uintXX_t
#include <stdint.h>
// gettimeofday()
#include <sys/time.h>
}
// Software ARQ
#include "sctrltp/us_sctp_if.h"

// gimme warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wall -Wextra"

using namespace facets;
using namespace sctrltp;

// cmd line parameters
static size_t no_hicanns;
static size_t no_cmds;
static bool   do_read;

// global parameters used by threads
static uint64_t dncid;
static Stage2Comm* g_comm;
static S2C_JtagPhys2Fpga* fpga_comm;
static bool send_finished = false;

// some helpers
static double mytime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return 1.0 * now.tv_sec + now.tv_usec / 1e6;
}


static void check_arq_registers(Stage2Comm* comm) {
	jtag_cmdbase::arqcounters_t cntrs = comm->jtag->GetARQCounters();
	uint32_t rcounter = cntrs.first, wcounter = cntrs.second;
	std::cout << "ARQ read (from HW) counter: " << std::dec <<
		(rcounter & 0x7fffffff)
		<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ") << std::endl
		<< "ARQ write (to HW) counter: " <<
		(wcounter & 0x7fffffff)
		<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)" << std::endl;
	jtag_cmdbase::arqdebugregs_t dregs = comm->jtag->GetARQNetworkDebugReg();
	std::cout << "Debug Registers: down: " << std::dec << dregs.first << " up: " << dregs.second << std::endl;
}




static void *sending(void *param) {
	struct sctp_descr<ParametersFcpBss1>* desc = (struct sctp_descr<ParametersFcpBss1>*) param;
	struct buf_desc<ParametersFcpBss1> buffer;
	__u64 data[ParametersFcpBss1::MAX_PDUWORDS];
	__s32 ret;
	uint16_t const packet_type = application_layer_packet_types::HICANNCONFIG;
	uint16_t last_fpga_time = 0;
	size_t cmd_counter = 0;
	size_t frame_counter = 0;

	printf ("sending: thread up\n");


	//uint64_t min_cmd = 0x3800e00000000000ull;
	//uint64_t max_cmd = 0x3800e06f00000000ull;
	//uint64_t step_cmd = 0x0000006f00000000ull/111ull;

	assert(no_hicanns > 0 && no_hicanns <= 8);

	// fill packet
	for (size_t i = 0; i < ParametersFcpBss1::MAX_PDUWORDS; i++) {
		size_t id = i % no_hicanns;
		uint64_t cmd;
		// verticle setup counts inverted
		if (! g_comm->running_on_reticle)
			id = 7 - id;
		switch(id) {
			case 0:
				cmd = dncid | 0x3800e00000000007ull;
				break;
			case 1:
				cmd = dncid | 0x3000e00000000006ull;
				break;
			case 2:
				cmd = dncid | 0x2800e00000000005ull;
				break;
			case 3:
				cmd = dncid | 0x2000e00000000004ull;
				break;
			case 4:
				cmd = dncid | 0x1800e00000000003ull;
				break;
			case 5:
				cmd = dncid | 0x1000e00000000002ull;
				break;
			case 6:
				cmd = dncid | 0x0800e00000000001ull;
				break;
			case 7:
				cmd = dncid | 0x0000e00000000000ull;
				break;
			default:
				break;
		}
		if (do_read)
			cmd |= 0x0000000080000000ull; // reading
		else
			cmd |= 0x0001000000000000ull; // writing
		data[i] = htobe64(cmd);
	}

	size_t const full_frames = no_cmds / ParametersFcpBss1::MAX_PDUWORDS;
	size_t const last_frame_cmds = no_cmds - full_frames * ParametersFcpBss1::MAX_PDUWORDS;

	double start = mytime();
	for (size_t i = 0; i < full_frames; i++) {
		acq_buf (desc, &buffer, 0);
		init_buf (&buffer);
		__s32 ret = append_words (&buffer, packet_type,  ParametersFcpBss1::MAX_PDUWORDS, data);
		send_buf (desc, &buffer, 0);
	}

	if (last_frame_cmds) {
		acq_buf (desc, &buffer, 0);
		init_buf (&buffer);
		__s32 ret = append_words (&buffer, packet_type,  last_frame_cmds, data);
		send_buf (desc, &buffer, 0);
	}
	send_buf (desc, (buf_desc<ParametersFcpBss1>*)NULL, MODE_FLUSH);

	while(! tx_queues_empty(desc))
		usleep(1000); // 1ms
	double stop = mytime();

	double bytes_sent = no_cmds *8.0/1024/1024;
	printf("sending: %lld cmds (%.2f MiB) took %.6fs = %.3fMiB/s\n", no_cmds, bytes_sent, stop-start, 1.0*bytes_sent/(stop-start));

	send_finished = true;

	printf("sending: exiting\n");
	pthread_exit(NULL);
}



class EcmTmHicannReads : public Testmode {
protected:
	virtual std::string ClassName() {
		std::stringstream ss;
		ss << "tmecm_hicannreads";
		return ss.str();
	}

private:
	FPGAControl *fc;

public:

	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------
	bool test() {

		// custom testmode options
	    std::vector<boost::program_options::basic_option<char> >::iterator it;
	    log(Logger::INFO) << "shm_name is " << shm_name;

	    no_hicanns = 8;
	    for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("no_hicanns").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> no_hicanns;
				break;
			}
	    }
	    log(Logger::INFO) << "no_hicanns is " << no_hicanns;

	    no_cmds = 1027*176;
	    for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("no_cmds").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> no_cmds;
				break;
			}
	    }
	    log(Logger::INFO) << "no_cmds is " << no_cmds;

	    do_read = true;
	    for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("do_read").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> do_read;
				break;
			}
	    }
	    log(Logger::INFO) << "do_read is " << do_read;

		dncid = comm->getConnId().get_jtag_port() - 1700;
		log(Logger::INFO) << "dncid set to " << dncid;
		dncid = dncid << 62ull;

		// setup system
		g_comm = comm;
		fc = dynamic_cast<FPGAControl*>(chip[0]);
		fc->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1);

		// get ARQ comm pointer, if available
		fpga_comm = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);

		// more relaxed HICANN-ARQ timings
		if (!fpga_comm->is_k7fpga())
			jtag->SetARQTimings(31, 1000, 30);

		log(Logger::INFO) << "Try Init() ...";
		uint hicannr = 0;
		HicannCtrl* hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		if (g_comm->running_on_reticle) {
			std::bitset<8> used_hicanns;
			used_hicanns.set();
			if (dynamic_cast<S2C_JtagPhys2Fpga*>(comm)->Init(used_hicanns) != Stage2Comm::ok) {
				log(Logger::ERROR) << "ERROR: Init failed, abort";
				return false;
			}
		} else {
			std::bitset<8> used_hicanns;
            for (unsigned int nhicann=0; nhicann<no_hicanns;++nhicann)
            	used_hicanns[nhicann] = true;
			if (dynamic_cast<S2C_JtagPhys2Fpga*>(comm)->Init(used_hicanns) != Stage2Comm::ok) {
				log(Logger::ERROR) << "ERROR: HICANN Init on vertical setup failed, abort";
				return false;
			}
		}
		log(Logger::INFO) << "Init() ok";

		// unset ARQ reset
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, false);

		printf ("Connecting to HostARQ Shmem %s", shm_name.c_str());
		struct buf_desc<ParametersFcpBss1> buffer;
		struct sctp_descr<ParametersFcpBss1>* desc = open_conn<ParametersFcpBss1>(shm_name.c_str());
		if (!desc) {
			printf ("Error: make sure Core and testbench are up\n");
			return false;
		}

		std::cout << "checking arq registers" << std::endl;
		check_arq_registers(g_comm);

		pthread_t threadvar;

		// start sending thread
		pthread_create(&threadvar, NULL, sending, desc);


		log(Logger::INFO) << "Receiving";

		size_t recvd_cmds = 0;
		size_t cmd_counter = 0;
		size_t frame_cnt = 0;
		double last_print = 0.0;

		if (do_read) {
			log(Logger::INFO) << "Waiting for data...";
			double start = mytime();
			while(true) {
				if (!rx_queues_empty(desc)) {
					recv_buf (desc, &buffer, 0);

					// FIXME: add data verify here
					recvd_cmds += sctpreq_get_len(buffer.arq_sctrl);
					frame_cnt++;

					rel_buf (desc, &buffer, 0);

					if (recvd_cmds >= no_cmds) {
						printf("Testrun OK\n");
						break;
					}
					else {
						double now = mytime();
						if (now - last_print > 1.0) {
							printf("%llu/%llu recvd (%5.2f%%)\n", recvd_cmds, no_cmds, 100.0*recvd_cmds/no_cmds);
							last_print = now;
						}
					}

				} else {
					/*no packets in queue*/
					if (send_finished) {
						/*wait and check if still no packets*/
						usleep(10000);
						if (rx_queues_empty(desc)) {
							/*no packets to recieve and sending is finish, test is done*/
							printf("Data underflow: cmds sent: %d cmds recvd: %d\n", no_cmds, recvd_cmds);
							return false;
						}
					}
				}
			}
			double stop = mytime();
			log(Logger::INFO) << "Done " << recvd_cmds << " cmds in " << frame_cnt << " frames.";
			log(Logger::INFO) << (recvd_cmds*8.0) << "bytes took " << stop-start << "s => " << (recvd_cmds*8.0/1024/1024)/(stop-start) << "MB/s";
		}

		std::cout << "sending: checking arq registers" << std::endl;
		check_arq_registers(g_comm);

		size_t i = 0, ret = 0;
		size_t const expected_time = no_cmds / (10*1024*1024) + 3;
		log(Logger::INFO) << "waiting up to " << expected_time << "s";
		do {
			sleep(1);
			ret = pthread_tryjoin_np(threadvar, NULL);
			if (ret == 0)
				break;
		} while (i++ < expected_time);

		if (ret != 0)
			log(Logger::ERROR) << "Could not join sending thread";

		// FIXME: add check for data validity
		return true;
	}
};


class EcmTmHicannReadsTest : public Listee<Testmode>{
public:
	EcmTmHicannReadsTest() :
		Listee<Testmode>(std::string("tmecm_hicannreads"),
		std::string("Read from HICANNs directly via HostARQ"))
	{}
	Testmode * create(){
		return new EcmTmHicannReads();
	}
};

static EcmTmHicannReadsTest mytest;
#pragma GCC diagnostic pop

