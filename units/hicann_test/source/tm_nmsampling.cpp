// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_nmsampling.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann2
//                    			
// Create Date     :   Mon Dec 19 11
// Last Change     :   Mon Dec 19 11
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

class TmNmSampling : public Testmode{

public:
	// repeater control 
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	static const int TAG = 0;

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
	
    void activate_neuron_output(uint address, uint nnumber){
		nbc->enable_firing(address); 
		nbc->set_aout(address); 
		nbc->set_spl1address(address, nnumber);
		
		nbc->setOutputEn(true,true);
		if (address%2) nbc->setOutputMux(1,6);
		else nbc->setOutputMux(1,4);
    }
    
    void activate_neuron_nooutput(uint address, uint nnumber){
		nbc->enable_firing(address); 
		nbc->set_spl1address(address, nnumber);
    }
    
    void activate2neurons_nooutput(uint address1, uint nnum1, uint nnum2){
		nbc->enable_firing(address1); 
		nbc->set_spl1address(address1, nnum1);
		nbc->enable_firing(address1+1); 
		nbc->set_spl1address(address1+1, nnum2);
	}
    
    void activate2neurons_output(uint address1, uint nnum1, uint nnum2, bool even_output_on){
		nbc->enable_firing(address1); 
		nbc->set_aout(address1); 
		nbc->set_spl1address(address1, nnum1);
		nbc->enable_firing(address1+1); 
		nbc->set_aout(address1+1); 
		nbc->set_spl1address(address1+1, nnum2);
		
		nbc->setOutputEn(true,true);
		if (even_output_on) nbc->setOutputMux(1,4);
		else nbc->setOutputMux(1,6);
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
			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(1);
			fc[fg_num]->set_voltagewritetime(4);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(15);
			fc[fg_num]->set_pulselength(15);
			fc[fg_num]->set_fg_biasn(15);
			fc[fg_num]->set_fg_bias(15);
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
		nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
				DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
		bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
		bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		vector<nc_merger> merg(mer, mer+23);
		vector<bool> slow(sl, sl+23);
		vector<bool> select(se, se+23);
		vector<bool> enable(en, en+23);
		if (receive) select[7] = 0; //channel 0 receives SPL1 data
		if (merge) enable[7] = 1; //channel 0 merges background and SPL1 data

		nc->nc_reset(); //reset neuron control
		nc->set_seed(seed); //set seed for BEGs
		nc->beg_configure(8, poisson, delay);
		nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
		nc->outputphase_set(1,1,1,1,1,1,1,1); //not sure if it makes any difference :)
		if (on) nc->beg_on(8); //turn all BEGs on
		else nc->beg_off(8); //turn all BEGs off
	}

