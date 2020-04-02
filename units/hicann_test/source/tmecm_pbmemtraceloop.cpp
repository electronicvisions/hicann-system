// max test size (in Bytes)
#define MAX_PBMEM_SIZE 500*1000*1000

// usage example on wafer
// tests2 -bje2f 8 0 -ip TARGETIP -jp JTAGPORT -r 0 -m tmecm_pbmemtraceloop -- --no_spikes=1000 --min_fpga_clk_dist=8

// usage example on vsetup
// tests2 -bje2f 1 0 -ip TARGETIP -jp 1701          -m tmecm_pbmemtraceloop -- --no_spikes=1000 --min_fpga_clk_dist=8

// the ARQ daemon (needs root) has to be started:
// sudo start_core testit SOURCEIP TARGETIP 1
//                                          ^ sends HostARQ reset frame

#ifdef NCSIM
#error testmode not implemented for ncsim
#endif

#include "testmode.h"
#include "common.h"
#include "s2comm.h"
#include "s2ctrl.h"
#include "fpga_control.h"
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

// global parameters used by threads
static size_t no_spikes;
static size_t min_fpga_clk_dist;
static S2C_JtagPhys2Fpga * fpga_comm;
static double start, stop;

// some helpers
static double mytime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return 1.0 * now.tv_sec + now.tv_usec / 1e6;
}

static void trigger_systime() {
	assert(fpga_comm);
	std::cout << "triggering systime" << std::endl;
	fpga_comm->trigger_systime(fpga_comm->getConnId().get_fpga_ip().to_ulong(), false); // stop systime
	fpga_comm->trigger_systime(fpga_comm->getConnId().get_fpga_ip().to_ulong(), true); // start systime
}


// bitfields for data
union singlepulsegroup_hack {
	uint64_t data;
	struct {
		uint64_t _zero1:     1;
		uint64_t fpga_time: 14;
		uint64_t _pad1:      1;
		uint64_t _zero2:     1;
		uint64_t _pad2:      1;
		uint64_t count:     14;

		uint64_t _pad3:      1;
		uint64_t timestamp: 15;
		uint64_t _pad4:      2;
		uint64_t label:     12;
		uint64_t _pad5:      2;
	};
};

union fpgatracepulsedata_hack {
	uint64_t data;
	struct {
		uint32_t timestamp:  15;
		uint32_t label:      12;
		uint32_t _pad1:       2;
		uint32_t fm:          1;
		uint32_t _empty:      1;
		uint32_t _zero2:      1;
	} pulse[2];

	struct overflow {
		uint64_t _pad1:      30;
		uint64_t _one1:       1;
		uint64_t _zero1:      1;

		uint64_t overflow:   31;
		uint64_t _one2:       1;
	};
	struct overflow_and_pulse{
		uint64_t timestamp1: 15;
		uint64_t label1:     12;
		uint64_t _pad1:       2;
		uint64_t fm1:         1;
		uint64_t _zero1:      1;
		uint64_t _zero2:      1;

		uint64_t overflow:   31;
		uint64_t _one2:       1;
	};
};

union fpgaconfigcmd_hack {
	uint64_t data;
	struct {
		uint64_t _pad3:           23;
		uint64_t systime_replace:  1; // 23
		uint64_t pulse_loop:       1; // 24
		uint64_t read_traced_cfg:  1;
		uint64_t read_traced_pls:  1; // 10, bit 25 ?
		uint64_t stop_trace:       1; // 11, bit 26
		uint64_t start_trace:      1; // 12? bit 27
		uint64_t start_experiment: 1; // 13?, bit 28
		uint64_t clear_trace:      1;
		uint64_t clear_pbmem:      1;
		uint64_t _pad0:           32; // bits 0-21
	};
};


