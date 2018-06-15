// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tmak_2hicanns.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann2            
//                    			
// Create Date     :   Tue Jan 10 12
// Last Change     :   Tue Jan 10 12    
// by              :   akononov
//------------------------------------------------------------------------
#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" //Testmode and Lister classes

#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class
#include "repeater_control.h" //repeater control class
#include "neuron_control.h" //neuron control class (merger, background genarators)
#include "neuronbuilder_control.h" //neuron builder control class
#include "fg_control.h" //floating gate control

#include "dnc_control.h"
#include "fpga_control.h"
#include "s2c_jtagphys_2fpga.h" //jtag-fpga library
#include "spl1_control.h" //spl1 control class
#include "iboardv2_ctrl.h" //iBoard controll class

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class TmAK2Hicanns : public Testmode{

public:
	enum rc_loc {rc_l=0, rc_r, rc_tl, rc_bl, rc_tr, rc_br}; //repeater locations
	static const uint num_hc = 2;         //number of HICANNs in chain
	enum hicann_loc {target=0, source=1}; //HICANN locations as they are registered on the iBoard
	
	//creating control objects: num_hc of each kind
	HicannCtrl            *hc[num_hc]; 
	FGControl             *fc[num_hc][4];  //array of the four floating gate controlers
	NeuronBuilderControl  *nbc[num_hc];
	NeuronControl         *nc[num_hc];     //neuron control
	RepeaterControl       *rca[num_hc][6]; //array of the 6 repeater blocks
	SynapseControl        *sc_top[num_hc], *sc_bot[num_hc];
	L1SwitchControl       *lcl[num_hc], *lcr[num_hc], *lctl[num_hc], *lcbl[num_hc], *lctr[num_hc], *lcbr[num_hc];
	SPL1Control           *spc[num_hc];


	//sets the repeater frequency (use 2,1 to get 100 MHz) on BOTH HICANNS
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		jtag->set_hicann_pos(0);
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
		jtag->set_hicann_pos(1);
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
	}

	void readout_repeater(hicann_loc hloc, rc_loc loca, RepeaterControl::rep_direction dir, uint repnr){
		vector<uint> times(3,0);
		vector<uint> nnums(3,0);
		readout_repeater(hloc, loca, dir, repnr, times, nnums, 1);
		cout << "Repeater " << repnr << ": ";
		cout << "Received neuron numbers " << dec << nnums[0] << ", " << nnums[1] << ", " << nnums[2];
		cout << " with delays " << times[1]-times[0] << " and " << times[2]-times[1] << " cycles" << endl;
	}
	
	void readout_repeater(hicann_loc hloc, rc_loc loca, RepeaterControl::rep_direction dir, uint repnr, vector<uint>& times, vector<uint>& nnums, bool silent){
		uint odd=0;
		#if HICANN_V>=2
		if (rca[hloc][loca]->repaddr(repnr)%2) odd=1;
		#elif HICANN_V==1
		if (repnr%2) odd=1;
		#endif
		rca[hloc][loca]->conf_repeater(repnr, RepeaterControl::TESTIN, dir, true); //configure repeater in desired block to receive BEG data
		usleep(500); //time for the dll to lock
		if (loca==rc_l && (repnr==28 || repnr==0)) rca[hloc][loca]->tin(repnr, true);
		rca[hloc][loca]->stopin(odd); //reset the full flag
		rca[hloc][loca]->startin(odd); //start recording received data to channel 0
		usleep(1000);  //recording lasts for ca. 4 µs - 1ms
		
		if (!silent) cout << endl << "Repeater " << repnr << ":" << endl;
		for (int i=0; i<3; i++){
			rca[hloc][loca]->tdata_read(odd, i, nnums[i], times[i]);
			if (!silent) cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
		}
		rca[hloc][loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
		rca[hloc][loca]->stopin(odd);
		rca[hloc][loca]->tout(repnr, false); //set tout back to 0 to prevent conflicts
	}

	bool test() {
		IBoardV2Ctrl board(conf, jtag, 0); //create an iboard control instance
		
		//create HICANN objects
		for (uint i=0; i < num_hc; i++)	hc[i] = new HicannCtrl(comm, i);
		
		// get pointes to left crossbar layer 1 switch control class of both HICANNs
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		for (uint i=0; i < num_hc; i++){
			nbc[i]        = &hc[i]->getNBC();
			nc[i]         = &hc[i]->getNC();
			fc[i][0]      = &hc[i]->getFC_tl();
			fc[i][1]      = &hc[i]->getFC_tr();
			fc[i][2]      = &hc[i]->getFC_bl();
			fc[i][3]      = &hc[i]->getFC_br();
			sc_top[i]     = &hc[i]->getSC_top();  // get pointer to synapse control class of HICANN 0
			sc_bot[i]     = &hc[i]->getSC_bot();  // get pointer to synapse control class of HICANN 0
			lcl[i]        = &hc[i]->getLC_cl();  // get pointer to left crossbar layer 1 switch control class of HICANN 0
			lcr[i]        = &hc[i]->getLC_cr();
			lctl[i]       = &hc[i]->getLC_tl();
			lcbl[i]       = &hc[i]->getLC_bl();
			lctr[i]       = &hc[i]->getLC_tr();
			lcbr[i]       = &hc[i]->getLC_br();
			rca[i][rc_l]  = &hc[i]->getRC_cl(); // get pointer to left repeater of HICANN 0
			rca[i][rc_r]  = &hc[i]->getRC_cr();
			rca[i][rc_tl] = &hc[i]->getRC_tl();
			rca[i][rc_bl] = &hc[i]->getRC_bl();
			rca[i][rc_tr] = &hc[i]->getRC_tr();
			rca[i][rc_br] = &hc[i]->getRC_br();
			spc[i]        = &hc[i]->getSPL1Control(); // get pointer to SPL1 Control
		}
		
		jtag->reset_jtag();
		
		log(Logger::INFO) << "Try Init() ..." ;
		for (uint c=0; c < num_hc; c++){
			if (hc[c]->GetCommObj()->Init(hc[c]->addr()) != Stage2Comm::ok) {
			 	log(Logger::ERROR) << "ERROR: Init HICANN " << c << " failed, abort";
				return 0;
			}
		}
	 	log(Logger::INFO) << "Init() ok" << flush;

		char c;
		bool cont=true;
		do{
			cout << endl << "Select test option:" << endl;
			cout << "1: Set PLL frequency on both HICANNs" << endl;
			cout << "a: Test 5 undriven lines over 2 HICANNs" << endl;
			cout << "b: Test one driven (externally shorted) line" << endl;
			cout << "c: Test intra-reticle connections" << endl;
			cout << "x: Exit" << endl;
			cin >> c;
			
			switch(c){
				case '1':{
					uint mult;
					cout << "Select PLL Frequency:" << endl;
					cout << "1:  50 MHz" << endl;
					cout << "2: 100 MHz" << endl;
					cout << "3: 150 MHz" << endl;
					cout << "4: 200 MHz" << endl;
					cout << "5: 250 MHz" << endl << endl;
					cin >> mult;
					
					if (mult < 6) set_pll(mult, 1);
				} break;
				
				case 'a':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time;
					rc_loc loca;
					hicann_loc hloc;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars in both chips...";
					for (int hloc=0; hloc < 2; hloc++){
						lcl[hloc]->reset();
						lctl[hloc]->reset();
						lcbl[hloc]->reset();
						lcr[hloc]->reset();
						lctr[hloc]->reset();
						lcbr[hloc]->reset();
					}
					
					hloc=source; //source HICANN
					
					log(Logger::INFO) << "Configuring repeaters 6, 26, 36 for output...";
					//bottom left sends events inside the chips, bottom right - inside the chips
					for (int loca=rc_bl; loca <= rc_br; loca=loca+(rc_br-rc_bl)){
						rca[hloc][loca]->reset();

						rca[hloc][loca]->conf_repeater(6, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						rca[hloc][loca]->conf_repeater(26, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						rca[hloc][loca]->conf_repeater(36, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						rca[hloc][loca]->tdata_write(0,0,0,time);
						rca[hloc][loca]->tdata_write(0,1,0,time);
						rca[hloc][loca]->tdata_write(0,2,0,time);
						rca[hloc][loca]->tdata_write(1,0,0,time);
						rca[hloc][loca]->tdata_write(1,1,0,time);
						rca[hloc][loca]->tdata_write(1,2,0,time);
						rca[hloc][loca]->startout(0);
						rca[hloc][loca]->startout(1);
					}
					usleep(10000); //DLL locking
					
					hloc=target; //target HICANN
					
					log(Logger::INFO) << "Reading out target repeaters...";
					rca[hloc][rc_br]->reset();
					rca[hloc][rc_bl]->reset();
					
					cout << endl << "Received data bottom right:" << endl;
					readout_repeater(hloc, rc_br, RepeaterControl::OUTWARDS, 6);
					readout_repeater(hloc, rc_br, RepeaterControl::OUTWARDS, 26);
					readout_repeater(hloc, rc_br, RepeaterControl::OUTWARDS, 36);
						
					cout << endl << "Received data bottom left:" << endl;
					readout_repeater(hloc, rc_bl, RepeaterControl::OUTWARDS, 26);
					readout_repeater(hloc, rc_bl, RepeaterControl::OUTWARDS, 36);
				} break;
				
				case 'b':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time;
					rc_loc loca;
					hicann_loc hloc;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars in both chips...";
					for (int hloc=0; hloc < 2; hloc++){
						lcl[hloc]->reset();
						lctl[hloc]->reset();
						lcbl[hloc]->reset();
						lcr[hloc]->reset();
						lctr[hloc]->reset();
						lcbr[hloc]->reset();
					}
					
					hloc=source; //source HICANN
					
					log(Logger::INFO) << "Configuring bottom left repeater 6 for output...";

					loca=rc_br;
					rca[hloc][loca]->reset();

					rca[hloc][loca]->conf_repeater(6, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
					rca[hloc][loca]->tdata_write(0,0,0,time);
					rca[hloc][loca]->tdata_write(0,1,0,time);
					rca[hloc][loca]->tdata_write(0,2,0,time);
					rca[hloc][loca]->tdata_write(1,0,0,time);
					rca[hloc][loca]->tdata_write(1,1,0,time);
					rca[hloc][loca]->tdata_write(1,2,0,time);
					rca[hloc][loca]->startout(0);
					rca[hloc][loca]->startout(1);
					usleep(10000); //DLL locking
					
					hloc=target; //target HICANN
					
					log(Logger::INFO) << "Reading out target repeater...";
					rca[hloc][rc_tl]->reset();
					rca[hloc][rc_bl]->reset();
					
					cout << endl << "Received data top left:" << endl;
					readout_repeater(hloc, rc_tl, RepeaterControl::INWARDS, 5);
					
					log(Logger::INFO) << "Switching top left repeater 5 to forward the signal down...";
					rca[hloc][rc_tl]->reset();
					rca[hloc][rc_tl]->set_dir(5, true); //set direction into the chip
					lctl[hloc]->switch_connect(107, 11, 1); //connect vertical lines 11 and 10 via horizontal 107
					lctl[hloc]->switch_connect(107, 10, 1);
					
					cout << endl << "Received data bottom left:" << endl;
					readout_repeater(hloc, rc_bl, RepeaterControl::OUTWARDS, 5);
				} break;

				case 'c':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time=10;
					hicann_loc hloc;
					
					log(Logger::INFO) << "Resetting crossbars in both chips...";
					for (int hloc=0; hloc < 2; hloc++){
						lcl[hloc]->reset();
						lctl[hloc]->reset();
						lcbl[hloc]->reset();
						lcr[hloc]->reset();
						lctr[hloc]->reset();
						lcbr[hloc]->reset();
					}
					

					// looop over all repeaters in block
					for(uint r=0;r<64;r++){
						// reset source and target repeaters
						rca[source][rc_tl]->reset();
						rca[target][rc_tl]->reset();

						//log(Logger::INFO) << "Configuring source repeater for output...";
						hloc=target; ///SOURCE HICANN (on the wafer the order is other than on the iBoard!!!)
						
						rca[hloc][rc_tl]->conf_repeater(r, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, true);
						rca[hloc][rc_tl]->tdata_write((r%2),0, r,      time);
						rca[hloc][rc_tl]->tdata_write((r%2),1,(r+1)%64,time);
						rca[hloc][rc_tl]->tdata_write((r%2),2,(r+2)%64,time);
						rca[hloc][rc_tl]->startout(r%2);
					
						usleep(100000); //DLL locking
						
						//log(Logger::INFO) << "Reading out target repeater...";
						hloc=source; ///TARGET HICANN (on the wafer the order is other than on the iBoard!!!)
					
						//cout << endl << "Received data top left:" << endl;
						for(uint l=0;l<10;l++) readout_repeater(hloc, rc_tl, RepeaterControl::OUTWARDS, (r+1)%64, times, nnums, true);
						
						// check result (verify that sum of nnums ist correct since order may be permuted)
						if((r + (r+1)%64 + (r+2)%64) != std::accumulate(nnums.begin(), nnums.end(), 0)){
							for(uint i=0;i<3;i++){
								//if(nnums[i] != r+i)
									cout << "Repeater " << r << ": expected NN 0x" << hex << r+i << " got 0x" << nnums[i] << endl;
							}
							cout << endl;
						}

						// disable sender
						rca[hloc][rc_tl]->stopout(r%2);
						rca[hloc][rc_tl]->conf_repeater(r, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, false);
					}

				} break;
				
				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};


class LteeTmAK2Hicanns : public Listee<Testmode>{
public:         
	LteeTmAK2Hicanns() : Listee<Testmode>(string("tmak_2hicanns"),string("Send events from one HICANN to another")){};
	Testmode * create(){return new TmAK2Hicanns();};
};
LteeTmAK2Hicanns ListeeTmAK2Hicanns;
