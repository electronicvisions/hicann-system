// Company         :   kip
// Author          :   smillner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_currentsource.cpp
// Project Name    :   p_facets
// Subproject Name :   s_system            
//                    			
// Create Date     :   6/2010
//------------------------------------------------------------------------

#include "s2comm.h"
#include "s2ctrl.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
#include "fg_control.h"
#include "neuronbuilder_control.h"
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"

using namespace facets;

class TmCurrentsource : public Testmode  {
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	static const int TAG             = 0;


	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to fgctrl
	
	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::ERROR)<<"object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	log(Logger::ERROR)<<"object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		ll = hc->tags[TAG];
		ll->SetDebugLevel(5); // no ARQ deep debugging
		
		// ----------------------------------------------------
		// test
		// ----------------------------------------------------


		// prepare ramtest
	

		log(Logger::INFO)<< "starting...." << endl;
	
		comm->Clear();
	

		log(Logger::INFO)<< "Asserting ARQ reset via JTAG..." << endl;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
                jtag->HICANN_arq_write_ctrl(0x0, 0x0);

		log(Logger::INFO)<< "Setting timeout values via JTAG..." << endl;
                jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
                jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1
		
	   	unsigned char val=0x30;
		jtag->HICANN_set_bias(val);	


		log(Logger::INFO)<< "ok" << endl;
		return 1;
	};
};


class LteeTmCurrentsource : public Listee<Testmode>{
public:
	LteeTmCurrentsource() : Listee<Testmode>(string("tm_currentsource"), string("Programm current source from dresden and set it to analog output")){};
	Testmode * create(){return new TmCurrentsource();};
};

LteeTmCurrentsource ListeeTmCurrentsource;

