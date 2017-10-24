// base class
#include "testmode.h"

#include <fstream>
#include <numeric>
#include <iostream>

#include "common.h"
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga_arq.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "jtag_cmdbase.h"

using namespace std;
using namespace facets;


struct TmVKARQPerfTest : public Testmode {

	void reset_arq(std::bitset<8> used_hicanns, S2C_JtagPhys2FpgaArq* comm) {
		comm->Init(used_hicanns, true, /*force init*/ false);
	}

	struct measurement {
		measurement() {}

		measurement(uint32_t n, std::bitset<16> u,
			uint16_t ht0,
			uint16_t ht1,
			uint16_t ht2,
			uint16_t ht3,
			uint16_t ft0,
			uint16_t ft1,
			uint16_t a
		) : // holy shit, c++98!!!
			num_packets(n),
			used_tags(u),
			arbiterdelay(a)
		{
			hicann_timeouts[0] = ht0;
			hicann_timeouts[1] = ht1;
			hicann_timeouts[2] = ht2;
			hicann_timeouts[3] = ht3;
			fpga_timeouts[0] = ft0;
			fpga_timeouts[1] = ft1;
		}

		uint32_t num_packets;
		std::bitset<16> used_tags;
		uint16_t hicann_timeouts[4]; // rx tag0, tag1, tx tag0, tag1
		uint16_t fpga_timeouts[2]; // rx, tx
		uint16_t arbiterdelay;
		bool fail;

		struct result {
			uint32_t test_time_push, test_time_pop;
			jtag_cmdbase::arqdebugregs_t dregs; // fpga out count (post-experiment)
			jtag_cmdbase::arqdout_t dout[4]; // arq counter (post-experiment)
			jtag_cmdbase::arqcounters_t cntrs; // arq upper layer counter (post-experiment)
		};
		std::vector<result> results;

		friend std::ostream& operator<< (std::ostream& stream, const measurement& m);
		friend std::istream& operator>> (std::istream& stream, measurement& m);
	};


