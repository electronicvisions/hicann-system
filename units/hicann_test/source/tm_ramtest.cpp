// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_ramtest.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   3/2009
//------------------------------------------------------------------------

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

// jtag
#include "common.h"
//#include "verification_control.h"
//#include "s2c_jtag.h"

//only if needed
#include "ctrlmod.h"
#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class

#include "ramtest.h"

using namespace facets;

class TmRamTest : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_ramtest"; return ss.str();}
public:

	HicannCtrl *hc; 
	L1SwitchControl *lc;		
	LinkLayer *ll;

	vector<bool> get_data(int data) {
		vector<bool> d;
		d.clear();
		for(int i=0;i<16;i++) d.push_back((data >> i) & 1 == 1);
		return d;
	}

	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	void clear() {};
	void write(int addr, uint value) { lc->write_cfg(addr, get_data(value), 10); }
	void read(int addr) { lc->read_cfg(addr, 10); }

	// get readdata directly from tag0 linklayer until correctly implemented by switch_loc
	bool rdata(uint& value) { cmd_pck_t p; int r=ll->get_data(&p, false); value=p.data; if(r) return true; else return false; }


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
        // get pointer to left crossbar layer 1 switch control class of HICANN 0
		lc = hc->getLC_cl();		
		ll = hc->tags[0];
		ll->SetDebugLevel(3);

		// ----------------------------------------------------
		// do ramtest of switch matrices
		// ----------------------------------------------------

		int mem_base  = 0;
		int mem_size  = 16;
		int data_size = 16;  // 4 bit data entries
		RamTestUint ramtest(this, mem_base, mem_size, data_size);
		ramtest.Clear();
		classname = ClassName();
		debug = true;
		ramtest.SetDebug(true);

		// control software ARQ
		ll->arq.ClearStats();
		ll->arq.SetTimeouts(300, 120);
		string starttime = sc_time_stamp().to_string();
		dbg(3) << "starting test of switch matrices memory ... " << endl;
		int ret = ramtest.Test(100);
		dbg(3) << "finished test, execution time from " << starttime << " to " << sc_time_stamp() << endl; 
		ll->arq.PrintStats();

		// wait last (write) commands in fifos to execute
		wait(10, SC_US);

		return ret;
	};
};


class LteeTmRamTest : public Listee<Testmode>{
public:
	LteeTmRamTest() : Listee<Testmode>(string("tm_ramtest"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmRamTest();};
};

LteeTmRamTest ListeeTmRamTest;