	void prepare_repeaters(){
		configure_hicann_bgen(1, 70, 0, 0, 0, 0);
					
		rca[rc_l]->reset();
		rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(8, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(16, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(20, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
		rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
	}
					
	void setProperMux(uint fgn, SBData::ADtype& var){
		nbc->setOutputEn(true,true);
		if (fgn==0 || fgn==1)var=SBData::aout0;
		else if (fgn==2 || fgn==3) var=SBData::aout1;
		else log(Logger::ERROR)<<"ERROR: setProperMux: Illegal controler specified!" << endl;
		nbc->setOutputMux(3+(fgn%2)*6,3+(fgn%2)*6);
	}

	bool test() {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::ERROR)<<"ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		debug_level = 4;
		
		log(Logger::INFO) << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort";
			return 0;
		}
	 	log(Logger::INFO) << "Init() ok";
	
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		nbc        = &hc->getNBC();
		nc         = &hc->getNC();
		fc[0]      = &hc->getFC_tl();
		fc[1]      = &hc->getFC_tr();
		fc[2]      = &hc->getFC_bl();
		fc[3]      = &hc->getFC_br();
		sc_top     = &hc->getSC_top();  // get pointer to synapse control class of HICANN 0
		sc_bot     = &hc->getSC_bot();  // get pointer to synapse control class of HICANN 0
		lcl        = &hc->getLC_cl();  // get pointer to left crossbar layer 1 switch control class of HICANN 0
		lcr        = &hc->getLC_cr();
		lctl       = &hc->getLC_tl();
		lcbl       = &hc->getLC_bl();
		lctr       = &hc->getLC_tr();
		lcbr       = &hc->getLC_br();
		rca[rc_l]  = &hc->getRC_cl(); // get pointer to left repeater of HICANN 0
		rca[rc_r]  = &hc->getRC_cr();
		rca[rc_tl] = &hc->getRC_tl();
		rca[rc_bl] = &hc->getRC_bl();
		rca[rc_tr] = &hc->getRC_tr();
		rca[rc_br] = &hc->getRC_br();
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
			cout << "0: Reset everything and verify it" << endl;
			cout << "1: Reset everything except the synapses" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "a: Run test" << endl;
			cout << "b: Run test with one neuron and two BG sources" << endl;
			cout << "x: Exit" << endl;
			cin >> c;
			
			switch(c){

				case '0':{
					cout << "resetting everything" << endl;
					cout << "resetting all synapse drivers and synapses" << endl;
					sc_top->reset_all();
					sc_top->print_config();
					sc_bot->reset_all();
					sc_bot->print_config();
					cout << "resetting crossbars and switches" << endl;
					lcl->reset();
					lcl->print_config();
					lcr->reset();
					lcr->print_config();
					lctl->reset();
					lctl->print_config();
					lctr->reset();
					lctr->print_config();
					lcbl->reset();
					lcbl->print_config();
					lcbr->reset();
					lcbr->print_config();
					cout << "resetting repeater blocks" << endl;
					rca[rc_l]->reset();
					rca[rc_l]->print_config();
					rca[rc_tl]->reset();
					rca[rc_tl]->print_config();
					rca[rc_tr]->reset();
					rca[rc_tr]->print_config();
					rca[rc_bl]->reset();
					rca[rc_bl]->print_config();
					rca[rc_br]->reset();
					rca[rc_br]->print_config();
					rca[rc_r]->reset();
					rca[rc_r]->print_config();
					cout << "resetting neuron builder" << endl;
					nbc->initzeros();
					cout << "resetting neuron control" << endl;
					nc->nc_reset();
					nc->print_config();
				} break;
				
				case '1':{
					cout << "resetting everything" << endl;
					cout << "resetting synapse drivers" << endl;
					sc_top->reset_drivers();
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

				case 'p':{
					setupFg();
					log(Logger::INFO)<<"tm_jitter::setting floating gate arrays to zero ...";
					for (int fg=0; fg<4; fg++) fc[fg]->initzeros(0);
					for(int i=0; i<24; i++){
						for(int cntlr=0; cntlr<4; cntlr++){
							while(fc[cntlr]->isBusy());
							fc[cntlr]->program_cells(i,0,0);
						}
					}
					
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

				case 'a':{
					uint num1, num2, num3, numout, nflag=0, adjacent=0, igmaxinh, igmaxex, poisson, rate;
					uint bggmaxex, bggmaxinh, bgsyn1, bgsyn2, bgsyn3, bgsyn4, bgsyn5, bgsyn6;
					uint ex12, ex13, ex21, ex23, ex31, ex32, in12, in13, in21, in23, in31, in32;
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top->configure();
					set_pll(2,1); // set PLL to 100 MHz
					
					cout << "Enter HW number of the first neuron: ";
					cin >> num1;
					cout << "Enter HW number of the second neuron: ";
					cin >> num2;
					cout << "Enter HW number of the third neuron: ";
					cin >> num3;
					cout << "Enter HW number of the output neuron: ";
					cin >> numout;
					
					cout << "Enter excitatory background gmax divisor (1 to 15): ";
					cin >> bggmaxex;
					cout << "Enter inhibitory background gmax divisor (1 to 15): ";
					cin >> bggmaxinh;
					
					cout << "Enter excitatory interconnection gmax divisor (1 to 15): ";
					cin >> igmaxex;
					cout << "Enter inhibitory interconnection gmax divisor (1 to 15): ";
					cin >> igmaxinh;
					
					cout << "Enter excitatory BG synapse weight for neuron 1 (0-15): ";
					cin >> dec >> bgsyn1;
					cout << "Enter excitatory BG synapse weight for neuron 2 (0-15): ";
					cin >> bgsyn3;
					cout << "Enter excitatory BG synapse weight for neuron 3 (0-15): ";
					cin >> bgsyn5;
					cout << "Enter inhibitory BG synapse weight for neuron 1 (0-15): ";
					cin >> bgsyn2;
					cout << "Enter inhibitory BG synapse weight for neuron 2 (0-15): ";
					cin >> bgsyn4;
					cout << "Enter inhibitory BG synapse weight for neuron 3 (0-15): ";
					cin >> bgsyn6;
					
					cout << "Enter excitatory weight matrix in a manner: " << endl;
					cout << "O                            "<< endl;
					cout << "U   n1: |  0    ex12   ex13 |"<< endl;
					cout << "T   n2: | ex21    0    ex23 |"<< endl;
					cout << "P   n3: | ex31  ex32     0  |"<< endl;
					cout << "U         n1    n2      n3   "<< endl;
					cout << "T             INPUT          "<< endl;
					cout << "Order of input is: ex12, ex13, ex21, ex23, ex31, ex32: "<< endl;
					cin >> ex12;
					cin >> ex13;
					cin >> ex21;
					cin >> ex23;
					cin >> ex31;
					cin >> ex32;
					
					cout << "Enter inhibitory weight matrix in the same manner: " << endl;
					cin >> in12;
					cin >> in13;
					cin >> in21;
					cin >> in23;
					cin >> in31;
					cin >> in32;
					
					cout << "Enter 1 for poisson, 0 for regular spiking: ";
					cin >> poisson;
					cout << "Enter input spike delay (in clock cycles): ";
					cin >> rate;
					
					//set flag for the output neuron
					if (num1==numout) nflag=1;
					else if (num2==numout) nflag=2;
					else if (num3==numout) nflag=3;
					
					//if 2 neurons are adjacent, mark the adjacency spot
					if ((num2-num1 == 1) && !(num1%2)) adjacent = 1;
					else if ((num3-num2 == 1) && !(num2%2)) adjacent = 2;
					
					///activate correct neurons
					cout << "Activating the neurons..." << endl;
					nbc->clear_sram();
					nbc->write_sram();
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					
					if (adjacent == 1){
						if (nflag == 1){
							activate2neurons_output(num1,7,8,1);
							activate_neuron_nooutput(num3,9);
						}
						else if (nflag == 2){
							activate2neurons_output(num1,7,8,0);
							activate_neuron_nooutput(num3,9);
						}
						else if (nflag == 3){
							activate2neurons_nooutput(num1,7,8);
							activate_neuron_output(num3,9);
						}
						else{
							activate2neurons_nooutput(num1,7,8);
							activate_neuron_nooutput(num3,9);
						}
					}
					else if (adjacent ==2){
						if (nflag == 1){
							activate_neuron_output(num1,7);
							activate2neurons_nooutput(num2,8,9);
						}
						else if (nflag == 2){
							activate_neuron_nooutput(num1,7);
							activate2neurons_output(num2,8,9,1);
						}
						else if (nflag == 3){
							activate_neuron_nooutput(num1,7);
							activate2neurons_output(num2,8,9,0);
						}
						else{
							activate_neuron_nooutput(num1,7);
							activate2neurons_nooutput(num2,8,9);
						}
					}
					else{
						if (nflag == 1){
							activate_neuron_output(num1,7);
							activate_neuron_nooutput(num2,8);
							activate_neuron_nooutput(num3,9);
						}
						else if (nflag == 2){
							activate_neuron_nooutput(num1,7);
							activate_neuron_output(num2,8);
							activate_neuron_nooutput(num3,9);
						}
						else if (nflag == 3){
							activate_neuron_nooutput(num1,7);
							activate_neuron_nooutput(num2,8);
							activate_neuron_output(num3,9);
						}
						else{
							activate_neuron_nooutput(num1,7);
							activate_neuron_nooutput(num2,8);
							activate_neuron_nooutput(num3,9);
						}
					}

					nbc->write_sram();
					nbc->setNeuronReset(1,1);
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					/// neuron control
					cout << "Setting up neuron control..." << endl;
					configure_hicann_bgen(1, 2000, 0xaa, 0, 0, 1); //set merger tree to forward SPL1_7 to the DNC
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					
					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					
					lcl->switch_connect(1,0,1);
					lctl->switch_connect(14,0,1);

					///configure repeaters
					cout << "Setting up repeaters..." << endl;
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

					nc->dnc_enable_set(1,0,0,0,0,0,0,0);  //channel 0 for DNC input, channels 1-7 for BEG/spl1 forwarding
					
					///configure synapse drivers
					cout << "Configuring synapse drivers..." << endl;
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					
					//left driver 0
					sc_top->drv_config(0, 0, 1, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, bggmaxex, bggmaxinh);
					//left driver 14
					sc_top->drv_config(28, 0, 1, 1, 0, 1, 1, 0);
					sc_top->preout_set(28, 0, 0, 0, 0);
					sc_top->drv_set_gmax(28, 0, 0, igmaxex, igmaxinh);
					//left driver 12
					sc_top->drv_config(24, 0, 1, 1, 0, 1, 0, 1);
					sc_top->preout_set(24, 0, 0, 0, 0);
					sc_top->drv_set_gmax(24, 0, 0, igmaxex, igmaxinh);
					
					cout << "Resetting synapses..." << endl;
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, weights, weights); //to avoid false output
					sc_top->write_weight(28, weights);
					sc_top->write_weight(29, weights);
					sc_top->write_decoder(28, weights, weights); //to avoid false output
					sc_top->write_weight(24, weights);
					sc_top->write_weight(25, weights);
					sc_top->write_decoder(24, weights, weights); //to avoid false output
					
					cout << "Programming background synapses..." << endl;
					sc_top->write_decoder(0, num1, 1); //excitatory BG
					sc_top->write_decoder(0, num2, 3);
					sc_top->write_decoder(0, num3, 5);
					sc_top->write_weight(0, num1, bgsyn1);
					sc_top->write_weight(0, num2, bgsyn3);
					sc_top->write_weight(0, num3, bgsyn5);
					
					sc_top->write_decoder(1, num1, 2); //inhibitory BG
					sc_top->write_decoder(1, num2, 4);
					sc_top->write_decoder(1, num3, 6);
					sc_top->write_weight(1, num1, bgsyn2);
					sc_top->write_weight(1, num2, bgsyn4);
					sc_top->write_weight(1, num3, bgsyn6);
					
					cout << "Programming loopback synapses..." << endl;
					if (ex31) sc_top->write_decoder(24, num1, 9); //excitatory loopback
					if (ex32) sc_top->write_decoder(24, num2, 9);
					if (ex23) sc_top->write_decoder(24, num3, 8);
					if (ex21) sc_top->write_decoder(28, num1, 8);
					if (ex12) sc_top->write_decoder(28, num2, 7);
					if (ex13) sc_top->write_decoder(28, num3, 7);
					sc_top->write_weight(24, num1, ex31);
					sc_top->write_weight(24, num2, ex32);
					sc_top->write_weight(24, num3, ex23);
					sc_top->write_weight(28, num1, ex21);
					sc_top->write_weight(28, num2, ex12);
					sc_top->write_weight(28, num3, ex13);
					
					if (in31) sc_top->write_decoder(25, num1, 9); //inhibitory loopback
					if (in32) sc_top->write_decoder(25, num2, 9);
					if (in23) sc_top->write_decoder(25, num3, 8);
					if (in21) sc_top->write_decoder(29, num1, 8);
					if (in12) sc_top->write_decoder(29, num2, 7);
					if (in13) sc_top->write_decoder(29, num3, 7);
					sc_top->write_weight(25, num1, in31);
					sc_top->write_weight(25, num2, in32);
					sc_top->write_weight(25, num3, in23);
					sc_top->write_weight(29, num1, in21);
					sc_top->write_weight(29, num2, in12);
					sc_top->write_weight(29, num3, in13);

					/// SPL1 Control
					cout << "Programming DNC-Communication..." << endl;
					//only channel 0 enabled in the DNC direction
					spc->write_cfg(0x07fff); //configure timgetFpgaRxDataestamp, direction, enable of l1
					dc->setTimeStampCtrl(0x0); //Disable/Enable DNC timestamp features
					//set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0x7f);   //direction of the l1 busses: only 0 towards DNC
					
					/// FPGA BEG enable
					cout << "Running FPGA Background generator..." << endl;
					uint chan=0, synaddr1=0, synaddr2=6;
					jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 80, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr())); //to lock the DLL(s)
					usleep(1000);
					jtag->FPGA_set_cont_pulse_ctrl(1, 1<<(chan&0x7), poisson, rate, 0x1aaaa, synaddr1, synaddr2-synaddr1, comm->jtag2dnc(hc->addr())); //actual activity
					
					/// enable the FIFO and read back the events
					cout << "Running trace FIFO..." << endl;
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
					usleep(1000);
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

						cout << "Event 1: Neuron number " << dec << nnumber1 << " at time " << timestamp1 << " on channel " << channel1 << ". ";
						if(flag) cout << "Event 2: Neuron number " << dec << nnumber2 << " at time " << timestamp2 << " on channel " << channel2 << ". ";
						cout << endl;
					}

					jtag->FPGA_reset_tracefifo(); //throw away the rest of the events
					spc->write_cfg(0x00000);
					cout << "DONE!" << endl;
				} break;

				case 'b':{
					uint num, weiex, weiinh, poisson, rate, gmaxex, gmaxinh;
					bool odd=false, low=false;
					cout << "Hardware neuron to activate: ";
					cin >> num;
					cout << "Enter excitatory synaptic weight (0-15): ";
					cin >> weiex;
					cout << "Enter inhibitory synaptic weight (0-15): ";
					cin >> weiinh;
					cout << "Enter 1 for poisson, 0 for regular spiking: ";
					cin >> poisson;
					cout << "Enter input spike delay (in clock cycles): ";
					cin >> rate;
					cout << "Enter excitatory gmax (11 is max, ff is min): ";
					cin >> hex >> gmaxex;
					cout << "Enter inhibitory gmax (11 is max, ff is min): ";
					cin >> hex >> gmaxinh;
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top->configure();
					set_pll(2,1); // set PLL to 100 MHz
					
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

					activate_neuron_output(num,7);
					//~ activate_neuron_output(2,8);
					//~ activate_neuron_output(6,9);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					/// neuron control
					configure_hicann_bgen(0, 200, 200, 0, 1, 0); //set merger tree to forward SPL1_7 to the DNC
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					
					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);

					///configure repeaters
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					
					nc->dnc_enable_set(1,0,0,0,0,0,0,0);  //channel 0 for DNC input, channels 1-7 for BEG/spl1 forwarding
					
					///configure synapse drivers
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					
					sc_top->drv_config(0, 0, 1, 1, 0, 1, 1, 0); //temporary
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, gmaxex, gmaxinh);
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, weights, weights); //to avoid false output
					sc_top->write_decoder(0, num, 1);
					sc_top->write_weight(0, num, weiex);
					//~ sc_top->write_decoder(0, 2, 3);
					//~ sc_top->write_decoder(0, 6, 5);
					sc_top->write_decoder(1, num, 2);
					sc_top->write_weight(1, num, weiinh);
					//~ sc_top->write_decoder(1, 2, 4);
					//~ sc_top->write_decoder(1, 6, 6);
					
					/// SPL1 Control
					//only channel 0 enabled in the DNC direction
					spc->write_cfg(0x07fff); //configure timgetFpgaRxDataestamp, direction, enable of l1
						
					/// DNC: this stuff doesnt do anything (???)
					dc->setTimeStampCtrl(0x0); //Disable/Enable DNC timestamp features
					//set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0x7f);   //direction of the l1 busses: only 0 towards DNC
					
					/// FPGA BEG enable
					uint chan=0, delay=rate, synaddr1=0, synaddr2=2;
					jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 80, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr())); //to lock the DLL(s)
					usleep(1000);
					jtag->FPGA_set_cont_pulse_ctrl(1, 1<<(chan&0x7), poisson, delay, 0x1aaaa, synaddr1, synaddr2-synaddr1, comm->jtag2dnc(hc->addr())); //actual activity
					
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
					usleep(1000);
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

						cout << "Event 1: Neuron number " << dec << nnumber1 << " at time " << timestamp1 << " on channel " << channel1 << ". ";
						if(flag) cout << "Event 2: Neuron number " << dec << nnumber2 << " at time " << timestamp2 << " on channel " << channel2 << ". ";
						cout << endl;
					}

					jtag->FPGA_reset_tracefifo(); //throw away the rest of the events
					spc->write_cfg(0x00000);
				} break;

				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};


class LteeTmNmSampling : public Listee<Testmode>{
public:         
	LteeTmNmSampling() : Listee<Testmode>(string("tm_nmsampling"),string("Mihais LIF-Sampling")){};
	Testmode * create(){return new TmNmSampling();};
};
LteeTmNmSampling LteeTmNmSampling;
