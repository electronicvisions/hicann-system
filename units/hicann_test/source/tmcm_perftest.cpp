/*
Test for HICANNARQ communication performance:

Host sends two words to fpga via hostarq.
 - first 16 bits represent bitmask to set which HICANNs are testet wich which channels
   2 bits for each HICANN where each bit set one channel
 - second word sets how many hicann commands are looped back

FPGA sends two words to the host.
 - first word represents the duration to insert all commands into the HICANN ARQ
 - second represents the duration of receving all commands
*/


#include "common.h"
#include "hicann_ctrl.h"
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga_arq.h"
#include "s2comm.h"
#include "s2ctrl.h"
#include "testmode.h"

extern "C" {
// uintXX_t
#include <stdint.h>
// gettimeofday()
#include <sys/time.h>
}

using namespace facets;

struct perftest_result
{
	int64_t send_duration = 0;
	int64_t receive_duration = 0;
};

static perftest_result do_perftest(
	sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* arq, size_t no_cmds, std::bitset<16> testmask)
{
	sctrltp::packet<sctrltp::ParametersFcpBss1> packet;

	// send parameters to FPGA whcih start test
	packet.pdu[0] = testmask.to_ullong();
	packet.pdu[1] = no_cmds;
	packet.pid = PTYPE_PERFTEST;
	packet.len = 2;
	arq->send(packet, sctrltp::ARQStream<sctrltp::ParametersFcpBss1>::NONBLOCK);
	arq->flush();

	// wait for response from FPGA
	size_t accumulated_sleep = 0;
	size_t timeout = 10000000; // in us
	struct perftest_result result;
	// sleep timings in us (back-off to longer times; don't use >= 1s!)
	std::tr1::array<size_t, 8> const sleep_intervals = {5, 10, 100, 500, 1000, 5000, 10000, 100000};
	size_t sleep_interval_idx = 0;
	while (true) {
		if (!arq->received_packet_available()) {
			// nothing in HostARQ layer, let's sleep
			int ret;
			timespec towait;
			towait.tv_sec = 0; // we don't support sleeping >= 1s
			towait.tv_nsec = sleep_intervals[sleep_interval_idx] * 1000;
			accumulated_sleep += sleep_intervals[sleep_interval_idx];
			do {
#ifdef NCSIM
				wait(sleep_intervals[sleep_interval_idx], SC_US);
				ret = 0;
#else
				ret = nanosleep(&towait, NULL);
#endif
			} while (ret == EINTR); // sleep again if interrupted ;)
			if (ret > 0) {
				throw std::runtime_error("Perftest: cannot sleep ?!?!?");
				continue;
			}
			if (sleep_interval_idx + 1 < sleep_intervals.size())
				sleep_interval_idx++;
			if (accumulated_sleep > timeout) {
				std::cout << "timeout" << std::endl;
				result.send_duration = -1;
				result.receive_duration = -1;
				break;
			}
		} else {
			// packet received
			arq->receive(packet, sctrltp::ARQStream<sctrltp::ParametersFcpBss1>::NONBLOCK);
			if (packet.pid == PTYPE_PERFTEST) {
				if (packet.len == 18) {
					result.send_duration = packet.pdu[0];
					result.receive_duration = packet.pdu[1];
					for (size_t i = 0; i < packet.len; i++) {
						std::cout << "response word " << i << ": " << packet.pdu[i] << "\n";
					}
					break;
				} else {
					throw std::runtime_error("Error: too few/many answer words\n");
				}
			} else {
				// wrong packet type
				std::cout << "Received packet of wrong type:" << std::endl;
				std::cout << "pid: " << std::hex << packet.pid << std::endl;
				std::cout << "pdu[0]: " << std::hex << packet.pdu[0] << std::endl;
#ifndef NCSIM
				throw std::runtime_error("Error: Wrong packet type\n");
#endif
			}
		}
	}
	return result;
}

class TmCmPerftest : public Testmode
{
protected:
	virtual std::string ClassName()
	{
		std::stringstream ss;
		ss << "tmcm_hostarq_loopback";
		return ss.str();
	}

private:
public:
	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------
	bool test()
	{
		S2C_JtagPhys2FpgaArq* my_comm = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
		sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* my_arq = my_comm->getHostAL()->getARQStream();
		size_t max_command_count = 1000;
		std::bitset<16> testmask(std::string("0000000000000001"));


#ifndef NCSIM
		// parsing
		std::vector<boost::program_options::basic_option<char> >::iterator it;
		for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("no_cmds").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value.at(0));
				buffer >> max_command_count;
			}
			if (std::string("mask").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value.at(0));
				buffer >> testmask;
			}
		}
#endif // NCSIM

		if (max_command_count >= 0xfffffffff) {
			max_command_count = 0xfffffffff;
			log(Logger::WARNING) << "too many commands given, truncating to 36bit";
		}
		log(Logger::INFO) << "mask: 0x" << std::hex << testmask << " no_cmds: " << std::dec
						  << max_command_count << std::endl;

		// HICANN init
		// FIXME use tests2 api to define which HICANN to use
		std::bitset<8> used_hicanns;
		used_hicanns.set();
		if (my_comm->Init(used_hicanns) != Stage2Comm::ok) {
			log(Logger::ERROR) << "ERROR: Init failed, abort";
			return 0;
		}

		struct perftest_result curr_result;
		curr_result = do_perftest(my_arq, max_command_count, testmask);
		std::cout << "Sending " << max_command_count << " HICANN commands took " << std::dec
				  << curr_result.send_duration << " clk cycles aka "
				  << 8 * max_command_count / (curr_result.send_duration * 8e-3) << " MB/s | "
				  << 1. * curr_result.send_duration / max_command_count << " clk/word" << std::endl;
		std::cout << "Receiving " << max_command_count << " HICANN commands took " << std::dec
				  << curr_result.receive_duration << " clk cycles aka "
				  << 8 * max_command_count / (curr_result.receive_duration * 8e-3) << " MB/s | "
				  << 1. * curr_result.receive_duration / max_command_count << " clk/word"
				  << std::endl;
		return EXIT_SUCCESS;
	}
};


class LteeTmCmPerftest : public Listee<Testmode>
{
public:
	LteeTmCmPerftest()
		: Listee<Testmode>(
			  std::string("tmcm_perftest"), std::string("Test for FPGA perftest module"))
	{}
	Testmode* create() { return new TmCmPerftest(); }
};

LteeTmCmPerftest ListeeTmCmPerftest;
