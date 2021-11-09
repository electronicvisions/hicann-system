// Company         :   kip
// Author          :   Andreas Grübl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tmjenkins_switchramtest_arq.cpp
//                    			
// Create Date     :   4/2013
//------------------------------------------------------------------------

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

//only if needed
#include "l1switch_control.h" //layer 1 switch control class

using namespace facets;
using namespace std;

class TmJenkinsSwitchRamTest : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmjenkins_switchramtest_arq"; return ss.str();}
public:

	HicannCtrl *hc;
	L1SwitchControl *lc;

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
		 	log(Logger::INFO) << "ERROR: object 'chip' in testmode not set, abort";
			return 0;
		}
		if (!jtag) {
		 	log(Logger::INFO) << "ERROR: object 'jtag' in testmode not set, abort";
			return 0;
		}

		uint hicannr = 0;
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		lc = &hc->getLC(HicannCtrl::L1SWITCH_TOP_LEFT);
		

		// ----------------------------------------------------
		// reset everything
		// ----------------------------------------------------
		FPGAControl *fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1);
		

		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------
	 	log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr(), false, true) != Stage2Comm::ok) {
		 	log(Logger::INFO) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
	 	log(Logger::INFO) << "Init() ok" << endl;


		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		std::vector<int> testdatas;

		for(uint i=startaddr;i<=maxaddr;i++){
			testdata = 0;
			while(testdata == 0)  // avoid testing against reset value
				testdata = rand()%(1<<datawidth);
			lc->write_cfg(i, testdata);
			testdatas.push_back(testdata);
		}
		comm->Flush();


		// ----------------------------------------------------
		// read from switch ram using FPGA ARQ
		// ----------------------------------------------------

		for(uint i=startaddr;i<=maxaddr;i++){
			lc->read_cfg(i);
		}


		// ----------------------------------------------------
		// verify result
		// ----------------------------------------------------

		for(uint i=startaddr;i<=maxaddr;i++){
			testdata = testdatas.at(i);
			lc->get_read_cfg(rcvaddr, rcvdata);
			
			if(i==rcvaddr && testdata==rcvdata)
				log(Logger::INFO) << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": " << "OK :-)";
			else{
				log(Logger::INFO) << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": " << "ERROR :-(";
				result = false;
			}
		}

		return result;
	}
};


class LteeTmJenkinsSwitchRamTest : public Listee<Testmode>{
public:
	LteeTmJenkinsSwitchRamTest() : Listee<Testmode>(string("tmjenkins_switchramtest_arq"), string("ramtest of L1 switch configuration memory using FPGA ARQ")){};
	Testmode * create(){return new TmJenkinsSwitchRamTest();};
};

LteeTmJenkinsSwitchRamTest ListeeTmJenkinsSwitchRamTest;