static void *sending(void *param) {
	struct sctp_descr<> *desc = (struct sctp_descr<> *)param;
	struct buf_desc<ParametersFcpBss1> buffer;
	fpgaconfigcmd_hack cfg;
	__u64 data[ParametersFcpBss1::MAX_PDUWORDS];
	__s32 ret;
	uint16_t type = application_layer_packet_types::FPGACONFIG;
	uint16_t last_fpga_time = 0;
	size_t cmd_counter = 0;
	size_t frame_counter = 0;


	printf ("sending: thread up\n");

	assert(no_spikes * 8 < MAX_PBMEM_SIZE);
	assert(sizeof(fpgaconfigcmd_hack)      == sizeof(uint64_t));
	assert(sizeof(fpgatracepulsedata_hack) == sizeof(uint64_t));
	assert(sizeof(singlepulsegroup_hack)   == sizeof(uint64_t));

	trigger_systime();

	// configure PBMem pulse loopback
	printf("clearing trace and pbmem\n");
	cfg.data = 0;
	cfg.clear_trace = 1;
	cfg.clear_pbmem = 1;
	data[0] = htobe64(cfg.data);
	acq_buf(desc, &buffer, 0);
	init_buf(&buffer);
	ret = append_words (&buffer, type,  1/*payload*/, data);
	send_buf(desc, &buffer, MODE_FLUSH);
	usleep(500000); // to be sure... ;)
	printf("done\n");

	printf("setting pulse loopback\n");
	cfg.data = 0;
	cfg.pulse_loop = 1;
	data[0] = htobe64(cfg.data);
	acq_buf (desc, &buffer, 0);
	init_buf (&buffer);
	ret = append_words (&buffer, type,  1/*payload*/, data);
	send_buf (desc, &buffer, MODE_FLUSH);
	usleep(500000);
	printf("done\n");

	printf("sending pulse frames\n");
	double start = mytime();
	while(cmd_counter < no_spikes) {
		singlepulsegroup_hack pulse;
		pulse.data = 0;
		pulse.count = 1;

		size_t iii;
		for (iii = 0; iii < ParametersFcpBss1::MAX_PDUWORDS; iii++) {
			pulse.fpga_time = last_fpga_time;
			// lower bits into label
			pulse.label     = cmd_counter & 0xfff;
			// higher bits into timestamp
			pulse.timestamp = (cmd_counter >> 12) & 0x7fff;
			data[iii] = htobe64(pulse.data);
			if (cmd_counter >= no_spikes)
				break;
			cmd_counter++;
			last_fpga_time += min_fpga_clk_dist;
		}

		acq_buf (desc, &buffer, 0);
		init_buf (&buffer);
		ret = append_words (&buffer, application_layer_packet_types::FPGAPLAYBACK, iii, data);
		send_buf (desc, &buffer, 0);
		frame_counter++;
	}
	send_buf (desc, (buf_desc<ParametersFcpBss1>*)NULL, MODE_FLUSH);
	double stop = mytime(); // not totally correct ;p
	printf("done\n");

	for (size_t qei = 0; ! tx_queues_empty(desc); qei++) {
		if (!qei)
			std::cout << "(after sending pulses) tx_queues not empty: " << std::flush;
		std::cout << "." << std::flush;
		usleep(100*1000);
		if (qei > 100) {
			std::cout << "ignoring after 10s" << std::endl;
			break;
		}
	}
	std::cout << std::endl;

	printf("##### sending %lld (%.2f MB) cmds (in %lld frames) took %.6fs\n", cmd_counter, cmd_counter*8.0/1024/1024, frame_counter, stop-start);
	sleep(1);

	// configure PBMem pulse loopback
	printf("starting playback/trace + wait 1s!\n");
	cfg.data = 0;
	cfg.start_experiment = 1;
	cfg.pulse_loop = 1;
	data[0] = htobe64(cfg.data);
	acq_buf (desc, &buffer, 0);
	init_buf (&buffer);
	ret = append_words (&buffer, type,  1/*payload*/, data);
	send_buf (desc, &buffer, MODE_FLUSH);
	sleep(1);
	printf("done\n");

	printf("stopping trace\n");
	cfg.data = 0;
	cfg.stop_trace = 1;
	cfg.pulse_loop = 1;
	data[0] = htobe64(cfg.data);
	acq_buf (desc, &buffer, 0);
	init_buf (&buffer);
	ret = append_words (&buffer, type,  1/*payload*/, data);
	send_buf (desc, &buffer, MODE_FLUSH);
	usleep(500000);
	printf("done\n");

	printf("starting read out\n");
	cfg.data = 0;
	cfg.read_traced_pls = 1;
	cfg.pulse_loop = 1;
	data[0] = htobe64(cfg.data);
	acq_buf (desc, &buffer, 0);
	init_buf (&buffer);
	append_words (&buffer, type,  1/*payload*/, data);
	send_buf (desc, &buffer, MODE_FLUSH);
	printf("done\n");

	printf("wait for data... 5s\n");
	sleep(5);

	for (size_t qei = 0; ! tx_queues_empty(desc); qei++) {
		if (!qei)
			std::cout << "(after experiment) tx_queues not empty: " << std::flush;
		std::cout << "." << std::flush;
		usleep(100*1000);
		if (qei > 100) {
			std::cout << "ignoring after 10s" << std::endl;
			break;
		}
	}
	std::cout << std::endl;

	printf("sending: more sleep before exiting\n");
	sleep(5);

	printf ("sending: thread exiting\n");
	pthread_exit(NULL);
}


