/*
Test for HostARQ communication:

Sends either ascending loopback data, that is checked if continously acending,
or flush data which gets dropped by FPGA. Ratio between flush and loopback can
be set arbitrarily. Framesize can be constant, at max value ParametersFcpBss1::MAX_PDUWORDS, or
random between 1 and ParametersFcpBss1::MAX_PDUWORDS. Ascending dummy data from FPGA can be
activated an checked. Arbitrary number of loopback data can be set.
*/

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

#define RX_SLEEPTIME 1000 /*us*/
#define SLOWMODE_WAIT 20 /* 20 ~20mb/s */
namespace po = boost::program_options;
static size_t sent_loop_cmds= 0;

/*mutex controlled variables*/
static pthread_mutex_t mutex;
static bool send_finished = false;

/*simple time*/
static double mytime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return 1.0 * now.tv_sec + now.tv_usec / 1e6;
}

/*struct to hand over parameters to sending function*/
struct send_args_hostarq {
	struct sctp_descr<ParametersFcpBss1>* desc;
	size_t no_cmds;
	std::string testmode;
	std::vector<int> frame_value;
	double loop_flush_ratio;
	bool slow;
	bool do_dummy;
};

/*sends given cfg to set dummy*/
static void send_dummy_cfg(struct sctp_descr<ParametersFcpBss1>* desc, __u64 cfg)
{
	struct buf_desc<ParametersFcpBss1> send_buffer;
	cfg = htobe64(cfg);
	acq_buf (desc, &send_buffer, 0);
	init_buf (&send_buffer);
	append_words (&send_buffer, PTYPE_SENDDUMMY, 1, &cfg);
	send_buf (desc, &send_buffer, 0);
}

