// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tmak_wafertest.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann2            
//                    			
// Create Date     :   Tue Jan 24 12
// Last Change     :   Tue Mar 27 12    
// by              :   akononov
//------------------------------------------------------------------------

#ifdef NCSIM
#include "systemc.h"
#endif

#include <tr1/array>
#include "common.h"
#include "s2comm.h"
#include "s2_types.h"
#include "testmode.h"

#include "s2c_jtagphys_2fpga.h" //throw all this out?
#include "s2ctrl.h"
#include "linklayer.h"
#include "hicann_ctrl.h"
#include "dnc_control.h"
#include "fpga_control.h"

#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class
#include "repeater_control.h" //repeater control class
#include "neuron_control.h" //neuron control class (merger, background genarators)
#include "neuronbuilder_control.h" //neuron builder control class
#include "fg_control.h" //floating gate control
#include "spl1_control.h" //spl1 control class

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class bino
{
public:
	bino(uint v, uint s=32):val(v),size(s){};
	uint val,size;
	friend std::ostream & operator <<(std::ostream &os, class bino b)
	{
		for(int i=b.size-1 ; i>=0 ; i--)
			if( b.val & (1<<i) ) os<<"1"; else os<<"0";
		return os;
	}
};

class TmAKWaferTest : public Testmode{

public:
	enum rc_loc {rc_l=0, rc_r, rc_tl, rc_bl, rc_tr, rc_br}; //repeater locations
	static const uint num_hc = 8;  //number of HICANNs in chain
	static const uint ret_num = 1; //number of reticles in a jtag chain, variable, default is 2
	
	//creating control objects: num_hc*ret_num of each kind
	//HicannCtrl            *hc[num_hc*ret_num];
	//FGControl             *fc[num_hc*ret_num][4];  //array of the four floating gate controlers
	//NeuronBuilderControl  *nbc[num_hc*ret_num];
	//NeuronControl         *nc[num_hc*ret_num];     //neuron control
	//RepeaterControl       *rca[num_hc*ret_num][6]; //array of the 6 repeater blocks
	//SynapseControl        *sc_top[num_hc*ret_num], *sc_bot[num_hc*ret_num];
	//L1SwitchControl       *lcl[num_hc*ret_num], *lcr[num_hc*ret_num], *lctl[num_hc*ret_num], *lcbl[num_hc*ret_num], *lctr[num_hc*ret_num], *lcbr[num_hc*ret_num];
	//SPL1Control           *spc[num_hc*ret_num];
	std::tr1::array<HicannCtrl*, num_hc*ret_num> 				hc;
	std::tr1::array<std::tr1::array<FGControl*, 4>,num_hc*ret_num>		fc;
	std::tr1::array<NeuronBuilderControl*, num_hc*ret_num>			nbc;
	std::tr1::array<NeuronControl*, num_hc*ret_num> 			nc;
	std::tr1::array<std::tr1::array<RepeaterControl*, 6>,num_hc*ret_num>	rca;
	std::tr1::array<SynapseControl*, num_hc*ret_num> 			sc_top, sc_bot;
	std::tr1::array<L1SwitchControl*, num_hc*ret_num> 			lcl, lcr, lctl, lcbl, lctr, lcbr;
	std::tr1::array<SPL1Control*, num_hc*ret_num> 				spc;

