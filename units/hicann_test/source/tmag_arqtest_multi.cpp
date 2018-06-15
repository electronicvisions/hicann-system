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


using namespace facets;

// testmode used for faraday pll testing of real HICANN chip.
class TmARQTestMultiAG : public Testmode{

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

		cout << "reset test logic" << endl;
		jtag->reset_jtag();

		char c;
		bool cont=true;
		do{

			cout << "PLL control reg values: ns=0x" << hex << setw(2) << ns << " ms=0x" << ms << " pdn=" << setw(1) << pdn << " frg=" << frg << " tst=" << tst << endl << endl;

			cout << "Select test option:" << endl;
			cout << "1: set HICANN control register (reset, sys_start)" << endl;
			cout << "2: Ram test using S2Comm class S2C_JtagPhysFpga" << endl;
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
		
			case '1':{
				uint t;
				cout << "8 bit hex value (RESET_N at pos 0, SYS_START at pos 4)? "; cin >> hex >> t;

				jtag->FPGA_set_hcctrl(t);
			} break;

			case '2':{
				// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
				uint hicannr = 0;
				HicannCtrl* hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];

				// get pointer to left crossbar layer 1 switch control class of HICANN 0
				L1SwitchControl* lc = hc->getLC_tl();		

				log(Logger::INFO) << "Ram test starting ...." << endl;

				// initialize JTAG read access:
				comm->Clear();

				log(Logger::INFO) << "Asserting ARQ reset via JTAG..." << endl;

		    jtag->HICANN_arq_write_ctrl(CMD3_ARQ_CTRL_RESET, CMD3_ARQ_CTRL_RESET);
		    jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		
				// set timeouts
				log(Logger::INFO) << "Setting timeout values via JTAG..." << endl;
				jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
				jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1

				// start linklayer threads 
//				for(int i=0;i<tagid_num;i++){
				//for(int i=0;i<1;i++){
				//	chip[FPGA_COUNT+DNC_COUNT+hicannr]->tags[i]->Start();
				//	log(Logger::INFO) << "Starting LinkLayer for tag " << i << " on chip " << hicannr << endl;
				//}

 				ci_payload p;
				vector<bool> data;
				uint taddr;//,tcfg;

				for(int i = 0; i <10;i++){
					log(Logger::INFO) << "Write data L1 Config for: ADDR " << i << ", DATA " << (i+4) << endl;

					lc->write_cfg(i, get_data(i+4), 10);
					// options: tag id, addr [, delay]
					lc->read_cfg(i, 10);
					lc->get_read_cfg(taddr, data);

					uint td = 0;
					for(uint y=0;y<data.size();y++) td += (data[y] << y);
					log(Logger::INFO) << "Read back data L1 Config for: ADDR " << taddr << ", DATA " << td << endl;
				}
			} break;

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
				jtag->HICANN_arq_write_ctrl(CMD3_ARQ_CTRL_RESET, CMD3_ARQ_CTRL_RESET);
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

				cout << "plltest: enabling tag0 and tag1 loopback via JTAG..." << endl;
        jtag->HICANN_arq_write_ctrl(CMD3_ARQ_CTRL_LOOPBACK, CMD3_ARQ_CTRL_LOOPBACK);


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
					uint64 rdata;
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
					uint64 rdata;
					if (!jtag->HICANN_receive_ocp_packet(rdata))
						cout << "plltest: received none" << endl;
					else {
						ciprec = rdata; // automatic cast
						cout << "plltest: received " << ciprec << endl; 
					}
				}		
			} break;
			
			case '8': {
/*				uint tst;
				cout << "activate and deactivate reset, then press return!" << endl;
				cin >> tst;
		
				cout << "reset test logic" << endl;
				jtag->reset_jtag();

				cout << "sending TMS_SYS_START pulse..." << endl;
				jtag->jtag_write(0, 1);
				jtag->jtag_write(0, 0);

				cout << "reset test logic" << endl;
				jtag->reset_jtag();

				uint syst=0xf0f0;
				if(!jtag->read_systime(syst))
					cout << "read systime failed!" << endl;
				else
					cout << "--read systime: " << syst << endl;
*/			} break;

			case '9': {
				unsigned short cmd=0x0;
				cout << "enable or disable (1/0)? "; cin >> cmd;
				// disable dnc interface
				jtag->HICANN_set_reset(cmd);
			} break;

			case 'x': cont = false; 
			}

		} while(cont);

		return true;
	};
};


class LteeTmARQTestMultiAG : public Listee<Testmode>{
public:
	LteeTmARQTestMultiAG() : Listee<Testmode>(string("tmag_arqtest_multi"),string("simple arq test using multi-device jtag base")){};
	Testmode * create(){return new TmARQTestMultiAG();};
};

LteeTmARQTestMultiAG ListeeTmARQTestMultiAG;