//function that takes
static void *sending(void *param) {
	struct send_args_hostarq *args = (struct send_args_hostarq *) param;
	struct buf_desc<ParametersFcpBss1> buffer;
	__u64 data[ParametersFcpBss1::MAX_PDUWORDS];
	uint16_t packet_type = PTYPE_LOOPBACK;
	uint64_t cnt = 0;
	size_t cmds_left = args->no_cmds;
	size_t frame_size = ParametersFcpBss1::MAX_PDUWORDS;
	size_t frame_cnt = 0;
	size_t sent_flush_cmds = 0;
	double start = mytime();
	double sum_frames = 0;
	double last_print = 0.0, now = 0.0;
	size_t exp_flush_cmds = (1-args->loop_flush_ratio)*args->no_cmds;

	srand(mytime());

	printf ("sending: thread up\n");
	// set framesize
	if(args->testmode == "const")
		frame_size = args->frame_value[0];

	//check if order of framevalues makes sense in random mode
	if(args->testmode.compare("rand") == 0 && args->frame_value[0] > args->frame_value[1]) {
		int swap_temp = args->frame_value[0];
		args->frame_value[0] = args->frame_value[0];
		args->frame_value[0] = swap_temp;
	}

	/*set FPGA to send dummy data*/
	if (args->do_dummy) {
		send_dummy_cfg(args->desc, 0b11);
	}

	//send packets
	while (cmds_left > 0) {
		// set frame sizes
		if(args->testmode.compare("rand") == 0) {
			frame_size = (rand() % (args->frame_value[1] - args->frame_value[0])) + 1 + args->frame_value[0];
		}
		if(args->testmode.compare("array") == 0) {
			frame_size = args->frame_value[frame_cnt % args->frame_value.size()];
		}
		if(args->testmode.compare("randarray") == 0) {
			frame_size = args->frame_value[rand() % args->frame_value.size()];
		}
		if(args->testmode.compare("ascend") == 0) {
			frame_size = (frame_cnt % ParametersFcpBss1::MAX_PDUWORDS) + 1;
		}
		if(args->testmode.compare("descend") == 0) {
			frame_size = ((ParametersFcpBss1::MAX_PDUWORDS - frame_cnt) % ParametersFcpBss1::MAX_PDUWORDS) + 1;
		}

		if (cmds_left < frame_size)
			frame_size = cmds_left;
		/*setting packet type according to loop/flush ratio*/
		if((((double)rand()/(double)RAND_MAX) < args->loop_flush_ratio) ? true : false) {
			/*sending LOOPBACK data*/
			for (size_t j = 0; j < frame_size; j++) {
				/* write ascending numbers as data */
				data[j] = htobe64(cnt);
				cnt++;
			}
			packet_type = PTYPE_LOOPBACK;
			sent_loop_cmds += frame_size;
		} else {
			/*sending FLUSH data*/
			for (size_t j = 0; j < frame_size; j++) {
				data[j] = 0;
			}
			packet_type = PTYPE_FLUSH;
			sent_flush_cmds += frame_size;
			now = mytime();
			if (now - last_print > 1.0) {
				printf("Flush\t %llu sent of ~%llu (%5.2f%)\n",
				        sent_flush_cmds, exp_flush_cmds , 100.0 * sent_flush_cmds / (exp_flush_cmds + 1.0));
				last_print = now;
			}
		}

		/*wait if slow mode*/
		if (args->slow) {
			usleep(SLOWMODE_WAIT);
		}

		// send packet
		acq_buf (args->desc, &buffer, 0);
		init_buf (&buffer);
		__s32 ret = append_words (&buffer, packet_type, frame_size, data);
		send_buf (args->desc, &buffer, 0);
		// counter, conditions ...
		frame_cnt++;
		sum_frames = sum_frames + frame_size;
		cmds_left = cmds_left-frame_size;
		//printf("%lld frames sent. value cmds_left: %lld. value frame_size: %lld\n", frame_cnt, cmds_left, frame_size) ;
	}

	/*stop fpga from sending dummy data*/
	send_dummy_cfg(args->desc, 0b10);
	// dump tx
	send_buf (args->desc, (buf_desc<ParametersFcpBss1>*)NULL, MODE_FLUSH);
	// wait for tx dump
	while(! tx_queues_empty(args->desc))
		usleep(1000); // 1ms
	double stop = mytime();
	// infodump
	double mean_frames = sum_frames / frame_cnt;
	double bytes_sent = args->no_cmds * 8.0/1024/1024;
	printf("Last frame size: %lld. Number of frames sent: %lld. Mean of frame size: %.2f\n", frame_size, frame_cnt, mean_frames);
	printf("Loopback data sent: %lld Flush data sent: %lld\n", sent_loop_cmds , sent_flush_cmds);
	printf("sending: %lld (%.2f MB) cmds in %u frames took %.6fs = %.3fMB/s\n", args->no_cmds, bytes_sent, frame_cnt, stop-start, 1.0*bytes_sent/(stop-start));
	sleep(1);
	printf("sending: exiting\n");
	pthread_mutex_lock (&mutex);
	send_finished = true;
	pthread_mutex_unlock (&mutex);
	pthread_exit(NULL);
}

static void is_data_ascending(struct buf_desc<ParametersFcpBss1> packet, uint64_t *cnt, uint64_t *skippedp, uint64_t *skippedn, size_t *errs, size_t frame_cnt) {
	uint64_t data = 0;
	for (size_t j = 0; j < sctpreq_get_len(packet.arq_sctrl); j++) {
		data = be64toh(sctpreq_get_pload(packet.arq_sctrl)[j]);
		if (data != *cnt) {
			if (*errs < 100)
				std::cerr << "frame " << frame_cnt << " data/p[" << j << "] = " << data << " != cnt " << *cnt << std::endl;
			if (data > *cnt)
				*skippedp += data - *cnt;
			else
				*skippedn += *cnt - data;
			*cnt = data+1; // set to last
			*errs += 1;
		} else {
			*cnt += 1;
		}
	}
}


