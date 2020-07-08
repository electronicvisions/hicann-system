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
#include "s2ctrl.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

// jtag
	#include "s2c_jtagphys.h"
#ifndef HICANN_DESIGN
	#include "s2c_jtagphys_2fpga.h"
	#include "s2c_jtagphys_2fpga_arq.h"
#endif

//only if needed
#include "l1switch_control.h" //layer 1 switch control class
#include "synapse_control.h"

#include "ramtest.h"

using namespace facets;
using namespace std;

class TmSwitchRamTest : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmag_switchramtest"; return ss.str();}
public:

	HicannCtrl *hc, *hc_jtag; 
	L1SwitchControl *lc, *lc_jtag;

	SynapseControl *sc;

	ci_data_t rcvdata;
	ci_addr_t rcvaddr;


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		uint startaddr = 0;
		uint maxaddr   = 111;
		uint datawidth = 16;

		uint testdata = 0;
		rcvdata  = 0;
		rcvaddr  = 0;

		bool result = true;


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
		lc = &hc->getLC(HicannCtrl::L1SWITCH_TOP_LEFT);
		
		sc = &hc->getSC(HicannCtrl::SYNAPSE_TOP);
		
		// OPTIONAL - Debug only: generate JTAG-only comm stuff for readback:
		//S2C_JtagPhys* s2c_jtag;
		//Stage2Comm *local_comm;
		//s2c_jtag   = new S2C_JtagPhys(comm->getConnId(), jtag, false);
		//local_comm = static_cast<Stage2Comm*>(s2c_jtag);
		//hc_jtag    = new HicannCtrl(local_comm,hicannr);
		//lc_jtag    = &hc_jtag->getLC_tl();


		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		FPGAControl *fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();
		
		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		log(Logger::INFO) << "HICANN ID: 0x" << hex << jtagid;

		// set arbiter delay to determine wether we can trigger DNC bug...
		if (!comm->is_k7fpga())
			comm->jtag->SetARQTimings(0x4, 0x0c8, 0x032);

		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------
	 	log(Logger::INFO) << "Try Init() ...";

		if (hc->GetCommObj()->Init(hc->addr(), false, true) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort";
			return 0;
		}
	 	log(Logger::INFO) << "Init() ok";


		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		log(Logger::INFO) << "start SwitchCtrl access...";

		// Debug only: test read on tag 1
		//sc->read_cmd(0x4002, 0);
		
		std::vector<int> testdatas;

		// write via hostal
		for(uint i=startaddr;i<=maxaddr;i++){
			testdata = 0;
			while(testdata == 0)  // avoid testing against reset value
				testdata = rand()%(1<<datawidth);
			lc->write_cfg(i, testdata);
			testdatas.push_back(testdata);
		}

		// ----------------------------------------------------
		// OPTIONAL - Debug only: Initialize JTAG only comm for readback: ARQ reset and other stuff
		// ----------------------------------------------------
		// This will crash High-Speed communication to HICANN -> do only after timeout in simulation

// 		if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
// 			dbg(0) << "ERROR: Init failed, abort" << endl;
// 			return 0;
// 		}
// 
// 		// ----------------------------------------------------
// 		// read back via JTAG
// 		// ----------------------------------------------------
// 		for(uint i=startaddr;i<=maxaddr;i++){
// 			testdata = testdatas.at(i);
// 
// 			lc_jtag->read_cfg(i);
// 			lc_jtag->get_read_cfg(rcvaddr, rcvdata);
// 			
// 			cout << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
// 			
// 			if(i==rcvaddr && testdata==rcvdata)
// 				cout << "OK :-)" << endl;
// 			else{
// 				cout << "ERROR :-(" << endl;
// 				result = false;
// //				break;
// 			}
// 		}

		uint32_t rbulks = 37;   // for testing: just a random, non-power-of-2 (esp. not 1 or 16), number...
		                        // -> this way, problems that are related to multiple-commands-in-one-packet, or 
		                        //    are related to the ARQ (window size 16) will be more clearly separable
		for(uint i=startaddr;i<=maxaddr;i++){
			lc->read_cfg(i);

			log(Logger::DEBUG0)  << "reading value " << i;

			if(i%rbulks == (rbulks-1) || i == maxaddr){

				uint start = i-(i%rbulks);
				for(uint a=start; a<=i; a++){
					testdata = testdatas.at(a);

					lc->get_read_cfg(rcvaddr, rcvdata);
			
					log(Logger::INFO) << hex << "test \t0x" << a << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": " << ((a==rcvaddr && testdata==rcvdata) ? "OK :-)" : "ERROR :-(");
			
					if(!(a==rcvaddr && testdata==rcvdata)) {
						result = false;
//						break;
					}
				}
			}
		}

//  		// ----------------------------------------------------
//  		// shutdown any remaining arq activity in HICANN...
//  		// ----------------------------------------------------
//  		if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
//  			dbg(0) << "ERROR: Init failed, abort" << endl;
//  			return 0;
//  		}
// 
		return result;
	}
};


class LteeTmSwitchRamTest : public Listee<Testmode>{
public:
	LteeTmSwitchRamTest() : Listee<Testmode>(string("tmag_switchramtest"), string("ramtest of L1 switch configuration memory")){};
	Testmode * create(){return new TmSwitchRamTest();};
};

LteeTmSwitchRamTest ListeeTmSwitchRamTest;
