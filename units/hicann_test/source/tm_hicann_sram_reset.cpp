// Company         :   kip
// Author          :   Andreas Gruebl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//
// Filename        :   tm_hicann_reset.cpp
//------------------------------------------------------------------------

#include "hicann_ctrl.h"
#include "testmode.h"

// jtag
#include "s2c_jtagphys.h"

using namespace facets;
using namespace std;

class TmHicannSramReset : public Testmode {
protected:
	virtual string ClassName() { stringstream ss; ss << "tm_hicann_sram_reset"; return ss.str(); }

public:
	HicannCtrl *hc;

	//sets the repeater frequency (use e.g. 2,1 to get 100 MHz)
	//frequency = multiplicator * 50 MHz / divisor
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1; // enable (power-down-negated)
		uint frg = 0x1; // Note (CK): frange=1 means only output between 100-300Mhz, see datasheet
		                // http://freelibrary.faraday-tech.com/AIP/FXPLL031HA0A_APGD_Datasheet.pdf
		uint tst = 0x0; // testmode enable (not available on bondpads)
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
	}

	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------

	bool test()
	{

		bool result  = true;

		// SRAM controller timing (used for all controllers)
		uint read_delay      = 64;
		uint setup_precharge = 8;
		uint write_delay     = 8;

		// set to 125 MHz
		set_pll(5,2);

		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		uint hicannr = 0;
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		// ----------------------------------------------------
		// Initialize comm class (using the one generated in tests2)
		// ----------------------------------------------------
	 	log(Logger::INFO) << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr(), false, false) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
	 	log(Logger::INFO) << "Init() ok" << endl;

		log(Logger::INFO) << "Setting all full-custom SRAMS of HICANN " << hc->addr() << " to 0";

		hc->getSPL1Control().write_reset();

		// assume that all enum values are consecutive (true for HW indices)
		// and counted C-style (top-left to bottom-right)
		for(uint i = HicannCtrl::L1SWITCH_TOP_LEFT ; i != HicannCtrl::L1SWITCH_BOTTOM_RIGHT+1 ; i++) {
			hc->getLC(i).reset();
		}

		for(uint i = HicannCtrl::REPEATER_TOP_LEFT ; i != HicannCtrl::REPEATER_BOTTOM_RIGHT+1 ; i++) {
			// set SRAM controller timings, first:
			hc->getRC(i).set_sram_timings(read_delay, setup_precharge, write_delay);
			hc->getRC(i).reset();
		}

		for(uint i = HicannCtrl::SYNAPSE_TOP ; i != HicannCtrl::SYNAPSE_BOTTOM+1 ; i++) {
			// set SRAM controller timings, first:
			hc->getSC(i).set_sram_timings(read_delay, setup_precharge, write_delay);
			hc->getSC(i).reset_drivers();
		}

		// FG controller reset has no real effect (no custom srams there) - doesn't have to be used.
		//for(uint i=HicannCtrl::FG_TOP_LEFT ; i != HicannCtrl::FG_BOTTOM_RIGHT ; i++)
		//	hc->getFC(i).reset();

		// set SRAM controller timings, first:
		hc->getNBC().set_sram_timings(read_delay, setup_precharge, write_delay);
		hc->getNBC().reset();

		hc->getNC().nc_reset();

		// super nice parametrization...
		if (std::getenv("I_KNOW_WHAT_I_AM_CLEARING") != NULL) {
			log(Logger::INFO) << "Setting all synapses of HICANN " << hc->addr() << " to 0";
			hc->getSC(HicannCtrl::SYNAPSE_TOP).reset_all();
			hc->getSC(HicannCtrl::SYNAPSE_BOTTOM).reset_all();
		}

		return result;
	}
};


class LteeTmHicannSramReset : public Listee<Testmode>
{
public:
	LteeTmHicannSramReset() :
		Listee<Testmode>(string("tm_hicann_sram_reset"),
		                 string("JTAG only: sets all full-custom memories (but synapses) to 0"))
	{};
	Testmode * create(){ return new TmHicannSramReset(); };
};

LteeTmHicannSramReset ListeeTmHicannSramReset;
