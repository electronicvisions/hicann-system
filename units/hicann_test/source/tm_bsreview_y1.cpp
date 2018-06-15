// Company         :   kip                      
// Author          :   Andreas Gruebl        
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_bsreview_y1.cpp                
//------------------------------------------------------------------------

#include "common.h"
#include "testmode.h" //Testmode and Lister classes

#include "s2comm.h"
#include "s2c_jtagphys_2fpga.h" //jtag-fpga library
#include "s2c_jtagphys.h" //jtag-fpga library

#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class
#include "repeater_control.h" //repeater control class
#include "neuron_control.h" //neuron control class (merger, background genarators)
#include "neuronbuilder_control.h" //neuron builder control class
#include "fg_control.h" //floating gate control
#include "spl1_control.h" //spl1 control class

#include "hicann_ctrl.h"
#include "dnc_control.h"
#include "fpga_control.h"

#include<iostream>
#include<fstream>
#include<string>

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace facets;
using namespace std;

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

class TmBSReviewY1 : public Testmode{

public:
	// repeater control 
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	
	// hardware infrastructure
	static const uint num_reticles = 2;
	static const uint num_dncs = 2;
	static const uint num_hicanns = 2;
	static const uint src_hicann = 0;
	static const uint tgt_hicann = 1;
	
	uint curr_hc;
	uint jtag_addr[num_hicanns];
	uint reticle_nr[num_hicanns];

	uint fpga_ip[num_hicanns][4];
	uint jtag_port[num_hicanns];

	myjtag_full *jtag_ret[num_reticles];
	S2C_JtagPhys2Fpga* comm_ret[num_reticles];
	//~ S2C_JtagPhys* comm_ret[num_reticles];
	
	HicannCtrl   *hc,  *hc_ret[num_hicanns]; 
	DNCControl   *dc,  *dc_ret[num_dncs];
	FPGAControl *fpc, *fpc_ret[num_reticles];

