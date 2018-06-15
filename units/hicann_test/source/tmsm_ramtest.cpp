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
	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	vector<bool> get_data(int data) {
		vector<bool> d;
		d.clear();
		for(int i=0;i<16;i++) d.push_back(((data >> i) & 1) == 1);
		return d;
	}

	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	void clear() {
		for(int i = 0; i < 64; i++){
			write(i,0);
		}
}
	void write(int addr, uint value) {
		lc->write_cfg(addr, get_data(value), 10);

	 }
	void read(int addr) { 
		lc->read_cfg(addr, 10);

	 }

	// get readdata directly from tag0 linklayer until correctly implemented by switch_loc
	bool rdata(uint& value) {
		vector<bool>data;
		uint taddr;//, tcfg;
		uint deci = 1;
		lc->get_read_cfg(taddr, data);
		if(data.size()<1) return false;
		value = 0;
		for(uint i = 0; i < data.size(); i++){
			value+=(deci*data[i]);
			deci*=2;
		}		
		return true;
	}

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
		uint hicannr = 0;
    
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		lc = hc->getLC_cl();
		
		ll = hc->tags[0];
		ll->SetDebugLevel(2);

		// reset test logic
		jtag->reset_jtag();

		// disable DNC interface
		jtag->HICAN_set_reset(0);

		cout << "tm_ramtest:Ram Test starting ...." << endl;

                //initialize JTAG read access:
                comm->Clear();
                
                cout << "tm_ramtest: Asserting ARQ reset via JTAG..." << endl;
                // Note: jtag commands defined in "jtag_emulator.h"
                jtag->HICANN_arq_write_ctrl(CMD3_ARQ_CTRL_RESET, CMD3_ARQ_CTRL_RESET);
                jtag->HICANN_arq_write_ctrl(0x0, 0x0);
                
                // set timeouts
                cout << "tm_ramtest: Setting timeout values via JTAG..." << endl;
                jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
                jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1
                //
/*
		for(int i=0;i<1;i++){
                	chip[FPGA_COUNT+DNC_COUNT+hicannr]->tags[i]->Start();
                        cout << "tm_ramtest_mini: Starting LinkLayer for tag " << i << " on chip " << hicannr << endl;
                }
*/


		// ----------------------------------------------------
		// do ramtest of switch matrices
		// ----------------------------------------------------

		int mem_base  = 0;
		int mem_size  = 72;
		int data_size = 16;  // 4 bit data entries
		RamTestUint ramtest(this, mem_base, mem_size, data_size);
		ramtest.Clear();
		classname = ClassName();
		debug = true;
		ramtest.SetDebug(false);

		dbg(3) << "starting test of switch matrices memory ... " << endl;
		int ret = ramtest.Test(11);

		return ret;
	}
};


class LteeTmRamTest : public Listee<Testmode>{
public:
	LteeTmRamTest() : Listee<Testmode>(string("tmsm_ramtest"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmRamTest();};
};

LteeTmRamTest ListeeTmRamTest;