	//sets the repeater frequency (use 2,1 to get 100 MHz) on ALL HICANNS
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		for(int i=0; i<num_hc*ret_num; i++){
			jtag->set_hicann_pos(i);
			jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
		}
		jtag->set_hicann_pos(0);
	}

	void setupFg(){
		for(uint i=0; i<num_hc*ret_num; i++){
			for(int fg_num = 0; fg_num < 4; fg_num++){
				fc.at(i).at(fg_num)->set_maxcycle(255);
				fc.at(i).at(fg_num)->set_currentwritetime(1);
				fc.at(i).at(fg_num)->set_voltagewritetime(4);
				fc.at(i).at(fg_num)->set_readtime(63);
				fc.at(i).at(fg_num)->set_acceleratorstep(15);
				fc.at(i).at(fg_num)->set_pulselength(15);
				fc.at(i).at(fg_num)->set_fg_biasn(15);
				fc.at(i).at(fg_num)->set_fg_bias(15);
				fc.at(i).at(fg_num)->write_biasingreg();
				fc.at(i).at(fg_num)->write_operationreg();
			}
		}
	}

	void readout_repeater(uint hicann, rc_loc loca, RepeaterControl::rep_direction dir, uint repnr){
		vector<uint> times(3,0);
		vector<uint> nnums(3,0);
		readout_repeater(hicann, loca, dir, repnr, times, nnums, 1);
		cout << "Repeater " << repnr << ": ";
		cout << "Received neuron numbers " << dec << nnums[0] << ", " << nnums[1] << ", " << nnums[2];
		cout << " with delays " << times[1]-times[0] << " and " << times[2]-times[1] << " cycles" << endl;
	}
	
	void readout_repeater(uint hicann, rc_loc loca, RepeaterControl::rep_direction dir, uint repnr, vector<uint>& times, vector<uint>& nnums, bool silent){
		uint odd=0;
		#if HICANN_V>=2
		if (rca.at(hicann).at(loca)->repaddr(repnr)%2) odd=1;
		#elif HICANN_V==1
		if (repnr%2) odd=1;
		#endif
		rca.at(hicann).at(loca)->conf_repeater(repnr, RepeaterControl::TESTIN, dir, true); //configure repeater in desired block to receive BEG data
		usleep(1000); //time for the dll to lock
		rca.at(hicann).at(loca)->stopin(odd); //reset the full flag
		rca.at(hicann).at(loca)->startin(odd); //start recording received data to channel 0
		usleep(1000);  //recording lasts for ca. 4 µs - 1ms
		
		if (!silent) cout << endl << "Repeater " << repnr << ":" << endl;
		for (int i=0; i<3; i++){
			rca[hicann][loca]->tdata_read(odd, i, nnums[i], times[i]);
			if (!silent) cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
		}
		rca.at(hicann).at(loca)->stopin(odd); //reset the full flag, one time is not enough somehow...
		rca.at(hicann).at(loca)->stopin(odd);
		rca.at(hicann).at(loca)->tout(repnr, false); //set tout back to 0 to prevent conflicts
	}

	void configure_hicann_bgen(uint hicann, uint on, uint delay, uint seed, bool poisson, bool receive, bool merge){
		nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
				DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
		bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
		bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		vector<nc_merger> merg(mer, mer+23);
		vector<bool> slow(sl, sl+23);
		vector<bool> select(se, se+23);
		vector<bool> enable(en, en+23);
		if (receive) select[7] = 0; //channel 7 receives SPL1 data
		if (merge) enable[7] = 1; //channel 7 merges background and SPL1 data

		nc.at(hicann)->nc_reset(); //reset neuron control
		nc.at(hicann)->set_seed(seed); //set seed for BEGs
		nc.at(hicann)->beg_configure(0, poisson, delay); // changed all BEGs to only BEG 0 to limit clock noise
		for(int i=1; i<8; i++) nc.at(hicann)->beg_configure(i, 1, delay);
		nc.at(hicann)->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
		nc.at(hicann)->outputphase_set(0,0,0,0,0,0,0,0); //important for timing
		if (on) for(int i=0; i<8; i++) nc.at(hicann)->beg_on(i); //turn BEG 0 on
		else for(int i=0; i<8; i++) nc.at(hicann)->beg_off(i); //turn BEG 0 off
	}

	void prepare_spl1_repeaters(uint hicann, uint delay, RepeaterControl::rep_mode mode, RepeaterControl::rep_direction dir){
		if (mode==RepeaterControl::SPL1OUT) configure_hicann_bgen(hicann, 1, delay, 0, 0, 0, 0);
		for (uint i=0; i<31; i+=4){
			rca.at(hicann).at(rc_l)->conf_spl1_repeater(i, mode, dir, true);
		}
	}

    //fireb and connect - for even addresses, firet - for odd addresses, ntop, nbot for every 3+4*n'th address
	uint nbdata(uint fireb, uint firet, uint connect, uint ntop, uint nbot, uint spl1){
		uint num=0;
		for(int i=0; i<6; i++) num |= (spl1 & (1<<i))?(1<<(5-i)):0; //reverse bit order bug
		spl1=num;
		return ((4* fireb + 2*firet + connect)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;
    }
    
	//addresses 0-255: top half, 256-511: bottom half (only one neuron of a 4-group can be activated)
    void activate_neuron(uint hicann, uint address, uint nnumber, bool currentin){
		uint ntop=0, nbot=0, value;

		if (currentin){ //current input activated for this neuron
			if (address%2) ntop=0x85; //right neuron connected to aout, left/right disconnected
			else ntop=0x16; //left neuron connected to aout, left/right disconnected
		}

		else {
			if (address%2) ntop=0x81; //right neuron connected to aout, left/right disconnected
			else ntop=0x12; //left neuron connected to aout, left/right disconnected
		}
		
		if (address/256) value = nbdata(1,0,0,0,ntop,nnumber); //bottom neuron connected to spl1
		else value = nbdata(0,1,0,ntop,0,nnumber); //top neuron connected to spl1

		address%=256; //to make address compatible with bottom half
		//write the same configuration in 4 consequtive registers (temporary solution)
		for (int i=(address/2)*4; i<(address/2)*4+4; i++) nbc.at(hicann)->write_data(i, value);
    }
				
	bool test_switchram(uint hicann, uint runs) {
		int mem_size;
		ci_data_t rcvdata;
		ci_addr_t rcvaddr;

		// full L1 switch test
		cout << endl << "########## TEST SWITCH RAM ##########" << endl;
		
		uint error=0;
		uint lerror=0;
		std::tr1::array<L1SwitchControl*, num_hc*ret_num> l1;
		uint rmask;
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<6;r++){
				switch (r){
				case 0:l1=lcl;mem_size=64;rmask=15;break;
				case 1:l1=lcr;mem_size=64;rmask=15;break;
				case 2:l1=lctl;mem_size=112;rmask=0xffff;break;
				case 3:l1=lctr;mem_size=112;rmask=0xffff;break;
				case 4:l1=lcbl;mem_size=112;rmask=0xffff;break;
				case 5:l1=lcbr;mem_size=112;rmask=0xffff;break;
				}

				uint rseed = ((randomseed+(1000*r))+l);
				srand(rseed);
				uint tdata[0x80];
				for(int i=0;i<mem_size;i++)
					{
						tdata[i]=rand() & rmask;
						l1.at(hicann)->write_cmd(i,tdata[i],0);
					}

				srand(rseed);
				for(int i=0;i<mem_size;i++)
				{
					l1.at(hicann)->read_cmd(i,0);
					l1.at(hicann)->get_data(rcvaddr,rcvdata);
					if(rcvdata != tdata[i]){
						error++;
						lerror++;
						cout << hex << "ERROR: row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<
							bino(tdata[i],16)<<endl;
					}
				}
				cout <<"Switchramtest loop " << l << " errors: " << lerror << ", total errors: " << error << endl;
			}
			cout << endl;
		
		}
		return (error == 0);
	}

	
	bool test() {
		//create HICANN objects
		for (uint i=0; i < num_hc*ret_num; i++) hc.at(i) = new HicannCtrl(comm, i);
		// get pointes to left crossbar layer 1 switch control class of both HICANNs
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		for (uint i=0; i < num_hc*ret_num; i++){
			nbc.at(i)			= &hc.at(i)->getNBC();
			nc.at(i)			= &hc.at(i)->getNC();
			fc.at(i).at(0)			= &hc.at(i)->getFC_tl();
			fc.at(i).at(1)			= &hc.at(i)->getFC_tr();
			fc.at(i).at(2)			= &hc.at(i)->getFC_bl();
			fc.at(i).at(3)			= &hc.at(i)->getFC_br();
			sc_top.at(i)			= &hc.at(i)->getSC_top(); // get pointer to synapse control class of HICANN 0
			sc_bot.at(i)			= &hc.at(i)->getSC_bot(); // get pointer to synapse control class of HICANN 0
			lcl.at(i)			= &hc.at(i)->getLC(HicannCtrl::L1SWITCH_CENTER_LEFT);  // get pointer to left crossbar layer 1 switch control class of HICANN 0
			lcr.at(i)			= &hc.at(i)->getLC(HicannCtrl::L1SWITCH_CENTER_RIGHT);
			lctl.at(i)			= &hc.at(i)->getLC(HicannCtrl::L1SWITCH_TOP_LEFT);
			lctr.at(i)			= &hc.at(i)->getLC(HicannCtrl::L1SWITCH_TOP_RIGHT);
			lcbl.at(i)			= &hc.at(i)->getLC(HicannCtrl::L1SWITCH_BOTTOM_LEFT);
			lcbr.at(i)			= &hc.at(i)->getLC(HicannCtrl::L1SWITCH_BOTTOM_RIGHT);
			rca.at(i).at(rc_l)		= &hc.at(i)->getRC_cl(); // get pointer to left repeater of HICANN 0 Sending rep
			rca.at(i).at(rc_r)		= &hc.at(i)->getRC_cr(); //sending rep.
			rca.at(i).at(rc_tl)		= &hc.at(i)->getRC_tl();
			rca.at(i).at(rc_bl)		= &hc.at(i)->getRC_bl();
			rca.at(i).at(rc_tr)		= &hc.at(i)->getRC_tr();
			rca.at(i).at(rc_br)		= &hc.at(i)->getRC_br();
			spc.at(i)			= &hc.at(i)->getSPL1Control(); // get pointer to SPL1 Control
		}

		log(Logger::INFO) << "Try Init() ..." ;
		for (uint c=0; c < num_hc*ret_num; c++){
			if (hc.at(c)->GetCommObj()->Init(hc.at(c)->addr()) != Stage2Comm::ok) {
				log(Logger::ERROR) << "ERROR: Init HICANN " << c << " failed, abort";
				return 0;
			}
		}
	 	log(Logger::INFO) << "Init() ok" << flush;

		char c;
		bool cont=true;
		do{
			cout << endl << "Select test option:" << endl;
			cout << "1: Set PLL frequency on all HICANNs of the chain" << endl;
			cout << "2: Reset HICANN RAMs in the chain (except for synapses)" << endl;
			cout << "3: Program all floating gates in the chain" << endl;
			cout << "4: Program all floating gates in the chain to zero" << endl;
			cout << "5: Test switch memory of currently selected HICANN" << endl;
			cout << "a: Test vertical L1 lines in one reticle" << endl;
			cout << "A: Test vertical L1 lines between reticles" << endl;
			cout << "b: Test horizontal L1 lines in one reticle" << endl;
			cout << "B: Test horizontal L1 lines in between reticles" << endl;
			cout << "c: Test horizontal L1 lines over 4 HICANNs" << endl;
			cout << "C: Test horizontal L1 lines over 8 HICANNs (2 reticles)" << endl;
			cout << "d: Test L1 lines circularily connected over 8 HICANNs" << endl;
			cout << "e: Test sending different neuron numbers on vertical lines" << endl;
			cout << "f: Build a synfire chain over 8 HICANNs" << endl;
			cout << "F: Build a synfire chain over 16 HICANNs" << endl;
			cout << "g: Cut the synfire chain" << endl;
			cout << "t: Test analog output of an even neuron" << endl;
			cout << "o: Activate analog output of even denmems" << endl;
			cout << "O: Deactivate all analog outputs in chain" << endl;
			cout << "v: Deactivate HICANN BEGs in chain" << endl;
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
				
				case '2':{
					for (uint i=0; i<num_hc*ret_num; i++){
						cout << "resetting HICANN " << i << "..." << endl;
						cout << "resetting synapse drivers" << endl;
						sc_top.at(i)->reset_drivers();
						sc_bot.at(i)->reset_drivers();
						cout << "resetting crossbars and switches" << endl;
						lcl.at(i)->reset();
						lcr.at(i)->reset();
						lctl.at(i)->reset();
						lctr.at(i)->reset();
						lcbl.at(i)->reset();
						lcbr.at(i)->reset();
						cout << "resetting repeater blocks" << endl;
						rca.at(i).at(rc_l)->reset();
						rca.at(i).at(rc_tl)->reset();
						rca.at(i).at(rc_tr)->reset();
						rca.at(i).at(rc_bl)->reset();
						rca.at(i).at(rc_br)->reset();
						rca.at(i).at(rc_r)->reset();
						cout << "resetting neuron builder" << endl;
						nbc.at(i)->initzeros();
						cout << "resetting neuron control" << endl;
						nc.at(i)->nc_reset();
					}
				} break;

				case '3':{
					setupFg();
					cout << "Programming floating gate arrays on "<< num_hc*ret_num <<" HICANNs... ";
					int bank=0; // bank number, programming data is written to

					//write currents first as ops need biases before they work.
					cout<< endl<<"writing currents... " << flush;
					for(uint lineNumber = 1; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
						cout<<"Line: "<<lineNumber << " " <<flush;
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
						for(uint hc=0; hc<num_hc*ret_num; hc++){
							for(int fg_num = 0; fg_num < 4; fg_num++){
								log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
								fc.at(hc).at(fg_num)->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
								while(fc.at(hc).at(fg_num)->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
								fc.at(hc).at(fg_num)->program_cells(lineNumber,bank,0); //programm down first
							}
							for(int fg_num = 0; fg_num < 4; fg_num++){
								log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
								while(fc.at(hc).at(fg_num)->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
								fc.at(hc).at(fg_num)->program_cells(lineNumber,bank,1); //programm up
							}
						}
						bank =( bank +1)%2; //bank is 1 or 0
					}

					cout<< endl<<"writing voltages... " << flush;
					for(uint lineNumber = 0; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
						cout<<"Line: "<<lineNumber << " " <<flush;
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
						for(uint hc=0; hc<num_hc*ret_num; hc++){
							for(int fg_num = 0; fg_num < 4; fg_num++){
								log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
								fc.at(hc).at(fg_num)->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
								while(fc.at(hc).at(fg_num)->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
								fc.at(hc).at(fg_num)->program_cells(lineNumber,bank,0); //programm down first
							}
							for(int fg_num = 0; fg_num < 4; fg_num++){
								log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
								while(fc.at(hc).at(fg_num)->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
								fc.at(hc).at(fg_num)->program_cells(lineNumber,bank,1);//programm up
							}
						}
						bank =( bank +1)%2; //bank is 1 or 0
					}
				} break;
				
				case '4':{
					setupFg();
					cout << "Setting floating gate arrays to zero... ";
					for(uint hc=0; hc<num_hc*ret_num; hc++){
						cout << "HICANN " << hc << "... " << flush;
						for(int fg=0; fg<4; fg++) fc.at(hc).at(fg)->initzeros(0);
						for(int i=0; i<24; i++){
							for(int cntlr=0; cntlr<4; cntlr++){
								while(fc.at(hc).at(cntlr)->isBusy());
								fc.at(hc).at(cntlr)->program_cells(i,0,0);
							}
						}
					}
				} break;
				
				case '5':{
					uint hicann;
					cout << "Which HICANN in JTAG chain?" << endl;
					cin >> hicann;
					uint loops = 1;
					cout << "Testing   Switch ram... " << flush;
					cout << test_switchram(hicann, loops) << endl;
				} break;
		
				case 'a':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "0: HICANN0 - HICANN1" << endl;
					cout << "1: HICANN2 - HICANN3" << endl;
					cout << "2: HICANN4 - HICANN5" << endl;
					cout << "3: HICANN6 - HICANN7" << endl;
					cout << "--- Only if more than one reticle connected! ---" << endl;
					cout << "4: HICANN8 - HICANN9" << endl;
					cout << "5: HICANN10 - HICANN11" << endl;
					cout << "6: HICANN12 - HICANN13" << endl;
					cout << "7: HICANN14 - HICANN15" << endl;
					cin >> c;
					
					switch(c){
						case '0':{source=1; target=0;} break;
						case '1':{source=3; target=2;} break;
						case '2':{source=5; target=4;} break;
						case '3':{source=7; target=6;} break;
						#if ret_num>1
						case '4':{source=9; target=8;} break;
						case '5':{source=11; target=10;} break;
						case '6':{source=13; target=12;} break;
						case '7':{source=15; target=14;} break;
						#endif
						default: {log(Logger::ERROR) << "not enough reticles connected or invalid choice";} break;
					};
					
					uint a = c - '0';
					if (((ret_num==1) && (a>3)) || (a>7)) break;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in both chips...";
					for (uint hicann=target; hicann <= source; hicann++){
						lcl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
					}
					
					//source HICANN
					log(Logger::INFO) << "Starting downward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					for (uint i=0; i<64; i++){
						rca.at(source).at(rc_bl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_tl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_br)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_tr)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=2; i<6; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data bottom left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_bl, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data bottom right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_br, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data top left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tl, RepeaterControl::INWARDS, i);
					cout << endl << "Received data top right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tr, RepeaterControl::INWARDS, i);
					
					//revert data flow direction
					temp=source;
					source=target;
					target=temp;
					
					//source HICANN
					log(Logger::INFO) << "Starting upward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					for (uint i=0; i<64; i++){
						rca.at(source).at(rc_bl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_tl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_br)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_tr)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
					}
					
					for (uint i=2; i<6; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data bottom left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_bl, RepeaterControl::INWARDS, i);
					cout << endl << "Received data bottom right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_br, RepeaterControl::INWARDS, i);
					cout << endl << "Received data top left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tl, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data top right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tr, RepeaterControl::OUTWARDS, i);
				} break;

				case 'A':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "--- Only if more than one reticle connected! ---" << endl;
					cout << "0: HICANN0 - HICANN9" << endl;
					cout << "1: HICANN2 - HICANN11" << endl;
					cout << "2: HICANN4 - HICANN13" << endl;
					cout << "3: HICANN6 - HICANN15" << endl;
					cin >> c;
					
					switch(c){
						#if ret_num>1
						case '0':{source=0; target=8;} break;
						case '1':{source=2; target=10;} break;
						case '2':{source=4; target=12;} break;
						case '3':{source=6; target=14;} break;
						#endif
						default: {log(Logger::ERROR) << "not enough reticles connected or invalid choice";} break;
					};
					
					if (ret_num == 1) break;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in both chips...";
					for (uint hicann=target; hicann <= source; hicann++){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
					}
					
					//source HICANN
					log(Logger::INFO) << "Starting downward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					for (uint i=0; i<64; i++){
						rca.at(source).at(rc_bl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_tl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_br)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_tr)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=2; i<6; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data bottom left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_bl, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data bottom right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_br, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data top left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tl, RepeaterControl::INWARDS, i);
					cout << endl << "Received data top right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tr, RepeaterControl::INWARDS, i);
					
					//revert data flow direction
					temp=source;
					source=target;
					target=temp;
					
					//source HICANN
					log(Logger::INFO) << "Starting upward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					for (uint i=0; i<64; i++){
						rca.at(source).at(rc_bl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_tl)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_br)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_tr)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
					}
					
					for (uint i=2; i<6; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data bottom left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_bl, RepeaterControl::INWARDS, i);
					cout << endl << "Received data bottom right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_br, RepeaterControl::INWARDS, i);
					cout << endl << "Received data top left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tl, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data top right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(target, rc_tr, RepeaterControl::OUTWARDS, i);
				} break;

				case 'b':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "0: HICANN0 - HICANN2" << endl;
					cout << "1: HICANN2 - HICANN4" << endl;
					cout << "2: HICANN4 - HICANN6" << endl;
					cout << "3: HICANN1 - HICANN3" << endl;
					cout << "4: HICANN3 - HICANN5" << endl;
					cout << "5: HICANN5 - HICANN7" << endl;
					cout << "--- Only if more than one reticle connected! ---" << endl;
					cout << "6: HICANN8 - HICANN10" << endl;
					cout << "7: HICANN10 - HICANN12" << endl;
					cout << "8: HICANN12 - HICANN14" << endl;
					cout << "9: HICANN9 - HICANN11" << endl;
					cout << "a: HICANN11 - HICANN13" << endl;
					cout << "b: HICANN13 - HICANN15" << endl;
					cin >> c;
					
					switch(c){
						case '0':{source=0; target=2;} break;
						case '1':{source=2; target=4;} break;
						case '2':{source=4; target=6;} break;
						case '3':{source=1; target=3;} break;
						case '4':{source=3; target=5;} break;
						case '5':{source=5; target=7;} break;
						#if ret_num>1
						case '6':{source=8; target=10;} break;
						case '7':{source=10; target=12;} break;
						case '8':{source=12; target=14;} break;
						case '9':{source=9; target=11;} break;
						case 'a':{source=11; target=13;} break;
						case 'b':{source=13; target=15;} break;
						#endif
						default: {log(Logger::ERROR) << "not enough reticles connected or invalid choice";} break;
					};
					
					uint a = c - '0';
					if (((ret_num==1) && (a>5)) || ((a>9) && (c!='a','b'))) break;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in both chips...";
					for (uint hicann=source; hicann<=target; hicann+=2){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
						nc.at(hicann)->nc_reset();
					}
					
					//source HICANN
					log(Logger::INFO) << "Starting leftward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::INWARDS, i);

					//revert data flow direction
					temp=source;
					source=target;
					target=temp;
					
					//source HICANN
					log(Logger::INFO) << "Starting rightward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::INWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::OUTWARDS, i);
				} break;
				
				case 'B':{ //RETICLES 40-37
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "--- Only if more than one reticle connected! ---" << endl;
					cout << "0: HICANN0 - HICANN14" << endl;
					cout << "1: HICANN1 - HICANN15" << endl;
					cin >> c;
					
					switch(c){
						#if ret_num>1
						case '0':{source=14; target=0;} break;
						case '1':{source=15; target=1;} break;
						#endif
						default: {log(Logger::ERROR) << "not enough reticles connected or invalid choice";} break;
					};
					
					if (ret_num == 1) break;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in both chips...";
					for (uint hicann=source; hicann<=target; hicann+=2){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
						nc.at(hicann)->nc_reset();
					}
					
					//source HICANN
					log(Logger::INFO) << "Starting leftward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::INWARDS, i);

					//revert data flow direction
					temp=source;
					source=target;
					target=temp;
					
					//source HICANN
					log(Logger::INFO) << "Starting rightward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::INWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::OUTWARDS, i);
				} break;
				
				case 'c':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, mid1, mid2, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "0: HICANN0 - 2 - 4 - 6" << endl;
					cout << "1: HICANN1 - 3 - 5 - 7" << endl;
					cin >> c;
					
					switch(c){
						case '0':{source=0; mid1=2; mid2=4; target=6;} break;
						case '1':{source=1; mid1=3; mid2=5; target=7;} break;
					};
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in all chips...";
					for (uint hicann=0; hicann < 8; hicann++){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
						nc.at(hicann)->nc_reset();
					}
					
					log(Logger::INFO) << "Starting leftward direction test: resetting all repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(mid1).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(mid2).at(i)->reset();
					
					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					//source HICANN
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					
					//mid1 HICANN
					prepare_spl1_repeaters(mid1, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid1).at(rc_l)->is_spl1rep(i)) rca.at(mid1).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid1).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					
					//mid2 HICANN
					prepare_spl1_repeaters(mid2, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid2).at(rc_l)->is_spl1rep(i)) rca.at(mid2).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid2).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::INWARDS, i);

					//revert data flow direction
					temp=source;
					source=target;
					target=temp;
					temp=mid1;
					mid1=mid2;
					mid2=temp;
					
					//source HICANN
					log(Logger::INFO) << "Starting rughtward direction test: resetting all repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(mid1).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(mid2).at(i)->reset();
					
					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					//source HICANN
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					
					//mid1 HICANN
					prepare_spl1_repeaters(mid1, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid1).at(rc_l)->is_spl1rep(i)) rca.at(mid1).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid1).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					
					//mid2 HICANN
					prepare_spl1_repeaters(mid2, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid2).at(rc_l)->is_spl1rep(i)) rca.at(mid2).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid2).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::INWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::OUTWARDS, i);
				} break;
				
				case 'C':{ //RETICLES 40-37
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, mid1, mid2, mid3, mid4, mid5, mid6, temp;
					uint c;
					
					cout << "Choose possible connections:" << endl;
					cout << "--- Only if more than one reticle connected! ---" << endl;
					cout << "0: HICANN8 - 10 - 12 - 14 - 0 - 2 - 4 - 6" << endl;
					cout << "1: HICANN7 - 5 - 3 - 1 - 15 - 13 - 11 - 9" << endl;
					cin >> c;
					
					switch(c){
						#if ret_num>1
						case 0:{source=8; mid1=10; mid2=12; mid3=14; mid4=0; mid5=2; mid6=4; target=6;} break;
						case 1:{source=9; mid1=11; mid2=13; mid3=15; mid4=1; mid5=3; mid6=5; target=7;} break;
						#endif
						default: {log(Logger::ERROR) << "not enough reticles connected or invalid choice";} break;
					};
					
					if (ret_num == 1) break;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in all chips...";
					for (uint hicann=c%2; hicann < num_hc*ret_num; hicann+=2){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
						nc.at(hicann)->nc_reset();
					}
					
					log(Logger::INFO) << "Starting leftward direction test: resetting all repeaters...";
					for (uint h=c%2; h < num_hc*ret_num; h+=2){
						for (uint i=0; i<6; i++) rca.at(h).at(i)->reset();
					}

					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					//source HICANN
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					
					//mid1 HICANN
					prepare_spl1_repeaters(mid1, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid1).at(rc_l)->is_spl1rep(i)) rca.at(mid1).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid1).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					
					//mid2 HICANN
					prepare_spl1_repeaters(mid2, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid2).at(rc_l)->is_spl1rep(i)) rca.at(mid2).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid2).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					
					//mid3 HICANN
					prepare_spl1_repeaters(mid3, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid3).at(rc_l)->is_spl1rep(i)) rca.at(mid3).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid3).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					
					//mid4 HICANN
					prepare_spl1_repeaters(mid4, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid4).at(rc_l)->is_spl1rep(i)) rca.at(mid4).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid4).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					
					//mid5 HICANN
					prepare_spl1_repeaters(mid5, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid5).at(rc_l)->is_spl1rep(i)) rca.at(mid5).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid5).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					
					//mid6 HICANN
					prepare_spl1_repeaters(mid6, time, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid6).at(rc_l)->is_spl1rep(i)) rca.at(mid6).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
						rca.at(mid6).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::INWARDS, i);

					//revert data flow direction
					temp=source;
					source=target;
					target=temp;
					
					temp=mid6;
					mid6=mid1;
					mid1=temp;
					
					temp=mid5;
					mid5=mid2;
					mid2=temp;
					
					temp=mid4;
					mid4=mid3;
					mid3=temp;
					
					//source HICANN
					log(Logger::INFO) << "Starting rightward direction test: resetting all repeaters...";
					for (uint h=c%2; h < num_hc*ret_num; h+=2){
						for (uint i=0; i<6; i++) rca.at(h).at(i)->reset();
					}
					
					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					//source HICANN
					prepare_spl1_repeaters(source, time, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(source).at(rc_l)->is_spl1rep(i)) rca.at(source).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca.at(source).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(0,j,0,time);
						}
						for (uint j=0; j<=2; j++) {
							rca.at(source).at(i)->tdata_write(1,j,0,time);
						}
						rca.at(source).at(i)->startout(0);
						rca.at(source).at(i)->startout(1);
					}
					
					//mid1 HICANN
					prepare_spl1_repeaters(mid1, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid1).at(rc_l)->is_spl1rep(i)) rca.at(mid1).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid1).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					
					//mid2 HICANN
					prepare_spl1_repeaters(mid2, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid2).at(rc_l)->is_spl1rep(i)) rca.at(mid2).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid2).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					
					//mid3 HICANN
					prepare_spl1_repeaters(mid3, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid3).at(rc_l)->is_spl1rep(i)) rca.at(mid3).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid3).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					
					//mid4 HICANN
					prepare_spl1_repeaters(mid4, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid4).at(rc_l)->is_spl1rep(i)) rca.at(mid4).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid4).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					
					//mid5 HICANN
					prepare_spl1_repeaters(mid5, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid5).at(rc_l)->is_spl1rep(i)) rca.at(mid5).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid5).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					
					//mid6 HICANN
					prepare_spl1_repeaters(mid6, time, RepeaterControl::FORWARD, RepeaterControl::INWARDS);
					for (uint i=0; i<32; i++){
						if (!rca.at(mid6).at(rc_l)->is_spl1rep(i)) rca.at(mid6).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::INWARDS, 1);
						rca.at(mid6).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, 1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_l, RepeaterControl::INWARDS, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(target, rc_r, RepeaterControl::OUTWARDS, i);
				} break;
				
				case 'd':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time;
					static const uint h0=1, h1=0, h2=3, h3=2, h4=5, h5=4, h6=7, h7=6; //defined for better code readability
					RepeaterControl::rep_direction dir_in, dir_out; //defined according to clockwise direction
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "0: Clockwise" << endl;
					cout << "1: Counter-clockwise" << endl;
					cin >> c;
					
					switch(c){
						case '0':{dir_in=RepeaterControl::INWARDS; dir_out=RepeaterControl::OUTWARDS;} break;
						case '1':{dir_in=RepeaterControl::OUTWARDS; dir_out=RepeaterControl::INWARDS;} break;
					};
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars, switches and repeaters in all chips...";
					for (uint hicann=0; hicann < 8; hicann++){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
						nc.at(hicann)->nc_reset();
						for (uint i=0; i<6; i++) rca.at(hicann).at(i)->reset();
					}
					
					log(Logger::INFO) << "Configuring repeaters and crossbars for circular forwarding...";
					
					//HICANN0, top right
					prepare_spl1_repeaters(h0, 0, RepeaterControl::FORWARD, dir_in);
					for (uint i=0; i<32; i++) if (!rca.at(h0).at(rc_l)->is_spl1rep(i)) rca.at(h0).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
					for (uint i=0; i<64; i++) rca.at(h0).at(rc_br)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
					for (uint i=0; i<64; i+=2) lcr.at(h0)->switch_connect(i, 128+(i/2), true);
					for (uint i=1; i<64; i+=2) lcr.at(h0)->switch_connect(i, 160+(i/2), true);
										
					//HICANN1, bottom right
					prepare_spl1_repeaters(h1, 0, RepeaterControl::FORWARD, dir_out);
					for (uint i=0; i<32; i++) if (!rca.at(h1).at(rc_l)->is_spl1rep(i)) rca.at(h1).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
					for (uint i=0; i<64; i++) rca.at(h1).at(rc_tr)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
					for (uint i=0; i<64; i+=2) lcr.at(h1)->switch_connect(i, 128+(i/2), true);
					for (uint i=1; i<60; i+=2) lcr.at(h1)->switch_connect(i, 160+(i/2), true);
					lcr.at(h1)->switch_connect(61, 254, true); //because of line swapping...
					lcr.at(h1)->switch_connect(63, 255, true);
					
					//HICANN3, bottom
					prepare_spl1_repeaters(h3, 0, RepeaterControl::FORWARD, dir_out);
					for (uint i=0; i<32; i++){
						if (!rca.at(h3).at(rc_l)->is_spl1rep(i)) rca.at(h3).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
						rca.at(h3).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
					}
					
					//HICANN5, bottom
					prepare_spl1_repeaters(h5, 0, RepeaterControl::FORWARD, dir_out);
					for (uint i=0; i<32; i++){
						if (!rca.at(h5).at(rc_l)->is_spl1rep(i)) rca.at(h5).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
						rca.at(h5).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
					}
					
					//HICANN7, bottom left
					for (uint i=0; i<32; i++) rca.at(h7).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
					for (uint i=0; i<64; i++) rca.at(h7).at(rc_tl)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
					for (uint i=0; i<64; i+=2) lcl.at(h7)->switch_connect(i, i/2, true);
					for (uint i=1; i<64; i+=2) lcl.at(h7)->switch_connect(i, 32+(i/2), true);
					
					//HICANN6, top left
					for (uint i=0; i<32; i++) rca.at(h6).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
					for (uint i=0; i<64; i++) rca.at(h6).at(rc_bl)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
					for (uint i=4; i<64; i+=2) lcl.at(h6)->switch_connect(i, i/2, true);
					for (uint i=1; i<64; i+=2) lcl.at(h6)->switch_connect(i, 32+(i/2), true);
					lcl.at(h6)->switch_connect(0, 64, true); //because of line swapping...
					lcl.at(h6)->switch_connect(2, 65, true);
					
					//HICANN4, top
					prepare_spl1_repeaters(h4, 0, RepeaterControl::FORWARD, dir_in);
					for (uint i=0; i<32; i++){
						if (!rca.at(h4).at(rc_l)->is_spl1rep(i)) rca.at(h4).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
						rca.at(h4).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
					}
					
					//HICANN2, top
					prepare_spl1_repeaters(h2, 0, RepeaterControl::FORWARD, dir_in);
					for (uint i=0; i<32; i++){
						if (!rca.at(h2).at(rc_l)->is_spl1rep(i)) rca.at(h2).at(rc_l)->conf_repeater(i, RepeaterControl::FORWARD, dir_in, true);
						rca.at(h2).at(rc_r)->conf_repeater(i, RepeaterControl::FORWARD, dir_out, true);
					}
					
					log(Logger::INFO) << "HICANN2 will be sending data, HICANN4 will be receiving...";
					log(Logger::INFO) << "Configuring horizontal repeaters for output...";
					//source HICANN
					prepare_spl1_repeaters(h2, time, RepeaterControl::SPL1OUT, dir_in);
					for (uint i=0; i<32; i++){
						if (!rca.at(h2).at(rc_l)->is_spl1rep(i)) rca.at(h2).at(rc_l)->conf_repeater(i, RepeaterControl::TESTOUT, dir_in, true);
						rca.at(h2).at(rc_r)->conf_repeater(i, RepeaterControl::TESTOUT, dir_out, true);
					}
					
					for (uint i=0; i<2; i++){
						for (uint j=0; j<=2; j++) {rca.at(h2).at(i)->tdata_write(0,j,0,time);}
						for (uint j=0; j<=2; j++) {rca.at(h2).at(i)->tdata_write(1,j,0,time);}
						rca.at(h2).at(i)->startout(0);
						rca.at(h2).at(i)->startout(1);
					}
					
					usleep(10000); //DLL locking
					//target HICANN
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data center left:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(h4, rc_l, dir_in, i);
					cout << endl << "Received data center right:" << endl;
					for (uint i=0; i<32; i++) readout_repeater(h4, rc_r, dir_out, i);
					
				} break;

				case 'e':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "0: HICANN0 - HICANN1" << endl;
					cout << "1: HICANN2 - HICANN3" << endl;
					cout << "2: HICANN4 - HICANN5" << endl;
					cout << "3: HICANN6 - HICANN7" << endl;
					cin >> c;
					
					switch(c){
						case '0':{source=0; target=1;} break;
						case '1':{source=2; target=3;} break;
						case '2':{source=4; target=5;} break;
						case '3':{source=6; target=7;} break;
					};
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in both chips...";
					for (uint hicann=source; hicann <= target; hicann++){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
					}
					
					log(Logger::INFO) << "Starting upward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					for (uint rep=2; rep<6; rep++){
						RepeaterControl::rep_direction dir;
						rc_loc loca;
						if (rep==2){
							cout << "Testing top left repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_tl;
						}
						if (rep==3){
							cout << "Testing bottom left repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_bl;
						}
						if (rep==4){
							cout << "Testing top right repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_tr;
						}
						if (rep==5){
							cout << "Testing bottom right repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_br;
						}
						
						for (uint i=0; i<64; i++){
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, true);
							rca.at(source).at(loca)->tdata_write(0,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(0,1,i,time);
							rca.at(source).at(loca)->tdata_write(0,2,i,time);
							rca.at(source).at(loca)->tdata_write(1,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(1,1,i,time);
							rca.at(source).at(loca)->tdata_write(1,2,i,time);
							rca.at(source).at(loca)->startout(0);
							rca.at(source).at(loca)->startout(1);
							usleep(1000); //DLL locking
							readout_repeater(target, loca, dir, (i+1)%64); //due to line swapping i+1
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, false);
							rca.at(source).at(loca)->stopout(0);
							rca.at(source).at(loca)->stopout(1);
						}
					}
					
					//revert data flow direction
					temp=source;
					source=target;
					target=temp;

					log(Logger::INFO) << "Starting downward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					for (uint rep=2; rep<6; rep++){
						RepeaterControl::rep_direction dir;
						rc_loc loca;
						if (rep==2){
							cout << "Testing top left repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_tl;
						}
						if (rep==3){
							cout << "Testing bottom left repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_bl;
						}
						if (rep==4){
							cout << "Testing top right repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_tr;
						}
						if (rep==5){
							cout << "Testing bottom right repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_br;
						}
						
						for (uint i=0; i<64; i++){
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, true);
							rca.at(source).at(loca)->tdata_write(0,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(0,1,i,time);
							rca.at(source).at(loca)->tdata_write(0,2,i,time);
							rca.at(source).at(loca)->tdata_write(1,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(1,1,i,time);
							rca.at(source).at(loca)->tdata_write(1,2,i,time);
							rca.at(source).at(loca)->startout(0);
							rca.at(source).at(loca)->startout(1);
							usleep(1000); //DLL locking
							readout_repeater(target, loca, dir, (i-1)%64); //due to line swapping i-1
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, false);
							rca.at(source).at(loca)->stopout(0);
							rca.at(source).at(loca)->stopout(1);
						}
					}
				} break;
				
				case 'E':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, target, source, temp;
					char c;
					
					cout << "Choose possible connections:" << endl;
					cout << "--- Only if more than one reticle connected! ---" << endl;
					cout << "0: HICANN0 - HICANN9" << endl;
					cout << "1: HICANN2 - HICANN11" << endl;
					cout << "2: HICANN4 - HICANN13" << endl;
					cout << "3: HICANN6 - HICANN15" << endl;
					cin >> c;
					
					switch(c){
						#if ret_num>1
						case '0':{source=0; target=9;} break;
						case '1':{source=2; target=11;} break;
						case '2':{source=4; target=13;} break;
						case '3':{source=6; target=15;} break;
						#endif
						default: {log(Logger::ERROR) << "not enough reticles connected or invalid choice";} break;
					};
					
					if (ret_num == 1) break;
					
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					log(Logger::INFO) << "Resetting crossbars and switches in both chips...";
					for (uint hicann=source; hicann <= target; hicann++){
						lcl.at(hicann)->reset();
						lctl.at(hicann)->reset();
						lcbl.at(hicann)->reset();
						lcr.at(hicann)->reset();
						lctr.at(hicann)->reset();
						lcbr.at(hicann)->reset();
					}
					
					log(Logger::INFO) << "Starting upward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					for (uint rep=2; rep<6; rep++){
						RepeaterControl::rep_direction dir;
						rc_loc loca;
						if (rep==2){
							cout << "Testing top left repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_tl;
						}
						if (rep==3){
							cout << "Testing bottom left repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_bl;
						}
						if (rep==4){
							cout << "Testing top right repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_tr;
						}
						if (rep==5){
							cout << "Testing bottom right repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_br;
						}
						
						for (uint i=0; i<64; i++){
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, true);
							rca.at(source).at(loca)->tdata_write(0,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(0,1,i,time);
							rca.at(source).at(loca)->tdata_write(0,2,i,time);
							rca.at(source).at(loca)->tdata_write(1,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(1,1,i,time);
							rca.at(source).at(loca)->tdata_write(1,2,i,time);
							rca.at(source).at(loca)->startout(0);
							rca.at(source).at(loca)->startout(1);
							usleep(1000); //DLL locking
							readout_repeater(target, loca, dir, (i+1)%64); //due to line swapping i+1
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, false);
							rca.at(source).at(loca)->stopout(0);
							rca.at(source).at(loca)->stopout(1);
						}
					}
					
					//revert data flow direction
					temp=source;
					source=target;
					target=temp;

					log(Logger::INFO) << "Starting downward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca.at(source).at(i)->reset();
					for (uint i=0; i<6; i++) rca.at(target).at(i)->reset();
					
					for (uint rep=2; rep<6; rep++){
						RepeaterControl::rep_direction dir;
						rc_loc loca;
						if (rep==2){
							cout << "Testing top left repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_tl;
						}
						if (rep==3){
							cout << "Testing bottom left repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_bl;
						}
						if (rep==4){
							cout << "Testing top right repeaters..."<< endl;
							dir=RepeaterControl::INWARDS;
							loca=rc_tr;
						}
						if (rep==5){
							cout << "Testing bottom right repeaters..."<< endl;
							dir=RepeaterControl::OUTWARDS;
							loca=rc_br;
						}
						
						for (uint i=0; i<64; i++){
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, true);
							rca.at(source).at(loca)->tdata_write(0,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(0,1,i,time);
							rca.at(source).at(loca)->tdata_write(0,2,i,time);
							rca.at(source).at(loca)->tdata_write(1,0,0,time); //for locking purposes
							rca.at(source).at(loca)->tdata_write(1,1,i,time);
							rca.at(source).at(loca)->tdata_write(1,2,i,time);
							rca.at(source).at(loca)->startout(0);
							rca.at(source).at(loca)->startout(1);
							usleep(1000); //DLL locking
							readout_repeater(target, loca, dir, (i-1)%64); //due to line swapping i-1
							rca.at(source).at(loca)->conf_repeater(i, RepeaterControl::TESTOUT, dir, false);
							rca.at(source).at(loca)->stopout(0);
							rca.at(source).at(loca)->stopout(1);
						}
					}
				} break;

				case 'f':{ //RETICLE 37
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					uint hicann, number0=4, number1=0, number2=0, number3=0, number4=0, number5=0, number6=12, number7=14; //reticle 37
					bool showflag=0;
					uint source=7; //source HICANN with the initial BEG
					
					//~ cout << "Enter neuron number in HICANN with JTAG ID 0: ";
					//~ cin >> number0;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 1: ";
					//~ cin >> number1;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 2: ";
					//~ cin >> number2;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 3: ";
					//~ cin >> number3;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 4: ";
					//~ cin >> number4;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 5: ";
					//~ cin >> number5;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 6: ";
					//~ cin >> number6;
					//~ cout << "Enter neuron number in HICANN with JTAG ID 7: ";
					//~ cin >> number7;
					cout << "Enter 1 to show the single neurons, 0 to skip it: ";
					cin >> showflag;
					
					cout << "Resetting HICANNs: " << flush;
					for (uint i=0; i<num_hc; i++){
						cout << "HICANN" << i << "... " << flush;

						nbc.at(i)->setOutputEn(false, false); //switched nbc->at(hicann) to nbc->at(i) because constant doesn't make any sense in for loop DEBUG
						sc_top.at(i)->reset_drivers(); //reset top synapse block drivers
						sc_bot.at(i)->reset_drivers(); //reset bottom synapse block drivers
						sc_top.at(i)->configure();
						
						lcl.at(i)->reset(); //reset left crossbar
						lcr.at(i)->reset();
						lctl.at(i)->reset(); //reset top left select switch
						lctr.at(i)->reset();
						lcbl.at(i)->reset();
						lcbr.at(i)->reset();
						
						for (uint u=0; u<6; u++) rca.at(i).at(u)->reset();
					}
					
					//starting with HICANN7
					cout << endl << endl << "Configuring HICANN7" << endl;
					hicann=7;

					configure_hicann_bgen(hicann, 1, 300, 0xaa, 0, 1, 0); //start BEG
	
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					lcl.at(hicann)->switch_connect(57,28,1); //background
					lctl.at(hicann)->switch_connect(0,28,1); //to driver 0
					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number7, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number7, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number7, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number7 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number7 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);

					cout << "Events after the BEG:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << "Events after the first neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					//continuing with HICANN5
					cout << endl << "Configuring HICANN5" << endl;
					hicann=5;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the second HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the second neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number5, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number5, 0);
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number5 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number5 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number5, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the second neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					//continuing with HICANN3
					cout << endl << "Configuring HICANN3" << endl;
					hicann=3;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the third HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the third neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number5, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number5, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number3, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number3 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number3 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the third neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					//continuing with HICANN1
					cout << endl << "Configuring HICANN1" << endl;
					hicann=1;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the fourth HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the fourth neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number5, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number5, 0);
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number1 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number1 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number1, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the fourth neuron left:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					lcr.at(hicann)->switch_connect(1,128,1); //to the bottom HICANN
					usleep(10000);
					cout << "Events before the edge of the fifth HICANN:" << endl;
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 0);

					rca.at(hicann).at(rc_br)->conf_repeater(0, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, true); //configure repeater 0 to forward data vertically on line 128
					
					//continuing with HICANN0
					cout << endl << "Configuring HICANN0" << endl;
					hicann=0;
					
					usleep(10000);
					cout << "Events before the fifth neuron:" << endl;
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 63);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 63);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 63);
					lctr.at(hicann)->switch_connect(1,254,1); //from the top HICANN to driver 1

					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(2, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(2, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(2, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(2, weights);
					sc_top.at(hicann)->write_weight(3, weights);
					sc_top.at(hicann)->write_decoder(2, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(2, number0, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(3, number0, 0);
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number0, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number0 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number0 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the fifth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					//continuing with HICANN2
					cout << endl << "Configuring HICANN2" << endl;
					hicann=2;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the sixth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number2, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number2, 0);
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number2, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number2 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number2 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the sixth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					//continuing with HICANN4
					cout << endl << "Configuring HICANN4" << endl;
					hicann=4;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the seventh neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number4, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number4, 0);
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number4, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number4 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number4 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the seventh neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					//continuing with HICANN6
					cout << endl << "Configuring HICANN6" << endl;
					hicann=6;

					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the eighth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number6, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number6, 0);
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number6, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number6 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number6 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1); //to the upper HICANN
					usleep(10000);
					cout << "Events after the eighth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);

					///And back to HICANN7
					cout << endl << "Configuring HICANN7 again!" << endl;
					hicann=7;
					
					usleep(10000);
					cout << "Events before the first neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(hicann, rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(hicann, rc_bl, RepeaterControl::INWARDS, 1);
					
					rca.at(hicann).at(rc_bl)->conf_repeater(1, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 1 to forward data on vertical line 2
					
					lctl.at(hicann)->switch_connect(110,2,1); //to driver 110
					
					sc_top.at(hicann)->drv_config(220, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(220, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(220, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(220, weights);
					sc_top.at(hicann)->write_weight(221, weights);
					sc_top.at(hicann)->write_decoder(220, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(220, number7, 0); //configure the synapses
					sc_top.at(hicann)->write_decoder(221, number7, 0);
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the first neuron again:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number7 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number7 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					usleep(10000);
					cout << "Events after the BEG:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << endl << "Switching off the BEG..." << endl;
					nc.at(source)->beg_off(8);
					
					usleep(10000);
					cout << "Events after the BEG is switched off:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << endl << "BEG is off. Network in self-sustain mode!" << endl;
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(100000);
					cout << "Events after the first neuron in self-sustain mode:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					cout << "Synfire-chain complete. Putting the source neuron on the AOUT..." << endl;
					nbc.at(hicann)->setOutputEn(false, true);
					nbc.at(hicann)->setOutputMux(1,4);
				} break;

				case 'F':{
					if (ret_num == 1){
						log(Logger::ERROR) << "not enough reticles connected";
						break;
					}
					
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					uint hicann, number0=2, number1=2, number2=0, number3=0, number4=0, number5=0, number6=0, number7=4; //reticle 37
					uint number8=2, number9=2, number10=6, number11=0, number12=0, number13=0, number14=0, number15=0; //reticle 40
					bool showflag=0;
					uint source=7; //source HICANN with the initial BEG

					cout << "Enter 1 to show the single neurons, 0 to skip it: ";
					cin >> showflag;
					
					cout << "Resetting HICANNs: " << flush;
					for (uint i=0; i<num_hc*ret_num; i++){
						cout << "HICANN" << i << "... " << flush;

						nbc.at(i)->setOutputEn(false, false);
						sc_top.at(i)->reset_drivers(); //reset top synapse block drivers
						sc_bot.at(i)->reset_drivers(); //reset bottom synapse block drivers
						sc_top.at(i)->configure();
						
						lcl.at(i)->reset(); //reset left crossbar
						lcr.at(i)->reset();
						lctl.at(i)->reset(); //reset top left select switch
						lctr.at(i)->reset();
						lcbl.at(i)->reset();
						lcbr.at(i)->reset();
						
						for (uint u=0; u<6; u++) rca.at(i).at(u)->reset();
					}
					
					//starting with HICANN7
					cout << endl << endl << "Configuring HICANN7" << endl;
					hicann=7;

					configure_hicann_bgen(hicann, 1, 300, 0xaa, 0, 1, 0); //start BEG
	
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					lcl.at(hicann)->switch_connect(57,28,1); //background
					lctl.at(hicann)->switch_connect(0,28,1); //to driver 0
					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number7, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number7, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number7, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number7 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number7 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);

					cout << "Events after the BEG:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << "Events after the first neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl[hicann]->switch_connect(1,0,0);

					//continuing with HICANN5
					cout << endl << "Configuring HICANN5" << endl;
					hicann=5;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the second HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the second neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number5, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number5, 0);
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number5 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number5 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number5, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the second neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					//continuing with HICANN3
					cout << endl << "Configuring HICANN3" << endl;
					hicann=3;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the third HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the third neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number3, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number3, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number3, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number3 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number3 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the third neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					//continuing with HICANN1
					cout << endl << "Configuring HICANN1" << endl;
					hicann=1;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the fourth HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the fourth neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number1, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number1, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number1, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number1 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number1 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the fourth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					//continuing with HICANN15
					cout << endl << "Configuring HICANN15" << endl;
					hicann=15;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the fifth HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the fifth neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number15, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number15, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number15, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number15 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number15 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the fifth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					//continuing with HICANN13
					cout << endl << "Configuring HICANN13" << endl;
					hicann=13;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the sixth HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the sixth neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number13, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number13, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number13, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number13 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number13 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the sixth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					//continuing with HICANN11
					cout << endl << "Configuring HICANN11" << endl;
					hicann=11;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the seventh HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the seventh neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number11, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number11, 0);

					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number11, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number11 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number11 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the seventh neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					//continuing with HICANN9
					cout << endl << "Configuring HICANN9" << endl;
					hicann=9;
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					usleep(10000);
					cout << "Events on the edge of the eighth HICANN:" << endl;
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					readout_repeater(hicann, rc_l, RepeaterControl::INWARDS, 31);
					
					rca.at(hicann).at(rc_l)->conf_repeater(31, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 31 to forward data on line 63
					lcl.at(hicann)->switch_connect(63,95,1); //from previous HICANN
					lctl.at(hicann)->switch_connect(0,95,1); //to driver 0
					
					usleep(10000);
					cout << "Events before the eighth neuron:" << endl;
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					readout_repeater(hicann, rc_tl, RepeaterControl::OUTWARDS, 47);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(0, number9, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(1, number9, 0);
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number9 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number9 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number9, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the eighth neuron left:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					lcr.at(hicann)->switch_connect(1,128,1); //to the bottom HICANN
					usleep(10000);
					cout << "Events before the edge of the nineth HICANN:" << endl;
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 0);

					rca.at(hicann).at(rc_br)->conf_repeater(0, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, true); //configure repeater 0 to forward data vertically on line 128
					
					//continuing with HICANN8
					cout << endl << "Configuring HICANN8" << endl;
					hicann=8;
					
					usleep(10000);
					cout << "Events before the nineth neuron:" << endl;
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 63);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 63);
					readout_repeater(hicann, rc_br, RepeaterControl::OUTWARDS, 63);
					lctr.at(hicann)->switch_connect(1,254,1); //from the top HICANN to driver 1

					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(2, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(2, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(2, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(2, weights);
					sc_top.at(hicann)->write_weight(3, weights);
					sc_top.at(hicann)->write_decoder(2, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(2, number8, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(3, number8, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number8, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number8 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number8 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the nineth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					//continuing with HICANN10
					cout << endl << "Configuring HICANN10" << endl;
					hicann=10;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the tenth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number10, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number10, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number10, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number10 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number10 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the tenth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					//continuing with HICANN12
					cout << endl << "Configuring HICANN12" << endl;
					hicann=12;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the eleventh neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number12, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number12, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number12, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number12 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number12 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the eleventh neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					//continuing with HICANN14
					cout << endl << "Configuring HICANN14" << endl;
					hicann=14;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the twelveth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number14, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number14, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number14, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number14 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number14 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the twelveth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					//continuing with HICANN0
					cout << endl << "Configuring HICANN0" << endl;
					hicann=0;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the thirteenth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number0, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number0, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number0, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number0 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number0 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the thirteenth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1

					//continuing with HICANN2
					cout << endl << "Configuring HICANN2" << endl;
					hicann=2;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the fourteenth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number2, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number2, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number2, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number2 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number2 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the fourteenth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					//continuing with HICANN4
					cout << endl << "Configuring HICANN4" << endl;
					hicann=4;
					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the fifteenth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number4, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number4, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number4, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number4 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number4 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the fifteenth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					//continuing with HICANN6
					cout << endl << "Configuring HICANN6" << endl;
					hicann=6;

					lcr.at(hicann)->switch_connect(3,129,1); //from the left HICANN
					
					usleep(10000);
					cout << "Events before the sixteenth neuron:" << endl;
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_tr, RepeaterControl::OUTWARDS, 0);
					
					lctr.at(hicann)->switch_connect(111,129,1); //to driver 111
					
					configure_hicann_bgen(hicann, 0, 0, 0xaa, 0, 1, 0); //configure merger tree (channel 7)
					
					sc_top.at(hicann)->drv_config(222, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(222, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(222, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(222, weights);
					sc_top.at(hicann)->write_weight(223, weights);
					sc_top.at(hicann)->write_decoder(222, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(222, number6, 0); //configure the synapses and the neuron
					sc_top.at(hicann)->write_decoder(223, number6, 0);
					
					nbc.at(hicann)->initzeros(); //neuron configuration
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);
					activate_neuron(hicann, number6, 0, 0);
					nbc.at(hicann)->setSpl1Reset(0);
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number6 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number6 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1					
					lcl.at(hicann)->switch_connect(1,0,1); //to the upper HICANN
					usleep(10000);
					cout << "Events after the sixteenth neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);

					///And back to HICANN7
					cout << endl << "Configuring HICANN7 again!" << endl;
					hicann=7;
					
					usleep(10000);
					cout << "Events before the first neuron:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(hicann, rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(hicann, rc_bl, RepeaterControl::INWARDS, 1);
					
					rca.at(hicann).at(rc_bl)->conf_repeater(1, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 1 to forward data on vertical line 2
					
					lctl.at(hicann)->switch_connect(110,2,1); //to driver 110
					
					sc_top.at(hicann)->drv_config(220, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(220, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(220, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(220, weights);
					sc_top.at(hicann)->write_weight(221, weights);
					sc_top.at(hicann)->write_decoder(220, weights, weights); //to prevent mistakes
					sc_top.at(hicann)->write_decoder(220, number7, 0); //configure the synapses
					sc_top.at(hicann)->write_decoder(221, number7, 0);
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the first neuron again:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);

					if(showflag){
						nbc.at(hicann)->setOutputEn(false, true);
						nbc.at(hicann)->setOutputMux(1,4);
						nc.at(source)->beg_off(8);
						cout << "Neuron " << number7 << " is on the AOUT now. Initial BEG is disabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nc.at(source)->beg_on(8);
						cout << "Neuron " << number7 << " is on the AOUT now. initial BEG is enabled. Press ENTER to continue..." << flush;
						cin.ignore();
						nbc.at(hicann)->setOutputEn(false, false);
					}
					
					usleep(10000);
					cout << "Events after the BEG:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << endl << "Switching off the BEG..." << endl;
					nc.at(source)->beg_off(8);
					
					usleep(10000);
					cout << "Events after the BEG is switched off:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << endl << "BEG is off. Network in self-sustain mode!" << endl;
					
					lcl.at(hicann)->switch_connect(1,0,1);
					usleep(100000);
					cout << "Events after the first neuron in self-sustain mode:" << endl;
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					usleep(100000);
					readout_repeater(hicann, rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl.at(hicann)->switch_connect(1,0,0);
					
					cout << "Synfire-chain complete. Putting the source neuron on the AOUT..." << endl;
					nbc.at(hicann)->setOutputEn(false, true);
					nbc.at(hicann)->setOutputMux(1,4);
				} break;

				case 'g':{ //RETICLES 40,37
					uint hicann;
					cout << "Which HICANN do you want to cut the chain in?" << endl;
					cin >> hicann;
					lcl.at(hicann)->reset();
					lcr.at(hicann)->reset();
					cout << "Done." << endl;
				} break;

				case 't':{
					uint hicann, neuron;
					cout << "Which HICANN in JTAG chain?" << endl;
					cin >> hicann;
					cout << "Which neuron?" << endl;
					cin >> neuron;
					
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top.at(hicann)->reset_drivers(); //reset top synapse block drivers
					sc_top.at(hicann)->configure();
					rca.at(hicann).at(rc_tl)->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					sc_top.at(hicann)->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top.at(hicann)->preout_set(0, 0, 0, 0, 0);
					sc_top.at(hicann)->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top.at(hicann)->write_weight(0, weights);
					sc_top.at(hicann)->write_weight(1, weights);
					sc_top.at(hicann)->write_decoder(0, address, address);
					lcl.at(hicann)->reset(); //reset left crossbar
					lcr.at(hicann)->reset();
					lctl.at(hicann)->reset(); //reset top left select switch
					lctr.at(hicann)->reset();
					lcbl.at(hicann)->reset();
					lcbr.at(hicann)->reset();

					lcl.at(hicann)->switch_connect(57,28,1);
					lctl.at(hicann)->switch_connect(0,28,1);
					rca.at(hicann).at(rc_l)->reset();
					rca.at(hicann).at(rc_l)->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					nbc.at(hicann)->setOutputEn(true,true);
					nbc.at(hicann)->setOutputMux(1,4);
					nbc.at(hicann)->initzeros(); //first, clear neuron builder
					nbc.at(hicann)->setNeuronBigCap(0,0);
					nbc.at(hicann)->setNeuronGl(0,0,1,1);
					nbc.at(hicann)->setNeuronGladapt(0,0,0,0);
					nbc.at(hicann)->setNeuronReset(1,1);

					activate_neuron(hicann,neuron,0,0);
					nbc.at(hicann)->setSpl1Reset(0); //this may be not necessary
					nbc.at(hicann)->setSpl1Reset(1);
					nbc.at(hicann)->configNeuronsGbl();
					
					configure_hicann_bgen(hicann, 1, 300, 0xaa, 0, 0, 0);
					cout << "Do not forget to deactivate analog output!" << endl;
				} break;

				case 'o':{
					uint hicann;
					cout << "Which HICANN in JTAG chain?" << endl;
					cin >> hicann;
					nbc.at(hicann)->setOutputEn(false, true);
					nbc.at(hicann)->setOutputMux(1,4);
					cout << "Do not forget to deactivate it!" << endl;
				} break;

				case 'O':{
					for (uint i=0; i<num_hc*ret_num; i++) nbc.at(i)->setOutputEn(false, false);
				} break;
				
				case 'v':{
					for (uint i=0; i<num_hc*ret_num; i++) nc.at(i)->beg_off(8);
				} break;

				case 'x':{
					cont=false;
				} break;
			}
			
		}while(cont);
		return true;
	};
};


class LteeTmAKWaferTest : public Listee<Testmode>{
public:         
	LteeTmAKWaferTest() : Listee<Testmode>(string("tmak_wafertest"),string("Wafer testing routines")){};
	Testmode * create(){return new TmAKWaferTest();};
};
LteeTmAKWaferTest ListeeTmAKWaferTest;