	// only one pointer to control classes of all HICANNs - select HICANN using select_hicann()
	FGControl *fc[4];
	NeuronBuilderControl *nbc;
	NeuronControl *nc;
	RepeaterControl *rca[6];
	SynapseControl *sc_top, *sc_bot;
	L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;
	SPL1Control *spc;


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
			fc[fg_num]->set_voltagewritetime(15);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(9);
			fc[fg_num]->set_pulselength(9);
			fc[fg_num]->set_fg_biasn(5);
			fc[fg_num]->set_fg_bias(8);
			fc[fg_num]->write_biasingreg();
			fc[fg_num]->write_operationreg();
		}
	}

	void readout_repeater(rc_loc loca, RepeaterControl::rep_direction dir, uint repnr){
		vector<uint> times(3,0);
		vector<uint> nnums(3,0);
		readout_repeater(loca, dir, repnr, times, nnums, 1);
		cout << "Repeater " << repnr << ": ";
		cout << "Received neuron numbers " << dec << nnums[0] << ", " << nnums[1] << ", " << nnums[2];
		cout << " with delays " << times[1]-times[0] << " and " << times[2]-times[1] << " cycles" << endl;
	}
	
	void readout_repeater(rc_loc loca, RepeaterControl::rep_direction dir, uint repnr, vector<uint>& times, vector<uint>& nnums, bool silent){
		uint odd=0;
		#if HICANN_V>=2
		if (rca[loca]->repaddr(repnr)%2) odd=1;
		#elif HICANN_V==1
		if (repnr%2) odd=1;
		#endif
		rca[loca]->conf_repeater(repnr, RepeaterControl::TESTIN, dir, true); //configure repeater in desired block to receive BEG data
		usleep(1000); //time for the dll to lock
		rca[loca]->stopin(odd); //reset the full flag
		rca[loca]->startin(odd); //start recording received data to channel 0
		usleep(1000);  //recording lasts for ca. 4 µs - 1ms
		
		if (!silent) cout << endl << "Repeater " << repnr << ":" << endl;
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

		if (receive) select[7] = 0; //channel 7 receives SPL1 data
		if (merge) enable[7] = 1; //channel 7 merges background and SPL1 data

		nc->nc_reset(); //reset neuron control
		nc->set_seed(seed); //set seed for BEGs
		nc->beg_configure(8, poisson, delay);
		nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
		nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
		if (on) nc->beg_on(8); //turn all BEGs on
		else nc->beg_off(8); //turn all BEGs off
	}

	bool test_spl1_loopback(bool from_fpga, uint loops){
		bool result = true;
		uint64_t systime = 0xffffffff;
		uint neuron, rec_neuron;
		uint64_t jtag_sendpulse = 0;
		uint64_t jtag_recpulse = 0;

		jtag->HICANN_stop_time_counter();
		jtag->HICANN_start_time_counter();
		jtag->FPGA_set_systime_ctrl(1);

		cout << endl << "########## TEST SPL1 LOOPBACK via " << (from_fpga?"FPGA":"JTAG") << " ##########" << endl;
		
		nc->nc_reset();		
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
						
		spc->write_cfg(0x55ff);
			
		if(from_fpga){
			//clear routing memory
			printf("tmag_spl1_loopback: Clearing routing memory\n");
			for(int addr=0;addr<64;++addr)
				jtag->FPGA_set_routing_data(7-jtag_addr[curr_hc],(1<<6)+addr,0); // l1 channel 1
			for(int addr=0;addr<64;++addr)
				jtag->FPGA_set_routing_data(7-jtag_addr[curr_hc],(3<<6)+addr,0); // l1 channel 3

			// set DNC to ignore time stamps and directly forward l2 pulses
			dc->setTimeStampCtrl(0x0);
			// set transmission directions in DNC (for HICANN 0 only)
			uint64_t dirs = (uint64_t)0x55<<(7-jtag_addr[curr_hc]*8);
			cout << "dirs = 0x" << hex << dirs << endl;
			dc->setDirection(dirs); // set transmission directions in DNC (for HICANN 0 only)
			//jtag->DNC_set_loopback(0xff); // DNC loopback
		} else {
			jtag->HICANN_set_test(0x2);
			jtag->HICANN_set_double_pulse(0);
		}
					
		srand(randomseed);
		
		for(uint ch=0;ch<8;ch+=2){
			uint errors = 0;
			for(uint loop=0;loop<loops;loop++){
				uint nrn = rand()%64;

				jtag->HICANN_read_systime(systime);
				neuron = (ch << 6) | nrn;
				jtag_sendpulse = (neuron<<15) | (systime+1000)&0x7fff; 
		
				if(from_fpga){
					fpc->sendSingleL2Pulse(0,jtag_sendpulse);
					fpc->getFpgaRxData(jtag_recpulse);
				} else {
					jtag->HICANN_start_pulse_packet(jtag_sendpulse);
					jtag->HICANN_get_rx_data(jtag_recpulse);
				}
		
				rec_neuron = (jtag_recpulse>>15)&0x1ff;
				if(neuron+64 != rec_neuron){
					cout << "ERROR: sent 0x" << hex << neuron << ", got 0x" << rec_neuron << ", mask 0x" << (neuron^rec_neuron) << 
			    		  " | RAW: sent 0x" << jtag_sendpulse << ", got 0x" << jtag_recpulse << endl;;
					result = false;
					errors++;
				}
			}
			cout << "###SPL1 Loopback Channel " << ch << ", ERRORS: " << errors << endl;
		}
  	return result;
	}
				
	bool test_switchram(uint runs) {
		int mem_size;
		ci_data_t rcvdata;
		ci_addr_t rcvaddr;

		// get pointer to all crossbars
		lcl = &hc->getLC_cl();
		lcr = &hc->getLC_cr();
		lctl =&hc->getLC_tl();
		lcbl =&hc->getLC_bl();
		lctr =&hc->getLC_tr();
		lcbr =&hc->getLC_br();

		// full L1 switch test
		cout << endl << "########## TEST SWITCH RAM ##########" << endl;
		
		uint error=0;
		uint lerror=0;
		L1SwitchControl *l1;
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
						l1->write_cmd(i,tdata[i],0);
					}

				srand(rseed);
				for(int i=0;i<mem_size;i++)
				{
					l1->read_cmd(i,0);
					l1->get_data(rcvaddr,rcvdata);
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

	void select_hicann(uint hicann_nr) {
		// obtain pointers to control classes
		this->jtag = jtag_ret[hicann_nr];
		this->jtag->set_hicann_pos(jtag_addr[hicann_nr]);
		this->comm = static_cast<Stage2Comm*>(comm_ret[hicann_nr]);
		
		dc  =  dc_ret[hicann_nr];
		fpc = fpc_ret[hicann_nr];
		hc  =  hc_ret[hicann_nr];

		nbc        = &hc->getNBC();
		nc         = &hc->getNC();
		fc[0]      = &hc->getFC_tl();
		fc[1]      = &hc->getFC_tr();
		fc[2]      = &hc->getFC_bl();
		fc[3]      = &hc->getFC_br();
		sc_top     = &hc->getSC_top();
		sc_bot     = &hc->getSC_bot();
		lcl        = &hc->getLC_cl();
		lcr        = &hc->getLC_cr();
		lctl       = &hc->getLC_tl();
		lcbl       = &hc->getLC_bl();
		lctr       = &hc->getLC_tr();
		lcbr       = &hc->getLC_br();
		rca[rc_l]  = &hc->getRC_cl();
		rca[rc_r]  = &hc->getRC_cr();
		rca[rc_tl] = &hc->getRC_tl();
		rca[rc_bl] = &hc->getRC_bl();
		rca[rc_tr] = &hc->getRC_tr();
		rca[rc_br] = &hc->getRC_br();
		spc        = &hc->getSPL1Control();
	}

	bool test() {
		// delete objects generated by tests2 main program
		for (vector<Stage2Ctrl *>::iterator it = chip.begin(); it!=chip.end(); ++it) delete *it;
		delete jtag;
		delete comm;
		
		jtag_addr[src_hicann]  = 7;
		jtag_addr[tgt_hicann]  = 6;
		
		reticle_nr[src_hicann] = 25;
		reticle_nr[tgt_hicann] = 28;

		//fpga_ip[src_hicann] = {192, 168, 1, 1}; // ECM: THIS DOES NO LEGAL C++ CODE
		//fpga_ip[tgt_hicann] = {192, 168, 1, 1};
		fpga_ip[src_hicann][0] = 192;
		fpga_ip[src_hicann][1] = 168;
		fpga_ip[src_hicann][2] =   2;
		fpga_ip[src_hicann][3] =   1;
		fpga_ip[tgt_hicann][0] = 192;
		fpga_ip[tgt_hicann][1] = 168;
		fpga_ip[tgt_hicann][2] =   2;
		fpga_ip[tgt_hicann][3] =   1;

		jtag_port[src_hicann] = 1700;
		jtag_port[tgt_hicann] = 1703;

		// ignore instances created by main program and instantiate everything locally:
		jtag_ret[src_hicann] = new myjtag_full(true, true, 8, jtag_addr[src_hicann]);
		jtag_ret[tgt_hicann] = new myjtag_full(true, true, 8, jtag_addr[tgt_hicann]);
		
		comm_ret[src_hicann] = new S2C_JtagPhys2Fpga(comm->getConnId(), jtag_ret[src_hicann],true);
		comm_ret[tgt_hicann] = new S2C_JtagPhys2Fpga(comm->getConnId(), jtag_ret[tgt_hicann],true);
		//~ comm_ret[src_hicann] = new S2C_JtagPhys(jtag_ret[src_hicann]);
		//~ comm_ret[tgt_hicann] = new S2C_JtagPhys(jtag_ret[tgt_hicann]);

		hc_ret[src_hicann] = new HicannCtrl(comm_ret[src_hicann],jtag_addr[src_hicann]);
		hc_ret[tgt_hicann] = new HicannCtrl(comm_ret[tgt_hicann],jtag_addr[tgt_hicann]);
		
		dc_ret[src_hicann] = new DNCControl(comm_ret[src_hicann],0);
		dc_ret[tgt_hicann] = new DNCControl(comm_ret[tgt_hicann],0);

		fpc_ret[src_hicann] = new FPGAControl(comm_ret[src_hicann],0);
		fpc_ret[tgt_hicann] = new FPGAControl(comm_ret[tgt_hicann],0);
		
		
		//configure all the instances (read memory registers to get the current internal configuration)
		//otherwise the instance assumes to have zeros in all registers
		for (uint i=0;i<num_hicanns;i++) {
			// first initialize JTAG
			if(!jtag_ret[i]->open(jtag_lib::JTAG_ETH)){
			  log(Logger::ERROR)<< "JTAG open failed!" ;
			  exit(EXIT_FAILURE);
			}
			if (!jtag_ret[i]->jtag_init( jtag_lib::ip_number(fpga_ip[i][0],fpga_ip[i][1],fpga_ip[i][2],fpga_ip[i][3]), jtag_port[i])) {
				log(Logger::ERROR)<< "JTAG init failed!" ;
				exit(EXIT_FAILURE);
			}
			jtag_ret[i]->jtag_speed(10000);
			jtag_ret[i]->jtag_bulkmode(true);
	
			select_hicann(i);
		
			// Reset HICANNs and comm classes
			jtag->reset_jtag();
			jtag->FPGA_set_fpga_ctrl(1);
			jtag->reset_jtag();

			if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
				log(Logger::INFO) << "ERROR: Init failed - EXIT..." << endl;
				exit(EXIT_FAILURE);
			}

			nc->configure(); 
			lcl->configure(); 
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
		}


		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "s: select HICANN for subsequent operations" << endl;
			cout << "0: Reset everything (all rams)" << endl;
			cout << "1: Send HICANN reset pulse (restart the testmode after reset!)" << endl;
			cout << "2: JTAG reset" << endl;
			cout << "3: perform quick SPL1 loopback test on selected HICANN" << endl;
			cout << "4: test switch memory of currently selected HICANN" << endl;
			cout << "5: Test FPGA BG input on the neuron (scope needed)" << endl;
			cout << "6: Test inner-HICANN neuron input (scope needed)" << endl;
			cout << "7: Loopback test with FIFO/BG/Rx" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "t: Start test run with 2 neurons" << endl;
			cout << "x: Exit" << endl;
			cin >> c;
			
			switch(c){

				case 's':{
					cout << "which HICANN (0-" << (num_hicanns-1) << ")? ";
					cin >> curr_hc;
					if (curr_hc>num_hicanns-1) log(Logger::ERROR) << "*** Illegal HICANN nr! ***";
					else select_hicann(curr_hc);
				} break;

				case '0':{
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

				case '1':{
					cout << "resetting HICANN" << endl;
					jtag->FPGA_set_fpga_ctrl(1);
				} break;

				case '2':{
					cout << "resetting test logic" << endl;
					jtag->reset_jtag();
				} break;

				case '3':{
					uint spl1loops = 512;
					test_spl1_loopback(true, spl1loops);
				} break;
				
				case '4':{
					uint loops = 1;
					cout << "Testing   Switch ram... " << flush;
					cout << test_switchram(loops) << endl;
				} break;
 	     
				case '5':{
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
					uint64_t dirs = (uint64_t)0xff<<(7-jtag_addr[curr_hc]*8);
					cout << "dirs = 0x" << hex << dirs << endl;
					dc->setDirection(dirs); // set transmission directions in DNC
					
					jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 80, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr()));
				} break;
				
				case '6':{
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
					
					configure_hicann_bgen(1, 500, 200, 0, 0, 0);
				} break;
				
				case '7':{
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
					uint64_t dirs = (uint64_t)0x55<<(7-jtag_addr[curr_hc]*8);
					cout << "dirs = 0x" << hex << dirs << endl;
					dc->setDirection(dirs); // set transmission directions in DNC
					
					jtag->HICANN_read_systime(systime);
					
					cout << "Choose Loopback Mode" << endl;
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
							ev1=gen_spl1(jtag_addr[curr_hc], 0, neuron, 150*2*i);
							ev2=gen_spl1(jtag_addr[curr_hc], 0, neuron, 150*(2*i+1));
							fpgaev=gen_fpga(ev1, ev2);
							jtag->FPGA_write_playback_fifo(fpgaev);
						}
						jtag->HICANN_stop_time_counter();
						jtag->HICANN_start_time_counter();
						system("./sys_start.py 1");

						// start experiment execution
						printf("tm_l2pulses: Start: enabling fifos\n");
						// wait...
						#ifdef NCSIM
						#else
						usleep(1000000);
						#endif
						jtag->FPGA_enable_tracefifo();
						jtag->FPGA_enable_pulsefifo();
						#ifdef NCSIM
						wait(500,SC_US);
						#else
						usleep(2000000);
						#endif

						// stop experiment
						printf("tm_l2pulses: Stopping experiment\n");
						jtag->FPGA_disable_pulsefifo();
						jtag->FPGA_disable_tracefifo();
						// call python script to disable sys_Start
						system("./sys_start.py 0");
						
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
						
						system("./sys_start.py 1");
						// start experiment execution
						printf("tm_l2pulses: Start: enabling fifos\n");
						// wait...
						usleep(1000000);
						jtag->FPGA_enable_tracefifo();
						usleep(100000);

						// stop experiment
						printf("tm_l2pulses: Stopping experiment\n");
						jtag->FPGA_disable_tracefifo();
						// call python script to disable sys_Start
						system("./sys_start.py 0");

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
				
				case 'p':{
					setupFg();

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

				case 't':{
					///source neuron is on HICANN7 reticle 25, target neuron is on HICANN6 reticle 28
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					
					//configuring source HICANN					
					select_hicann(src_hicann);
					curr_hc=src_hicann;
					
					set_pll(2,1);
					
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

					lcl->switch_connect(57,28,1); //from BEG0 / Repeater 28
					lctl->switch_connect(0,28,1); //to syn. driver 0
					lcl->switch_connect(1,0,1); //to the upper HICANN
					
					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
										
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(0,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					///initial test using BEG
					configure_hicann_bgen(1, 300, 200, 0, 1, 0);
					///final test using L2
					//~ configure_hicann_bgen(0, 300, 200, 0, 1, 0);
					//~ nc->dnc_enable_set(1,1,1,1,1,1,1,1); //enable all DNC inputs
					//~ spc->write_cfg(0x0ffff);
					//~ // set DNC to ignore time stamps and directly forward l2 pulses
					//~ dc->setTimeStampCtrl(0x0);
					//~ uint64_t dirs = (uint64_t)0xff<<(7-jtag_addr[curr_hc]*8);
					//~ cout << "dirs = 0x" << hex << dirs << endl;
					//~ dc->setDirection(dirs); // set transmission directions in DNC
					//~ //enable FPGA BEG on channel 0
					//~ jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 300, 0xaaaa, 0, 0, comm->jtag2dnc(hc->addr())); ///is it also correct for HICANN!=0 ???
					
					///test of activity
					lcl->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the BEG:" << endl;
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << "Events after the first neuron:" << endl;
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl->switch_connect(1,0,0);
					///end of test of activity
					
					//configuring target HICANN
					select_hicann(tgt_hicann);
					curr_hc=tgt_hicann;
					
					set_pll(2,1);
					
					sc_top->reset_drivers();
					sc_top->configure();
					lcl->reset();
					lcr->reset();
					lctl->reset();
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					rca[rc_l]->reset();
					rca[rc_bl]->reset();
					
					///test of activity
					usleep(10000);
					cout << "Events before the first neuron:" << endl;
					readout_repeater(rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(rc_bl, RepeaterControl::INWARDS, 1);
					///end of test of activity
					
					configure_hicann_bgen(0, 300, 200, 0, 1, 0); //configure merger tree
					
					rca[rc_bl]->conf_repeater(1, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 1 to forward data on vertical line 2
					lctl->switch_connect(110,2,1); //to driver 110
					
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					sc_top->drv_config(220, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(220, 0, 0, 0, 0);
					sc_top->drv_set_gmax(220, 0, 0, 1, 1);
					sc_top->write_weight(220, weights);
					sc_top->write_weight(221, weights);
					sc_top->write_decoder(220, address, address); //to prevent mistakes

					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(0,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					///initial test using L1
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					lcl->switch_connect(1,0,1);
					usleep(10000);
					cout << "Events after the first neuron again:" << endl;
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl->switch_connect(1,0,0);
					
					///final test using L2
					//~ spc->write_cfg(0x000ff); ///CORRECT???
					//~ // set DNC to ignore time stamps and directly forward l2 pulses
					//~ dc->setTimeStampCtrl(0x0);
					//~ uint64_t dirs2 = (uint64_t)0x0<<(7-jtag_addr[curr_hc]*8);
					//~ cout << "dirs = 0x" << hex << dirs2 << endl;
					//~ dc->setDirection(dirs2); // set transmission directions in DNC
					//~ 
					//~ system("./sys_start.py 1");
					//~ // start experiment execution
					//~ printf("tm_l2pulses: Start: enabling fifos\n");
					//~ // wait...
					//~ usleep(1000000);
					//~ jtag->FPGA_enable_tracefifo();
					//~ usleep(100000);
					//~ 
					//~ // stop experiment
					//~ printf("tm_l2pulses: Stopping experiment\n");
					//~ jtag->FPGA_disable_tracefifo();
					//~ // call python script to disable sys_Start
					//~ system("./sys_start.py 0");
					//~ 
					//~ cout << "From trace FIFO received:" << endl;
					//~ int i=0;					
					//~ while((!jtag->FPGA_empty_pulsefifo()) && (i < 20)) {
						//~ i++;
						//~ jtag->FPGA_get_trace_fifo(jtag_recpulse);
						//~ cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						//~ cout << " from channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						//~ cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					//~ }
					//~ jtag->FPGA_reset_tracefifo();
					
				} break;
				
				case 'A':{
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					uint time=97;
					
					select_hicann(tgt_hicann);
					curr_hc=tgt_hicann;

					lcl->reset();
					lctl->reset();
					lcbl->reset();
					lcr->reset();
					lctr->reset();
					lcbr->reset();
					
					//source HICANN
					log(Logger::INFO) << "Starting downward direction test: resetting repeaters...";
					for (uint i=0; i<6; i++) rca[i]->reset();
					
					log(Logger::INFO) << "Configuring vertical repeaters for output...";
					for (uint i=0; i<64; i++){
						rca[rc_bl]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca[rc_tl]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
						rca[rc_br]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::OUTWARDS, 1);
						rca[rc_tr]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, 1);
					}
					
					for (uint i=2; i<6; i++){
						rca[i]->tdata_write(0,0,0,time);
						rca[i]->tdata_write(0,1,0,time);
						rca[i]->tdata_write(0,2,0,time);
						rca[i]->tdata_write(1,0,0,time);
						rca[i]->tdata_write(1,1,0,time);
						rca[i]->tdata_write(1,2,0,time);
						rca[i]->startout(0);
						rca[i]->startout(1);
					}
					usleep(10000); //DLL locking
					
					//target HICANN			
							
					select_hicann(src_hicann);
					curr_hc=src_hicann;
					lcl->reset();
					lctl->reset();
					lcbl->reset();
					lcr->reset();
					lctr->reset();
					lcbr->reset();
					
					for (uint i=0; i<6; i++) rca[i]->reset();
					log(Logger::INFO) << "Reading out target repeaters...";
					cout << endl << "Received data bottom left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(rc_bl, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data bottom right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(rc_br, RepeaterControl::OUTWARDS, i);
					cout << endl << "Received data top left:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(rc_tl, RepeaterControl::INWARDS, i);
					cout << endl << "Received data top right:" << endl;
					for (uint i=0; i<64; i++) readout_repeater(rc_tr, RepeaterControl::INWARDS, i);
		
				} break;
				
				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};


class LteeTmBSReviewY1 : public Listee<Testmode>{
public:         
	LteeTmBSReviewY1() : Listee<Testmode>(string("tm_bsreview_y1"),string("contains all functionality for year 1 BrainScaleS review low level demo")){};
	Testmode * create(){return new TmBSReviewY1();};
};
LteeTmBSReviewY1 ListeeTmBSReviewY1;
