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

class TmJtagBulkTest : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmag_jtagbulktest"; return ss.str();}
public:

	HicannCtrl *hc; 
	L1SwitchControl *lc;		

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		// perform jtag reset and disable(?) FPGA reset
		jtag->reset_jtag();

		jtag->FPGA_set_fpga_ctrl(1);
		jtag->FPGA_set_fpga_ctrl(0);

		uint hicannr = 0;
    
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		lc = hc->getLC_cl();

		// read jtag id
		uint64 jtag_id = 0;


//		jtag->set_jtag_instr_chain (CMD_READID, POS_FPGA);
//		jtag->get_jtag_data_chain(jtag_id,64,POS_FPGA);
		jtag->read_id(jtag_id, jtag->pos_fpga);

		cout << "id: 0x" << hex << jtag_id << endl;

		// init comm class
	 	dbg(0) << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
	 	dbg(0) << "Init() ok" << endl;

		// ----------------------------------------------------
		// do ramtest of switch matrices
		// ----------------------------------------------------

		uint startaddr = 0;
		uint maxaddr   = 63;
		uint datawidth = 4;

		uint testdata = 0;
		uint rcvdata  = 0;
		uint rcvaddr  = 0;

		bool result = true;

		srand(1);
		
		for(uint i=startaddr;i<=maxaddr;i++){
			testdata = rand()%(1<<datawidth);
			lc->write_cfg(i, testdata);
			lc->read_cfg(i);
			
			lc->get_read_cfg(rcvaddr, rcvdata);
			
			cout << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
			
			if(i==rcvaddr && testdata==rcvdata)
				cout << "OK :-)" << endl;
			else{
				cout << "ERROR :-(" << endl;
				result = false;
				//break;
			}
		}

		return result;
	}
};


class LteeTmJtagBulkTest : public Listee<Testmode>{
public:
	LteeTmJtagBulkTest() : Listee<Testmode>(string("tmag_jtagbulktest"), string("test bulk mode of jtag cable")){};
	Testmode * create(){return new TmJtagBulkTest();};
};

LteeTmJtagBulkTest ListeeTmJtagBulkTest;