	bool test() {

		bool result = true;

		uint num_hicanns;
		vector<HicannCtrl*> hicann_hs, hicann_jtag;
		std::vector<pulse_float_t> send_pulses;
		std::vector<pulse_float_t> rec_pulses;
		S2C_JtagPhys2FpgaArq* local_comm_hs;
		S2C_JtagPhys* s2c_jtag;
		Stage2Comm *local_comm;
		bitset<8> used_hicanns;
		const size_t avg_runs = 3;
		std::vector<measurement> measurements;

		std::string sweep_file; // read sweep parameters from here
		std::vector<boost::program_options::basic_option<char> >::iterator it;
		for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("sweep_file").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> sweep_file;
				break;
			}
		}

		if (sweep_file.size()) {
			std::cout << "reading from " << sweep_file << std::endl;
			std::ifstream is(sweep_file.c_str());
			while(true) {
				measurement m;
				if(!(is >> m))
					break;
				measurements.push_back(m);
			}
		}

		//measurements.push_back({100, std::bitset<16>(0x0101), 10, 10, 20, 20, 80, 320, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  2, 2, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  3, 3, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  4, 4, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  5, 5, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  6, 6, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  7, 7, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  8, 8, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001),  9, 9, 40, 40, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001), 10,10,200,200, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001), 20,20,200,200, 80,256, 5});
		//measurements.push_back({1000, std::bitset<16>(0x0001), 50,50,200,200, 80,256, 5});

		// we need all hicanns
		used_hicanns.set();
		num_hicanns = 8;

		// ----------------------------------------------------
		// generate all HICANN objects, locally:
		// ----------------------------------------------------

		// high-speed
		for(uint i=0; i<8; i++) {
			if ((i<num_hicanns) && (i+jtag->pos_hicann < 8))
				used_hicanns[comm->jtag2dnc(i + jtag->pos_hicann)] = true;
		}
		for(uint hc=0; hc<num_hicanns; hc++) {
			hicann_hs.push_back( new HicannCtrl(comm, hc) );
		}

		// JTAG only
		s2c_jtag   = new S2C_JtagPhys(comm->getConnId(), jtag, false);
		local_comm = static_cast<Stage2Comm*>(s2c_jtag);
		for(uint hc = 0; hc < num_hicanns; hc++) {
			hicann_jtag.push_back( new HicannCtrl(local_comm,hc) );
		}


		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		FPGAControl *fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1); // "DNC" reset

		unsigned int curr_ip = comm->jtag->get_ip();

		// try to set vitalis teststuff to off... (FIXME)
		jtag->SetARQPerformanceTest(false /*enable*/, 0, 0);

		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		log(Logger::INFO) << "HICANN ID: 0x" << hex << jtagid << endl;


		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------

		dbg(0) << "Try Init() ..." ;
		local_comm_hs = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);

		// init -> test transmissions only once :)
		local_comm_hs->disable_multiple_test_transmissions = true;

		if(local_comm_hs) {
			if (local_comm_hs->Init(used_hicanns) != Stage2Comm::ok) {

				dbg(0) << "ERROR: Init failed, abort" << endl;
				return 1;
			}
			dbg(0) << "Init() ok" << endl;
		} else {
			dbg(0) << "Wrong Comm Class!!! EXIT" << endl;
			exit(EXIT_FAILURE);
		}



		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		dbg(0)<<"start SwitchCtrl access..."<<endl;

		srand ( time(NULL) ); // use individual seed for each run
		bool fatal_failure = false;


		if (jtag->GetFPGADesignVersion() < 961) {
			std::cout << "unsupported bitfile revision: " << jtag->GetFPGADesignVersion() << std::endl;
			exit(EXIT_FAILURE);
		}

		const size_t max_fail_cnt = 100;
		size_t fail_cnt = 0;
		for(size_t ii=0; ii < measurements.size(); ii++) {
			measurement& m = measurements[ii];
			bool fail = false;
			for (size_t avg=0; avg < avg_runs; avg++) {
				std::cout << "arbiterdelay " << m.arbiterdelay << std::endl;
				if (!local_comm_hs->is_k7fpga())
					comm->jtag->SetARQTimings(m.arbiterdelay, m.fpga_timeouts[0], m.fpga_timeouts[1]);

				// toggle arq reset one more time to update arq timings
				Stage2Comm::set_fpga_reset(curr_ip, false, false, false, false, true);
				Stage2Comm::set_fpga_reset(curr_ip, false, false, false, false, false);

				m.results.resize(1 + m.results.size()); // add new result
				measurement::result& r = m.results.back();

				local_comm_hs->hicann_ack_timeout[0] = m.hicann_timeouts[0];
				local_comm_hs->hicann_ack_timeout[1] = m.hicann_timeouts[1];
				local_comm_hs->hicann_resend_timeout[0] = m.hicann_timeouts[2];
				local_comm_hs->hicann_resend_timeout[1] = m.hicann_timeouts[3];
				reset_arq(used_hicanns, local_comm_hs);

				// ok, get some debug registers multiple times (to be sure ;))
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;

				std::cout << print_arq_debug_sequence_numbers(local_comm_hs) << std::endl;
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;
				// TODO: assert for all zero!

				// all tag 0 and all tag 1
				std::cout << m.used_tags.to_string() << std::endl;

				// enable performance test
				std::cout << "enabling performance test" << std::endl;
				jtag->SetARQPerformanceTest(true  /*enable*/, m.num_packets, static_cast<uint16_t>(m.used_tags.to_ulong()));


				// wait for completion of test
				size_t sleep_time = 200*1000; // us
				size_t cnt = 1+m.num_packets/sleep_time;
				usleep(200*1000); // just wait a bit
				for (cnt; cnt > 0; cnt--) {
					if (jtag->GetARQPerformanceTestResults(r.test_time_push, r.test_time_pop))
						break;
					usleep(sleep_time);
					std::cout << "." << std::flush;
				}
				std::cout << std::endl;
				if (!cnt) {
					std::cout << "aborted due to iteration limit" << std::endl;
					fail = true;
				}

				r.dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();

				// disable performance test
				jtag->SetARQPerformanceTest(false /*enable*/, 0, 0);

				logmydata(m.num_packets, m.used_tags, r.test_time_push, r.test_time_pop);

				std::cout << print_arq_debug_sequence_numbers(local_comm_hs) << std::endl;
				std::cout << print_arq_debug_packet_count(local_comm_hs) << std::endl;

				logmydata(m.num_packets, m.used_tags, r.test_time_push, r.test_time_pop);

			}
			if (fail)
				fail_cnt++;
			else
				fail_cnt = 0; // reset
			m.fail = fail;
			std::cout << m << std::endl;
			if (fail_cnt > max_fail_cnt) {
				fatal_failure = true;
				result = false;
				break;
			}
		}
		// disable arq on fpga
		Stage2Comm::set_fpga_reset(curr_ip, false, false, false, false, true);

		if (fatal_failure) {
			std::cerr << "fatal failure... " << fail_cnt << "fails." << std::endl;
		}

		return !result; // shell wants 0 for success
	}


	void logmydata(size_t num_packets, std::bitset<16> used_tags, uint32_t test_time_push, uint32_t test_time_pop) {
		std::cout << "num_packets         = " << num_packets << "\n"
		          << "hicann_arq_tag_mask = " << used_tags.to_string() << "\n"
		          << "test_time_push      = " << test_time_push << " (" << 1.0*test_time_push/num_packets << " cycles/packet)" << "\n"
		          << "test_time_pop       = " << test_time_pop  << " (" << 1.0*test_time_pop/num_packets  << " cycles/packet)" << std::endl;
#ifndef NCSIM
		ofstream logfile("tmvk_arqperftest.log");
		logfile << "num_packets         = " << num_packets << "\n"
		        << "hicann_arq_tag_mask = " << used_tags.to_string() << "\n"
		        << "test_time_push      = " << test_time_push << " (" << 1.0*test_time_push/num_packets << " cycles/packet)" << "\n"
		        << "test_time_pop       = " << test_time_pop  << " (" << 1.0*test_time_pop/num_packets  << " cycles/packet)" << std::endl;
#endif
	}


	std::string print_arq_debug_packet_count(S2C_JtagPhys2FpgaArq* local_comm_hs) {
		std::stringstream sstr;
		jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
		sstr << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		return sstr.str();
	}


	std::string print_arq_debug_sequence_numbers(S2C_JtagPhys2FpgaArq* local_comm_hs) {
		std::stringstream sstr;
		// get all arq seq numbers from all (4) segments
		for (int ii=0; ii < 4; ii++) {
			jtag_cmdbase::arqdout_t dout = local_comm_hs->jtag->GetARQDout(ii);
			sstr << "ARQ Sequence counters after experiment (ARQDout): " << " " << dec
				<< dout[0] << " " << dout[1] << " " << dout[2] << " " << dout[3] << " "
				<< dout[4] << " " << dout[5] << " " << dout[6] << " " << dout[7] << "\n";
		}

		jtag_cmdbase::arqcounters_t cntrs = local_comm_hs->jtag->GetARQCounters();
		uint32_t rcounter = cntrs.first, wcounter = cntrs.second;
		sstr << "ARQ read (from HW) counter: " << dec <<
			(rcounter & 0x7fffffff)
			<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
			<< "ARQ write (to HW) counter: " <<
			(wcounter & 0x7fffffff)
			<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)\n";
		return sstr.str();
	}
};