class TmCmHostARQLoopbackTest : public Testmode {
protected:
	virtual std::string ClassName() {
		std::stringstream ss;
		ss << "tmcm_hostarq_loopback";
		return ss.str();
	}

private:

public:

	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------
	bool test() {
	// custom testmode options
	size_t no_cmds = 100000;
	std::string testmode ("const");
	std::vector<int> frame_value;
	bool do_read = true;
	bool slow = false;
	double loop_ratio = 1;
	bool do_dummy = false;
	pthread_mutex_init (&mutex, NULL);

	// parsing
	std::vector<boost::program_options::basic_option<char> >::iterator it;
	for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
		if (std::string("no_cmds").compare(it->string_key) == 0) {
			std::istringstream buffer(it->value.at(0));
			buffer >> no_cmds;
		}
		if (std::string("mode").compare(it->string_key) == 0) {
			std::istringstream buffer(it->value.at(0));
			buffer >> testmode;
		}
		if (std::string("slow").compare(it->string_key) == 0) {
			std::istringstream buffer(it->value.at(0));
			buffer >> slow;
		}
		if (std::string("loop_ratio").compare(it->string_key) == 0) {
			std::istringstream buffer(it->value.at(0));
			buffer >> loop_ratio;
		}
		if (std::string("do_dummy").compare(it->string_key) == 0) {
			std::istringstream buffer(it->value.at(0));
			buffer >> do_dummy;
		}
		std::vector< std::basic_string<char> >::iterator frame_it;
		if (std::string("frame_value").compare(it->string_key) == 0) {
			int frame;
			std::istringstream buffer(it->value.at(0));
			do {
				buffer >> frame;
				if (buffer)
					frame_value.push_back(frame);
			} while (buffer);
			std::cout << frame_value.size() << std::endl;
		}
	}

	// check parsing inputs
	if(testmode != "const" && testmode != "rand" && testmode != "randarray" && testmode != "array" && testmode != "ascend" && testmode != "descend") {
		std::cerr << "unkown test mode, exiting ..." << std::endl;
		return TESTS2_FAILURE;
	}
	if((testmode.compare("const") == 0 || testmode.compare("array") == 0 || testmode.compare("randarray") == 0) && frame_value.size() == 0)
		frame_value.push_back(ParametersFcpBss1::MAX_PDUWORDS);
	if(testmode.compare("rand") == 0 && frame_value.size()<2) {
		if(frame_value.size() == 0) {
			frame_value.push_back(1);
			frame_value.push_back(ParametersFcpBss1::MAX_PDUWORDS);
		} else {
			frame_value.push_back(ParametersFcpBss1::MAX_PDUWORDS);
		}
	}

	//correcting wrong framesize values
	for(int i = 0; i<frame_value.size(); i++) {
		if(frame_value[i] > ParametersFcpBss1::MAX_PDUWORDS)
			frame_value[i] = ParametersFcpBss1::MAX_PDUWORDS;
		if(frame_value[i] < 1)
			frame_value[i]=1;
	}

	// infodump
	printf("shm_name: %s no_cmds: %lld testmode: %s slowmode: %u", shm_name.c_str(), no_cmds, testmode.c_str(), slow);
	for (std::vector<int>::iterator fvalue = frame_value.begin(); fvalue != frame_value.end(); ++fvalue) {
		printf (" frame_value: %d", *fvalue);
	}
	printf ("\nConnecting to HostARQ Shmem %s\n", shm_name.c_str());

	struct buf_desc<ParametersFcpBss1> recv_buffer;
	struct buf_desc<ParametersFcpBss1> send_buffer;
	struct send_args_hostarq descargs;

	//initialising sending values
	descargs.desc = open_conn<ParametersFcpBss1>(shm_name.c_str());
	descargs.no_cmds = no_cmds;
	descargs.frame_value = frame_value;
	descargs.testmode = testmode;
	descargs.slow = slow;
	descargs.loop_flush_ratio = loop_ratio;
	descargs.do_dummy = do_dummy;
	if (!descargs.desc) {
		std::cerr << "Error: make sure Core and testbench are up\n";
		return TESTS2_FAILURE;
	}

	pthread_t threadvar;
	bool justdrop = false;
	if (!justdrop)
		/* start sending thread */
		pthread_create(&threadvar, NULL, sending, &descargs);

	std::cerr << "Receiving" << std::endl;

