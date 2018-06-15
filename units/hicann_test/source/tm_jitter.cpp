// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_jitter.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jan 18 11
// Last Change     :   Tue May 30 11    
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

#include<iostream>
#include<fstream>
#include<string>

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class TmJitter : public Testmode{

public:
	// repeater control 
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	static const int TAG = 0;
	
	stringstream ss;
	string filename;
	fstream file;
	
	stringstream ss1;
	string filename1;
	fstream file1;
	
	HicannCtrl *hc; 
	LinkLayer<ci_payload,ARQPacketStage2> *ll;
	FGControl *fc[4]; //array of the four floating gate controlers
	NeuronBuilderControl *nbc;
	NeuronControl *nc; //neuron control
	RepeaterControl *rca[6]; //array of the 6 repeater blocks
	SynapseControl *sc_top, *sc_bot;
	L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;
	
	// DNC stuff
	SPL1Control *spc;
	DNCControl  *dc;
	FPGAControl *fpc;

    //fireb and connect - for even addresses, firet - for odd addresses, ntop, nbot for every 3+4*n'th address
	uint nbdata(uint fireb, uint firet, uint connect, uint ntop, uint nbot, uint spl1){
		uint num=0;
		for(int i=0; i<6; i++) num |= (spl1 & (1<<i))?(1<<(5-i)):0; //reverse bit order bug
		spl1=num;
		return ((4* fireb + 2*firet + connect)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;
    }
	
	//addresses 0-255: top half, 256-511: bottom half (only one neuron of a 4-group can be activated)
    void activate_neuron(uint address, uint nnumber, bool currentin){
		uint ntop=0, nbot=0, value;
		#if HICANN_V==1
		if (currentin) ntop=0x8e; //right neuron connected to L1 bus, left/right connected
		#elif HICANN_V>=2
		if (currentin){ //current input activated for this neuron
			if (address%2) ntop=0x85; //right neuron connected to aout, left/right disconnected
			else ntop=0x16; //left neuron connected to aout, left/right disconnected
		}
		#endif
		else {
			if (address%2) ntop=0x81; //right neuron connected to aout, left/right disconnected
			else ntop=0x12; //left neuron connected to aout, left/right disconnected
		}
		
		if (address/256) value = nbdata(1,0,0,0,ntop,nnumber); //bottom neuron connected to spl1
		else value = nbdata(0,1,0,ntop,0,nnumber); //top neuron connected to spl1
		
		address%=256; //to make address compatible with bottom half
		//write the same configuration in 4 consequtive registers (temporary solution)
		for (int i=(address/2)*4; i<(address/2)*4+4; i++) nbc->write_data(i, value);
    }
    
    void activate_neuron_jitter(uint address, uint nnumber){
		uint ntop=0, nbot=0, value;
		if (address%2) ntop=0x81; //right neuron connected to aout, left/right disconnected
		else ntop=0x12; //left neuron connected to aout, left/right disconnected
		if (address/256) value = nbdata(1,0,0,0,ntop,nnumber); //bottom neuron connected to spl1
		else value = nbdata(0,1,0,ntop,0,nnumber); //top neuron connected to spl1
		address%=256; //to make address compatible with bottom half
		for (int i=(address/2)*4; i<(address/2)*4+4; i++) nbc->write_data(i, value);
    }
	
	//sets the repeater frequency (use 2,1 to get 100 MHz)
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
	}

	//generate an SPL1 event (27 bits) for HICANN 0
	uint64_t gen_spl1(uint channel, uint number, uint time){
		return ((channel & 0x7) << 21) | ((number & 0x3f) << 15) | (time & 0x7fff);
	}
	
	//generate an FPGA event packet (64 bits)
	uint64_t gen_fpga(uint64_t event1, uint64_t event2){
		return 0x8000000000000000 | (event2 << 32) | event1;
	}

	void setupFg(){
		for(int fg_num = 0; fg_num < 4; fg_num += 1){
			#if HICANN_V == 1
			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(10);
			fc[fg_num]->set_voltagewritetime(28);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(20);
			fc[fg_num]->set_pulselength(8);
			fc[fg_num]->set_fg_biasn(4);
			fc[fg_num]->set_fg_bias(4);
			#elif HICANN_V >= 2
			//~ fc[fg_num]->set_maxcycle(255);		
			//~ fc[fg_num]->set_currentwritetime(1);
			//~ fc[fg_num]->set_voltagewritetime(15);
			//~ fc[fg_num]->set_readtime(63);
			//~ fc[fg_num]->set_acceleratorstep(9);
			//~ fc[fg_num]->set_pulselength(9);
			//~ fc[fg_num]->set_fg_biasn(5);
			//~ fc[fg_num]->set_fg_bias(8);
			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(1);
			fc[fg_num]->set_voltagewritetime(4);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(15);
			fc[fg_num]->set_pulselength(15);
			fc[fg_num]->set_fg_biasn(15);
			fc[fg_num]->set_fg_bias(15);
			#endif
			fc[fg_num]->write_biasingreg();
			fc[fg_num]->write_operationreg();
		}
	}

	void readout_repeater(rc_loc loca, uint repnr){
		vector<uint> times(3,0);
		vector<uint> nnums(3,0);
		readout_repeater(loca, repnr, times, nnums, 0);
	}
	
	void readout_repeater(rc_loc loca, uint repnr, vector<uint>& times, vector<uint>& nnums, bool silent){
		uint odd=0;
		#if HICANN_V>=2
		if (rca[loca]->repaddr(repnr)%2) odd=1;
		#elif HICANN_V==1
		if (repnr%2) odd=1;
		#endif
		rca[loca]->conf_repeater(repnr, RepeaterControl::TESTIN, RepeaterControl::OUTWARDS, true); //configure repeater in desired block to receive BEG data
		usleep(500); //time for the dll to lock
		if (loca==rc_l && (repnr==28 || repnr==0)) rca[loca]->tin(repnr, true);
		rca[loca]->stopin(odd); //reset the full flag
		rca[loca]->startin(odd); //start recording received data to channel 0
		usleep(1000);  //recording lasts for ca. 4 µs - 1ms
		
		for (int i=0; i<3; i++){
			rca[loca]->tdata_read(odd, i, nnums[i], times[i]);
			if (!silent) cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
		}
		rca[loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
		rca[loca]->stopin(odd);
		rca[loca]->tout(repnr, false); //set tout back to 0 to prevent conflicts
	}

	bool there_are_spikes(bool silent){
		vector<uint> times1(3,0), times2(3,0), nnums1(3,0), nnums2(3,0);
		readout_repeater(rc_l, 28, times1, nnums1, silent);
		readout_repeater(rc_l, 28, times2, nnums2, silent);
		bool flag=false;
		for (int n=0; n<3; n++){
			if (times1[n]!=times2[n]) flag=true;
		}
		return flag;
	}
	
	void configure_fpga_bgen(uint on, uint startn, uint stopn, uint chan, uint delay, uint poisson){
		nc->nc_reset(); //reset neuron control
		nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs
		spc->write_cfg(0x0ffff);
		// set DNC to ignore time stamps and directly forward l2 pulses
		dc->setTimeStampCtrl(0x0);
		// set transmission directions in DNC (for HICANN 0 only)
		dc->setDirection(0xff);

		jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 80, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr())); //to lock the DLL(s)
		usleep(100000);
		jtag->FPGA_set_cont_pulse_ctrl(on, 1<<(chan&0x7), poisson, delay, 0x1aaaa, startn, stopn-startn, comm->jtag2dnc(hc->addr())); //actual activity
	}
	
	void configure_hicann_bgen(uint on, uint delay, uint seed, bool poisson, bool receive, bool merge){
		#if HICANN_V==1
		nc_merger mer[15] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0};
		bool sl[15] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		bool se[15] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1}; //all channels forward BEG data
		bool en[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		//initialize a vector from an array
		vector<nc_merger> merg(mer, mer+15);
		vector<bool> slow(sl, sl+15);
		vector<bool> select(se, se+15);
		vector<bool> enable(en, en+15);
		#elif HICANN_V>=2
		nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
				DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
		bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
		bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		vector<nc_merger> merg(mer, mer+23);
		vector<bool> slow(sl, sl+23);
		vector<bool> select(se, se+23);
		vector<bool> enable(en, en+23);
		#endif
		if (receive) select[0] = 0; //channel 0 receives SPL1 data
		if (merge) enable[0] = 1; //channel 0 merges background and SPL1 data

		nc->nc_reset(); //reset neuron control
		nc->set_seed(seed); //set seed for BEGs
		nc->beg_configure(8, poisson, delay);
		nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
		nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
		if (on) nc->beg_on(8); //turn all BEGs on
		else nc->beg_off(8); //turn all BEGs off
	}

	bool test() {
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
		
		dbg(0) << "Try Init() ..." ;


		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;
	
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		nbc = &hc->getNBC();
		nc  = &hc->getNC();
		fc[0]=&hc->getFC_tl();
		fc[1]=&hc->getFC_tr();
		fc[2]=&hc->getFC_bl();
		fc[3]=&hc->getFC_br();
		sc_top = &hc->getSC_top();  // get pointer to synapse control class of HICANN 0
		sc_bot = &hc->getSC_bot();  // get pointer to synapse control class of HICANN 0
		lcl = &hc->getLC_cl();  // get pointer to left crossbar layer 1 switch control class of HICANN 0
		lcr = &hc->getLC_cr();
		lctl= &hc->getLC_tl();
		lcbl= &hc->getLC_bl();
		lctr= &hc->getLC_tr();
		lcbr= &hc->getLC_br();
		rca[rc_l]=&hc->getRC_cl(); // get pointer to left repeater of HICANN 0
		rca[rc_r]=&hc->getRC_cr();
		rca[rc_tl]=&hc->getRC_tl();
		rca[rc_bl]=&hc->getRC_bl();
		rca[rc_tr]=&hc->getRC_tr();
		rca[rc_br]=&hc->getRC_br();
		spc = &hc->getSPL1Control(); // get pointer to SPL1 Control
		dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC
		fpc = (FPGAControl*) chip[0]; // use FPGA
		
		IBoardV2Ctrl board(conf, jtag, 0); //create an iboard control instance
		
		nc->configure(); //configure all the instances (read memory registers to get the current internal configuration)
		lcl->configure(); //otherwise the instance assumes to have zeros in all registers
		lcr->configure();
		lctl->configure();
		lctr->configure();
		lcbl->configure();
		lcbr->configure();
		rca[rc_l]->configure();
		rca[rc_r]->configure();
		rca[rc_tl]->configure();
		rca[rc_bl]->configure();
		rca[rc_tr]->configure();
		rca[rc_br]->configure();

		//configure the floating gates
		setupFg();

		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "0: Reset everything (all rams)" << endl;
			cout << "1: Send HICANN reset pulse (restart the testmode after reset!)" << endl;
			cout << "2: JTAG reset" << endl;
			cout << "3: Reset synapse drivers" << endl;
			cout << "4: Reset synapse drivers and synapses in top half" << endl;
			cout << "5: ARQ reset" << endl;
			cout << "6: Turn all BEG off" << endl;
			cout << "7: Reset all neurons" << endl;
			cout << "8: Configure neuronbuilder MUXes manually" << endl;
			cout << "9: Turn FPGA BEG off / cut the DNC->HICANN connection" << endl;
			cout << "h: Write decoder values 0xf to all synapses" << endl;
			cout << "H: Write weights to all synapses" << endl;
			cout << "f: Set PLL to 100 MHz output" << endl;
			cout << "F: Set PLL to a custom value" << endl;
			cout << "e: Error search in synaptic drivers" << endl;
			cout << "s: Continously stimulate neuron" << endl;
			cout << "S: Continously stimulate neuron with ''noisy'' current" << endl;
			cout << "c: Configure a neuron manually" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "l: Loopback FPGA-HICANN-FPGA" << endl;
			cout << "u: Send a spike train from FPGA to a neuron" << endl;
			cout << "d: Activate FPGA pulse FIFO (playback memory)" << endl;
			cout << "D: Activate FPGA BEG" << endl;
			cout << "i: Activate synaptic input" << endl;
			cout << "I: Set preout configuration for a driver" << endl;
			cout << "o: Configure HICANN for simple sampling test" << endl;
			cout << "O: Configure HICANN for simple sampling test with FPGA BEG" << endl;
			cout << "j: Configure HICANN for jitter measurement (HICANNv2)" << endl;
			cout << "r: Configure repeaters and crossbars" << endl;
			cout << "R: Read out the neuron spikes via FPGA" << endl;
			cout << "t: Read out repeater test input" << endl;
			cout << "a: Activate neuron" << endl;
			cout << "A: Activate neuron without using preout as trigger" << endl;
			cout << "b: Turn on the BEG" << endl;
			cout << "B: Configure BEG" << endl;
			cout << "w: Set weight for a synapse" << endl;
			cout << "W: Set decoder for a synapse" << endl;
			cout << "v: Calibrate VOL / VOH (HICANNv2 only)" << endl;
			cout << "V: Evaluate synaptic driver concatenation" << endl;
			cout << "g: Write an FG line" << endl;
			cout << "x: Exit" << endl;
			cin >> c;
			
			switch(c){

				case '0':{
					cout << "resetting everything" << endl;
					cout << "resetting synapse drivers and top synapses" << endl;
					sc_top->reset_all();
					sc_bot->reset_drivers();
					cout << "resetting crossbars and switches" << endl;
					lcl->reset();
					lcr->reset();
					lctl->reset();
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					cout << "resetting repeater blocks" << endl;
					rca[rc_l]->reset();
					rca[rc_tl]->reset();
					rca[rc_tr]->reset();
					rca[rc_bl]->reset();
					rca[rc_br]->reset();
					rca[rc_r]->reset();
					cout << "resetting neuron builder" << endl;
					nbc->initzeros();
					cout << "resetting neuron control" << endl;
					nc->nc_reset();
				} break;

				case '1':{
					cout << "resetting HICANN" << endl;
					jtag->FPGA_set_fpga_ctrl(1);
				} break;

				case '2':{
					cout << "resetting test logic" << endl;
					jtag->reset_jtag();
				} break;

				case '3':{
					cout << "resetting synapse drivers" << endl;
					sc_top->reset_drivers();
					sc_bot->reset_drivers();
				} break;

				case '4':{
					cout << "resetting synapse drivers and synapses in top half" << endl;
					sc_top->reset_all();
				} break;

				case '5':{
					dbg(0) << "Try Init() ..." ;
					if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
						dbg(0) << "ERROR: Init failed, abort" << endl;
						return 0;
					}
					dbg(0) << "Reset software part of ARQ.";
					ll->arq.Reset();
				} break;

				case '6':{
					nc->beg_off(8);
				} break;

				case '7':{
					nbc->initzeros();
				} break;

				case '8':{
					uint one, two;
					cout << "Enter value for the first MUX: " << endl;
					cout << "Choices are:\n 0 - Vctrl0 top\n 1 - preout top\n 2 - fire line of Neuron 0\n" <<
					    " 3 - FG_out 0\n 4 - even denmems top\n 5 - even denmems bottom\n 6 - odd denmems top\n" << 
					    " 7 - odd denmems bottom\n 9 - FG_out 1" << endl;
					cin >> one;
					cout << "Enter value for the second MUX: ";
					cout << "Choices are:\n 0 - Vctrl0 bottom\n 1 - preout bottom\n" <<
					    " 3 - FG_out 2\n 4 - even denmems top\n 5 - even denmems bottom\n 6 - odd denmems top\n" << 
					    " 7 - odd denmems bottom\n 9 - FG_out 3" << endl;
					cin >> two;
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(one,two);
				} break;

				case '9':{
					jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, comm->jtag2dnc(hc->addr())); // stop the BEG
					nc->dnc_enable_set(0,0,0,0,0,0,0,0); //disable all DNC inputs
					spc->write_cfg(0x0); //disable spl1 channels
				} break;

				case 'h':{
					vector<uint> address(32,0xffffffff);
					for (int i=0; i<224; i++){
						if(kbhit()){ // if keyboard has input pending, break from loop
							getch(); // "resets" kbhit by removing a key from the buffer
							break;
						}
						cout << "Writing double-line " << i << ", press any key to stop..." << endl;
						#if HICANN_V == 1
						if ((i%4==0) || (i%4==1))sc_top->write_decoder(i, address, address);
						#elif HICANN_V >= 2
						if ((i%4==0) || (i%4==2))sc_top->write_decoder(i, address, address);
						#endif
					}
				} break;

				case 'H':{
					uint wt, k;
					cout << "Input weight (0-15): ";
					cin >> wt;
					wt = wt + (wt << 4) + (wt << 8) + (wt << 12) + (wt << 16) + (wt << 20) + (wt << 24) + (wt << 28);
					vector<uint> weights(32, wt), zeros(32,0);
					
					for (int i=0; i<224; i++){
						//~ if(kbhit()){ // if keyboard has input pending, break from loop
							//~ getch(); // "resets" kbhit by removing a key from the buffer
							//~ break;
						//~ }
						cout << "Writing line " << i << ", press any key to stop..." << endl;
						sc_top->write_weight(i, weights);
						cin >> k;
						if(k) sc_top->write_weight(i, zeros);
					}
				} break;

				case 'f':{
					set_pll(2,1);
				} break;

				case 'F':{
					uint mult, div;
					cout << "Base clock is 50 MHz" << endl;
					cout << "Enter multiplicator: ";
					cin >> mult;
					cout << "Enter divisor: ";
					cin >> div;
					set_pll(mult,div);
				} break;

				case 'e':{
					set_pll(2,1);
					///configure neuron
					cout << "Configuring neuron" << endl;
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(0,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					///configure neuron control and background generators
					cout << "Launching background generators" << endl;
					configure_hicann_bgen(1,200,0,0,0,0);
					
					/// configure switches and crossbars
					cout << "Configuring switches" << endl;
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					
					#if HICANN_V>=2
					lcl->switch_connect(1,0,1); //connect hor. line 1 to vert. line 0
					lcl->switch_connect(17,8,1);
					lcl->switch_connect(33,16,1);
					lcl->switch_connect(49,24,1);
					
					lcr->switch_connect(9,132,1); //connect hor. line 9 to vert. line 132
					lcr->switch_connect(57,156,1);
					lcr->switch_connect(41,148,1);
					lcr->switch_connect(25,140,1);
					
					lctl->switch_connect(110,0,1); //connect hor. line 110 to vert. line 0
					lctl->switch_connect(82,24,1);
					lctl->switch_connect(54,16,1);
					lctl->switch_connect(26,8,1);
					
					//upper row on the right side can not work due to this connectivity...
					lctr->switch_connect(109,132,1); //connect hor. line 109 to vert. line 132
					lctr->switch_connect(81,156,1);
					lctr->switch_connect(53,148,1);
					lctr->switch_connect(25,140,1);
					#elif HICANN_V==1
					lcl->switch_connect(0,32,1); //connect hor. line 0 to vert. line 0
					lcl->switch_connect(8,8,1);
					lcl->switch_connect(16,16,1);
					lcl->switch_connect(24,24,1);
					
					lcr->switch_connect(32,128,1); //connect hor. line 32 to vert. line 128
					lcr->switch_connect(40,136,1);
					lcr->switch_connect(48,144,1);
					lcr->switch_connect(56,152,1);
					
					lctl->switch_connect(110,0,1); //connect hor. line 110 to vert. line 0
					lctl->switch_connect(26,8,1);
					lctl->switch_connect(54,16,1);
					lctl->switch_connect(82,24,1);
					
					lctr->switch_connect(111,128,1); //connect hor. line 111 to vert. line 128
					lctr->switch_connect(27,136,1);
					lctr->switch_connect(55,144,1);
					lctr->switch_connect(83,152,1);
					#endif
					
					/// configure sending repeaters
					cout << "Configuring repeaters" << endl;
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit BEG data on line 0
					rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(8, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(16, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(20, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //reserved for the neuron spike output -> DNC
					
					/// configure synapse drivers and arrays
					uint gmax, top_ex, top_in, bot_ex, bot_in;
					gmax=6; top_ex=1; top_in=0; bot_ex=1; bot_in=0;
					
					vector<uint> weights(32,0xffffffff),address(32,0); //one row, all weights max, decoder 0
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset(); //do this before doing anything with the synapse drivers (resets the DLLs)
					rca[rc_tr]->reset(); //do this before doing anything with the synapse drivers (resets the DLLs)

					int k=0;
					cout << "Searching left synapses half" << endl;
					for(int u=52; u < 224; u+=56){
						nc->beg_on(8);
						cout << "Writing driver " << u << endl;
						sc_top->drv_config(u, top_ex, top_in, bot_ex, bot_in, 1, 1, 0); //activate local input at the upper driver
						sc_top->preout_set(u, 0, 0, 0, 0); //set preout signals to respond to 0,0
						sc_top->drv_set_gmax(u, 0, 0, gmax, gmax); //set gmax
						sc_top->write_weight(u, weights);
						#if HICANN_V>=2
						sc_top->write_weight(u+1, weights);
						#elif HICANN_V==1
						sc_top->write_weight(u+2, weights);
						#endif
						sc_top->write_decoder(u, address, address);
						usleep(100000);
						nc->beg_off(8);
						cin >> k;
						if(k){
							sc_top->drv_config(u, 0, 0, 0, 0, 0, 1, 0);
							sc_top->preout_set(u, 0, 0, 0, 0);
							sc_top->drv_set_gmax(u, 0, 0, 0, 0);
						}

						for(int i=u-4; i > u-54; i-=4){ //activate top input in 13 drivers below
							if (i==-1) {
								sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
								sc_top->preout_set(i, 0, 0, 0, 0);
								sc_top->drv_set_gmax(i, 0, 0, 0, 0);
							}
							else{
								nc->beg_on(8);
								cout << "Writing driver " << i << endl;
								sc_top->drv_config(i, top_ex, top_in, bot_ex, bot_in, 1, 0, 1);
								sc_top->preout_set(i, 0, 0, 0, 0);
								sc_top->drv_set_gmax(i, 0, 0, gmax, gmax);
								sc_top->write_weight(i, weights);
								#if HICANN_V>=2
								sc_top->write_weight(u+1, weights);
								#elif HICANN_V==1
								sc_top->write_weight(u+2, weights);
								#endif
								sc_top->write_decoder(i, address, address);
								usleep(100000);
								nc->beg_off(8);
								cin >> k;
								if(k){
									sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
									sc_top->preout_set(i, 0, 0, 0, 0);
									sc_top->drv_set_gmax(i, 0, 0, 0, 0);
								}
							}
						}									
						//~ ///usleep(1000000);
						//~ ///nc->beg_off(8);
						//~ ///cin >> k;
						//~ ///sc_top->reset_drivers();
						//~ ///nc->beg_on(8);
					}
					
					cout << "Searching right synapses half" << endl;
					#if HICANN_V>=2
					for(int u=50; u < 224; u+=56){
					#elif HICANN_V==1
					for(int u=53; u < 224; u+=56){
					#endif
						nc->beg_on(8);
						cout << "Writing driver " << u << endl;
						sc_top->drv_config(u, top_ex, top_in, bot_ex, bot_in, 1, 1, 0); //activate local input at the upper driver
						sc_top->preout_set(u, 0, 0, 0, 0); //set preout signals to respond to 0,0
						sc_top->drv_set_gmax(u, 0, 0, gmax, gmax); //set gmax
						sc_top->write_weight(u, weights);
						#if HICANN_V>=2
						sc_top->write_weight(u+1, weights);
						#elif HICANN_V==1
						sc_top->write_weight(u+2, weights);
						#endif
						sc_top->write_decoder(u, address, address);
						usleep(100000);
						nc->beg_off(8);
						cin >> k;
						if(k){
							sc_top->drv_config(u, 0, 0, 0, 0, 0, 1, 0);
							sc_top->preout_set(u, 0, 0, 0, 0);
							sc_top->drv_set_gmax(u, 0, 0, 0, 0);
						}
						for(int i=u-4; ((i > u-54) && (i >= 0)); i-=4){ //activate top input in 13 drivers below
							if (i==-1) {
								sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
								sc_top->preout_set(i, 0, 0, 0, 0);
								sc_top->drv_set_gmax(i, 0, 0, 0, 0);
							}
							else{
								nc->beg_on(8);
								cout << "Writing driver " << i << endl;
								sc_top->drv_config(i, top_ex, top_in, bot_ex, bot_in, 1, 0, 1);
								sc_top->preout_set(i, 0, 0, 0, 0);
								sc_top->drv_set_gmax(i, 0, 0, gmax, gmax);
								sc_top->write_weight(i, weights);
								#if HICANN_V>=2
								sc_top->write_weight(u+1, weights);
								#elif HICANN_V==1
								sc_top->write_weight(u+2, weights);
								#endif
								sc_top->write_decoder(i, address, address);
								usleep(100000);
								nc->beg_off(8);
								cin >> k;
								if(k){
									sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
									sc_top->preout_set(i, 0, 0, 0, 0);
									sc_top->drv_set_gmax(i, 0, 0, 0, 0);
								}
							}
						}
					}
				} break;

				case 's':{
					cout <<"Enter peak current of constant stimulus:";
					uint sval=0;
					cin >> dec >> sval;
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initval(0, sval);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}
				} break;

				case 'S':{
					uint mean, rndmax;
					std::vector<int> rndvalues(129);
					cout <<"Enter current mean (0-1023): ";
					cin >>dec>>mean;
					cout <<"Enter maximum current jitter (0-mean): ";
					cin >>dec>>rndmax;
					
					srand(time(0));
					for(int i=0; i<129; i++) rndvalues[i]=(rand() % (rndmax*2)) + (mean-rndmax);
					
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initArray(0, rndvalues);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}
				} break;

				case 'c':{
					uint add, fireb, firet, connect, ntop, nbot, spl1;

					cout<<"Address:";
					cin>>add;
					cout<<"firetop, firebot, and connect:";
					cin>>fireb>>firet>>connect;
					cout<<"Neuron configuration top and bottom (hex)";
					cin>>hex>>ntop>>nbot;
					cout<<"Spl1 address:";
					cin>>spl1;
					uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
					nbc->write_data(add, value);
				} break;

				case 'p':{
					setupFg();
					//~ log(Logger::INFO)<<"tm_jitter::setting floating gate arrays to zero ...";
					//~ for (int fg=0; fg<4; fg++) fc[fg]->initzeros(0);
					//~ for(int i=0; i<24; i++){
						//~ for(int cntlr=0; cntlr<4; cntlr++){
							//~ while(fc[cntlr]->isBusy());
							//~ fc[cntlr]->program_cells(i,0,0);
						//~ }
					//~ }
					
					int bank=0; // bank number, programming data is written to
					//write currents first as ops need biases before they work.
					log(Logger::INFO)<<"tm_jitter::writing currents ...";
					for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
						for(int fg_num = 0; fg_num < 4; fg_num += 1){	
							log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
							fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
							while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
							fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
							while(fc[fg_num]->isBusy());
							fc[fg_num]->program_cells(lineNumber,bank,1); //programm up
						}
						bank =( bank +1)%2; //bank is 1 or 0
					}
					log(Logger::INFO)<<"tm_jitter::writing voltages ...";
					for(uint lineNumber = 0; lineNumber < 24; lineNumber += 2){
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
						for(int fg_num = 0; fg_num < 4; fg_num += 1){	
							log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
							fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
							while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
							fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
							while(fc[fg_num]->isBusy());
							fc[fg_num]->program_cells(lineNumber,bank,1); //programm up
						}
						bank =( bank +1)%2; //bank is 1 or 0
					}
				}break;

				case 'l':{
					uint64_t systime = 0xffffffff;
					uint mode, neuron;
					uint64_t jtag_recpulse = 0;

					jtag->HICANN_set_test(0);
					jtag->HICANN_set_layer1_mode(0x55);
					
					nc->nc_reset();		
					nc->dnc_enable_set(1,0,1,0,1,0,1,0);
					nc->loopback_on(0,1);
					nc->loopback_on(2,3);
					nc->loopback_on(4,5);
					nc->loopback_on(6,7);
									
					spc->write_cfg(0x55ff);
					
					dc->setTimeStampCtrl(0x0); // Disable DNC timestamp features
					dc->setDirection(0x55); // set transmission directions in DNC (for HICANN 0 only)
					
					jtag->HICANN_read_systime(systime);
					
					cout << "Choose Loopback test mode" << endl;
					cout << "1: Using playback fifo and trace fifo" << endl;
					cout << "2: Using FPGA backgroud generator and trace fifo" << endl;
					cout << "3: Using single pulses and RX register" << endl;
					cin >> mode;
					cout << "Enter Neuron number: ";
					cin >> neuron;
					
					/// fill the fifo and start it
					if(mode==1) {
						uint64_t ev1, ev2, fpgaev;
						for (int i = 1; i < 50; i++){
							ev1=gen_spl1(0, neuron, 150*2*i);
							ev2=gen_spl1(0, neuron, 150*(2*i+1));
							fpgaev=gen_fpga(ev1, ev2);
							jtag->FPGA_write_playback_fifo(fpgaev);
						}
						jtag->HICANN_restart_time_counter();
						cout << "Press the start button, then push ENTER" << endl;
						cin.ignore();

						jtag->FPGA_enable_tracefifo();
						jtag->FPGA_enable_pulsefifo();
						usleep(1000);
						jtag->FPGA_disable_tracefifo();
						
						cout << "From trace FIFO received:" << endl;	
						int i=0;				
						while((!jtag->FPGA_empty_pulsefifo()) && (i < 20)) {
							i++;
							jtag->FPGA_get_trace_fifo(jtag_recpulse);
							cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
							cout << " from channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
							cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
						}
						jtag->FPGA_reset_tracefifo();
						jtag->FPGA_disable_pulsefifo(); //hopefully it should be empty by then...
					}

					/// activate background generator
					if(mode==2) {
						jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 1000, 0, neuron, 0, comm->jtag2dnc(hc->addr()));
						jtag->FPGA_enable_tracefifo();
						usleep(1000);
						jtag->FPGA_disable_tracefifo();
						jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, comm->jtag2dnc(hc->addr()));
						
						cout << "From trace FIFO received:" << endl;
						int i=0;					
						while((!jtag->FPGA_empty_pulsefifo()) && (i < 20)) {
							i++;
							jtag->FPGA_get_trace_fifo(jtag_recpulse);
							cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
							cout << " from channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
							cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
						}
						jtag->FPGA_reset_tracefifo();
					}
					
					/// send single pulse
					if(mode==3) {
						uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
						jtag->HICANN_set_test(0x2);  //for manual pulse event insertion in HICANN
						jtag->HICANN_set_double_pulse(0);
						jtag->HICANN_start_pulse_packet(jtag_sendpulse);

						// last received pulse readout
						jtag->HICANN_get_rx_data(jtag_recpulse);
						cout << "Last Received pulse: 0x" << hex << jtag_recpulse << endl;
						cout << "Received Neuron number: " << dec << ((jtag_recpulse>>15) & 0x3f) << endl;
						cout << "From channel: " << dec << ((jtag_recpulse>>21) & 0x7) << endl;
					}
					
					jtag->HICANN_set_test(0x0);
					
					if ((uint)((jtag_recpulse>>15)&0x3f) != (neuron&0x3f)) cout << "TESTFAIL: Packet(s) not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
					else cout << "TESTPASS: Transmission of pulse packet(s) via JTAG->HICANN(L1)->JTAG successful." << endl;
				} break;

				case 'd':{
					uint64_t ev1, ev2, fpgaev, dummy;
					uint channel=4, nnumber=0, delay=100;
					
					cout << "Choose channel:" << endl;
					cin >> channel;
					cout << "Choose neuron number:" << endl;
					cin >> nnumber;
					cout << "Choose delay between events:" << endl;
					cin >> delay;
					
					//NeuronControl stuff
					nc->nc_reset(); //reset neuron control
					nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs

					//DNC stuff
					spc->write_cfg(0x0ffff); //0xff00?
					dc->setTimeStampCtrl(0x0);
					dc->setDirection(0xff); //all 8 l1 channels down into HICANN 0
					
					//generate events
					cout << "Generating events and transmitting them to FPGA..." << endl;
					for (int i = 1; i < 4000; i++){
						ev1=gen_spl1(channel, nnumber, delay*2*i);
						ev2=gen_spl1(channel, nnumber, delay*(2*i+1));
						fpgaev=gen_fpga(ev1, ev2);
						jtag->FPGA_write_playback_fifo(fpgaev);
					}

					jtag->HICANN_restart_time_counter();
					cout << "Press the start button, then push ENTER" << endl;
					cin.ignore();
					
					cout << "Running FIFO...";
					jtag->FPGA_enable_pulsefifo();
					usleep(5000000); //time for the FIFO to run
					jtag->FPGA_disable_pulsefifo();
				} break;

				case 'D':{
					uint synaddr1=0, synaddr2=0, chan=0, poisson=0, delay=100;
					cout << "Input beginning neuron number (0-63): ";
					cin >> synaddr1;
					cout << "Input end neuron number (0-63): ";
					cin >> synaddr2;
					cout << "Input channel number (0-7): ";
					cin >> chan;
					cout << "Input delay in cycles: ";
					cin >> delay;
					cout << "Input 0 for deterministic output, 1 for poisson: ";
					cin >> poisson;
					
					nc->nc_reset(); //reset neuron control
					nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs
					
					spc->write_cfg(0x0ffff);
					
					// set DNC to ignore time stamps and directly forward l2 pulses
					dc->setTimeStampCtrl(0x0);
					// set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0xff);

					jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 80, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr())); //to lock the DLL(s)
					usleep(1000);
					jtag->FPGA_set_cont_pulse_ctrl(1, 1<<(chan&0x7), poisson, delay, 0x1aaaa, synaddr1, synaddr2-synaddr1, comm->jtag2dnc(hc->addr())); //actual activity
				} break;

				case 'i':{
					uint gmax, top_ex, top_in, bot_ex, bot_in;
					cout << "Input digital Gmax (0-15): ";
					cin >> gmax;
					cout << "Input top_ex, top_in, bot_ex, bot_in: ";
					cin >> top_ex >> top_in >> bot_ex >> bot_in;
					
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					#if HICANN_V == 1
					sc_top->drv_config(20, 0, 0, 0, 0, 0, 1, 0); //activate local input at row 20
					sc_top->preout_set(20, 3, 3, 3, 3); //set preout signals to respond to 1,1
					sc_top->drv_set_gmax(20, 0, 0, gmax, gmax);
					for (int i=16; i>-1; i=i-4){
						sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
						sc_top->preout_set(i, 3, 3, 3, 3); //set preout signals to respond to 1,1
						sc_top->drv_set_gmax(i, 0, 0, gmax, gmax);
					}
					sc_top->drv_config(0, top_ex, top_in, bot_ex, bot_in, 1, 0, 1);
					sc_top->preout_set(0, 0, 0, 0, 0); //set preout signals to respond to 0,0
					#elif HICANN_V >= 2
					sc_top->drv_config(0, top_ex, top_in, bot_ex, bot_in, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, gmax, gmax);
					#endif
					
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, address, address);
				} break;

				case 'I':{
					uint drv, p0, p1, p2, p3;
					cout << "Enter driver number: "; cin >> drv;
					cout << "Enter value for preout 0: "; cin >> p0;
					cout << "Enter value for preout 1: "; cin >> p1;
					cout << "Enter value for preout 2: "; cin >> p2;
					cout << "Enter value for preout 3: "; cin >> p3;
					sc_top->preout_set(drv, p0, p1, p2, p3);
				} break;

				case 'o':{
					vector<uint> weightsex(32,0xffffffff), weightsinh(32,0x11111111), address(32,0);
					uint num, stim, period;

					cout << "Enter neuron number: ";
					cin >> dec >> num;
					cout << "Enter constant stimulus (0-1023): ";
					cin >> dec >> stim;
					cout << "Enter average delay between synapse pulses: ";
					cin >> dec >> period;

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0); //excitatory
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 1, 1, 1, 1);
					sc_top->write_weight(0, weightsex);
					sc_top->write_decoder(0, address, weightsex); //only one line

					sc_top->drv_config(4, 0, 1, 0, 1, 1, 1, 0); //inhibitory
					sc_top->preout_set(4, 0, 0, 0, 0);
					sc_top->drv_set_gmax(4, 1, 1, 1, 1);
					sc_top->write_weight(4, weightsinh);
					sc_top->write_decoder(4, address, weightsex); //only one line
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					lcl->switch_connect(49,24,1);
					lctl->switch_connect(2,24,1);
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(num,0,1); //current stimulus activated
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initval(0, stim);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}

					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					nc->nc_reset(); //reset neuron control
					nc->set_seed(222); //set seed for BEGs
					nc->beg_configure(0, 1, period);
					nc->beg_configure(1, 1, period);
					nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					nc->beg_on(0); //turn all BEGs on
					nc->set_seed(333); //set other seed for second BEG
					nc->beg_on(1); //turn all BEGs on
				} break;

				case 'O':{
					vector<uint> weightsex(32,0xffffffff), weightsinh(32,0x11111111), address(32,0);
					uint num, stim, period;

					cout << "Enter neuron number: ";
					cin >> dec >> num;
					cout << "Enter constant stimulus (0-1023): ";
					cin >> dec >> stim;
					cout << "Enter average delay between synapse pulses: ";
					cin >> dec >> period;

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0); //excitatory
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 1, 1, 1, 1);
					sc_top->write_weight(0, weightsex);
					sc_top->write_decoder(0, address, weightsex); //only one line

					sc_top->drv_config(4, 0, 1, 0, 1, 1, 1, 0); //inhibitory
					sc_top->preout_set(4, 0, 0, 0, 0);
					sc_top->drv_set_gmax(4, 1, 1, 1, 1);
					sc_top->write_weight(4, weightsinh);
					sc_top->write_decoder(4, address, weightsex); //only one line
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					lcl->switch_connect(49,24,1);
					lctl->switch_connect(2,24,1);
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(num,0,1); //current stimulus activated
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initval(0, stim);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}

					nc->nc_reset(); //reset neuron control
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs
					
					spc->write_cfg(0x0ffff);
					
					// set DNC to ignore time stamps and directly forward l2 pulses
					dc->setTimeStampCtrl(0x0);
					// set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0xff);

					jtag->FPGA_set_cont_pulse_ctrl(1, 0x3, 1, period, 0x1a5a5, 0, 0, comm->jtag2dnc(hc->addr()));
				} break;

				case 'j':{
					set_pll(2,1);
					
					/// configure switches and crossbars
					cout << "Configuring switches" << endl;
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset(); //reset top left select switch
					lctl->reset();
					
					lcl->switch_connect(1,0,1); //connect hor. line 1 to vert. line 0
					lcl->switch_connect(17,8,1);
					lcl->switch_connect(33,16,1);
					lcl->switch_connect(49,24,1);
					
					lcr->switch_connect(9,132,1); //connect hor. line 9 to vert. line 132
					lcr->switch_connect(57,156,1);
					lcr->switch_connect(41,148,1);
					lcr->switch_connect(25,140,1);
					
					lctl->switch_connect(110,0,1); //connect hor. line 110 to vert. line 0
					lctl->switch_connect(82,24,1);
					lctl->switch_connect(54,16,1);
					lctl->switch_connect(26,8,1);
					
					//upper row on the right side can not work due to this connectivity...
					lctr->switch_connect(109,132,1); //connect hor. line 109 to vert. line 132
					lctr->switch_connect(81,156,1);
					lctr->switch_connect(53,148,1);
					lctr->switch_connect(25,140,1);
					
					/// configure sending repeaters
					cout << "Configuring repeaters" << endl;
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit BEG data on line 0
					rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(8, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(16, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(20, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

					///configure background generators
					configure_hicann_bgen(1,400,0,0,0,0);
					
					/// configure synapse drivers and arrays
					uint gmax=4, top_ex=1, top_in=0, bot_ex=1, bot_in=0;
					vector<uint> weights(32,0xffffffff), address(32,0xffffffff); //one row, all weights 0, decoder 15

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					rca[rc_tr]->reset_dll();

					cout << "Configuring left synapses half" << endl;
					for(int u=52; u < 224; u+=56){
						cout << "Writing driver " << u << endl;
						sc_top->drv_config(u, top_ex, top_in, bot_ex, bot_in, 1, 1, 0); //activate local input at the upper driver
						sc_top->preout_set(u, 0, 0, 0, 0); //set preout signals to respond to 0,0
						sc_top->drv_set_gmax(u, 0, 0, gmax, gmax); //set gmax
						sc_top->write_weight(u, weights);
						sc_top->write_weight(u+1, weights);
						sc_top->write_decoder(u, address, address);
						
						for(int i=u-4; i > u-54; i-=4){ //activate top input in 13 drivers below
							if (i==84){
								cout << "Skipping driver " << i << endl;
								sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
								sc_top->preout_set(i, 3, 3, 3, 3);
								sc_top->write_decoder(i, address, address);
							}
							else{
								cout << "Writing driver " << i << endl;
								sc_top->drv_config(i, top_ex, top_in, bot_ex, bot_in, 1, 0, 1);
								sc_top->preout_set(i, 0, 0, 0, 0);
								sc_top->drv_set_gmax(i, 0, 0, gmax, gmax);
								sc_top->write_weight(i, weights);
								sc_top->write_weight(i+1, weights);
								sc_top->write_decoder(i, address, address);
							}
						}
					}
					
					cout << "Configuring right synapses half" << endl;
					for(int u=50; u < 224; u+=56){
						cout << "Writing driver " << u << endl;
						sc_top->drv_config(u, top_ex, top_in, bot_ex, bot_in, 1, 1, 0); //activate local input at the upper driver
						sc_top->preout_set(u, 0, 0, 0, 0); //set preout signals to respond to 0,0
						sc_top->drv_set_gmax(u, 0, 0, gmax, gmax); //set gmax
						sc_top->write_weight(u, weights);
						sc_top->write_weight(u+1, weights);
						sc_top->write_decoder(u, address, address);
						
						for(int i=u-4; ((i > u-54) && (i >= 0)); i-=4){ //activate top input in 13 drivers below
							if (i==62){
								cout << "Skipping driver " << i << endl;
								sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 0);
								sc_top->preout_set(i, 3, 3, 3, 3);
								sc_top->write_decoder(i, address, address);
							}
							else{
								cout << "Writing driver " << i << endl;
								sc_top->drv_config(i, top_ex, top_in, bot_ex, bot_in, 1, 0, 1);
								sc_top->preout_set(i, 0, 0, 0, 0);
								sc_top->drv_set_gmax(i, 0, 0, gmax, gmax);
								sc_top->write_weight(i, weights);
								sc_top->write_weight(i+1, weights);
								sc_top->write_decoder(i, address, address);
							}
						}
					}
				} break;

				case 'u':{
					uint gmax=5, top_ex=1, top_in=0, bot_ex=1, bot_in=0;
					vector<uint> weights(32,0x33333333), address(32,0); //one row, all weights 3, decoder 0
					bool odd=false, low=false;
					uint64_t ev1, ev2, fpgaev;
					uint channel=0, nnumber=0, num;
					uint linecount=0, maxlockpulse;
					std::string buffer;
					
					cout << "Hardware neuron to activate: ";
					cin >> num;
					
					///activate neuron
					set_pll(2,1);					
					if (num%2) odd=true;
					if (num/256) low=true;
					nbc->setOutputEn(true,true);
					if (odd && low) nbc->setOutputMux(1,7);
					else if (odd && !low) nbc->setOutputMux(1,6);
					else if (!odd && low) nbc->setOutputMux(1,5);
					else nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					//configure global parameters for neuronbuilder
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,0,0);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(num,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					/// configure switches and crossbars
					cout << "Configuring switches" << endl;
					lcl->reset(); //reset left crossbar
					lctl->reset(); //reset top left select switch

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);

					/// configure sending repeaters
					cout << "Configuring repeaters" << endl;
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); 
					
					/// configure synapse drivers and arrays
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset(); //do this before doing anything with the synapse drivers (resets the DLLs)

					cout << "Configuring synapses" << endl;
					cout << "Writing driver " << 0 << endl;
					sc_top->drv_config(0, top_ex, top_in, bot_ex, bot_in, 1, 1, 0); //activate local input at the driver
					sc_top->preout_set(0, 0, 0, 0, 0); //set preout signals to respond to 0,0
					sc_top->drv_set_gmax(0, 0, 0, gmax, gmax); //set gmax
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, address); //only one synapse per driver
					sc_top->write_decoder(0, address, weights); //only one synapse per driver
										
					///configure NeuronControl
					nc->nc_reset(); //reset neuron control
					nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs

					///configure DNC
					spc->write_cfg(0x0ffff);
					dc->setTimeStampCtrl(0x0);
					dc->setDirection(0xff); //all 8 l1 channels down into HICANN 0
					
					///read in the input spike data
					cout << "Reading spikes.txt file..." << endl;
					uint times[16000];
					ifstream file("spikes.txt");
					while(!file.eof()){ //read in the spike times (in FPGA cycles)
						getline(file,buffer);
						times[linecount]=atoi(buffer.c_str());
						linecount++;
					}
					file.close();
					
					cout << "Transmitting events to the FPGA..." << endl;
					for (int i = 1; i < 200; i++){ //DLL locking events
						ev1=gen_spl1(channel, nnumber, 2000*2*i);
						ev2=gen_spl1(channel, nnumber, 2000*(2*i+1));
						fpgaev=gen_fpga(ev1, ev2);
						jtag->FPGA_write_playback_fifo(fpgaev);
						maxlockpulse=3000*(2*i+1);
					}
					for (int i = 0; i < linecount-1; i+=2){ //actual events
						ev1=gen_spl1(channel, nnumber, times[i]+maxlockpulse+4000);
						//~ cout << times[i]-times[i-1] << " " << times[i+1]-times[i] << endl;
						ev2=gen_spl1(channel, nnumber, times[i+1]+maxlockpulse+4000);
						fpgaev=gen_fpga(ev1, ev2);
						jtag->FPGA_write_playback_fifo(fpgaev);
					}
					
					///FIFO handling
					jtag->HICANN_restart_time_counter();
					cout << "Press the start button, then push ENTER" << endl;
					cin.ignore();

					cout << "Running FIFO..." << flush;
					jtag->FPGA_enable_pulsefifo();
					usleep(5000000); //time for the FIFO to run
					jtag->FPGA_disable_pulsefifo();
					cout << "done";
				} break;
				
				case 'r':{
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					
					#if HICANN_V == 1
					lcl->switch_connect(40,40,1); //connect hor. line 40 to vert. line 40
					lctl->switch_connect(10,40,1); //connect hor. line 10 to vert. line 40
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(20, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 20 to transmit BEG data on line 40
					#elif HICANN_V >= 2
					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					#endif
				} break;

				case 'R':{
					// the mergers have to be configured extra (in "b")
					/// neuron control
					nc->dnc_enable_set(0,1,1,1,1,1,1,1);  //channel 0 for spl1 input, channels 1-7 for BEG forwarding
					
					/// SPL1 Control
					//only channel 0 enabled in the DNC direction
					spc->write_cfg(0x1fe01); //configure timgetFpgaRxDataestamp, direction, enable of l1
						
					/// DNC: this stuff doesnt do anything (???)
					dc->setTimeStampCtrl(0x1); //Disable/Enable DNC timestamp features
					//set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0xfe);   //direction of the l1 busses: only 0 towards DNC
					
					/// enable the FIFO and read back the events
					int i = 0;
					int m63=63, m32=32, m47=47, m53=53, m15=15, m21=21; //compiler produces warnings without this...
					bool flag;
					uint64_t jtag_recpulse = 0;
					uint64_t timestamp1, timestamp2, nnumber1, nnumber2, channel1, channel2;

			        ///Bit63: Zeigt an ob ein Eintrag 2 oder 1 Event enthält (0=1 Event, 1=2 Events)
			        ///Bit58-32: Event Nummer 2
			        ///Bit26-0: Event Nummer 1

			        ///Bit26-24: Auswahl HICANN am DNC 3bit
			        ///Bit23-21: Auswahl SPl1 Interface im HICANN 3bit
			        ///Bit20-15: Neuronevent Identifier 6bit
			        ///Bit14-0: Zeitmarke 15bit

					jtag->FPGA_enable_tracefifo();
					jtag->FPGA_disable_tracefifo();
					
					while((!jtag->FPGA_empty_pulsefifo()) && (i < 200)) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Pulse received 0x" << hex << jtag_recpulse << endl;

						if(jtag_recpulse & (1 << m63)) flag=true; //2 events in 1 entry
						else flag=false;
						
						timestamp1=jtag_recpulse & 0x7fff;
						nnumber1=(jtag_recpulse & (0x3f << m15)) >> m15;
						channel1=(jtag_recpulse & (0x7 << m21)) >> m21;
						if(flag){
							timestamp2=(jtag_recpulse & (0x7fff << m32)) >> m32;
							nnumber2=(jtag_recpulse & (0x3f << m47)) >> m47;
							channel2=(jtag_recpulse & (0x7 << m53)) >> m53;
						}

						cout << "Event 1: Neuron number " << dec << nnumber1 << " at time " << timestamp1 << ". ";
						if(flag) cout << "Event 2: Neuron number " << dec << nnumber2 << " at time " << timestamp2 << ". ";
						cout << endl;
					}

					jtag->FPGA_reset_tracefifo(); //throw away the rest of the events
				} break;

				case 't':{
					char sw;
					uint rep, odd=0;
					rc_loc loca;
					cout << "Choose repeater block: " << endl;
					cout << "1: Center left" << endl;
					cout << "2: Top left" << endl;
					cout << "3: Bottom left" << endl;
					cout << "4: Top right" << endl;
					cout << "5: Bottom right" << endl;
					cout << "6: Center right" << endl;
					cin >> sw;
					cout << "Choose repeater number: " << endl;
					cin >> rep;
					
					if (sw == '1') loca=rc_l;
					else if (sw == '2') loca=rc_tl;
					else if (sw == '3') loca=rc_bl;
					else if (sw == '4') loca=rc_tr;
					else if (sw == '5') loca=rc_br;
					else loca=rc_r;
					
					#if HICANN_V>=2
					if (rca[loca]->repaddr(rep)%2) odd=1;
					#elif HICANN_V==1
					if (rep%2) odd=1;
					#endif
					
					rca[loca]->conf_repeater(rep, RepeaterControl::TESTIN, RepeaterControl::OUTWARDS, true); //configure repeater in desired block to receive BEG data
					usleep(500); //time for the dll to lock
					if (rca[loca]->is_spl1rep(rep)) rca[loca]->tin(rep, true);
					rca[loca]->stopin(odd); //reset the full flag
					rca[loca]->startin(odd); //start recording received data to channel 0
					usleep(1000);  //recording lasts for ca. 4 µs - 1ms
					if (rca[loca]->fullflag(odd)) cout << "Full flag is on!" << endl;
					else cout << "Full flag is off!" << endl;
					
					uint neuron, time;
					for (int i=0; i<3; i++){
						rca[loca]->tdata_read(odd, i, neuron, time);
						cout << "Received neuron number " << dec << neuron << " at time of " << time << " cycles" << endl;
					}
					rca[loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
					rca[loca]->stopin(odd);
					rca[loca]->tout(rep, false); //set tout back to 0 to prevent conflicts
				} break;

				case 'a':{
					uint num, nnum, current;
					bool odd=false, low=false;
					cout << "Hardware neuron to activate: ";
					cin >> num;
					cout << "Neuron number to transmit: ";
					cin >> nnum;
					cout << "Activate current input: 0/1: ";
					cin >> current;
					
					if (num%2) odd=true;
					if (num/256) low=true;
					nbc->setOutputEn(true,true);
					if (odd && low) nbc->setOutputMux(1,7);
					else if (odd && !low) nbc->setOutputMux(1,6);
					else if (!odd && low) nbc->setOutputMux(1,5);
					else nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					//configure global parameters for neuronbuilder
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(num,nnum,current);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
				} break;

				case 'A':{
					uint num, nnum;
					bool odd=false, low=false;
					cout << "Hardware neuron to activate: ";
					cin >> num;
					cout << "Neuron number to transmit: ";
					cin >> nnum;
					
					if (num%2) odd=true;
					if (num/256) low=true;
					nbc->setOutputEn(true,true);
					if (odd && low) nbc->setOutputMux(8,7);
					else if (odd && !low) nbc->setOutputMux(8,6);
					else if (!odd && low) nbc->setOutputMux(8,5);
					else nbc->setOutputMux(8,4);
					
					nbc->initzeros(); //first, clear neuron builder
					//configure global parameters for neuronbuilder
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron_jitter(num,nnum);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
				} break;

				case 'b':{
					configure_hicann_bgen(1, 50, 200, 0, 0, 0);
				}break;

				case 'B':{
					uint delay, poisson, seed;
					cout << "Enter delay between spikes (in cycles): ";
					cin >> delay;
					cout << "Enter zero for constant delay, non-zero for Poisson: ";
					cin >> poisson;
					nc->beg_configure(8, poisson, delay);
					if (poisson) {
						cout << "Enter seed: ";
						cin >> seed;
						for(int i=0; i<8; i++){
							nc->beg_off(i);
							nc->set_seed(seed+5*i);
							nc->beg_on(i);
						}
					}
				}break;

				case 'w':{
					uint w, row, col;
					vector<uint> weights(32,0);
					cout << "Enter weight (0-15): ";
					cin >> dec >> w;
					cout << "Enter row (0-223): ";
					cin >> dec >> row;
					cout << "Enter column (0-223): ";
					cin >> dec >> col;
					sc_top->write_weight(row, col, w);
				} break;

				case 'W':{
					uint d, row, col;
					cout << "Enter decoder (0-15): ";
					cin >> dec >> d;
					cout << "Enter row (0-223): ";
					cin >> dec >> row;
					cout << "Enter column (0-223): ";
					cin >> dec >> col;
					sc_top->write_decoder(row, col, d);
				} break;

				case 'v':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint runs, vline, falsecount;
					bool silent;
					double ran_min, ran_max, ran_step, vol_min, voh_max, volh_step, ran_curr, vol_curr, offset;
					cout << "how many runs per neuron number?: " << endl;
					cin >> runs;
					cout << "enter minimum range Voh-Vol: " << endl;
					cin >> ran_min;
					cout << "enter maximum range Voh-Vol: " << endl;
					cin >> ran_max;
					cout << "enter step for range: " << endl;
					cin >> ran_step;
					cout << "enter start value for Vol: " << endl;
					cin >> vol_min;
					cout << "enter end value for Voh: " << endl;
					cin >> voh_max;
					cout << "enter step for Voh-Vol: " << endl;
					cin >> volh_step;
					cout << "enter offset of the voltages on the iBoard: " << endl;
					cin >> offset;
					cout << "silent? (do not display neuron numbers)" << endl;
					cin >> silent;
					
					ran_curr=ran_min;
					while (ran_curr <= ran_max){
						vol_curr=vol_min;

						ss.str("");
						ss <<"../results/tm_jitter/rawdata_range"<< ran_curr <<".dat";
						filename = ss.str();
						file.open(filename.c_str(), fstream::app | fstream::out);

						ss1.str("");
						ss1 <<"../results/tm_jitter/errordata_range"<< ran_curr <<".dat";
						filename1 = ss1.str();
						file1.open(filename1.c_str(), fstream::app | fstream::out);

						while (vol_curr+ran_curr <= voh_max){
							cout << "Setting Vol to " << vol_curr << endl;
							board.setVoltage(SBData::vol, vol_curr-offset);
							cout << "Setting Voh to " << vol_curr+ran_curr << endl;
							board.setVoltage(SBData::voh, vol_curr+ran_curr-offset);
							cout << "Voltages set, proceeding with tests..." << endl;
							usleep(1000000);
							
							for (int i=0; i<6; i++) rca[i]->reset();

							lcl->reset(); //reset left crossbar
							lcr->reset(); //reset right crossbar
							lctr->reset();
							lcbr->reset();
							lctl->reset();
							lcbl->reset();

							for (int r=0; r<29; r+=4){ //sending repeater sweep
								rca[rc_l]->conf_spl1_repeater(r, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
								cout << "Using sending repeater " << r << endl;
								file << "Using sending repeater " << r << endl << flush;
								
								cout << "Using bottom left repeater block to receive events" << endl;
								for (int s=0; s<1; s++){ //receiving repeater sweep
									vline=(r+s*32)&0x7f;
									cout << "Using receiving repeater " << vline/2 << endl;
									file << "Using receiving repeater " << vline/2 << endl << flush;
									file1 << vol_curr << "\t" << vol_curr+ran_curr << "\t" << r << "\t" << vline/2 << "\t" << flush;
									lcl->switch_connect(r*2+1,vline,1);
									configure_hicann_bgen(1,100,0,0,0,0); //turn BEGs on
									for (int i=0; i<64; i++){ //neuron numbers
										for (int beg=0; beg<8; beg++) nc->beg_set_number(beg,0); //set numbers to zero to lock
										usleep(50000); //time for the DLL to lock
										for (int beg=0; beg<8; beg++) nc->beg_set_number(beg,i); //set numbers
										usleep(50000);
										falsecount=0;
										readout_repeater(rc_bl, vline/2, times, nnums, 1); //dummy readout to clear the memory
										readout_repeater(rc_bl, vline/2, times, nnums, 1); //dummy readout to clear the memory
										for (int n=0; n<runs; n++){
											readout_repeater(rc_bl, vline/2, times, nnums, silent);
											for (int u=0; u<3; u++){
												file << nnums[u] << ":" << times[u] << "\t" << flush;
												if (nnums[u]!=i) falsecount++;
											}
										}
										file << "\n" << flush;
										file1 << falsecount << "\t" << flush;
									}
									lcl->switch_connect(r*2+1,vline,0);
									file1 << "\n" << flush;
								}
								
								//~ cout << "Using bottom right repeater block to receive events" << endl;
								//~ for (int s=0; s<1; s++){
									//~ vline=128+((r+s*32)&0x7f);
									//~ cout << "Using receiving repeater " << (vline-128)/2 << endl;
									//~ file << "Using receiving repeater " << (vline-128)/2 << endl << flush;
									//~ file1 << vol_curr << "\t" << vol_curr+ran_curr << "\t" << r << "\t" << (vline-128)/2 << "\t" << flush;
									//~ lcr->switch_connect(r*2+1,vline,1);
									//~ configure_hicann_bgen(1,100,0,0,0,0); //turn BEGs on
									//~ for (int i=0; i<64; i++){ //neuron numbers
										//~ for (int beg=0; beg<8; beg++) nc->beg_set_number(beg,0); //set numbers to zero to lock
										//~ usleep(50000); //time for the DLL to lock
										//~ for (int beg=0; beg<8; beg++) nc->beg_set_number(beg,i); //set numbers
										//~ usleep(50000);
										//~ falsecount=0;
										//~ readout_repeater(rc_br, (vline-128)/2, times, nnums, 1); //dummy readout to clear the memory
										//~ readout_repeater(rc_br, (vline-128)/2, times, nnums, 1); //dummy readout to clear the memory
										//~ for (int n=0; n<runs; n++){
											//~ readout_repeater(rc_br, (vline-128)/2, times, nnums, silent);
											//~ for (int u=0; u<3; u++){
												//~ file << nnums[u] << ":" << times[u] << "\t" << flush;
												//~ if (nnums[u]!=i) falsecount++;
											//~ }
										//~ }
										//~ file << "\n" << flush;
										//~ file1 << falsecount << "\t" << flush;
									//~ }
									//~ lcr->switch_connect(r*2+1,vline,0);
									//~ file1 << "\n" << flush;
								//~ }
							}
							vol_curr+=volh_step;
						}
						file.flush(); file.close();
						file1.flush(); file1.close();
						ran_curr+=ran_step;
					}
					cout << "Setting Vol to 0.7" << endl;
					board.setVoltage(SBData::vol, 0.7);
					cout << "Setting Voh to 0.9" << endl;
					board.setVoltage(SBData::voh, 0.9);
				} break;

				case 'V':{
					vector<uint> weights(32,0xffffffff), addressfail(32,0xffffffff), addresswork(32,0);
					uint gmax=1, top_ex=1, top_in=0, bot_ex=1, bot_in=0, neuron, runs;
					bool failureflag=false, silent;
					
					cout << "Silent (no neuron number output)?: " << endl;
					cin >> silent;
					cout << "Which neuron to use? (v1: 0-31, v2: 224-255)" << endl;
					cin >> neuron;
					cout << "How many runs?:" << endl;
					cin >> runs;
					
					activate_neuron(neuron,0,0); ///make sure that the neuron is connected to SPL1!!!
	
					for (int x=0; x<runs; x++){ //main counter
						for (int p=0; p<4; p++){
							ss.str("");
							ss <<"../results/tm_jitter/concatenation_test_freq"<<100+p*50<<".dat";
							filename = ss.str();
							if (p==0) set_pll(2,1);
							else if (p==1) set_pll(3,1);
							else if (p==2) set_pll(4,1);
							else if (p==3) set_pll(5,1);
							for (int rep=0; rep<26; rep+=4){ //do not use number 28: needed for input!
								cout << endl << "Using repeater " << rep << endl;
								rca[rc_l]->reset();
								rca[rc_l]->conf_spl1_repeater(rep, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
								lcl->reset(); //reset left crossbar
								lcr->reset(); //reset right crossbar
								#if HICANN_V==1
								lcl->switch_connect(rep*2,rep*2,1);
								#elif HICANN_V>=2
								lcl->switch_connect(rep*2+1,rep,1);
								#endif
								
								int run=-1;
								#if HICANN_V==1
								for(int u=(110-rep)*2; u > (139-p*30); u-=32){
								#elif HICANN_V>=2
								for(int u=(110-(rep/2))*2; u > (139-p*10); u-=32){
								#endif
									run++;
									sc_top->reset_drivers(); //reset top synapse block drivers
									sc_top->configure();
									rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
									lctl->reset(); //reset top left select switch
									#if HICANN_V==1
									lctl->switch_connect(110-rep-16*run,rep*2,1); //connect hor. line 110 to vert. line 0
									cout << endl << "Starting driver number: " << u/4 << endl;
									cout << endl << "Horizontal line: " << 110-rep-16*run << endl;
									#elif HICANN_V>=2
									lctl->switch_connect(110-(rep/2)-16*run,rep,1); //connect hor. line 110 to vert. line 0
									cout << endl << "Starting driver number: " << u/4 << endl;
									cout << endl << "Horizontal line: " << 110-(rep/2)-16*run << endl;
									#endif
									
									sc_top->drv_config(u, 1, 0, 1, 0, 1, 1, 0); //activate local input at the upper driver
									sc_top->preout_set(u, 1, 1, 1, 1); //set preout signals to false values
									sc_top->drv_set_gmax(u, 0, 0, gmax, gmax); //set gmax
									sc_top->write_weight(u, weights); //write weights to lower rows
									#if HICANN_V==1
									sc_top->write_weight(u+2, weights); //write weights to higher rows
									#elif HICANN_V>=2
									sc_top->write_weight(u+1, weights); //write weights to higher rows
									#endif
									sc_top->write_decoder(u, addressfail, addressfail); //write false decoder values
										
									int i=u-4;
									failureflag=false;
									configure_hicann_bgen(0,300,0,0,1,0); //turn off BEG
									usleep(100000);
									while (i>0 && !failureflag){
										if (there_are_spikes(silent)){
											if (there_are_spikes(silent)){ //double check
												cout << "There are spikes without input, test failed..."<< endl;
											}
										}
										cout << "Starting testing of level " << (u-i)/4 << endl;
										sc_top->drv_config(i, 1, 0, 1, 0, 1, 0, 1); //activate top input
										sc_top->preout_set(i, 0, 0, 0, 0); //set correct preout
										sc_top->drv_set_gmax(i, 0, 0, gmax, gmax); //set gmax
										sc_top->write_weight(i, weights); //write weights to lower rows
										#if HICANN_V==1
										sc_top->write_weight(i+2, weights); //write weights to upper rows
										#elif HICANN_V>=2
										sc_top->write_weight(i+1, weights); //write weights to upper rows
										#endif
										sc_top->write_decoder(i, addresswork, addresswork); //write correct decoder values
										configure_hicann_bgen(1,300,0,0,1,0);
										rca[rc_tr]->reset(); //contains dll resets...
										rca[rc_br]->reset();
										rca[rc_bl]->reset();
										rca[rc_tl]->reset();
										usleep(100000);
										if (!there_are_spikes(silent)){
											if (!there_are_spikes(silent)){ //double check (happens if the spikes come at the same time)
												cout << "Failure at concatenation level " << (u-i)/4 << endl;
												failureflag=true;
												file.open(filename.c_str(), fstream::app | fstream::out);
												file << rep << "\t" << u/4 << "\t" << (u-i)/4 << "\n" << flush;
												file.flush(); file.close();
											}
										}
										if (!failureflag)cout << "Level " << (u-i)/4 << " concatenation successful!" << endl;
										sc_top->preout_set(i, 1, 1, 1, 1); //set false preout
										sc_top->write_decoder(i, addressfail, addressfail); //write false decoder values
										configure_hicann_bgen(0,300,0,0,1,0);
										usleep(100000);
										i-=4;
									}
								}
							}
						}
					}
				} break;

				case 'g':{
					int fgn, line, value, first;
					std::vector<int> values;

					cout << "Choose controller: 0 - 3" << endl;
					cin >> fgn;
					cout << "Choose line: 0 - 23" << endl;
					cin >> line;
					cout << "Choose overall value: 0 - 1023" << endl;
					cin >> value;
					cout << "Global parameter value: " << endl;
					cin >> first;
					
					values.push_back(first);
					for (int b=0; b<129; b++) values.push_back(value);
					
					fc[fgn]->initArray(0, values);
					while(fc[fgn]->isBusy());
					fc[fgn]->program_cells(line, 0, 0);
					while(fc[fgn]->isBusy());
					fc[fgn]->program_cells(line, 0, 1);
					while(fc[fgn]->isBusy());
				} break;

				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};


class LteeTmJitter : public Listee<Testmode>{
public:         
	LteeTmJitter() : Listee<Testmode>(string("tm_jitter"),string("Testing of neurons and synapses (akononov diploma thesis)")){};
	Testmode * create(){return new TmJitter();};
};
LteeTmJitter ListeeTmJitter;
