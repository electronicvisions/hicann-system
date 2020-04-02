#include <iostream>
#include <boost/program_options.hpp>

extern "C" {
#include <sys/time.h>
}

#include "gtest/gtest.h"

#include "sctrltp/ARQStream.h"
#include "sctrltp/ARQFrame.h"

extern int g_argc;
extern char** g_argv;

#define NUM_PACKETS 1000000
#define MIN_RATE_IN_MBS 10
#define EXPECTED_TIME (sctrltp::Parameters<>::MAX_PDUWORDS * NUM_PACKETS * 8) / (MIN_RATE_IN_MBS*1024*1024)

// FIXME: VITALI PLEASE INTEGRATE INTO TRUNK!
#define HOSTARQ_LOOPBACK_TYPE 0x0

static uint64_t* globaldata;


static double mytime() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return 1.0 * now.tv_sec + now.tv_usec / 1e6;
}


void* sending_loop(void* p) {
	sctrltp::ARQStream<>& arq = *static_cast<sctrltp::ARQStream<>*>(p);

	std::cout << "sending " << (8.0 * sctrltp::Parameters<>::MAX_PDUWORDS * NUM_PACKETS / 1024 / 1024) << "MB" << std::endl;

	sctrltp::packet tmp;
	tmp.pid = HOSTARQ_LOOPBACK_TYPE;
	tmp.len = sctrltp::Parameters<>::MAX_PDUWORDS;

	double timediff = mytime();
	for (size_t i = 0; i < NUM_PACKETS; i++) {
		for (size_t j = 0; j < sctrltp::Parameters<>::MAX_PDUWORDS; j++) {
			tmp.pdu[j] = globaldata[i*sctrltp::Parameters<>::MAX_PDUWORDS+j];
		}
		arq.send(tmp, sctrltp::ARQStream<>::NOTHING);
	}
	arq.flush();
	while (! arq.all_packets_sent())
		usleep(1000); // 1ms
	timediff = mytime() - timediff;
	size_t bytes_sent = NUM_PACKETS*sctrltp::Parameters<>::MAX_PDUWORDS*8;

	std::cout << "sender done after " << timediff << "s (" << bytes_sent/(timediff)/1024/1024 << "MB/s)" << std::endl;
	pthread_exit(NULL);
}


void* receiving_loop(void* p) {
	sctrltp::ARQStream<>& arq = *static_cast<sctrltp::ARQStream<>*>(p);

	size_t errors = 0;
	sctrltp::packet tmp;

	double timediff = mytime();
	for (size_t i = 0; i < NUM_PACKETS; i++) {
		arq.receive(tmp, sctrltp::ARQStream<>::NOTHING);
		for (size_t j = 0; j < sctrltp::Parameters<>::MAX_PDUWORDS; j++) {
			if (tmp.pdu[j] != globaldata[i*sctrltp::Parameters<>::MAX_PDUWORDS+j]) {
				errors++;
			}
		}
	}
	timediff = mytime() - timediff;

	std::cout << "receiver done after " << timediff << "s, " << errors << " errors out of " << NUM_PACKETS*sctrltp::Parameters<>::MAX_PDUWORDS << std::endl;
	pthread_exit(NULL);
}


void do_loopback_tests(std::vector<sctrltp::ARQStream<>*> arq) {
	// fill global data
	double timediff = mytime();
	globaldata = new uint64_t[NUM_PACKETS*sctrltp::Parameters<>::MAX_PDUWORDS];
	srand(1234);
	for (size_t i = 0; i < NUM_PACKETS; i++)
		globaldata[i] = rand();
	timediff = mytime() - timediff;
	printf("creating globaldata took %.6fs\n", timediff);

	std::cout << "starting up " << arq.size() << " tests" << std::endl;

	// now start multiple tests
	std::vector<pthread_t> sending_threads(arq.size());
	std::vector<pthread_t> receiving_threads(arq.size());
	for (size_t i = 0; i < arq.size(); i++) {
		// start up sender/receiver threads
		pthread_create(&sending_threads[i], NULL, sending_loop, arq[i]);
		pthread_create(&receiving_threads[i], NULL, receiving_loop, arq[i]);
	}

	// wait for termination
	size_t oks = 0, tries = 0;
	do {
		sleep(1);
		for (size_t i = 0; i < arq.size(); i++) {
			if ((pthread_tryjoin_np(sending_threads[i], NULL) == 0) &&
				(pthread_tryjoin_np(receiving_threads[i], NULL) == 0))
				oks++;
		}

		if (oks == arq.size())
			exit(EXIT_SUCCESS);
	} while(++tries < EXPECTED_TIME);

	// kill 'em all
	std::cout << "hanging stuff" << std::endl;

	int const signals[] = { SIGTERM, SIGKILL };

	for (size_t ii = 0; ii < arq.size(); ii++) {
		for (size_t i = 0; i < sizeof(signals)/sizeof(int); i++) {
			int rets = pthread_tryjoin_np(sending_threads[ii], NULL);
			int retr = pthread_tryjoin_np(receiving_threads[ii], NULL);
			if (rets != 0) pthread_kill(sending_threads[ii], signals[i]);
			if (retr != 0) pthread_kill(receiving_threads[ii], signals[i]);
			if ((rets == 0) && (retr == 0))
				break;
		}
		sleep(1);
	}

	exit(EXIT_SUCCESS);
}


TEST(HostARQ, Loopback) {
	::testing::FLAGS_gtest_death_test_style = "threadsafe";

	// test HostARQ via loopback in HostARQ
	std::vector<std::string> shm_name;

	namespace po = boost::program_options;
	po::options_description desc("Allowed options");
	desc.add_options()
		("shm_name,S", po::value< std::vector<std::string> >(&shm_name), "shared memory names for multiple parallel streams")
	;
	po::variables_map vm;
	po::store(po::parse_command_line(g_argc, g_argv, desc), vm);

	if (vm.count("shm_name") == 0)
		// default name
		shm_name.push_back("testit");

	po::notify(vm);

	// daemons have to be up and running here
	std::vector<sctrltp::ARQStream<>*> arq;
	for (size_t i = 0; i < shm_name.size(); i++)
		arq.push_back(new sctrltp::ARQStream<>(shm_name[i]));

	EXPECT_EXIT(do_loopback_tests(arq), ::testing::ExitedWithCode(0), "");
}