	uint64_t loop_curr_cnt = 0, dummy_curr_cnt = 0;
	size_t recvd_loop_cmds = 0, recvd_dummy_cmds = 0;
	size_t frame_cnt = 0;
	double last_print = 0.0, now = 0.0;
	size_t loop_errs = 0, dummy_errs = 0;
	uint64_t loop_skippedp = 0, loop_skippedn = 0, dummy_skippedp = 0, dummy_skippedn = 0;
	size_t exp_loop_cmds = loop_ratio*no_cmds;
	// receive data
	if (do_read) {

		std::cerr << "Waiting for data..." << std::flush;
		double start = mytime();

		while(true) {
			/*check if there are packets to read*/
			if (!rx_queues_empty(descargs.desc)) {
				/*there are packets in queue*/
				recv_buf(descargs.desc, &recv_buffer, 0);
				frame_cnt++;
				/*check which packet type*/
				switch(sctpreq_get_typ(recv_buffer.arq_sctrl)) {
					case PTYPE_LOOPBACK:
						is_data_ascending(recv_buffer, &loop_curr_cnt, &loop_skippedp, &loop_skippedn, &loop_errs, frame_cnt);
						recvd_loop_cmds += sctpreq_get_len(recv_buffer.arq_sctrl);
						break;
					case PTYPE_SENDDUMMY:
						is_data_ascending(recv_buffer, &dummy_curr_cnt, &dummy_skippedp, &dummy_skippedn, &dummy_errs, frame_cnt);
						recvd_dummy_cmds += sctpreq_get_len(recv_buffer.arq_sctrl);
						break;
					default:
						printf("unexpected packet, type %u data %llx\n", sctpreq_get_typ(recv_buffer.arq_sctrl), be64toh(sctpreq_get_pload(recv_buffer.arq_sctrl)[0]));
				}
				rel_buf (descargs.desc, &recv_buffer, 0);

				if (loop_errs > no_cmds*0.05) {
					printf("Errorlimit reached, exiting...\n");
					return TESTS2_FAILURE;
				} else {
					now = mytime();
					if (now - last_print > 1.0) {
						printf("Loopback %llu recvd of ~%llu (%5.2f%) (%llu errors of %llu, skipped %llu+/%llu-)\n", recvd_loop_cmds, exp_loop_cmds , 100.0*recvd_loop_cmds/(exp_loop_cmds+1.0), loop_errs, loop_curr_cnt, loop_skippedp, loop_skippedn);
						printf("Dummy    %llu recvd (%llu errors of %llu, skipped %llu+/%llu-)\n", recvd_dummy_cmds, dummy_errs, dummy_curr_cnt, dummy_skippedp, dummy_skippedn);
						last_print = now;
					}
				}
			} else {
				/*no packets in queue*/
				if (send_finished) {
					/*wait and check if still no packets*/
					usleep(RX_SLEEPTIME);
					if (rx_queues_empty(descargs.desc)) {
						/*no packets to recieve and sending is finish, test is done*/
						printf("No more data to read...\n");
						break;
					}
				}
			}

		}
		double stop = mytime();
		// infodump
		std::cerr << "Done " << recvd_loop_cmds << " cmds in " << frame_cnt << " frames.";
		std::cerr << (recvd_loop_cmds*8.0) << "bytes took " << stop-start << "s => " << (recvd_loop_cmds*8.0/1024/1024)/(stop-start) << "MB/s" << std::endl;
		printf("\nsent loopback: %llu recvd loopback: %llu (%5.2f%% loss) missing: %llu, recvd dummy: %llu\n", sent_loop_cmds, recvd_loop_cmds , 100-100.0*recvd_loop_cmds/sent_loop_cmds, sent_loop_cmds - recvd_loop_cmds, recvd_dummy_cmds);
	}

	size_t i = 0, ret = 0;
	size_t const expected_time = no_cmds / (10*1024*1024) + 20;
	std::cerr << "waiting up to " << expected_time << "s" << std::endl;
	do {

		sleep(1);
		ret = pthread_tryjoin_np(threadvar, NULL);
		if (ret == 0)
			break;
	} while (i++ < expected_time);

	if (ret != 0)
		std::cerr << "Could not join sending thread";
	if (sent_loop_cmds > 0 && sent_loop_cmds == recvd_loop_cmds) {
		return TESTS2_SUCCESS;
	} else {
		return TESTS2_FAILURE;
	}

	}
};


class LteeTmCmHostARQLoopbackTest : public Listee<Testmode>{
public:
	LteeTmCmHostARQLoopbackTest() :
		Listee<Testmode>(std::string("tmcm_hostarq_loopback"),
		std::string("Test for HostARQ communication"))
	{}
	Testmode * create(){
		return new TmCmHostARQLoopbackTest();
	}
};

LteeTmCmHostARQLoopbackTest  ListeeTmCmHostARQLoopbackTest;