std::ostream& operator<< (std::ostream& stream, const TmVKARQPerfTest::measurement& m) {
	std::vector<double> pop_vals, push_vals, down_reg, up_reg;
	for (size_t jjj = 0; jjj < m.results.size(); jjj++) {
		const TmVKARQPerfTest::measurement::result &r = m.results[jjj];
		pop_vals.push_back(r.test_time_pop);
		push_vals.push_back(r.test_time_push);
		down_reg.push_back(r.dregs.first);
		up_reg.push_back(r.dregs.second);
		//jtag_cmdbase::arqdout_t dout[4]; // arq counter (post-experiment)
		//jtag_cmdbase::arqcounters_t cntrs; // arq upper layer counter (post-experiment)
	}
	stream << "num_packets " << m.num_packets << std::endl;
	stream << "used_tags " << m.used_tags.to_string() << std::endl;
	stream << "hicann timeouts " << m.hicann_timeouts[0] << " " << m.hicann_timeouts[1] << " " << m.hicann_timeouts[2] << " " << m.hicann_timeouts[3] << std::endl;
	stream << "fpga timeouts " << m.fpga_timeouts[0] << " " << m.fpga_timeouts[1] << std::endl;
	stream << "arbiterdelay " << m.arbiterdelay << std::endl;
	if (!m.results.size())
		return stream;
	double push_mean = mean(push_vals) / m.num_packets;
	double pop_mean = mean(pop_vals) / m.num_packets;
	double push_stdev = stdev(push_vals) / m.num_packets;
	double pop_stdev = stdev(pop_vals) / m.num_packets;
	double mean_ratio_down  = mean(down_reg)/m.num_packets - 1.0;
	double mean_ratio_up    = mean(up_reg)/m.num_packets - 1.0;
	double stdev_ratio_down = stdev(down_reg)/m.num_packets;
	double stdev_ratio_up   = stdev(up_reg)/m.num_packets;
	stream << "\tpush " << push_mean << "+/-" << push_stdev << std::endl;
	stream << "\tpop  " << pop_mean  << "+/-" << pop_stdev  << std::endl;
	stream << "\tretransmit ratio down " << mean_ratio_down << "+/-" << stdev_ratio_down << std::endl;
	stream << "\tretransmit ratio up   " << mean_ratio_up   << "+/-" << stdev_ratio_up   << std::endl;
	if (m.fail)
		stream << "FAILIT   ";
	else
		stream << "MACHINEIT";
	stream << " " << m.num_packets
	       << " " << m.used_tags.to_ulong()
	       << " " << m.hicann_timeouts[0] << " " << m.hicann_timeouts[1] << " " << m.hicann_timeouts[2] << " " << m.hicann_timeouts[3]
	       << " " << m.fpga_timeouts[0]   << " " << m.fpga_timeouts[1]
	       << " " << m.arbiterdelay
	       << " " << push_mean       << " " << push_stdev
	       << " " << pop_mean        << " " << pop_stdev
	       << " " << mean_ratio_down << " " << stdev_ratio_down
	       << " " << mean_ratio_up   << " " << stdev_ratio_up
	       << " " << std::endl;
	return stream;
}

