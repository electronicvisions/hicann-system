// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_quicktests.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann2            
//                    			
// Create Date     :   Tue Jun 6 11
// Last Change     :   Tue Jun 6 11    
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

class TmQuickTests : public Testmode{

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
	
	//addresses 0-255: top half, 256-511: bottom half (only one neuron of a 4-group can be activated)
    void activate_neuron(uint address, uint nnumber, bool currentin){
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
		for (int i=(address/2)*4; i<(address/2)*4+4; i++) nbc->write_data(i, value);
    }
    
    void activate2neurons(uint address1, uint nnum1, uint nnum2, bool evencurrentin){
		uint ntop=0, nbot=0, value;
		
		if (evencurrentin) ntop=0x94;
		else ntop=0x90; //no neurons connected to aout, left/right disconnected

		value = nbdata(0,1,0,ntop,0,nnum1); //top neurons connected to spl1
		//write the first configuration to registers 0-2 (of a 4-neuron-block)
		for (int i=(address1/2)*4; i<(address1/2)*4+3; i++) nbc->write_data(i, value);
		
		value = nbdata(0,1,0,ntop,0,nnum2); //top neurons connected to spl1
		//write the second configuration to register 3
		nbc->write_data((address1/2)*4+3, value);
    }
	
	//sets the repeater frequency (use 2,1 to get 100 MHz)
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
	}

	//generate an SPL1 event (27 bits)
	uint64_t gen_spl1(uint hicann, uint channel, uint number, uint time){
		return ((hicann & 0x7) << 24) | ((channel & 0x7) << 21) | ((number & 0x3f) << 15) | (time & 0x7fff);
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
		if (receive) select[0] = 0; //channel 0 receives SPL1 data
		if (merge) enable[0] = 1; //channel 0 merges background and SPL1 data

		nc->nc_reset(); //reset neuron control
		nc->set_seed(seed); //set seed for BEGs
		nc->beg_configure(8, poisson, delay);
		nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
		nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 output: rising edge sampling
		//~ nc->outputphase_set(1,1,1,1,1,1,1,1); //important for SPL1 output: falling edge sampling
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
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		uint hicannr = 0;
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
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

		fpc->reset();
		jtag->reset_jtag();

		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
			log(Logger::INFO) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
			log(Logger::INFO) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		log(Logger::INFO) << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
			log(Logger::INFO) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
		log(Logger::INFO) << "Init() ok" << endl;

		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;

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
			cout << "2: Turn off HICANN BEGs" << endl;
			cout << "m: Configure neuronbuilder MUXes manually" << endl;
			cout << "f: Set PLL frequency to a custom value" << endl;
			cout << "s: Continously stimulate neuron" << endl;
			cout << "c: Configure a neuron manually" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "l: Loopback FPGA-HICANN-FPGA" << endl;
			cout << "d: Verify FPGA->DNC->HICANN BEG input using scope" << endl;
			cout << "i: Verify inner-HICANN synaptic input using scope" << endl;
			cout << "a: Construct a simple 2 neuron network" << endl;
			cout << "t: Read out repeater test input" << endl;
			cout << "b: Adjust the BEG" << endl;
			cout << "w: Set weight for a synapse" << endl;
			cout << "g: Write an FG line" << endl;
			cout << "r: Read out an FG line using ADC" << endl;
			cout << "e: Test HICANN-BEG ability to transmit different neuron numbers" << endl;
			cout << "k: Use repeater's test input to receive L1 serial data" << endl;
			cout << "y: Test repeater's loopback ability" << endl;
			cout << "j: Test non-sending repeater's test output" << endl;
			cout << "n: Test L2 power consumption by sending on all repeaters" << endl;
			cout << "h: Make spl1 neuronal output visible on the FPGA FIFO" << endl;
			cout << "q: Use 64 neurons with all 64 neuron addresses on 2 SPL1 channels" << endl;
			cout << "o: Read out and print all registers on HICANN" << endl;
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

				case '2':{
					nc->beg_off(8);
				} break;

				case 'm':{
					uint one, two;
					cout << "Enter value for the first MUX: " << endl;
					cout << "Choices are:\n 0 - Vctrl0 top\n 1 - preout top\n 2 - fire line of Neuron 0\n" <<
					    " 3 - FG_out 0\n 4 - even denmems top\n 5 - even denmems bottom\n 6 - odd denmems top\n" << 
					    " 7 - odd denmems bottom\n 9 - FG_out 1" << endl;
					cin >> one;
					cout << "Enter value for the second MUX: " << endl;
					cout << "Choices are:\n 0 - Vctrl0 bottom\n 1 - preout bottom\n" <<
					    " 3 - FG_out 2\n 4 - even denmems top\n 5 - even denmems bottom\n 6 - odd denmems top\n" << 
					    " 7 - odd denmems bottom\n 9 - FG_out 3" << endl;
					cin >> two;
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(one,two);
				} break;

				case 'f':{
					uint mult, div;
					cout << "Base clock is 50 MHz" << endl;
					cout << "Enter multiplicator: ";
					cin >> mult;
					cout << "Enter divisor: ";
					cin >> div;
					set_pll(mult,div);
				} break;

				case 's':{					
					uint num, sval;
					bool odd=false, low=false;
					cout << "Hardware neuron to activate: ";
					cin >> num;
					cout <<"Enter value for constant stimulus:";
					cin >> sval;
					
					if (num%2) odd=true;
					if (num/256) low=true;
					nbc->setOutputEn(true,true);
					if (odd && low) nbc->setOutputMux(1,7);
					else if (odd && !low) nbc->setOutputMux(1,6);
					else if (!odd && low) nbc->setOutputMux(1,5);
					else nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					//configure global parameters for neuronbuilder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(num,0,1);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();

					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initval(0, sval);
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
					log(Logger::INFO)<<"tm_quicktests::setting floating gate arrays to zero ...";
					for (int fg=0; fg<4; fg++) fc[fg]->initzeros(0);
					for(int i=0; i<24; i++){
						for(int cntlr=0; cntlr<4; cntlr++){
							while(fc[cntlr]->isBusy());
							fc[cntlr]->program_cells(i,0,0);
						}
					}
					
					int bank=0; // bank number, programming data is written to
					//write currents first as ops need biases before they work.
					log(Logger::INFO)<<"tm_quicktests::writing currents ...";
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
					log(Logger::INFO)<<"tm_quicktests::writing voltages ...";
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

					comm->Flush(); //important!

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
						jtag->FPGA_reset_tracefifo();

						uint64_t event;
						for (int i = 1; i < 50; i++){
							event=gen_spl1(0, 0, neuron, 150*2*i);
							jtag->FPGA_fill_playback_fifo(false, 200, 0, event, 0);
						}

						log(Logger::INFO) << "Starting systime counters";
						unsigned int curr_ip = jtag->get_ip();

						S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
						if (comm_ptr != NULL) {
							comm_ptr->trigger_systime(curr_ip,false);
							jtag->HICANN_stop_time_counter();
							jtag->HICANN_start_time_counter();
							comm_ptr->trigger_systime(curr_ip,true);
						}
						else log(Logger::WARNING) << "Bad s2comm pointer, did not initialize systime counter";

						jtag->FPGA_enable_tracefifo();
						jtag->FPGA_enable_pulsefifo();
						usleep(1000);
						jtag->FPGA_disable_pulsefifo();
						jtag->FPGA_disable_tracefifo();
						comm_ptr->trigger_systime(curr_ip,false);

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
					spc->write_cfg(0x0ff00);
					comm->Flush();

					if ((uint)((jtag_recpulse>>15)&0x3f) != (neuron&0x3f)) cout << "TESTFAIL: Packet(s) not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
					else cout << "TESTPASS: Transmission of pulse packet(s) via JTAG->HICANN(L1)->JTAG successful." << endl;
				} break;

				case 'd':{
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 8, 8);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, address, address);
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(0,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					nc->nc_reset(); //reset neuron control
					nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs
					spc->write_cfg(0x0ffff);
					// set DNC to ignore time stamps and directly forward l2 pulses
					dc->setTimeStampCtrl(0x0);
					// set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0xff);

					jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 80, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr()));
				} break;

				case 'i':{
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 8, 8);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, address, address);
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(0,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					configure_hicann_bgen(1, 50, 200, 0, 0, 0);
				} break;

				case 'a':{
					vector<uint> weights(32,0xffffffff), address(32,0), address2(32,0xcccccccc); //one row, all weights max, decoder 0
					uint pre, post;
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					set_pll(2, 1); //set pll to 100 MHz
					
					cout << "Choose the pre-synaptic neuron number (usually 253)" << endl;
					cin >> pre;
					cout << "Choose the post-synaptic neuron number (usually 0)" << endl;
					cin >> post;
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 8, 8);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, address2, address2);
					
					sc_top->drv_config(196, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(196, 0, 0, 0, 0);
					sc_top->drv_set_gmax(196, 0, 0, 8, 8);
					sc_top->write_weight(196, weights);
					sc_top->write_weight(197, weights);
					sc_top->write_decoder(196, address, address);
					
					sc_top->write_decoder(196, post, 33); //prevent second neuron from getting wrong input
					sc_top->write_decoder(197, post, 33);
					sc_top->write_decoder(0, pre, 33); //prevent second neuron from getting wrong input
					sc_top->write_decoder(1, pre, 33);
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					rca[rc_l]->reset();

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					
					lcl->switch_connect(49,24,1);
					lctl->switch_connect(98,24,1);
					rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57

					nbc->setOutputEn(true,true);
					nbc->setOutputMux(6,4); //put both neurons on both channels
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);
					
					activate_neuron(pre,12,0); //1st neuron transmits number 1
					activate_neuron(post,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					configure_hicann_bgen(1, 300, 200, 0, 1, 0);
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

				case 'b':{
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

				case 'r':{
					int fgn, line;
					double temp;
					SBData::ADtype type;

					cout << "what controller? 0 - 3" << endl;
					cin >> fgn;
					cout << "what line? 0 - 23" << endl;
					cin >> line;
					
					board.setBothMux();
					setProperMux(fgn, type);
					
					cout << endl << "read back from FGs:" << endl;
					double mean=0;
					for(int col=0; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						mean+=temp;
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl << "Mean of the line is " << mean/129 << endl;
				} break;

				case 'e':{
					uint n0, n1, n2, n3, n4, n5, n6, n7;
					cout << "Choose number for BEG0: " << endl;
					cin >> n0;
					cout << "Choose number for BEG1: " << endl;
					cin >> n1;
					cout << "Choose number for BEG2: " << endl;
					cin >> n2;
					cout << "Choose number for BEG3: " << endl;
					cin >> n3;
					cout << "Choose number for BEG4: " << endl;
					cin >> n4;
					cout << "Choose number for BEG5: " << endl;
					cin >> n5;
					cout << "Choose number for BEG6: " << endl;
					cin >> n6;
					cout << "Choose number for BEG7: " << endl;
					cin >> n7;

					prepare_repeaters();
					lcl->reset();
					lcr->reset();
					lcl->switch_connect(1,0,1);
					lcl->switch_connect(9,4,1);
					lcl->switch_connect(17,8,1);
					lcl->switch_connect(25,12,1);
					lcl->switch_connect(33,16,1);
					lcl->switch_connect(41,20,1);
					lcl->switch_connect(49,24,1);
					lcl->switch_connect(57,28,1);

					nc->beg_set_number(0, n0);
					nc->beg_set_number(1, n1);
					nc->beg_set_number(2, n2);
					nc->beg_set_number(3, n3);
					nc->beg_set_number(4, n4);
					nc->beg_set_number(5, n5);
					nc->beg_set_number(6, n6);
					nc->beg_set_number(7, n7);
					
					rca[rc_bl]->reset();
					cout << "Bottom left repeater block:" << endl;
					for (int i=14; i>=0; i-=2){
						cout << "Received from BEG " << 7-(i/2) << ":" << endl;
						readout_repeater(rc_bl, i);
						cout << endl;
					}
					
					lcl->switch_connect(56,28,1);
					lcl->switch_connect(48,24,1);
					lcl->switch_connect(40,20,1);
					lcl->switch_connect(32,16,1);
					lcl->switch_connect(24,12,1);
					lcl->switch_connect(16,8,1);
					lcl->switch_connect(8,4,1);
					lcl->switch_connect(0,0,1);
					
					rca[rc_r]->reset();
					cout << "Center right repeater block:" << endl;
					for (int i=28; i>=0; i-=4){
						cout << "Received from BEG " << 7-(i/4) << ":" << endl;
						readout_repeater(rc_r, i);
						cout << endl;
					}
					lcl->reset();
				} break;

				case 'k':{
					uint sw;
					cout << "Choose repeater block for test input: " << endl;
					cout << "1: Center left (loopback)" << endl;
					cout << "2: Top left" << endl;
					cout << "3: Bottom left" << endl;
					cout << "4: Top right" << endl;
					cout << "5: Bottom right" << endl;
					cout << "6: Center right" << endl;
					cin >> sw;

					prepare_repeaters();
					
					if (sw==1)	for (int i=0; i<32; i+=4) readout_repeater(rc_l, i);
					
					else if (sw==2){
						lcl->reset();
						lcr->reset();
						lcl->switch_connect(1, 0,1);
						lcl->switch_connect(9, 4,1);
						lcl->switch_connect(17,8,1);
						lcl->switch_connect(25,12,1);
						lcl->switch_connect(33,16,1);
						lcl->switch_connect(41,20,1);
						lcl->switch_connect(49,24,1);
						lcl->switch_connect(57,28,1);
						lctl->reset();
						lctl->switch_connect(110,0,1);
						lctl->switch_connect(110,1,1);
						lctl->switch_connect(108,4,1);
						lctl->switch_connect(108,5,1);
						lctl->switch_connect(106,8,1);
						lctl->switch_connect(106,9,1);
						lctl->switch_connect(104,12,1);
						lctl->switch_connect(104,13,1);
						lctl->switch_connect(102,16,1);
						lctl->switch_connect(102,17,1);
						lctl->switch_connect(100,20,1);
						lctl->switch_connect(100,21,1);
						lctl->switch_connect(98, 24,1);
						lctl->switch_connect(98, 25,1);
						lctl->switch_connect(96, 28,1);
						lctl->switch_connect(96, 29,1);
						rca[rc_tl]->reset();
						for (int i=0; i<16; i+=2) readout_repeater(rc_tl, i);
					}
					
					else if (sw==3){
						lcl->reset();
						lcr->reset();
						lctl->reset();
						lcl->switch_connect(1, 0,1);
						lcl->switch_connect(9, 4,1);
						lcl->switch_connect(17,8,1);
						lcl->switch_connect(25,12,1);
						lcl->switch_connect(33,16,1);
						lcl->switch_connect(41,20,1);
						lcl->switch_connect(49,24,1);
						lcl->switch_connect(57,28,1);
						rca[rc_bl]->reset();
						for (int i=0; i<16; i+=2) readout_repeater(rc_bl, i);
					}
					
					else if (sw==4){
						lcl->reset();
						lcr->reset();
						lcr->switch_connect(1,128,1);
						lcr->switch_connect(9,132,1);
						lcr->switch_connect(17,136,1);
						lcr->switch_connect(25,140,1);
						lcr->switch_connect(33,144,1);
						lcr->switch_connect(41,148,1);
						lcr->switch_connect(49,152,1);
						lcr->switch_connect(57,156,1);
						lctr->reset();
						lctr->switch_connect(110,128,1);
						lctr->switch_connect(110,129,1);
						lctr->switch_connect(108,132,1);
						lctr->switch_connect(108,133,1);
						lctr->switch_connect(106,136,1);
						lctr->switch_connect(106,137,1);
						lctr->switch_connect(104,140,1);
						lctr->switch_connect(104,141,1);
						lctr->switch_connect(102,144,1);
						lctr->switch_connect(102,145,1);
						lctr->switch_connect(100,148,1);
						lctr->switch_connect(100,149,1);
						lctr->switch_connect(98,152,1);
						lctr->switch_connect(98,153,1);
						lctr->switch_connect(96,156,1);
						lctr->switch_connect(96,157,1);
						rca[rc_tr]->reset();
						for (int i=0; i<16; i+=2) readout_repeater(rc_tr, i);
					}
					
					else if (sw==5){
						lcl->reset();
						lcr->reset();
						lcr->switch_connect(1,128,1);
						lcr->switch_connect(9,132,1);
						lcr->switch_connect(17,136,1);
						lcr->switch_connect(25,140,1);
						lcr->switch_connect(33,144,1);
						lcr->switch_connect(41,148,1);
						lcr->switch_connect(49,152,1);
						lcr->switch_connect(57,156,1);
						lctr->reset();
						rca[rc_br]->reset();
						for (int i=0; i<16; i+=2) readout_repeater(rc_br, i);
					}
					
					else if (sw==6){
						lcl->reset();
						lcr->reset();
						lcl->switch_connect(1,0,1);
						lcl->switch_connect(0,0,1);
						lcl->switch_connect(9,4,1);
						lcl->switch_connect(8,4,1);
						lcl->switch_connect(17,8,1);
						lcl->switch_connect(16,8,1);
						lcl->switch_connect(25,12,1);
						lcl->switch_connect(24,12,1);
						lcl->switch_connect(33,16,1);
						lcl->switch_connect(32,16,1);
						lcl->switch_connect(41,20,1);
						lcl->switch_connect(40,20,1);
						lcl->switch_connect(49,24,1);
						lcl->switch_connect(48,24,1);
						lcl->switch_connect(57,28,1);
						lcl->switch_connect(56,28,1);
						rca[rc_r]->reset();
						for (int i=0; i<32; i+=4) readout_repeater(rc_r, i);
					}
				} break;

				case 'y':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint repnr=0, time=100, nnum=0;
					char sw;
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
					cin >> repnr;
					
					if (sw == '1') loca=rc_l;
					else if (sw == '2') loca=rc_tl;
					else if (sw == '3') loca=rc_bl;
					else if (sw == '4') loca=rc_tr;
					else if (sw == '5') loca=rc_br;
					else loca=rc_r;
					rca[loca]->reset();
					
					cout << "time period (not exact!): " << endl;
					cin >> time;
					uint odd=0;
					#if HICANN_V>=2
					if (rca[loca]->repaddr(repnr)%2) odd=1;
					#elif HICANN_V==1
					if (repnr%2) odd=1;
					#endif

					rca[loca]->tout(repnr,true);
					rca[loca]->tdata_write(0,0,nnum,time);
					rca[loca]->tdata_write(0,1,nnum,time);
					rca[loca]->tdata_write(0,2,nnum,time);
					rca[loca]->tdata_write(1,0,nnum,time);
					rca[loca]->tdata_write(1,1,nnum,time);
					rca[loca]->tdata_write(1,2,nnum,time);
					
					rca[loca]->startout(odd);
					
					rca[loca]->tin(repnr,true);
					usleep(500); //time for the dll to lock
					rca[loca]->stopin(odd); //reset the full flag
					rca[loca]->startin(odd); //start recording received data to channel 0
					usleep(1000);  //recording lasts for ca. 4 µs - 1ms
					
					for (int i=0; i<3; i++){
						rca[loca]->tdata_read(odd, i, nnums[i], times[i]);
						cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
					}
					rca[loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
					rca[loca]->stopin(odd);
					rca[loca]->stopout(odd);
					rca[loca]->tout(repnr, false); //set tout back to 0 to prevent conflicts
				} break;

				case 'j':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time, sw;
					rc_loc loca=rc_l, locb=rc_tl;
					
					cout << "Choose configuration:" << endl;
					cout << "1: Center left repeater block sending, top left receiving:" << endl;
					cout << "2: Top Left repeater block sending, center left receiving:" << endl;
					cout << "3: Bottom left repeater block sending, top left receiving:" << endl;
					cout << "4: Bottom right repeater block sending, top right receiving:" << endl;
					cin >> sw;
					cout << "Enter time period (not exact!): " << endl;
					cin >> time;
					
					if(sw<3){
						if (sw==2){
							loca=rc_tl;
							locb=rc_l;
						}
						
						lcl->reset();
						rca[loca]->reset();
						rca[locb]->reset();
						
						for (int i=1; i<32; i+=2) lcl->switch_connect(2*i+1,i,1);
						if (loca==rc_l) for (int i=1; i<32; i+=2) rca[loca]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						else for (int i=0; i<16; i++) rca[loca]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						
						rca[loca]->tdata_write(0,0,0,time);
						rca[loca]->tdata_write(0,1,0,time);
						rca[loca]->tdata_write(0,2,0,time);
						rca[loca]->tdata_write(1,0,0,time);
						rca[loca]->tdata_write(1,1,0,time);
						rca[loca]->tdata_write(1,2,0,time);
						rca[loca]->startout(0);
						rca[loca]->startout(1);
						usleep(10000);
						
						cout << "Received data:" << endl;
						if (loca==rc_l) for (int i=0; i<16; i++) readout_repeater(locb, i);
						else for (int i=1; i<32; i+=2) readout_repeater(locb, i);
					}
					
					if(sw==3){
						loca=rc_bl;
						locb=rc_tl;
						
						rca[loca]->reset();
						lctl->reset();
						for (int n=0; n<32; n++){
							lctl->switch_connect(111-n*2,0+n*4,1);
							lctl->switch_connect(111-n*2,1+n*4,1);
							lctl->switch_connect(110-n*2,2+n*4,1);
							lctl->switch_connect(110-n*2,3+n*4,1);
						}
						for (int i=0; i<64; i++) rca[loca]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						rca[loca]->tdata_write(0,0,0,time);
						rca[loca]->tdata_write(0,1,0,time);
						rca[loca]->tdata_write(0,2,0,time);
						rca[loca]->tdata_write(1,0,0,time);
						rca[loca]->tdata_write(1,1,0,time);
						rca[loca]->tdata_write(1,2,0,time);
						rca[loca]->startout(0);
						rca[loca]->startout(1);
						cout << "Received data:" << endl;
						for (int i=0; i<64; i++) readout_repeater(locb, i);
					}
					
					if(sw==4){
						loca=rc_br;
						locb=rc_tr;
						
						rca[loca]->reset();
						lctr->reset();
						for (int n=0; n<32; n++){
							lctr->switch_connect(111-n*2,128+0+n*4,1);
							lctr->switch_connect(111-n*2,128+1+n*4,1);
							lctr->switch_connect(110-n*2,128+2+n*4,1);
							lctr->switch_connect(110-n*2,128+3+n*4,1);
						}
						for (int i=0; i<64; i++) rca[loca]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						rca[loca]->tdata_write(0,0,0,time);
						rca[loca]->tdata_write(0,1,0,time);
						rca[loca]->tdata_write(0,2,0,time);
						rca[loca]->tdata_write(1,0,0,time);
						rca[loca]->tdata_write(1,1,0,time);
						rca[loca]->tdata_write(1,2,0,time);
						rca[loca]->startout(0);
						rca[loca]->startout(1);
						cout << "Received data:" << endl;
						for (int i=0; i<64; i++) readout_repeater(locb, i);
					}
				} break;

				case 'n':{
					uint time=1, nnum=21;
					prepare_repeaters();
					
					for (int i=0; i<8; i++) nc->beg_set_number(i,21);
					
					for (int loca=0; loca<6; loca++){
						rca[loca]->reset();
						rca[loca]->tdata_write(0,0,nnum,time);
						rca[loca]->tdata_write(0,1,nnum,time);
						rca[loca]->tdata_write(0,2,nnum,time);
						rca[loca]->tdata_write(1,0,nnum,time);
						rca[loca]->tdata_write(1,1,nnum,time);
						rca[loca]->tdata_write(1,2,nnum,time);
						if (loca < 2) for (int i=0; i<32; i++) rca[loca]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						else for (int i=0; i<64; i++) rca[loca]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
						rca[loca]->startout(0);
						rca[loca]->startout(1);
					}
				} break;

				case 'h':{
					uint neuron = 0;
					uint64_t jtag_recpulse = 0;
					
					cout << "Enter neuron number: ";
					cin >> neuron;

					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					//configure global parameters for neuronbuilder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(0,neuron,1);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();

					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initval(0, 800);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}
					
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //all channels forward SPL1 data
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					//initialize a vector from an array
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);
					
					nc->nc_reset(); //reset neuron control
					nc->merger_set(merg, enable, select, slow);
					
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					spc->write_cfg(0x00ff);
					dc->setTimeStampCtrl(0x0); // Disable DNC timestamp features
					dc->setDirection(0x00); // set transmission directions in DNC (for HICANN 0 only)
					
					jtag->FPGA_enable_tracefifo();
					usleep(10000);
					jtag->FPGA_disable_tracefifo();
					
					jtag->HICANN_get_rx_data(jtag_recpulse);
					cout << "From RX register received: " << hex << jtag_recpulse << endl;
					cout << "Received Neuron number: " << dec << ((jtag_recpulse>>15) & 0x3f) << endl;
					
					int i=0;
					cout << "From trace FIFO received:" << endl;					
					while((!jtag->FPGA_empty_pulsefifo()) && (i < 50)) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						cout << " from channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					}
				
					jtag->FPGA_reset_tracefifo();
				} break;

				case 'u':{
					uint nadr, nnum, strom;
					uint64_t jtag_recpulse = 0;
					vector<uint> weights(32,0xffffffff), address(32,0);
					
					cout << "Enter neuron address: ";
					cin >> nadr;
					cout << "Enter neuron number: ";
					cin >> nnum;
					cout << "Stimulation source: Current:1, Synapses:0: ";
					cin >> strom;

					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);

					nbc->initzeros(); //first, clear neuron builder
					//configure global parameters for neuronbuilder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(nadr, nnum, strom);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 1, 1);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, address, address);
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					lctl->switch_connect(0,28,1);
					
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						while(fc[fg_num]->isBusy());
						fc[fg_num]->initval(0, 800);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}
					
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //all channels forward SPL1 data
					bool en[23] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					//initialize a vector from an array
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);
					
					dc->setTimeStampCtrl(0x0); // Disable DNC timestamp features
					dc->setDirection(0x00); // set transmission directions in DNC (for HICANN 0 only)
					
					nc->nc_reset(); //reset neuron control
					nc->merger_set(merg, enable, select, slow);
					nc->beg_configure(8, 0, 300);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 output: rising edge sampling
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					nc->beg_on(0);
					nc->beg_on(7);
					
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(8, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(16, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(20, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

					lcl->reset();
					lcr->reset();
					lcl->switch_connect(1,0,1);
					lcl->switch_connect(9,4,1);
					lcl->switch_connect(17,8,1);
					lcl->switch_connect(25,12,1);
					lcl->switch_connect(33,16,1);
					lcl->switch_connect(41,20,1);
					lcl->switch_connect(49,24,1);
					lcl->switch_connect(57,28,1);
					
					usleep(100000);
					
					rca[rc_bl]->reset();
					cout << "Bottom left repeater block:" << endl;
					for (int i=0; i<=14; i+=2){
						cout << "Received from PE " << i/2 << ":" << endl;
						readout_repeater(rc_bl, i);
						cout << endl;
					}
					
					lcl->switch_connect(56,28,1);
					lcl->switch_connect(48,24,1);
					lcl->switch_connect(40,20,1);
					lcl->switch_connect(32,16,1);
					lcl->switch_connect(24,12,1);
					lcl->switch_connect(16,8,1);
					lcl->switch_connect(8,4,1);
					lcl->switch_connect(0,0,1);
					
					usleep(100000);
					
					rca[rc_r]->reset();
					cout << "Center right repeater block:" << endl;
					for (int i=0; i<=28; i+=4){
						cout << "Received from PE " << i/4 << ":" << endl;
						readout_repeater(rc_r, i);
						cout << endl;
					}
					lcl->reset();
					
					spc->write_cfg(0x00ff);
					jtag->FPGA_enable_tracefifo();
					usleep(10000);
					jtag->FPGA_disable_tracefifo();
					
					jtag->HICANN_get_rx_data(jtag_recpulse);
					cout << "From RX register received: " << hex << jtag_recpulse << endl;
					cout << "Received Neuron number: " << dec << ((jtag_recpulse>>15) & 0x3f) << endl;
					
					int i=0;
					cout << "From trace FIFO received:" << endl;					
					while((!jtag->FPGA_empty_pulsefifo()) && (i < 150)) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						cout << " from channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					}
				
					jtag->FPGA_reset_tracefifo();
				} break;

				case 'q':{
					uint64_t jtag_recpulse = 0;
					uint sval;
					bool cur;
					vector<uint> numbers(64,0);
					vector<uint> weights(32,0xffffffff), address(32,0xffffffff); //one row, all weights max, decoder 0
					
					cout << "Choose 0 for background generator stimulus, 1 for current stimulus: ";
					cin >> cur;
					if(cur){
						cout << "Enter current: ";
						cin >> sval;
						cout << "Note: Only every second neuron can have a current input!";
					}
					
					cout << "Remember to program the floating gates first so that neurons spike with VERY little input" << endl;
					
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 2, 1, 3);
					sc_top->drv_set_gmax(0, 0, 0, 3, 3);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					
					address[0]=0x11110000;
					address[1]=0x33332222;
					address[2]=0x55554444;
					address[3]=0x77776666;
					address[4]=0x99998888;
					address[5]=0xbbbbaaaa;
					address[6]=0xddddcccc;
					address[7]=0xffffeeee;
					sc_top->write_decoder(0, address, address);
					
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					lcl->switch_connect(57,28,1);
					lctl->switch_connect(0,28,1);
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(1,1,0,0);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate2neurons(0,0,16,cur);
					activate2neurons(2,32,48,cur);
					activate2neurons(4,1,17,cur);
					activate2neurons(6,33,49,cur);
					activate2neurons(8,2,18,cur);
					activate2neurons(10,34,50,cur);
					activate2neurons(12,3,19,cur);
					activate2neurons(14,35,51,cur);
					activate2neurons(16,4,20,cur);
					activate2neurons(18,36,52,cur);
					activate2neurons(20,5,21,cur);
					activate2neurons(22,37,53,cur);
					activate2neurons(24,6,22,cur);
					activate2neurons(26,38,54,cur);
					activate2neurons(28,7,23,cur);
					activate2neurons(30,39,55,cur);
					activate2neurons(32,8,24,cur);
					activate2neurons(34,40,56,cur);
					activate2neurons(36,9,25,cur);
					activate2neurons(38,41,57,cur);
					activate2neurons(40,10,26,cur);
					activate2neurons(42,42,58,cur);
					activate2neurons(44,11,27,cur);
					activate2neurons(46,43,59,cur);
					activate2neurons(48,12,28,cur);
					activate2neurons(50,44,60,cur);
					activate2neurons(52,13,29,cur);
					activate2neurons(54,45,61,cur);
					activate2neurons(56,14,30,cur);
					activate2neurons(58,46,62,cur);
					activate2neurons(60,15,31,cur);
					activate2neurons(62,47,63,cur);

					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					//~ bool en[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					nc->nc_reset(); //reset neuron control
					nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
					nc->dnc_enable_set(1,0,0,0,0,0,0,0); //enable 1 DNC input

					spc->write_cfg(0x101ff);
					// set DNC not to ignore time stamps
					dc->setTimeStampCtrl(0x1);
					// set transmission directions in DNC (for HICANN 0 only)
					dc->setDirection(0x01);
					
					if (!cur){
						jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 40, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr()));
						usleep(10000);
						jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 10, 0xaaaa, 0, 63, comm->jtag2dnc(hc->addr()));
					}
					else{
						for(int fg_num = 0; fg_num < 4; fg_num += 1){
							while(fc[fg_num]->isBusy());
							fc[fg_num]->initval(0, sval);
							fc[fg_num]->set_pulselength(15);
							fc[fg_num]->write_biasingreg();
							fc[fg_num]->stimulateNeuronsContinuous(0);
						}
					}
					
					jtag->FPGA_enable_tracefifo();
					usleep(100000);
					jtag->FPGA_disable_tracefifo();
					jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, comm->jtag2dnc(hc->addr()));
					
					cout << "From trace FIFO received:" << endl;
					int i=0;					
					while((!jtag->FPGA_empty_pulsefifo()) && (i < 300)) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						numbers[((jtag_recpulse>>15) & 0x3f)]=1;
						cout << " from channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					}
					jtag->FPGA_reset_tracefifo();
					
					cout << "Received neuron numbers: " << endl;
					for (int i=0; i<64; i++){
						if (numbers[i]) cout << i << ", ";
					}
					
					cout << endl << "Not received neuron numbers: " << endl;
					for (int i=0; i<64; i++){
						if (!numbers[i]) cout << i << ", ";
					}
					cout << endl;
				} break;

				case 'o':{
					cout << "PRINTING ALL CROSSBARS AND SWITCHES:" << endl;
					cout << "Center left:" << endl;
					lcl->print_config();
					cout << "Center right:" << endl;
					lcr->print_config();
					cout << "Top left:" << endl;
					lctl->print_config();
					cout << "Bottom left:" << endl;
					lcbl->print_config();
					cout << "Top right:" << endl;
					lctr->print_config();
					cout << "Bottom right:" << endl;
					lcbr->print_config();

					cout << "PRINTING NEURON BUILDER CONTROL:" << endl;
					nbc->print_config();

					cout << "PRINTING NEURON CONTROL:" << endl;
					nc->print_config();

					cout << "PRINTING ALL REPEATERS:" << endl;
					cout << "Center left:" << endl;
					rca[rc_l]->print_config();
					cout << "Center right:" << endl;
					rca[rc_r]->print_config();
					cout << "Top left:" << endl;
					rca[rc_tl]->print_config();
					cout << "Bottom left:" << endl;
					rca[rc_bl]->print_config();
					cout << "Top right:" << endl;
					rca[rc_tr]->print_config();
					cout << "Bottom right:" << endl;
					rca[rc_br]->print_config();

					cout << "PRINTING SYNAPSE DRIVERS:" << endl;
					cout << "Top:" << endl;
					sc_top->print_config();
					cout << "Bottom:" << endl;
					sc_bot->print_config();

					cout << "PRINTING SYNAPSE WEIGHTS:" << endl;
					cout << "Top:" << endl;
					sc_top->print_weight();
					cout << "Bottom:" << endl;
					sc_bot->print_weight();

					cout << "PRINTING SYNAPSE DECODERS:" << endl;
					cout << "Top:" << endl;
					sc_top->print_decoder();
					cout << "Bottom:" << endl;
					sc_bot->print_decoder();
				} break;

				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};


class LteeTmQuickTests : public Listee<Testmode>{
public:         
	LteeTmQuickTests() : Listee<Testmode>(string("tm_quicktests"),string("Quick testing of a new chip (HICANNv2 only)")){};
	Testmode * create(){return new TmQuickTests();};
};
LteeTmQuickTests ListeeTmQuickTests;
