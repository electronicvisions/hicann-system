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
using namespace std;

class TmSwitchRamTest2HC : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmag_switchramtest"; return ss.str();}
public:

	ci_data_t rcvdata;
	ci_addr_t rcvaddr;


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {

		static const uint nhc = 2;
		HicannCtrl*      hc[nhc];
		L1SwitchControl* lc[nhc];
		
		// generate HICANNs locally:
		for (uint c=0;c<nhc;c++)
			hc[c] = new HicannCtrl(comm,c);
		
		// get pointer to left crossbar layer 1 switch control class of both HICANNs
		for (uint c=0;c<nhc;c++)
			lc[c] = &hc[c]->getLC_cl();
		

		jtag->reset_jtag();

	 	dbg(0) << "Try Init() ..." ;
		for (uint c=0;c<2;c++){
			if (hc[c]->GetCommObj()->Init(hc[c]->addr()) != Stage2Comm::ok) {
			 	dbg(0) << "ERROR: Init HICANN " << c << " failed, abort" << endl;
				return 0;
			}
		}
	 	dbg(0) << "Init() ok" << endl;

		// read JTAG ID of HICANN that was selected on the command line:
		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;


		// ----------------------------------------------------
		// do ramtest of switch matrices
		// ----------------------------------------------------

		uint startaddr = 0;
		uint maxaddr   = 63;
		uint datawidth = 4;

		uint testdata = 0;
		rcvdata  = 0;
		rcvaddr  = 0;

		bool result = true;

		srand(1);
		
		dbg(0)<<"start SwitchCtrl access..."<<endl;

		for(uint i=startaddr;i<=maxaddr;i++){
			for (uint c=0;c<2;c++){
				testdata = rand()%(1<<datawidth);
				lc[c]->write_cfg(i, testdata);
				lc[c]->read_cfg(i);
				lc[c]->get_read_cfg(rcvaddr, rcvdata);
			
				cout << hex << "test HICANN " << c << ",\t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
			
				if(i==rcvaddr && testdata==rcvdata)
					cout << "OK :-)" << endl;
				else{
					cout << "ERROR :-(" << endl;
					result = false;
//					break;
				}
			}
		}

		return result;
	}
};


class LteeTmSwitchRamTest2HC : public Listee<Testmode>{
public:
	LteeTmSwitchRamTest2HC() : Listee<Testmode>(string("tmag_switchramtest_2hc"), string("ramtest of L1 switch configuration memory on 2 HICANNs in JTAG chain")){};
	Testmode * create(){return new TmSwitchRamTest2HC();};
};

LteeTmSwitchRamTest2HC ListeeTmSwitchRamTest2HC;

