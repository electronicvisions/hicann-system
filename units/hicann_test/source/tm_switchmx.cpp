// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_switchmx.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   3/2009
//------------------------------------------------------------------------
// Revisions
// smillner@12/09 updated and modified for hicann test
#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
#include "linklayer.h"
#include "testmode.h" 
#include "l1switch_control.h"
#include "ramtest.h"

using namespace facets;

class TmSwitchMx : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_switchmx"; return ss.str();}
public:

	HicannCtrl *hc; 
	LinkLayer<ci_payload,ARQPacketStage2> *ll;


	// -----------------------------------------------------	
	// access functions to switch matrixes
	// -----------------------------------------------------	

	// crossbar
 	static const int CB_BASE     = 0;
 	static const int CB_SIZE     = 64;
	static const int CB_WIDTH    = 4;

	// syndriver
 	static const int SYN_BASE    = 0;
 	static const int SYN_SIZE    = 112;
	static const int SYN_WIDTH   = 16;

	static const int TEST_NUM    = 5; // times size

	L1SwitchControl *lc;		
	switch_loc loc;
	string locs;
	int mem_base, mem_size, mem_width;

	void select(int sw)
	{
		switch (sw){
			case  0: loc=CBL; locs="CBL"; lc=hc->getLC_cl(); mem_base=CB_BASE; mem_size=CB_SIZE; mem_width=CB_WIDTH; break;
			case  1: loc=CBR; locs="CBR"; lc=hc->getLC_cr(); mem_base=CB_BASE; mem_size=CB_SIZE; mem_width=CB_WIDTH; break;
			case  2: loc=SYNTL; locs="SYNTL"; lc=hc->getLC_tl(); mem_base=SYN_BASE; mem_size=SYN_SIZE; mem_width=SYN_WIDTH; break;
			case  3: loc=SYNTR; locs="SYNTR"; lc=hc->getLC_tr(); mem_base=SYN_BASE; mem_size=SYN_SIZE; mem_width=SYN_WIDTH; break;
			case  4: loc=SYNBL; locs="SYNBL"; lc=hc->getLC_bl(); mem_base=SYN_BASE; mem_size=SYN_SIZE; mem_width=SYN_WIDTH; break;
			default: loc=SYNBR; locs="SYNBR"; lc=hc->getLC_br(); mem_base=SYN_BASE; mem_size=SYN_SIZE; mem_width=SYN_WIDTH; break;
		}
	}

	void clear() {};
	void write(int addr, uint value) { lc->write_cfg(addr, value, 10); }
	void read(int addr) { lc->read_cfg(addr, 10); }
	// get readdata directly from tag0 linklayer until correctly implemented by switch_loc
	bool rdata(uint& value) { 
                vector<bool>data;
                uint taddr;
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

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		ll = hc->tags[0];
		ll->SetDebugLevel(3); // no ARQ deep debugging
		debug_level=4;		

	// prepare ramtest
	

		dbg(0) << "starting...." << endl;
	
		comm->Clear();

		dbg(0) << "Asserting ARQ reset via JTAG..." << endl;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
                jtag->HICANN_arq_write_ctrl(0x0, 0x0);

		dbg(0) << "Setting timeout values via JTAG..." << endl;
                jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
                jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1


		
		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.Clear();
		ramtest.SetDebug(true);
		dbg(0) << hex<<endl;


		// ----------------------------------------------------
		// do ramtest within switch matrices
		// ----------------------------------------------------

		dbg(0) << "Testing switch matrices: CBL, CBR, SYNTL, SYNTR, SYNBL, SYNBR" << endl;

		// first init rams doing incremental tests
		for (int sw=0; sw<6; sw++)
		{
			select(sw);
			dbg(0) << "testing matrix " << locs << " (incremental)" << endl;
			ramtest.Resize(mem_base, mem_size, 1<<mem_width);
			if (!ramtest.Test(mem_size, RamTestUint::incremental)) return 0; 		
		}

		// random access ...
		for (int sw=0; sw<6; sw++)
		{
			select(sw);
			dbg(0) << "testing matrix " << locs << " (random)" << endl;
			ramtest.Resize(mem_base, mem_size, 1<<mem_width);
			ramtest.Init(mem_size, RamTestUint::incremental); // init compare mem with incremental values 
			if (!ramtest.Test(mem_size*TEST_NUM, RamTestUint::random)) return 0; 		
		}

		dbg(0) << "Ok" << endl;
		
		return 1;
	};
};


class LteeTmSwitchMx : public Listee<Testmode>{
public:
	LteeTmSwitchMx() : Listee<Testmode>(string("tm_switchmx"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmSwitchMx();};
};

LteeTmSwitchMx ListeeTmSwitchMx;

