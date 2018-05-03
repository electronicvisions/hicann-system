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

        std::cout << "Starting tmag_switchramtest" << std::endl;

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
		
		// generate JTAG-only comm stuff for readback:
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
		jtag->FPGA_set_fpga_ctrl(0x1);
		
		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;

#ifndef HICANN_DESIGN
		// get ARQ comm pointer, if available
		S2C_JtagPhys2FpgaArq * arq_comm = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);

		// set arbiter delay to determine wether we can trigger DNC bug...
		if (comm->is_k7fpga())
			comm->jtag->K7FPGA_SetARQTimings(0x4, 0x0c8, 0x032);
		else
			comm->jtag->SetARQTimings(0x4, 0x0c8, 0x032);

		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, true); // set reset
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, false); // unset reset
#endif


		/*
		// ----------------------------------------------------
		// Initialize JTAG only comm for initial readback: ARQ reset and other stuff
		// ----------------------------------------------------
		if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
			dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

		for(uint i=startaddr;i<=maxaddr;i++){
			lc_jtag->read_cfg(i);
			lc_jtag->get_read_cfg(rcvaddr, rcvdata);
			cout << hex << "read 0x" << rcvaddr << ", \t0x" << rcvdata << endl;
		}
		*/

		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------
	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr(), false, true) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
	 	dbg(0) << "Init() ok" << endl;


		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		dbg(0)<<"start SwitchCtrl access..."<<endl;

		// test read on tag 1
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
		// but read via jtag
#ifndef HICANN_DESIGN
		comm->Flush();
#endif

		// ----------------------------------------------------
		// Initialize JTAG only comm for readback: ARQ reset and other stuff
		// ----------------------------------------------------
		// This will crash High-Speed communication to HICANN -> do only after timeout in simulation
// #ifdef NCSIM
// 		wait(200,SC_US);
// #else
// 		usleep(200);
// #endif
		
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

#ifdef NCSIM
		wait(10,SC_US);
#else
		usleep(500);
#endif

		uint32_t rbulks = 1;
		for(uint i=startaddr;i<=maxaddr;i++){
			lc->read_cfg(i);

            std::cout << "reading value " << i << std::endl;

			if(i%rbulks == (rbulks-1) || i == maxaddr){

#ifndef HICANN_DESIGN
				if (arq_comm) {
					arq_comm->Flush();

					// ARQ counters for infering state of ARQ / high-speed links
#ifdef NCSIM
					std::cout << "Starting JTAG access at time " << sc_simulation_time()
							  << "ns" << std::endl;
#endif
					std::cout << *arq_comm  << std::endl;
				}
#endif


// #ifdef NCSIM
// 				wait(50,SC_US);
// #else
// 				usleep(50);
// #endif

				uint start = i-(i%rbulks);
				for(uint a=start; a<=i; a++){
					testdata = testdatas.at(a);

					lc->get_read_cfg(rcvaddr, rcvdata);
			
					cout << hex << "test \t0x" << a << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
			
					if(a==rcvaddr && testdata==rcvdata)
						cout << "OK :-)" << endl;
					else{
						cout << "ERROR :-(" << endl;
						result = false;
//						break;
					}
				}
			}
		}

#ifndef HICANN_DESIGN
		if (arq_comm) {
			for (uint i = 0; i < 1; i++) {
				// ARQ counters for infering state of ARQ / high-speed links
				if (arq_comm->is_k7fpga()) {
#ifdef NCSIM
					std::cout << "Starting JTAG access at time " << sc_simulation_time()
							  << "ns" << std::endl;
#endif
					std::cout << *arq_comm  << std::endl;
				}
			}
		}
#endif

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