std::istream& operator>> (std::istream& stream, TmVKARQPerfTest::measurement& m) {
	// read input data
	stream.unsetf(std::ios_base::basefield);
	stream >> m.num_packets;
	if (stream.eof())
		return stream;
	stream >> m.used_tags;
	stream >> m.hicann_timeouts[0]
	       >> m.hicann_timeouts[1]
	       >> m.hicann_timeouts[2]
	       >> m.hicann_timeouts[3];
	stream >> m.fpga_timeouts[0];
	stream >> m.fpga_timeouts[1];
	stream >> m.arbiterdelay;
	std::stringstream ss;
	if ((m.num_packets < 15) ||
		(m.used_tags.to_ulong() < 1) ||
		(m.hicann_timeouts[0] < 2) ||
		(m.hicann_timeouts[1] < 2) ||
		(m.hicann_timeouts[2] < 2) ||
		(m.hicann_timeouts[3] < 2) ||
		(m.fpga_timeouts[0] < 2) ||
		(m.fpga_timeouts[1] < 2) ||
		(m.arbiterdelay < 1)) {
		ss << m;
		throw std::runtime_error(std::string("wrong input:\n") + ss.str());
	} else {
		ss << m;
//		std::cout << "read some stuff:\n" << m << std::endl;
	}
	return stream;
}


struct LteeTmvvkArqPerfTest : public Listee<Testmode> {
	LteeTmvvkArqPerfTest() : Listee<Testmode>(string("tmvk_arqperftest"), string("Vitalis DNC-Bugrate / FPGA-HICANN ARQ performance tests")){};
	Testmode * create(){return new TmVKARQPerfTest();};
};
LteeTmvvkArqPerfTest ListeeTmvkArqPerfTest;
