/*
Test for FPGA Stats readout
*/

#ifdef NCSIM
#error testmode not implemented for ncsim
#endif

#include "testmode.h"
#include "fpga_control.h"
#include "s2c_jtagphys_2fpga_arq.h"

extern "C" {
// uintXX_t
#include <stdint.h>
}
// Software ARQ
#include "sctrltp/us_sctp_if.h"

using namespace facets;
using namespace sctrltp;

class TmCmFPGAStats : public Testmode {
protected:
	virtual std::string ClassName() {
		std::stringstream ss;
		ss << "tmcm_fpgastats";
		return ss.str();
	}

public:

	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------
	bool test() {

	//initializing sending values
	static struct sctp_descr<>* desc;
	desc = open_conn<sctrltp::Parameters<>>(shm_name.c_str());
	if (!desc) {
		std::cerr << "Error: make sure Core and testbench are up\n";
		return 1;
	}
	FPGAControl fc = FPGAControl(comm,0);

	// fc.print_all_stats() is not used because logger is not
	// properly supported in tests2
	uint64_t tracepc =  fc.get_trace_pulse_count();
	uint64_t pbpc = fc.get_pb_pulse_count();
	uint64_t debug_reg_rx = fc.get_network_debug_reg_rx();
	uint64_t debug_reg_tx = fc.get_network_debug_reg_tx();
	uint64_t ulr = fc.get_ul_packet_cnt_r();
	uint64_t ulw = fc.get_ul_packet_cnt_w();
	bool dirty = fc.get_git_dirty_flag();
	uint32_t hash = fc.get_bitfile_git_hash();
	bool pbrelerror = fc.get_pb_release_error();
	bool fifooverflow = fc.get_pb2arq_fifo_overflow();
	bool crc = fc.get_crc_counter();
	std::cout << std::dec << "FPGA Stats:\n"
	   << "Trace Pulse Count: " << tracepc << " | PB pulse count: " << pbpc
	   << "\nUplink packet counter reads: " << ulr << " | writes: " << ulw
	   << "\nNetwork debug register rx: " << debug_reg_rx << " | tx: " << debug_reg_tx
	   << "\nGit hash: 0x" << std::hex << hash << std::dec << " | Dirty flag: " << dirty
	   << "\nPb release error: " << pbrelerror << " | fifo overflow: " << fifooverflow
	   << "\nCRC counter: " << std::hex << crc << std::dec
	   << std::endl;

	return EXIT_SUCCESS;
	}
};

class LteeTmCmFPGAStats : public Listee<Testmode>{
public:
	LteeTmCmFPGAStats() :
		Listee<Testmode>(std::string("tmcm_fpgastats"),
		std::string("Test for FPGA stats module"))
	{}
	Testmode * create(){
		return new TmCmFPGAStats();
	}
};

LteeTmCmFPGAStats  ListeeTmCmFPGAStats;
