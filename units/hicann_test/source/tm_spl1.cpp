// Company         :   kip
// Author          :   Andreas Grübl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//------------------------------------------------------------------------


//example testmode for spl1 test via JTAG

#include "common.h"

#include "hicann_ctrl.h"
#include "testmode.h" 

#include "linklayer.h"

//only if needed
#include "neuron_control.h"   //neuron / "analog" SPL1 control class
#include "spl1_control.h"     //spl1 control class

using namespace std;
using namespace facets;

class TmSPL1 : public Testmode{

public:

	HicannCtrl *hc; 
	NeuronControl *nc;	
	SPL1Control *spc;
	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	bool test() 
	{
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "fpgai2cag: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "fpgai2cag: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		uint hicannr = 0;
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		// get pointers to control modules
		spc = &hc->getSPL1Control();
		nc  = &hc->getNC();


		cout << "reset test logic" << endl;
		jtag->reset_jtag();

	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		char c;
		bool cont=true;
		do{

			cout << "Select test option:" << endl;
			cout << "1: reset jtag " << endl;
			cout << "2: perform SPL1 loopback test via HICANN JTAG interface" << endl;
			cout << "3: reset arq" << endl;
			cout << "4: read value of system time counter" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){
		
			case '1':{
				jtag->reset_jtag();
			} break;
			
			case '2':{
				uint64_t systime = 0xffffffff;
				uint neuron  = 0x83;
				
				jtag->HICANN_set_test(0);
				jtag->HICANN_set_layer1_mode(0x55);
				
				nc->write_data(0x3, 0x55aa);
				
				spc->write_cfg(0x55ff);
				
				jtag->HICANN_set_test(2);
				
				jtag->HICANN_read_systime(systime);
				
				neuron = 0x23;
				uint64 jtag_sendpulse = (neuron<<15) | ((systime+100)&0x7fff);
				
				jtag->HICANN_set_double_pulse(0);
				jtag->HICANN_start_pulse_packet(jtag_sendpulse);
				
		    // wait ???
				
				uint64_t jtag_recpulse = 0;
				jtag->HICANN_get_rx_data(jtag_recpulse);
				
				cout << "Pulse event 0x" << hex << jtag_recpulse << endl;
				
				if ((jtag_recpulse>>15) != (neuron+1*64)){
					cout << "TUD_TESTBENCH:ERROR:TESTFAIL: Single pulse packet not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
				} else {
					cout << "TUD_TESTBENCH:NOTE:TESTPASS: Transmission of pulse packet via JTAG->HICANN(L1)->JTAG successful." << endl;
				}

			} break;
			
			case '3':{
			 	dbg(0) << "Try Init() ..." ;

				if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
				 	dbg(0) << "ERROR: Init failed, abort" << endl;
					return 0;
				}
	
			 	dbg(0) << "Init() ok" << endl;
				
				dbg(0) << "Reset software part of ARQ.";
				ll->arq.Reset();
				
			} break;
			
			case '4': {
				uint64_t syst=0xf0f0;
				if(!jtag->HICANN_read_systime(syst))
					cout << "read systime failed!" << endl;
				else
					cout << "--read systime: " << syst << endl;
			} break;

			case 'x': cont = false; 
			}

		} while(cont);

		return true;
	}
};


class LteeTmSPL1 : public Listee<Testmode>{
public:
	LteeTmSPL1() : Listee<Testmode>(string("tm_spl1"),string("test spl1 interface on HICANN")){};
	Testmode * create(){return new TmSPL1();};
};

LteeTmSPL1 ListeeTmSPL1;

