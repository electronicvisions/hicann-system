// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_plltest.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   3/2009
//------------------------------------------------------------------------

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
#include "linklayer.h"
#include "testmode.h" 
#include "l1switch_control.h"
#include "ramtest.h"
#include "jtag_emulator.h"
#include "map" // for pair

using namespace facets;

class TmPllTest : public Testmode, public RamTestIntfUint {
	virtual string ClassName() {stringstream ss; ss << "tm_plltest"; return ss.str();}
public:

	HicannCtrl *hc; 
	LinkLayer *ll;

	// -----------------------------------------------------	
	// PLL constants
	// -----------------------------------------------------	

	static const int PLL_REF_MHZ   = 50;
	static const int PLL_SPEED_MHZ = 100;  // limit between slow and fast setting

	enum PLL_FRANGE {PLL_SLOW=0, PLL_FAST=1};
	enum PLL_POWER  {PLL_DOWN=0, PLL_UP=1};
	enum PLL_TEST   {PLL_TESTOFF=0, PLL_TESTON=1};

	// JTAG commands defined in "jtag_emulator.h"
	
	void set_pll_fx(int n, int m) {
		if (1.0 * PLL_REF_MHZ * n / m >= 1.0 * PLL_SPEED_MHZ) {
			jtag->HICANN_set_pll_far_ctrl(m, n, PLL_DOWN, PLL_FAST, PLL_TESTOFF);
			jtag->HICANN_set_pll_far_ctrl(m, n, PLL_UP, PLL_FAST, PLL_TESTOFF);
		}
		else {
			jtag->HICANN_set_pll_far_ctrl(m, n, PLL_DOWN, PLL_SLOW, PLL_TESTOFF);
			jtag->HICANN_set_pll_far_ctrl(m, n, PLL_UP, PLL_SLOW, PLL_TESTOFF);
		}

		ll->arq.Reset();
        jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, 0x0);
        wait(100, SC_NS);
        jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		ll->arq.Reset();

		wait (10, SC_US); // documentation requires < 500 us (!)

		ll->arq.Reset();
        jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, 0x0);
        wait(100, SC_NS);
        jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		ll->arq.Reset();
	}


	// -----------------------------------------------------	
	// use ramtest to switch matrices for PLL / DNC communication test
	// -----------------------------------------------------	

	L1SwitchControl *lc;		
//deprecated since loc is known to L1SwitchControl class!	switch_loc loc;

	void clear() {};
	void write(int addr, uint value) { lc->write_cfg(addr, value, 10); }
	void read(int addr) { lc->read_cfg(addr, 10); }
	// get readdata directly from tag0 linklayer until correctly implemented by switch_loc
	bool rdata(uint& value) { cmd_pck_t p; int r=ll->get_data(&p, false); value=p.data; return (bool)r; }


	// -----------------------------------------------------
	// test func	
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "tm_plltest: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "tm_plltest: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		// ----------------------------------------------------
		// prepare ramtest to switch matrices
		// ----------------------------------------------------

		const int TAG          = 0;

	    const int MEM_BASE     = 0;
		const int MEM_SIZE     = 16; 
		const int MEM_WIDTH    = 4;
		const int MEM_TEST_NUM = MEM_SIZE*8;

		hc= (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		ll= hc->tags[TAG];
		ll->SetDebugLevel(3); // no ARQ deep debugging

		lc = hc->getLC_cl(); // use switch matrix for test		
//		loc = CBL;

		RamTestUint ramtest(this, MEM_BASE, MEM_SIZE, 1<<MEM_WIDTH);
		classname = ClassName();
		debug = true;
		ramtest.SetDebug(true);

		// ----------------------------------------------------
		// PLL test
		// ----------------------------------------------------

		cout << "tm_plltest: PLL Test starting ...." << endl;
		cout << "tm_plltest: use parameter 'No_initial_unknow = 1' on PLL to avoid x signals in simulation(!)" << endl;

		typedef pair<int,int> testset;
		vector<testset> t;
		t.push_back(testset(16,3)); // 266
		t.push_back(testset(61, 12)); // ~ 250   lot of possible timing errors
		t.push_back(testset(5, 1)); // 250
		t.push_back(testset(9, 2)); // 225
		t.push_back(testset(4, 1)); // 200
		t.push_back(testset(3, 1)); // 150
		t.push_back(testset(5, 2)); // 125
		t.push_back(testset(2, 1)); // 100
		t.push_back(testset(5, 3)); //  83
		t.push_back(testset(63, 62)); //  ~50  with lots of possible timing errors
		t.push_back(testset(1, 1)); //  50

		for (int i=0; i<(int)t.size(); i++)
		{
			int m=t[i].first; int n=t[i].second;
			cout << "tm_plltest: testing PLL factor "<<m<<", divider "<<n<<" ("<<1.0*PLL_REF_MHZ*m/n<<" MHz)" << endl;
			set_pll_fx(m, n);
			if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::incremental)) return 0;
		}

		wait (5, SC_US);
		return true;
	};
};


class LteeTmPllTest : public Listee<Testmode>{
public:
	LteeTmPllTest() : Listee<Testmode>(string("tm_plltest"),string("test min/max pll timings")){};
	Testmode * create(){return new TmPllTest();};
};

LteeTmPllTest ListeeTmPllTest;

