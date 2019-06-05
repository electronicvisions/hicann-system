
#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "s2comm.h"
#include "s2ctrl.h"
#include "testmode.h"

// jtag
#include "s2c_jtagphys.h"

// only if needed
#include "l1switch_control.h" //layer 1 switch control class

#include "ramtest.h"

using namespace facets;
using namespace std;

class TmSwRamJtagtest : public Testmode
{
protected:
	virtual string ClassName()
	{
		stringstream ss;
		ss << "tmag_swram_jtagtest";
		return ss.str();
	}

public:
	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------

	bool test()
	{
		HicannCtrl* hc_jtag;
		L1SwitchControl* lc_jtag;

		ci_data_t rcvdata = 0;
		ci_addr_t rcvaddr = 0;

		const uint startaddr = 0;
		const uint maxaddr = 1;
		const uint datawidth = 16;

		uint testdata = 0;

		std::deque<int> testdatas;

		bool result = TESTS2_SUCCESS;

		uint hicannr = 0;

		// generate JTAG-only comm stuff for readback:
		S2C_JtagPhys* s2c_jtag = new S2C_JtagPhys(comm->getConnId(), jtag, false);
		Stage2Comm* local_comm = static_cast<Stage2Comm*>(s2c_jtag);


		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		FPGAControl* fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1);

		uint64_t jtagid = 0xf;  // initialize with some value to see whether it's overwritten, correctly
		jtag->read_id(jtagid, jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;

		// write
		for (uint hicann_jtag_nr = 0; hicann_jtag_nr < 8; hicann_jtag_nr++) {
			// ----------------------------------------------------
			// Initialize JTAG only comm
			// ----------------------------------------------------
			hc_jtag = new HicannCtrl(local_comm, hicann_jtag_nr);
			lc_jtag = &hc_jtag->getLC_tl();

			if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
				dbg(0) << "ERROR: Init failed, abort" << endl;
				return TESTS2_FAILURE;
			}

			for (uint i = startaddr; i <= maxaddr; i++) {
				// in case non-random testdata is required:
				// testdata = hicann_jtag_nr;
				testdata = 0;
				while (testdata == 0) // avoid testing against reset value
					testdata = rand() % (1 << datawidth);
				lc_jtag->write_cfg(i, testdata);
				testdatas.push_back(testdata);
				cout << hex << "HICANN \t0x" << hicann_jtag_nr << ", \t0x" << i << ", \t0x"
				     << testdata << ": " << endl;
			}
		}

		// read
		for (uint hicann_jtag_nr = 0; hicann_jtag_nr < 8; hicann_jtag_nr++) {
			// ----------------------------------------------------
			// Initialize JTAG only comm
			// ----------------------------------------------------
			hc_jtag = new HicannCtrl(local_comm, hicann_jtag_nr);
			lc_jtag = &hc_jtag->getLC_tl();

			if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
				dbg(0) << "ERROR: Init failed, abort" << endl;
				return TESTS2_FAILURE;
			}

			for (uint i = startaddr; i <= maxaddr; i++) {
				testdata = testdatas.front();
				testdatas.pop_front();
				lc_jtag->read_cfg(i);
				lc_jtag->get_read_cfg(rcvaddr, rcvdata);

				cout << hex << "HICANN " << hicann_jtag_nr << ", test \t0x" << i << ", \t0x"
				     << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": "
				     << endl;
				if (i == rcvaddr && testdata == rcvdata)
					cout << "OK :-)" << endl;
				else {
					cout << "ERROR :-(" << endl;
					result = TESTS2_FAILURE;
				}
			}
		}

		return result;
	}
};


class LteeTmSwRamJtagtest : public Listee<Testmode>
{
public:
	LteeTmSwRamJtagtest() :
	    Listee<Testmode>(
	        string("tmag_swram_jtagtest"),
	        string("test correct JTAG access to 8 HICANNs using switch ram accesses")){};
	Testmode* create() { return new TmSwRamJtagtest(); };
};

LteeTmSwRamJtagtest ListeeTmSwRamJtagtest;