class EcmTmPbmemTraceMemLoopback : public Testmode {
protected:
	virtual std::string ClassName() {
		std::stringstream ss;
		ss << "tmecm_pbmemtraceloop";
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
	    std::string shm_name = "testit";
	    std::vector<boost::program_options::basic_option<char> >::iterator it;
	    for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("shm_name").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> shm_name;
				break;
			}
	    }
	    log(Logger::INFO) << "shm_name is " << shm_name << std::endl;

	    no_spikes = 1027*176;
	    for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("no_spikes").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> no_spikes;
				break;
			}
	    }
	    log(Logger::INFO) << "no_spikes is " << no_spikes;

	    min_fpga_clk_dist = 8;
	    for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("min_fpga_clk_dist").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> min_fpga_clk_dist;
				break;
			}
	    }
	    log(Logger::INFO) << "min_fpga_clk_dist is " << min_fpga_clk_dist;


		// setup system
		fc = dynamic_cast<FPGAControl*>(chip[0]);
		fc->reset();

		// get ARQ comm pointer, if available
		fpga_comm = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);

		std::cout << "verify that software ARQ server runs (shm_name is " << shm_name << ") and enter some char and press <Enter>: " << std::flush;
		char a;
		std::cin >> a;


		printf ("Connecting to HostARQ Shmem %s", shm_name.c_str());
		struct buf_desc<ParametersFcpBss1> buffer;
		struct sctp_descr<> *desc = open_conn<ParametersFcpBss1>(shm_name.c_str());
		if (!desc) {
			printf ("Error: make sure Core and testbench are up\n");
			return 1;
		}
		pthread_t threadvar;

		// start sending thread
		pthread_create(&threadvar, NULL, sending, desc);


		log(Logger::INFO) << "Receiving";

		size_t recvd_spikes = 0;
		size_t percentage = 0;

		size_t cmd_counter = 0, time_counter = 0;
		size_t frame_cnt = 0;
		size_t wrong_cnt = 0, ok_cnt = 0, ignore_cnt = 0;

		fpgatracepulsedata_hack pulse;

		double start = mytime();
		while(true) {
			recv_buf (desc, &buffer, 0);

			// iter all 64-bit entries
			for (size_t jj = 0; jj < sctpreq_get_len(buffer.arq_sctrl); jj++) {
				pulse.data = be64toh(buffer.payload[jj]);
				// iter two packed pulses in one 64-bit entry
				for (size_t jjj = 0; jjj < 2; jjj++) {
					if (pulse.pulse[jjj]._empty)
						continue; // empty entry
					if (pulse.pulse[jjj]._zero2 != 0) {
						ignore_cnt++;
						continue; // overflow!?!? (do not count)
					}

					uint16_t want_label = cmd_counter & 0xfff;
					uint16_t want_timestamp = (cmd_counter >> 12) & 0x7fff;
					uint16_t got_label = pulse.pulse[jjj].label;
					uint16_t got_timestamp = pulse.pulse[jjj].timestamp;

					if (   want_label     != got_label // lower 15 bits in timestamp
						|| want_timestamp != got_timestamp
					) {
						if (wrong_cnt++ < 100) {
							printf("frame %zu idx %zu want label %u got %u, timestamp %u got %u (other errors: %zu)\n",
								frame_cnt,
								jj,
								want_label,
								got_label,
								want_timestamp,
								got_timestamp,
								wrong_cnt);
						}
					} else
						ok_cnt++;
					cmd_counter++;
					time_counter += min_fpga_clk_dist;
				}
			}
			recvd_spikes += 2*sctpreq_get_len(buffer.arq_sctrl); // add another
			rel_buf (desc, &buffer, 0);
			frame_cnt++;

			if (recvd_spikes >= no_spikes)
				break;
			else {
				printf("errors: %zu\n", wrong_cnt);
				printf("%llu/%llu recvd\n", recvd_spikes, no_spikes);
			}
			if ((10.0 * recvd_spikes / no_spikes) > percentage) {
				printf("have %lld cmds (%.2f%%)\n", recvd_spikes, percentage);
				percentage = percentage + (100.0 * recvd_spikes / no_spikes);
			}
		}
		double stop = mytime();
		log(Logger::INFO) << "Done " << recvd_spikes << " cmds (" << ok_cnt << " ok, " << wrong_cnt << " wrong, " << ignore_cnt << " ignored) in " << frame_cnt << " frames.";
		log(Logger::INFO) << "##### took " << stop-start << "s";
		log(Logger::INFO) << "sleep and join threads";
		sleep(15);

		pthread_join(threadvar, NULL);

		if (wrong_cnt > 0 || ok_cnt < no_spikes)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}
};


class EcmTmPbmemTraceMemLoopbackTest : public Listee<Testmode>{
public:
	EcmTmPbmemTraceMemLoopbackTest() :
		Listee<Testmode>(std::string("tmecm_pbmemtraceloop"),
		std::string("PbMem to TraceMem Loopback Test"))
	{}
	Testmode * create(){
		return new EcmTmPbmemTraceMemLoopback();
	}
};

static EcmTmPbmemTraceMemLoopbackTest mytest;
#pragma GCC diagnostic pop
