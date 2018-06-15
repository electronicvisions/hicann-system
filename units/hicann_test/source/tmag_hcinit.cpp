// Company         :   kip
// Author          :   Andras Gruebl        
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tmag_hcinit.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   09/2010
// Last Change     :   
// by              :   
//------------------------------------------------------------------------


#include "common.h"

#include "s2comm.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

using namespace std;
using namespace facets;

class TmHcInitAG : public Testmode{

public:

	HicannCtrl *hc; 

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "HcInitAG: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "HcInitAG: ERROR: required jtag object in testmode not set, abort" << endl;
			return 0;
		}

		uint hicannr = 0;
    
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		// ensure correct HICANN position
		jtag->set_hicann_pos(hc->addr());

		// reset values for PLL control register:
		uint ms = 0x01;
		uint ns = 0x05;
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;

		char c;
		bool cont=true;
		do{

			cout << "PLL control reg values: ns=0x" << hex << setw(2) << ns << " ms=0x" << ms << " pdn=" << setw(1) << pdn << " frg=" << frg << " tst=" << tst << endl << endl;

			cout << "Select test option:" << endl;
			cout << "1: send HICANN reset pulse" << endl;
			cout << "2: send JTAG reset" << endl;
			cout << "3: set pll frequency divider" << endl;
			cout << "4: turn pll on/off" << endl;
			cout << "5: set pll frequency range" << endl;
			cout << "6: set pll test-mode" << endl;
			cout << "7: set DAC using S2C interface" << endl;
			cout << "8: enable HICANN bias current 100uA" << endl;
			cout << "9: disable HICANN bias current" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){
		
			case '1':{
				jtag->FPGA_set_fpga_ctrl(1);
			} break;
			
			case '2':{
				jtag->reset_jtag();
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
				char d;
				cout<<"Select DAC channel (1: vdd25, 2: vol, 3: voh, 4: vddbus, x: quit)?";
				cin>>d;
				double dv;

				switch (d){
					case '1':{
						cout << "Value for VDD25-DAC (DAC 0, channel 0)? ";
						cin >> dv;
					
						SBData dacval(SBData::vdd25, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '2':{
						cout << "Value for VOL-DAC (DAC 0, channel 0)? ";
						cin >> dv;
						
						SBData dacval(SBData::vol, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '3':{
		 				cout << "Value for VOH-DAC (DAC 0, channel 0)? ";
						cin >> dv;
					
						SBData dacval(SBData::voh, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '4':{
						cout << "Value for VDDBUS-DAC (DAC 0, channel 0)? ";
						cin >> dv;
						
						SBData dacval(SBData::vddbus, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					 }break;
					default: break;
				}
			} break;
			
			case '8':{
				jtag->HICANN_set_bias(0x30);
			} break;
			
			case '9':{
				jtag->HICANN_set_bias(0x10);
			} break;
			
			case 'x': cont = false; 
			}

		} while(cont);

		return true;
	};
};


class LteeTmHcInitAG : public Listee<Testmode>{
public:
	LteeTmHcInitAG() : Listee<Testmode>(string("tmag_hcinit"),string("contains functions for HICANN initialization")){};
	Testmode * create(){return new TmHcInitAG();};
};

LteeTmHcInitAG ListeeTmHcInitAG;

