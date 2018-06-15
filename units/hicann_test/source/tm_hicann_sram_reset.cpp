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
	HicannCtrl *hc, *hc_jtag;

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
		uint hicannr = 0;

		// SRAM controller timing (used for all controllers)
		uint read_delay      = 64;
		uint setup_precharge = 8;
		uint write_delay     = 8;

		if (!jtag) {
		 	log(Logger::ERROR) << "object 'jtag' in testmode not set, abort";
			return 0;
		}

		// assume JTAG object is valid, generate JTAG-only comm stuff locally:
		S2C_JtagPhys* s2c_jtag;
		s2c_jtag   = new S2C_JtagPhys(comm->getConnId(), jtag, false);
		hc_jtag    = new HicannCtrl(s2c_jtag,hicannr);

		// set to 125 MHz
		set_pll(5,2);

		// assume that only comm init stuff and no resets are neccessary...

		// ####################################################################################
		// loop over HICANNs in reticle:
		for(uint hc_jtnr = 0; hc_jtnr < 8; hc_jtnr++){

			jtag->set_hicann_pos(hc_jtnr);
			hc_jtag->setAddr(hc_jtnr);

			// ----------------------------------------------------
			// Initialize JTAG only comm for initial readback: ARQ reset and other stuff
			// ----------------------------------------------------
			if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
				log(Logger::ERROR) << "Init failed, abort";
				return 0;
			}

			log(Logger::INFO) << "Setting all full-custom SRAMS of HICANN " << hc_jtnr << " to 0";

			hc_jtag->getSPL1Control().write_reset();

			// assume that all enum values are consecutive (true for HW indices)
			// and counted C-style (top-left to bottom-right)
			for(uint i = HicannCtrl::L1SWITCH_TOP_LEFT ; i != HicannCtrl::L1SWITCH_BOTTOM_RIGHT+1 ; i++) {
				hc_jtag->getLC(i).reset();
			}

			for(uint i = HicannCtrl::REPEATER_TOP_LEFT ; i != HicannCtrl::REPEATER_BOTTOM_RIGHT+1 ; i++) {
				// set SRAM controller timings, first:
				hc_jtag->getRC(i).set_sram_timings(read_delay, setup_precharge, write_delay);
				hc_jtag->getRC(i).reset();
			}

			for(uint i = HicannCtrl::SYNAPSE_TOP ; i != HicannCtrl::SYNAPSE_BOTTOM+1 ; i++) {
				// set SRAM controller timings, first:
				hc_jtag->getSC(i).set_sram_timings(read_delay, setup_precharge, write_delay);
				hc_jtag->getSC(i).reset_drivers();
			}

			// FG controller reset has no real effect (no custom srams there) - doesn't have to be used.
			//for(uint i=HicannCtrl::FG_TOP_LEFT ; i != HicannCtrl::FG_BOTTOM_RIGHT ; i++)
			//	hc_jtag->getFC(i).reset();

			// set SRAM controller timings, first:
			hc_jtag->getNBC().set_sram_timings(read_delay, setup_precharge, write_delay);
			hc_jtag->getNBC().reset();

			hc_jtag->getNC().nc_reset();

			// super nice parametrization...
			if (std::getenv("I_KNOW_WHAT_I_AM_CLEARING") != NULL) {
				log(Logger::INFO) << "Setting all synapses of HICANN " << hc_jtnr << " to 0";
				hc_jtag->getSC(HicannCtrl::SYNAPSE_TOP).reset_all();
				hc_jtag->getSC(HicannCtrl::SYNAPSE_BOTTOM).reset_all();
			}
		}

		delete s2c_jtag;
		delete hc_jtag;

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
