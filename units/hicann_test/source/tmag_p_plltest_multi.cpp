// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   plltestag.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   12/2008
// Last Change     :   
// by              :   
//------------------------------------------------------------------------


//example file for a custom testmode implementation

#include "common.h"

//#ifdef NCSIM
//#include "systemc.h"
//#endif

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

//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

// testmode used for faraday pll testing of real HICANN chip.
class TmPLLTestAGMulti : public Testmode{

public:

	vector<bool> get_data(int data)
	{
		vector<bool> d;
		d.clear();
		for(int i=0;i<16;i++) d.push_back(((data >> i) & 1) == 1);
		return d;
	}

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "plltestag: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "plltestag: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		cout << "plltestag: ARQ Test starting ...." << endl;

		// reset values for PLL control register:
		uint ms = 0x01;
		uint ns = 0x05;
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;

//		cout << "reset test logic" << endl;
//		jtag->reset_jtag();
//		usleep(10000);

		char c;
		bool cont=true;
		do{

			cout << "PLL control reg values: ns=0x" << hex << setw(2) << ns << " ms=0x" << ms << " pdn=" << setw(1) << pdn << " frg=" << frg << " tst=" << tst << endl << endl;

			cout << "Select test option:" << endl;
			cout << "3: set pll frequency divider" << endl;
			cout << "4: turn pll on/off" << endl;
			cout << "5: set pll frequency range" << endl;
			cout << "6: set pll test-mode" << endl;
			cout << "7: test primitive arq loopback" << endl;
			cout << "8: read value of system time counter" << endl;
			cout << "9: enable(1) / disable(0) dnc interface macro (enable for systime!)" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){
		
			case '3':{
				cout << "value=ns/ms:" << endl << "ns? "; cin >> ns;
				cout << "ms? "; cin >> ms;

				jtag->HICANN_set_pll_far_ctrl(ms, ns, pdn, frg, tst);
			} break;

			case '4':{
				cout << "pll on/off (1/0):"; cin >> pdn;

				jtag->HICANN_set_pll_far_ctrl(ms, ns, pdn, frg, tst);
			} break;

			case '5':{
				cout << "0: 20-100MHz | 1: 100-300MHz: "; cin >> frg;

				jtag->HICANN_set_pll_far_ctrl(ms, ns, pdn, frg, tst);
			} break;
			
			case '6':{
				cout << "pll test-mode on/off (1/0)"; cin >> tst;

				jtag->HICANN_set_pll_far_ctrl(ms, ns, pdn, frg, tst);
			} break;
			 
			
			case '7':{
				uint t;
				cout << "which tag (0/1)? "; cin >> t;
				
				cout << "plltest: Asserting ARQ reset via JTAG..." << endl;
				// Note: jtag commands defined in "jtag_cmdbase_arq.h"
				jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
				usleep(100000);
				jtag->HICANN_arq_write_ctrl(0x0, 0x0);

				// set timeouts
				cout << "plltest: Setting timeout values via JTAG..." << endl;
				jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
				jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1

				jtag->HICANN_set_test(0x1);
				jtag->HICANN_set_test(0x0);

				// ----------------------------------------------------
				// testing loopback
				// ----------------------------------------------------

				usleep(100000);
				cout << "plltest: enabling tag0 and tag1 loopback via JTAG..." << endl;
        jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_LOOPBACK, jtag->CMD3_ARQ_CTRL_LOOPBACK);


				// test using packet transfers via JTAG backdoor
				ci_packet_t cip,ciprec;

				cip.clear();
				ciprec.clear();
				cip.SeqValid = 1;
				cip.Write = 1;
				cip.TagID = t;

				// sent 4 test packets via jtag
				int i;
				uint64 sop;
				for (i=0; i<4;i++) {
					cip.Seq  = i;
					cip.Ack  = i-1;
					cip.Addr = i;
					cip.Data = 256*(i+1);
					sop = cip.to_uint64();
					jtag->HICANN_submit_ocp_packet(sop);
					cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;
					uint64_t rdata;
					if (!jtag->HICANN_receive_ocp_packet(rdata))
						cout << "plltest: received none" << endl;
					else {
						ciprec = rdata; // automatic cast
						cout << "plltest: received " << ciprec << endl << "-----" << endl; 
					}
				}

				// loopback is on so packets are reflected
				// sent ACK to stop ARQ in hicann repeating the packats
				cip.SeqValid = 1;
				cip.Ack = i-1;  // ack packets 0-i
				sop = cip.to_uint64();
				jtag->HICANN_submit_ocp_packet(cip.to_uint64());
				cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;

				for(int i=0; i<2; i++)
				{	
					uint64_t rdata;
					if (!jtag->HICANN_receive_ocp_packet(rdata))
						cout << "plltest: received none" << endl;
					else {
						ciprec = rdata; // automatic cast
						cout << "plltest: received " << ciprec << endl; 
					}
				}		
			} break;
			
			case '8': {
				uint tst;
				cout << "activate and deactivate reset, then press return!" << endl;
				cin >> tst;
		
				cout << "reset test logic" << endl;
				jtag->reset_jtag();
				usleep(10000);

				cout << "sending TMS_SYS_START pulse (NOT WORKING with jtag_lib_v2!)" << endl;

				cout << "reset test logic" << endl;
				jtag->reset_jtag();
				usleep(10000);

				uint64_t syst=0xf0f0;
				if(!jtag->HICANN_read_systime(syst))
					cout << "read systime failed!" << endl;
				else
					cout << "--read systime: " << syst << endl;
			} break;

			case '9': {
				int cmd=0x0;
				cout << "enable or disable (1/0)? "; cin >> cmd;
				jtag->HICANN_set_reset(cmd);
			} break;
			case 'x':cont=false;
			}
			if(kbhit()>0)cont=false;
	  }while(cont);

		return true;
	};
};


class LteeTmPLLTestAGMulti : public Listee<Testmode>{
public:
	LteeTmPLLTestAGMulti() : Listee<Testmode>(string("tmag_p_plltest_multi"),string("like plltestag with multi-device chain")){};
	Testmode * create(){return new TmPLLTestAGMulti();};
};

LteeTmPLLTestAGMulti ListeeTmPLLTestAGMulti;

