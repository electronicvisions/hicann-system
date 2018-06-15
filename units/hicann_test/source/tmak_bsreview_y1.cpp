#include <time.h>

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
#include "iboardv2_ctrl.h" //iBoard control class

#include "pulse_float.h"

//#define USENR         0
#define fpga_master   1
#define DATA_TX_NR    10000

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

class TmAKBSReviewY1 : public Testmode{

public:
	// repeater control 
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	
    double starttime;
    double clkperiod_bio_dnc;
    double clkperiod_bio_fpga;
    double playback_begin_offset;
		
	uint hicann_nr, hicann_jtag_nr;

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

	void configure_mergers(){
		nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
			DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
		bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
		bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		vector<nc_merger> merg(mer, mer+23);
		vector<bool> slow(23, false);
		vector<bool> select(se, se+23);
		vector<bool> enable(en, en+23);

		nc->nc_reset(); //reset neuron control
		nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
		nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
		nc->dnc_enable_set(0,0,0,0,0,0,0,0);
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
							bino(tdata[i],16) << dec << endl;
					}
				}
				cout <<"Switchramtest loop " << l << " errors: " << lerror << ", total errors: " << error << endl;
			}
			cout << endl;
		
		}
		return (error == 0);
	}


	bool test() {
		this->hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
		dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC

		hicann_jtag_nr = hc->addr();
		hicann_nr      = jtag->pos_dnc-1-hicann_jtag_nr;
		jtag->set_hicann_pos(hicann_jtag_nr);
		
		uint64_t jtagid;

		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << dec << endl;
		
		jtag->read_id(jtagid,jtag->pos_dnc);
		cout << "DNC ID: 0x" << hex << jtagid <<  dec << endl;
		
		jtag->read_id(jtagid,jtag->pos_fpga);
		cout << "FPGA ID: 0x" << hex << jtagid << dec << endl;

		log(Logger::INFO) << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
			log(Logger::INFO) << "ERROR: Init failed,abort..." << endl;
			return false;
		}
			
		log(Logger::INFO) << "Init() ok" << endl;

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
		setupFg();

		IBoardV2Ctrl board(conf, jtag, 0); //create an iboard control instance
		
		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "0: Reset everything (all rams)" << endl;
			cout << "1: Send HICANN reset pulse (restart the testmode after reset!)" << endl;
			cout << "2: JTAG reset" << endl;
			cout << "3: Set PLL frequency on the HICANN" << endl;
			cout << "4: Test switch memory of currently selected HICANN" << endl;
			cout << "5: Loopback test with FIFO/BG/Rx" << endl;
			cout << "6: Turn FPGA BEG off" << endl;
			cout << "7: Turn HICANN BEG off" << endl;
			cout << "f: Test FPGA BEG input on the neuron (scope needed)" << endl;
			cout << "h: Test inner-HICANN neuron input (scope needed)" << endl;
			cout << "F: Adjust FPGA BEG" << endl;
			cout << "H: Adjust HICANN BEG" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "t: Start test: Source HICANN" << endl;
			cout << "T: Start test: Target HICANN (2 neurons)" << endl;
			cout << "o: Deactivate analog outputs on HICANN" << endl;
			cout << "r: ARQ reset (NOT AVAILABLE ANYMORE!)" << endl;
			cout << "s: STDP measurement setup" << endl;
			cout << "l: Loopback-channel collision check" << endl;
			cout << "m: Test BEG/merger tree output to trace FIFO: correct behavior" << endl;
			cout << "M: Test BEG/merger tree output to trace FIFO: incorrect behavior" << endl;
			cout << "n: Test denmem output to trace FIFO" << endl;
			cout << "d: Test L1 bit-flipping behavior: 1 number systematically" << endl;
			cout << "D: Test L1 bit-flipping behavior: 1 number" << endl;
			cout << "c: Test L1 bit-flipping behavior: 7 numbers systematically" << endl;
			cout << "C: Test L1 bit-flipping behavior: 7 numbers" << endl;
			cout << "u: Test L1 bit-flipping behavior: L2 input systematically" << endl;
			cout << "U: Test L1 bit-flipping behavior: Vol/Voh sweep" << endl;
			cout << "x: Exit" << endl;
			cin >> c;
			
			switch(c){

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
				
				case '4':{
					uint loops = 1;
					cout << "Testing   Switch ram... " << flush;
					cout << test_switchram(loops) << endl;
				} break;
				
				case '5':{
					uint mode, neuron;
					uint64_t jtag_recpulse = 0;
					
					cout << "HICANN number: "  << hicann_nr <<endl;
					nc->dnc_enable_set(1,0,1,0,1,0,1,0);
					nc->loopback_on(0,1);
					nc->loopback_on(2,3);
					nc->loopback_on(4,5);
					nc->loopback_on(6,7);
					spc->write_cfg(0x055ff);
								
					// setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

					comm->Flush(); //important!

					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3

					cout << "Choose Loopback Mode" << endl;
					cout << "1: Using playback fifo and trace fifo" << endl;
					cout << "2: Using FPGA backgroud generator and trace fifo" << endl;
					cout << "3: Using single pulses and RX register" << endl;
					cin >> mode;
					cout << "Enter Neuron number: ";
					cin >> neuron;
					
					/// fill the fifo and start it
					if(mode==1) {
						uint64_t event;

						cout << "current HICANN JTAG ID: " << 7-hicann_nr << endl;
						cout << "current HICANN DNC ID: " << hicann_nr << endl;
						
						jtag->FPGA_reset_tracefifo();
						
						for (int i = 1; i < 512; i++){
							event=gen_spl1(hicann_nr, 0, neuron, 150*2*i);
							jtag->FPGA_fill_playback_fifo(false, 200, hicann_nr, event, 0);
						}
						cout << endl << "Last event written: " << hex << event << dec << endl;

						cout << "Starting systime counters..." << endl;
						#ifdef NCSIM
						jtag->FPGA_set_systime_ctrl(0);
						jtag->HICANN_restart_time_counter();
						jtag->FPGA_set_systime_ctrl(1);
						#else
						unsigned int curr_ip = jtag->get_ip();
						S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
						if (comm_ptr != NULL){
							comm_ptr->trigger_systime(curr_ip,false);
							jtag->HICANN_stop_time_counter();
							jtag->HICANN_start_time_counter();
							comm_ptr->trigger_systime(curr_ip,true);
						}
						else  printf("tm_l2pulses warning: bad s2comm pointer; did not initialize systime counter.\n");
						#endif

						// start experiment execution
						cout << "Starting experiment..." << endl;
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
						jtag->FPGA_disable_pulsefifo();
						jtag->FPGA_disable_tracefifo();
						
						// disable sys_Start
						#ifdef NCSIM
						jtag->FPGA_set_systime_ctrl(0);
						#else
						comm_ptr->trigger_systime(curr_ip,false);
						#endif
						
						cout << "From trace FIFO received:" << endl;	
						int i=0;				
						while((!jtag->FPGA_empty_pulsefifo())) {
							i++;
							jtag->FPGA_get_trace_fifo(jtag_recpulse);
							cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
							cout << " from HICANN\t" << dec << ((jtag_recpulse>>24) & 0x7);
							cout << " and channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
							cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
						}
						cout << "Received event total: " << i << endl;
						
						jtag->FPGA_reset_tracefifo();
						spc->write_cfg(0x0ff00);
					}

					/// activate background generator
					if(mode==2) {
						uint channel, period;
						cout << "Enter channel number: ";
						cin >> dec >> channel;
						cout << "Enter desired period (cycles): ";
						cin >> dec >> period;
						jtag->FPGA_reset_tracefifo();

						jtag->FPGA_set_cont_pulse_ctrl(1, 1<<channel, 0, period, 0, neuron, neuron, hicann_nr);
						jtag->FPGA_enable_tracefifo();
						jtag->FPGA_disable_tracefifo();
						jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, hicann_nr);
						
						int i=0;				
						while((!jtag->FPGA_empty_pulsefifo()) && (i < 30)) {
							i++;
							jtag->FPGA_get_trace_fifo(jtag_recpulse);
							cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
							cout << " from HICANN\t" << dec << ((jtag_recpulse>>24) & 0x7);
							cout << " and channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
							cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
						}
						
						jtag->FPGA_reset_tracefifo();
						spc->write_cfg(0x0ff00);
					}

					/// send single pulse
					if(mode==3) {
						uint64_t systime = 0xffffffff;
						jtag->HICANN_read_systime(systime);
						uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
						jtag->HICANN_set_test(0x2);  //for manual pulse event insertion in HICANN
						jtag->HICANN_set_double_pulse(0);
						jtag->HICANN_start_pulse_packet(jtag_sendpulse);

						// last received pulse readout
						jtag->HICANN_get_rx_data(jtag_recpulse);
						cout << "Last Received pulse: 0x" << hex << jtag_recpulse << dec << endl;
						cout << "Received Neuron number: " << dec << ((jtag_recpulse>>15) & 0x3f) << endl;
						cout << "From channel: " << dec << ((jtag_recpulse>>21) & 0x7) << endl;
					}
					
					jtag->HICANN_set_test(0x0);
					
					if ((uint)((jtag_recpulse>>15)&0x3f) != (neuron&0x3f)) cout << "TESTFAIL: Packet(s) not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
					else cout << "TESTPASS: Transmission of pulse packet(s) via JTAG->HICANN(L1)->JTAG successful." << endl;
				} break;
				
				case '6':{
					jtag->FPGA_reset_tracefifo();
					jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, comm->jtag2dnc(hc->addr()));
					spc->write_cfg(0x0ff00);
				} break;
				
				case '7':{
					nc->beg_off(8);
				} break;

				case 'f':{
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					bool loopfifo;
					uint neuron;
					uint64_t event;
					
					cout << "Which neuron?" << endl;
					cin >> neuron;
					cout << "For loopfifo enter 1, for BEG enter 0: ";
					cin >> loopfifo;
					
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
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);
					activate_neuron(neuron,0,0);
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					cout << "HICANN number: "  << hicann_nr <<endl;
					nc->nc_reset();
					nc->dnc_enable_set(1,1,1,1,1,1,1,1);
					spc->write_cfg(0x0ffff);

					// setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0xff<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3
					
					if(loopfifo){
						jtag->FPGA_reset_tracefifo();
						
						for (int i = 1; i < 64; i++){
							event=gen_spl1(hicann_nr, 0, neuron, 150*2*i);
							jtag->FPGA_fill_playback_fifo(false, 500, hicann_nr, event, 0);
						}
						cout << endl << "Last event written: " << hex << event << dec << endl;

						cout << "Starting systime counters..." << endl;
						#ifdef NCSIM
						jtag->FPGA_set_systime_ctrl(0);
						jtag->HICANN_restart_time_counter();
						jtag->FPGA_set_systime_ctrl(1);
						#else
						unsigned int curr_ip = jtag->get_ip();
						S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
						if (comm_ptr != NULL){
							comm_ptr->trigger_systime(curr_ip,false);
							jtag->HICANN_stop_time_counter();
							jtag->HICANN_start_time_counter();
							comm_ptr->trigger_systime(curr_ip,true);
						}
						else  printf("tm_l2pulses warning: bad s2comm pointer; did not initialize systime counter.\n");
						#endif

						// start experiment execution
						cout << "Starting experiment..." << endl;
						#ifdef NCSIM
						#else
						usleep(1000000);
						#endif
						jtag->FPGA_enable_fifoloop();
						jtag->FPGA_enable_pulsefifo();
						
						cout << "Experiment running, press ENTER to interrupt";
						cin.ignore();
						cin.ignore();
						// disable sys_Start
						jtag->FPGA_disable_pulsefifo();
						jtag->FPGA_disable_fifoloop();

						#ifdef NCSIM
						jtag->FPGA_set_systime_ctrl(0);
						#else
						comm_ptr->trigger_systime(curr_ip,false);
						#endif

						jtag->FPGA_reset_tracefifo();
					}
					else jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 400, 0xaaaa, 0, 0, hicann_nr);
				} break;
				
				case 'h':{
					uint neuron;
					cout << "Which neuron?" << endl;
					cin >> neuron;
					
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
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(neuron,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					configure_hicann_bgen(1, 500, 200, 0, 0, 0);
				} break;

				case 'F':{
					uint delay, poisson, seed=0xaaaa;
					cout << "Enter delay between spikes (in cycles): ";
					cin >> delay;
					cout << "Enter zero for constant delay, non-zero for Poisson: ";
					cin >> poisson;
					if (poisson) {
						cout << "Enter seed: ";
						cin >> seed;
						}
					jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, poisson, delay, seed, 0, 0, hicann_nr);
				} break;

				case 'H':{
					uint delay, poisson, seed=0xaa;
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
					///source neuron is 12 on HICANN5(JTAG) reticle 29, target neuron is 20 on HICANN4(JTAG) reticle 31
					uint sourceneuron=0; //reticle 29, HICANN 7
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					
					cout << dec << "Starting source configuration..." << flush;
					set_pll(4,1); //pll to 200 MHz
					
					nbc->setOutputEn(false, false);
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_bot->reset_drivers(); //reset bottom synapse block drivers
					sc_top->configure();
					
					for (uint u=0; u<6; u++) rca[u]->reset();
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					
					lcl->switch_connect(57,28,1); //background
					lctl->switch_connect(0,28,1); //to driver 0
					lcl->switch_connect(1,0,1); //to the upper HICANN
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 15, 15);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, address, address);

					nbc->initzeros(); //neuron configuration
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);
					activate_neuron(sourceneuron,0,0);
					nbc->setSpl1Reset(0);
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();

					///initial test using HICANN BEG
					//~ configure_hicann_bgen(1, 300, 0xaa, 0, 1, 0); //start BEG
	
					///final test using L2
					configure_hicann_bgen(0, 300, 200, 0, 1, 0);
					
					cout << "HICANN number: " << hicann_nr <<endl;
					nc->dnc_enable_set(1,0,0,0,0,0,0,0);
					spc->write_cfg(0x0ffff);

					//setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0xff<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

					//~ //clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3
					
					//start background generator
					jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 1000, 0xaaaa, 0, 0, hicann_nr);
					
					///test of activity
					cout << "Events after the BEG:" << endl;
					usleep(10000);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << "Events after the first neuron:" << endl;
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);

					nbc->setOutputEn(false,true);
					nbc->setOutputMux(1,4);
					///end of activity test
				} break;

				case 'T':{
					///source neuron is 12 on HICANN5(JTAG) reticle 29, target neuron is 20 on HICANN4(JTAG) reticle 31
					uint targetneuron=0; //reticle 31, HICANN 6
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					
					set_pll(4,1);
					
					nbc->setOutputEn(false, false);
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_bot->reset_drivers(); //reset bottom synapse block drivers
					sc_top->configure();
					
					for (uint u=0; u<6; u++) rca[u]->reset();
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					///test of activity
					usleep(10000);
					cout << "Events before the first neuron:" << endl;
					readout_repeater(rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(rc_bl, RepeaterControl::INWARDS, 1);
					readout_repeater(rc_bl, RepeaterControl::INWARDS, 1);
					///test of activity end
					
					rca[rc_bl]->conf_repeater(1, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true); //configure repeater 1 to forward data on vertical line 2
					
					lctl->switch_connect(110,2,1); //to driver 110
					
					sc_top->drv_config(220, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(220, 0, 0, 0, 0);
					sc_top->drv_set_gmax(220, 0, 0, 1, 1);
					sc_top->write_weight(220, weights);
					sc_top->write_weight(221, weights);
					sc_top->write_decoder(220, address, address);

					nbc->initzeros(); //neuron configuration
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);
					activate_neuron(targetneuron,0,0);
					nbc->setSpl1Reset(0);
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();

					configure_hicann_bgen(0, 300, 200, 0, 1, 0); //configure merger tree
					
					///initial test using L1, test of activity
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					lcl->switch_connect(1,0,1);					
					cout << "Events after the second neuron:" << endl;
					usleep(100000);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					lcl->switch_connect(1,0,0);
					nbc->setOutputEn(true,false);
					nbc->setOutputMux(4,1);
					///test of activity end
					
					///final test using L2
					uint64_t jtag_recpulse = 0;
					spc->write_cfg(0x000ff);
								
					// setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x00<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3

					jtag->FPGA_reset_tracefifo();
					jtag->FPGA_enable_tracefifo();
					usleep(100000);
					jtag->FPGA_disable_tracefifo();
					
					int i=0;				
					while((!jtag->FPGA_empty_pulsefifo()) && (i < 30)) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						cout << " from HICANN\t" << dec << ((jtag_recpulse>>24) & 0x7);
						cout << " and channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					}
					
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x0ff00);
				} break;

				case 'o':{
					nbc->setOutputEn(false, false);
				} break;

				case 's':{
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					uint mode, neuron;
					uint64_t jtag_recpulse = 0;
					
					cout << "Which neuron?" << endl; //tested on: reticle 31, HICANN 0, neuron 8
					cin >> neuron; 

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					rca[rc_l]->reset();
					nc->nc_reset();

					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57					
					lcl->switch_connect(57,28,1);
					lctl->switch_connect(16,28,1);
					
					sc_top->drv_config(32, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(32, 0, 0, 0, 0);
					sc_top->drv_set_gmax(32, 0, 0, 15, 15);
					sc_top->write_weight(32, weights);
					sc_top->write_weight(33, weights);
					sc_top->write_decoder(32, address, address);
					
					nbc->setOutputEn(true,true);
					nbc->setOutputMux(1,4);
					
					//configure neuron
					nbc->initzeros(); //first, clear neuron builder
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);
					activate_neuron(neuron,1,0);
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					configure_hicann_bgen(0, 300, 200, 0, 1, 0); //configure merger tree
					
					nc->dnc_enable_set(1,0,1,0,0,0,0,0); //only two channels into the HICANN
					///one has to put identical (and simultaneous) input on channel 0 and 2 to be able
					///to record input and output at the same time
					nc->loopback_on(2,3); //loopback to record the incoming pulses (they will come from channel 3)
					spc->write_cfg(0x005ff);
								
					// setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x05<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					
					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3

					/// fill the fifo and start it
					uint64_t event1, event2;
					uint channel=0, nnumber=0, delay=500;
					cout << "current HICANN JTAG ID: " << 7-hicann_nr << endl;
					cout << "current HICANN DNC ID: " << hicann_nr << endl;
					
					jtag->FPGA_reset_tracefifo();
					for (int i = 1; i < 64; i++){
						channel=0; //goes to the neuron
						event1=gen_spl1(hicann_nr, channel, nnumber, 150*2*i); //pulse generation: generate 2 identical pulses on 2 channels
						channel=2; //goes to the loopback for recording purposes
						event2=gen_spl1(hicann_nr, channel, nnumber, 150*2*i); //pulse generation: generate 2 identical pulses on 2 channels
						jtag->FPGA_fill_playback_fifo(false, delay, hicann_nr, event1, 0); //push the pulses into the FIFO
						jtag->FPGA_fill_playback_fifo(false, 2, hicann_nr, event2, 0); ///minimal timestamp difference is 2... TODO!!! -> TUD?
					}
					cout << endl << "Last event written: " << hex << event2 << dec << endl;
					
					cout << "Starting systime counters..." << endl;
					#ifdef NCSIM
					jtag->FPGA_set_systime_ctrl(0);
					jtag->HICANN_restart_time_counter();
					jtag->FPGA_set_systime_ctrl(1);
					#else
					unsigned int curr_ip = jtag->get_ip();
					S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
					if (comm_ptr != NULL){
						comm_ptr->trigger_systime(curr_ip,false);
						jtag->HICANN_stop_time_counter();
						jtag->HICANN_start_time_counter();
						comm_ptr->trigger_systime(curr_ip,true);
					}
					else  printf("tm_l2pulses warning: bad s2comm pointer; did not initialize systime counter.\n");
					#endif
					
					// start experiment execution
					cout << "Starting experiment..." << endl;
					
					jtag->FPGA_enable_tracefifo();
					usleep(100000);
					jtag->FPGA_enable_fifoloop();
					jtag->FPGA_enable_pulsefifo();
					usleep(2000000); //running time
					// stop experiment
					jtag->FPGA_disable_pulsefifo();
					jtag->FPGA_disable_fifoloop();
					jtag->FPGA_disable_tracefifo();
					
					// disable sys_Start
					#ifdef NCSIM
					jtag->FPGA_set_systime_ctrl(0);
					#else
					comm_ptr->trigger_systime(curr_ip,false);
					#endif
				
					cout << "From trace FIFO received:" << endl;	
					int i=0;				
					while((!jtag->FPGA_empty_pulsefifo()) && (i < 50)) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						cout << " from HICANN\t" << dec << ((jtag_recpulse>>24) & 0x7);
						cout << " and channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					}
					
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x0ff00);
				} break;
				
				case 'l':{
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					uint mode, neuron;
					uint64_t jtag_recpulse = 0;
					
					configure_hicann_bgen(0, 300, 200, 0, 0, 1); //configure merger tree
					nc->dnc_enable_set(1,0,1,0,0,0,0,0); //only two channels into the HICANN
					nc->loopback_on(0,1);
					nc->loopback_on(2,3);
					nc->beg_set_number(7,33);
					//nc->beg_on(7); ///AUSKOMMENTIEREN
					spc->write_cfg(0x005ff);
								
					// setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x05<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					
					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3

					/// fill the fifo and start it
					uint64_t event1, event2;
					uint channel=0, delay=500;
					cout << "current HICANN JTAG ID: " << 7-hicann_nr << endl;
					cout << "current HICANN DNC ID: " << hicann_nr << endl;
					
					jtag->FPGA_reset_tracefifo();
					for (int i = 1; i < 64; i++){
						channel=0;
						event1=gen_spl1(hicann_nr, channel, 11, 150*2*i); //pulse generation
						channel=2;
						event2=gen_spl1(hicann_nr, channel, 55, 150*2*i);
						jtag->FPGA_fill_playback_fifo(false, delay, hicann_nr, event1, 0); //push the pulses into the FIFO
						jtag->FPGA_fill_playback_fifo(false, delay, hicann_nr, event2, 0);
					}
					cout << endl << "Last event written: " << hex << event2 << dec << endl;
					
					cout << "Starting systime counters..." << endl;
						#ifdef NCSIM
						jtag->FPGA_set_systime_ctrl(0);
						jtag->HICANN_restart_time_counter();
						jtag->FPGA_set_systime_ctrl(1);
						#else
						unsigned int curr_ip = jtag->get_ip();
						S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
						if (comm_ptr != NULL){
							comm_ptr->trigger_systime(curr_ip,false);
							jtag->HICANN_stop_time_counter();
							jtag->HICANN_start_time_counter();
							comm_ptr->trigger_systime(curr_ip,true);
						}
						else  printf("tm_l2pulses warning: bad s2comm pointer; did not initialize systime counter.\n");
						#endif
					
					// start experiment execution
					cout << "Starting experiment..." << endl;
					
					jtag->FPGA_enable_tracefifo();
					jtag->FPGA_enable_fifoloop();
					jtag->FPGA_enable_pulsefifo();
					nc->beg_on(7); ///EINKOMMENTIEREN
                    usleep(1000000);
					// stop experiment
					jtag->FPGA_disable_pulsefifo();
					jtag->FPGA_disable_fifoloop();
					jtag->FPGA_disable_tracefifo();
					
					// disable sys_Start
					#ifdef NCSIM
					jtag->FPGA_set_systime_ctrl(0);
					#else
					comm_ptr->trigger_systime(curr_ip,false);
					#endif
					
					cout << "From trace FIFO received:" << endl;	
					int i=0;				
					while((!jtag->FPGA_empty_pulsefifo())) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						cout << "Received Neuron number:\t" << dec << ((jtag_recpulse>>15) & 0x3f);
						cout << " from HICANN\t" << dec << ((jtag_recpulse>>24) & 0x7);
						cout << " and channel\t" << dec << ((jtag_recpulse>>21) & 0x7);
						cout << " at time:\t" << dec << (jtag_recpulse & 0x7fff) << endl;
					}
					
					jtag->FPGA_reset_tracefifo();
					nc->nc_reset();
					spc->write_cfg(0x0ff00);
				} break;

				case 'm':{
					uint period;
					uint64_t jtag_recpulse = 0;

					cout << "Enter BEG period in clock cycles: " << flush;
					cin >> period;

					spc->write_cfg(0x0ff00); //turn off DNC input to prevent in from interpreting it as config-packet
					
					//setup merger tree
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					nc->nc_reset(); //reset neuron control
					nc->beg_configure(8, false, period);
					nc->set_seed(42);
					nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					
					//setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x00<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					
					nc->beg_on(0);
					nc->beg_on(1);
					nc->beg_on(2);
					nc->beg_on(3);
					nc->beg_on(4);
					nc->beg_on(5);
					nc->beg_on(6);
					nc->beg_on(7);
					cout << "Neuron Control configuration: " << endl;
					nc->print_config();
					
					spc->write_cfg(0x000ff); //turn on DNC input
					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(7-hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(7-hicann_nr,(3<<6)+addr,0); // l1 channel 3
						
					//start experiment
					jtag->FPGA_reset_tracefifo();
					jtag->FPGA_enable_tracefifo();

			        #ifdef NCSIM
                    wait(10,SC_MS);
                    #else
					usleep(1000000);
					#endif

					jtag->FPGA_disable_tracefifo();
					spc->write_cfg(0x0ff00); //turn off DNC input to prevent it from interpreting it as config-packet
					
   					nc->beg_off(8); //turn all BEGs off
                    
                    std::vector<uint> pls_count(8,0);
                    std::vector<uint> time_vals(8,0);
                    std::vector<uint> last_time_ch(8,0);
                    uint last_time = 0;
                    uint pls_total = 0;
                    int time_total = 0;
					uint rechicann, recnum, recchan, rectime;
					while(!(jtag->FPGA_empty_pulsefifo())) {
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						rechicann = (jtag_recpulse>>24) & 0x7;
						recchan = (jtag_recpulse>>21) & 0x7;
						recnum = (jtag_recpulse>>15) & 0x3f;
						rectime = jtag_recpulse & 0x7fff;

						int dt_ch = 0;
						if (pls_count[recchan] > 0)
                        {
                        	dt_ch = (rectime+0x8000-last_time_ch[recchan]) & 0x3fff;
                            time_vals[recchan] += dt_ch;
                        }
                        last_time_ch[recchan] = rectime;
                        pls_count[recchan] += 1;
                        
                        int dt = 0;
                        if (pls_total > 0)
                        {
                        	if (last_time-rectime < 8000) // pulses unsorted by one clk
                            {
                            	dt = rectime-last_time;
                                time_total -= last_time-rectime;
                            }
                            else
                            {
	                        	dt = (rectime+0x8000-last_time) & 0x3fff;
								time_total += dt;
                            }
						}
                        ++pls_total;
                        
                        
                        last_time = rectime;
                        
						cout << "Received: from channel " << recchan << " Number " << recnum << " At time " << rectime << ", dt: " << dt << ", dt on channel: " << dt_ch << endl;
					}
					
                    for (uint nchan=0;nchan<8;++nchan)
                    {
                    	cout << "Channel " << nchan << ": received " << pls_count[nchan] << " pulses";
                        if (pls_count[nchan] > 0)
                        	cout << ", mean dt: " << (float)(time_vals[nchan])/pls_count[nchan];
                           
                        cout << endl;
					}

					cout << "Total pulse count: " << pls_total << ", total time: " << time_total << ", mean inter-spike interval: " << (float)(time_total)/(pls_total-1) << endl;
                    
					jtag->FPGA_reset_tracefifo();
				} break;

				case 'M':{
					uint period;
					bool bg0, bg1, bg2, bg3, bg4, bg5, bg6, bg7;
					uint64_t jtag_recpulse = 0;

					cout << "Enter BEG period in clock cycles: " << flush;
					cin >> period;
					//~ cout << "Enter 8 booleans to switch 8 BEGs on or off: " << flush;
					//~ cin >> bg0 >> bg1 >> bg2 >> bg3 >> bg4 >> bg5 >> bg6 >> bg7;
					
					//setup merger tree
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					nc->nc_reset(); //reset neuron control
					nc->beg_configure(8, false, period);
					nc->set_seed(42);
					nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					
					spc->write_cfg(0x000ff);
					
					//setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x00<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					
					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(7-hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(7-hicann_nr,(3<<6)+addr,0); // l1 channel 3
						
					//start experiment
					jtag->FPGA_reset_tracefifo();
                    uint off_beg = 4;
					//for (uint i=0; i<8; i++)
                    //{
                    //	if (i != off_beg)
		            //        nc->beg_on(i); //turn all BEGs on one-by-one
					//}
					nc->beg_on(0);
					nc->print_config();
					nc->beg_on(1);
					nc->print_config();
					nc->beg_on(2);
					nc->print_config();
					nc->beg_on(3);
					nc->print_config();
					nc->beg_on(4);
					nc->print_config();
					nc->beg_on(5);
					nc->print_config();
					nc->beg_on(6);
					nc->print_config();
					nc->beg_on(7);

					jtag->FPGA_enable_tracefifo();

			        #ifdef NCSIM
                    wait(10,SC_MS);
                    #else
					usleep(1000000);
					#endif

					jtag->FPGA_disable_tracefifo();
					
   					for (uint i=0; i<8; i++) nc->beg_off(i); //turn all BEGs off
                    
                    std::vector<uint> pls_count(8,0);
                    std::vector<uint> time_vals(8,0);
                    std::vector<uint> last_time_ch(8,0);
                    uint last_time = 0;
                    uint pls_total = 0;
                    int time_total = 0;
					uint rechicann, recnum, recchan, rectime;
					while(!(jtag->FPGA_empty_pulsefifo())) {
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						rechicann = (jtag_recpulse>>24) & 0x7;
						recchan = (jtag_recpulse>>21) & 0x7;
						recnum = (jtag_recpulse>>15) & 0x3f;
						rectime = jtag_recpulse & 0x7fff;

						int dt_ch = 0;
						if (pls_count[recchan] > 0)
                        {
                        	dt_ch = (rectime+0x8000-last_time_ch[recchan]) & 0x3fff;
                            time_vals[recchan] += dt_ch;
                        }
                        last_time_ch[recchan] = rectime;
                        pls_count[recchan] += 1;
                        
                        int dt = 0;
                        if (pls_total > 0)
                        {
                        	if (last_time-rectime < 8000) // pulses unsorted by one clk
                            {
                            	dt = rectime-last_time;
                                time_total -= last_time-rectime;
                            }
                            else
                            {
	                        	dt = (rectime+0x8000-last_time) & 0x3fff;
								time_total += dt;
                            }
						}
                        ++pls_total;
                        
                        
                        last_time = rectime;
                        
						cout << "Received: from channel " << recchan << " Number " << recnum << " At time " << rectime << ", dt: " << dt << ", dt on channel: " << dt_ch << endl;
					}
					
                    for (uint nchan=0;nchan<8;++nchan)
                    {
                    	cout << "Channel " << nchan << ": received " << pls_count[nchan] << " pulses";
                        if (pls_count[nchan] > 0)
                        	cout << ", mean dt: " << (float)(time_vals[nchan])/pls_count[nchan];
                           
                        cout << endl;
					}

					cout << "Total pulse count: " << pls_total << ", total time: " << time_total << ", mean inter-spike interval: " << (float)(time_total)/(pls_total-1) << endl;
                    
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x0ff00);
				} break;

				case 'n':{
					uint min_denmem, max_denmem;
					cout << "Enter denmem range" << endl;
					cout << "First denmem: " << flush;
					cin >> min_denmem;
					cout << "Last denmem: " << flush;
					cin >> max_denmem;

					nbc->clear_sram();
					nbc->write_sram();
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,1,1);
					nbc->setNeuronGladapt(0,0,0,0);

					for(uint denmem=min_denmem; denmem<=max_denmem; denmem++){
						nbc->enable_firing(denmem); 
						nbc->set_spl1address(denmem, (denmem)%64);
					}

					//~ nbc->print_sram(std::cout);
					nbc->write_sram();

					nbc->setNeuronReset(1,1);
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);

					configure_mergers();
					spc->write_cfg(0x000ff);
					
					// setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x00<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					
					//clear routing memory
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
					for(int addr=0;addr<64;++addr)
						jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3
					
					jtag->FPGA_enable_tracefifo();
					jtag->FPGA_disable_tracefifo();
					
					uint64_t jtag_recpulse;
					uint rechicann, recnum, recchan, rectime;
					while(!(jtag->FPGA_empty_pulsefifo())) {
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						rechicann = (jtag_recpulse>>24) & 0x7;
						recchan = (jtag_recpulse>>21) & 0x7;
						recnum = (jtag_recpulse>>15) & 0x3f;
						rectime = jtag_recpulse & 0x7fff;
						cout << "Received: from channel \t" << recchan << " Number \t" << recnum << " At time \t" << rectime << endl;
					}
					
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x0ff00);
				} break;
				
				case 'd':{
					uint nnumber1, stopnumber, nnumber2=0;
					vector < vector <uint> > result(64); //outer vector: sent numbers, inner vector: received numbers
					for (uint i=0; i<64; i++) {
						result[i].clear();
						result[i].resize(64);
					}
					
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //only BEGs 6, 7 active
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //merging channels (6+7)->6
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					vector<uint> times(3,0);
					vector<uint> nnums(3,0);					

					stringstream ss;
					string filename;
					fstream file;
	
					cout << "Enter number of events for statistics: " << flush;
					cin >> stopnumber;

					lcl->reset();
					rca[rc_l]->reset();
					rca[rc_r]->reset();
					rca[rc_tl]->reset();
					rca[rc_bl]->reset();
					
					lcl->reset();
					lcr->reset();
					lctl->reset();
					nc->nc_reset();
					
					//simulating crosstalk
					for (int i=0; i<64; i++) rca[rc_tl]->conf_repeater(i, RepeaterControl::TESTOUT, RepeaterControl::INWARDS, true);
					rca[rc_tl]->tdata_write(0,0,21,10); //sending 21 every 10 cycles to enforce largest crosstalk
					rca[rc_tl]->tdata_write(0,1,21,10);
					rca[rc_tl]->tdata_write(0,2,21,10);
					rca[rc_tl]->tdata_write(1,0,21,10);
					rca[rc_tl]->tdata_write(1,1,21,10);
					rca[rc_tl]->tdata_write(1,2,21,10);
					rca[rc_tl]->startout(0);
					rca[rc_tl]->startout(1);
						
					nc->merger_set(merg, enable, select, slow);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					
					rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					lcl->switch_connect(9, 4, 1);
					
					//accumulate experimental data
					for (nnumber1=1; nnumber1<64; nnumber1++){
						nc->beg_configure(6, 0, 2000);
						nc->beg_configure(7, 0, 100);
						nc->beg_set_number(6, nnumber2);
						nc->beg_set_number(7, nnumber1);
						nc->beg_on(6); //sends 0
						usleep(3000);  //locking
						nc->beg_on(7); //sends nnumber1
						usleep(6000);  //stabilizing (event "34" shows up without this)

						cout << endl << endl; //space for output
						uint count=0, loop=0, stopflag=0;
						while (loop<stopnumber){
							loop+=3;
							readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 2, times, nnums, true);
							//outer vector: sent numbers, inner vector: received numbers
							result[nnumber1][nnums[0]]++;
							result[nnumber1][nnums[1]]++;
							result[nnumber1][nnums[2]]++;
							
							cout << "\033[" << count+1 << "A"; //go back count+1 lines
							cout << "Transmitting number " << nnumber1 << ". Accumulated events: " << endl;
							count=0;
							for (uint i=0; i<64; i++){
								if (result[nnumber1][i]){
									cout << "Number \t" << i << ":\t" << result[nnumber1][i] << endl;
									count++;
								}
							}
						}
					}
					
					//calculating error rates
					vector <double> errate (64,0.0);
					for (uint u=0; u<64; u++){ //zeros are not errors!
						errate[u]=1.0-(((double)result[u][u])/((double)(accumulate(result[u].begin(), result[u].end(), 0)-result[u][0])));
					}
					
					cout << "Writing output file" << endl;
					ss.str("");
					ss <<"../results/l1_bitflips/results.dat";
					filename = ss.str();
					file.open(filename.c_str(), fstream::app | fstream::out);
					
					file << "Results columnwise: sent number patern and what was received, (decimal, binary, and event number). Last is the error rate." << endl;
					for (uint u=0; u<64; u++){ //print error rates
						file << setw(2) << u << ":\t" << bino(u,6) << ":\tpttrn.\t|\t";
					}
					file << endl;
					
					for (uint s=0; s<64; s++){
						for (uint i=0; i<64; i++){
							bool fillflag=false;
							for (uint j=0; j<64; j++){
								if ((result[i][j]) && (!fillflag)) {
									file << setw (2) << j << ":\t" << bino(j,6) << ":\t" << setw(6) << result[i][j] << "\t|\t";
									result[i][j]=0;
									fillflag=true;
								}
							}
							if (!fillflag) file << "   \t       \t      \t|\t";
						}
						uint flag=0;
						for (uint g=0; g<64; g++) flag+=accumulate(result[g].begin(), result[g].end(), 0);
						if (!flag) s=64; //empty array => interrupt
						file << endl;
					}
					
					for (uint u=0; u<64; u++){ //print error rates
						file << "ER:\t" << setw(6) << setprecision(4) << errate[u] << "\t      \t|\t";;
					}
					
					file << "\n" << flush;
					file.flush(); file.close();
				} break;

				case 'D':{
					uint period, nnumber1, nnumber2=0;
					vector <uint> result(64,0);
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //only BEGs 6, 7 active
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //merging channels (6+7)->6
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);
					
					cout << "Enter the neuron number to test: " << flush;
					cin >> nnumber1;
					cout << "Enter period between events: " << flush;
					cin >> period;

					nc->nc_reset();
					nc->merger_set(merg, enable, select, slow);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output

					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

					lcl->reset();
					lcr->reset();
					lctl->reset();
					lcl->switch_connect(9, 4,1);
					
					rca[rc_bl]->reset();

					nc->beg_configure(6, 0, 2000);
					nc->beg_configure(7, 0, period);
					nc->beg_set_number(6, nnumber2);
					nc->beg_set_number(7, nnumber1);
					nc->beg_on(6); //sends 0
					usleep(3000);  //locking
					nc->beg_on(7); //sends nnumber1
					usleep(6000);  //stabilizing (event "34" shows up without this)
					
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					
					cout << endl << endl; //space for output
					bool stopflag=false;
					uint count=0;
					while (!stopflag){
						readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 2, times, nnums, true);
						result[nnums[0]]++;
						result[nnums[1]]++;
						result[nnums[2]]++;
						
						cout << "\033[" << count+1 << "A"; //go back count+1 lines
						cout << "Accumulated events: " << endl;
						count=0;
						for (uint i=0; i<64; i++){
							if (result[i]){
								cout << "Number \t" << i << ":\t" << result[i] << endl;
								count++;
							}
						}
						
						if(kbhit()){ // if keyboard has input pending, break from loop
							getch(); // "resets" kbhit by removing a key from the buffer
							stopflag=true;
						}
					}
				} break;
				
				case 'c':{
					uint period, nnumber[7], errors=0;
					vector <uint> result(64,0), numevents;
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; //slow only for DNC merger
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1}; //all BEGs active
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //merging all channels to chan.4
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					cout << "Enter period between events: " << flush;
					cin >> period;

					nc->nc_reset();
					nc->merger_set(merg, enable, select, slow);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output

					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

					lcl->reset();
					lcr->reset();
					lctl->reset();
					lcl->switch_connect(25,12,1);
					
					rca[rc_bl]->reset();
					
					srand(time(NULL));
					for (uint i=0; i<7; i++) nnumber[i]=rand()%64;
					
					nc->beg_configure(8, 0, period); //all BEGs
					nc->beg_set_number(0, 0);
					nc->beg_set_number(1, nnumber[0]);
					nc->beg_set_number(2, nnumber[1]);
					nc->beg_set_number(3, nnumber[2]);
					nc->beg_set_number(4, nnumber[3]);
					nc->beg_set_number(5, nnumber[4]);
					nc->beg_set_number(6, nnumber[5]);
					nc->beg_set_number(7, nnumber[6]);
					nc->beg_on(0); //sends 0
					nc->beg_on(1); 
					nc->beg_on(2); 
					nc->beg_on(3); 
					nc->beg_on(4); 
					nc->beg_on(5); 
					nc->beg_on(6); 
					nc->beg_on(7); 
					usleep(6000);  //locking
					
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					
					time_t last_time=time(NULL); //every 2 seconds, new number is generated
					uint last_generated=0; //BEG number of last change
					numevents.clear(); //number of currently transmitted events
					
					cout << endl << endl; //space for output
					bool stopflag=false;
					uint count=0;
					while (!stopflag){
						if ((time(NULL)-last_time)>=2){ //generate new number
							numevents.clear();
							last_time=time(NULL);
							last_generated++;
							nnumber[last_generated%7]=rand()%64;
							nc->beg_set_number(last_generated%7+1, nnumber[last_generated%7]);
							usleep(6000); //time for the new numbers to replace old ones
						}
						readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 6, times, nnums, true);
						result[nnums[0]]++;
						result[nnums[1]]++;
						result[nnums[2]]++;
						
						for (uint i=0; i<3; i++){
							bool numeventflag=false;
							for (uint j=0; j<numevents.size(); j++)
								if (numevents[j]==nnums[i]) numeventflag=true;
							if (!numeventflag) numevents.push_back(nnums[i]);
						}
						
						for (uint i=0; i<3; i++){
							bool fail=true;
							if (nnums[i]==0) fail=false; //number 0 is generally allowed
							else 
								for (uint j=0; j<7; j++)
									if (nnumber[j]==nnums[i]) fail=false; //if number is not currently transmitted => error
							if (fail) errors++;
						}

						cout << "\033[" << count+4 << "A"; //go back count+4 lines
						cout << "Accumulated events: " << endl;
						count=0;
						for (uint i=0; i<64; i++){
							if (result[i]){
								cout << "Number " << setw(2) << i << ":  " << result[i] << "                               " << endl;
								count++;
							}
						}
						cout << "Currently transmitting " << numevents.size() << " different numbers              " << endl;
						cout << "Total events: " << accumulate(result.begin(), result.end(), 0) << "                             " << endl;
						cout << "Total errors: " << errors << "                 " << endl;
						
						if(kbhit()){ // if keyboard has input pending, break from loop
							getch(); // "resets" kbhit by removing a key from the buffer
							stopflag=true;
						}
					}
				} break;
				
				case 'C':{
					uint period, nnumber1=0, nnumber2=0, nnumber3=0, nnumber4=0, nnumber5=0, nnumber6=0, nnumber7=0;
					vector <uint> result(64,0);
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}; //slow only for DNC merger
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1}; //all BEGs active
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //merging all channels to chan.4
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);
					
					cout << "Enter 7 neuron numbers to test: " << flush;
					cin >> nnumber1 >> nnumber2 >> nnumber3 >> nnumber4 >> nnumber5 >> nnumber6 >> nnumber7;
					cout << "Enter period between events: " << flush;
					cin >> period;

					nc->nc_reset();
					nc->merger_set(merg, enable, select, slow);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output

					rca[rc_l]->reset();
					rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

					lcl->reset();
					lcr->reset();
					lctl->reset();
					lcl->switch_connect(25,12,1);
					
					rca[rc_bl]->reset();

					nc->beg_configure(8, 0, period); //all BEGs
					nc->beg_set_number(0, 0);
					nc->beg_set_number(1, nnumber1);
					usleep(rand()%200);
					nc->beg_set_number(2, nnumber2);
					usleep(rand()%200);
					nc->beg_set_number(3, nnumber3);
					usleep(rand()%200);
					nc->beg_set_number(4, nnumber4);
					usleep(rand()%200);
					nc->beg_set_number(5, nnumber5);
					usleep(rand()%200);
					nc->beg_set_number(6, nnumber6);
					usleep(rand()%200);
					nc->beg_set_number(7, nnumber7);
					nc->beg_on(0); //sends 0
					nc->beg_on(1); 
					nc->beg_on(2); 
					nc->beg_on(3); 
					nc->beg_on(4); 
					nc->beg_on(5); 
					nc->beg_on(6); 
					nc->beg_on(7); 
					usleep(6000);  //locking
					
					vector<uint> times(3,0);
					vector<uint> nnums(3,0);
					
					cout << endl << endl; //space for output
					bool stopflag=false;
					uint count=0;
					while (!stopflag){
						readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 6, times, nnums, true);
						result[nnums[0]]++;
						result[nnums[1]]++;
						result[nnums[2]]++;
						
						cout << "\033[" << count+1 << "A"; //go back count+1 lines
						cout << "Accumulated events: " << endl;
						count=0;
						for (uint i=0; i<64; i++){
							if (result[i]){
								cout << "Number \t" << i << ":\t" << result[i] << endl;
								count++;
							}
						}
						
						if(kbhit()){ // if keyboard has input pending, break from loop
							getch(); // "resets" kbhit by removing a key from the buffer
							stopflag=true;
						}
					}
				} break;

				case 'u':{
					uint stopnumber, delay;
					vector < vector <uint> > result(64); //outer vector: sent numbers, inner vector: received numbers
					
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					vector<uint> times(3,0);
					vector<uint> nnums(3,0);					

					stringstream ss;
					string filename;
					fstream file;
	
					cout << "Enter number of events for statistics: " << flush;
					cin >> stopnumber;
					cout << "Enter delay between single events: " << flush;
					cin >> delay;

					spc->write_cfg(0x0ff00); //turn off DNC input to prevent in from interpreting it as config-packet

					rca[rc_l]->reset();
					rca[rc_r]->reset();
					rca[rc_tl]->reset();
					rca[rc_bl]->reset();
					rca[rc_tr]->reset();
					rca[rc_br]->reset();
					
					lcl->reset();
					lcr->reset();
					lctl->reset();
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					nc->nc_reset();
					nc->merger_set(merg, enable, select, slow);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					nc->beg_configure(8, 0, 2000);
					nc->beg_on(8); //all BEGs fire to keep DLLs locked
					usleep(10000); //locking

					//setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x00<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					spc->write_cfg(0x0ffff); //turn on DNC input
					
					for(uint sending_rep=0; sending_rep<32; sending_rep+=4){ ///looping over sending repeaters
						cout << "Testing with sending repeater " << sending_rep << endl;
						rca[rc_l]->reset();
						rca[rc_l]->conf_spl1_repeater(sending_rep, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //turn on
						
						///looping over bottom left receiving repeaters
						for(uint receiving_rep=sending_rep/2; receiving_rep<64; receiving_rep+=16){
							cout << "Testing with receiving repeater: bottom left " << receiving_rep << endl;
							lcl->switch_connect(sending_rep*2+1, receiving_rep*2, true); //switch on
							rca[rc_bl]->reset();
							
							//accumulate experimental data
							for (uint i=0; i<64; i++) { //reser result vector
								result[i].clear();
								result[i].resize(64);
							}
							for (uint nnumber=1; nnumber<64; nnumber++){ ///looping over addresses
								uint chan=7-(sending_rep/4); //calculate channel number from sending repeater number
								jtag->FPGA_set_cont_pulse_ctrl(true, 1<<(chan&0x7), false, delay, 0x1aaaa, nnumber, nnumber, hicann_nr); //FPGA BEG
								usleep(3000);  //stabilizing
								
								cout << endl << endl << endl; //space for output
								uint count=0, loop=0, stopflag=0;
								while (loop<stopnumber){
									loop+=3;
									readout_repeater(rc_bl, RepeaterControl::OUTWARDS, receiving_rep, times, nnums, true);
									//outer vector: sent numbers, inner vector: received numbers
									result[nnumber][nnums[0]]++;
									result[nnumber][nnums[1]]++;
									result[nnumber][nnums[2]]++;
									
									cout << "\033[" << count+2 << "A"; //go back count+2 lines
									cout << "Transmitting number " << nnumber << "." << endl;
									cout << "Accumulated events: " << endl;
									count=0;
									for (uint i=0; i<64; i++){
										if (result[nnumber][i]){
											cout << "Number \t" << i << ":\t" << result[nnumber][i] << "           " << endl;
											count++;
										}
									}
								}
							}
							lcl->switch_connect(sending_rep*2+1, receiving_rep*2, false); //switch off
							
							//calculating error rates and writing a file
							vector <double> errate (64,0.0);
							uint totalevents=0, totalerrors=0;
							double totalrate=0;
							for (uint u=0; u<64; u++){ //zeros are neither errors nor real events!
								errate[u]=1.0-(((double)result[u][u])/((double)(accumulate(result[u].begin(), result[u].end(), 0))));
								totalevents+=accumulate(result[u].begin()+1, result[u].end(), 0);
								totalerrors+=accumulate(result[u].begin()+1, result[u].end(), 0)-result[u][u];
							}
							totalrate=(double)totalerrors/(double)totalevents;
							
							cout << "Writing output file" << endl << endl;
							ss.str("../results/l1_bitflips/resultsFPGAinputRaw.dat");
							file.open(ss.str().c_str(), fstream::app | fstream::out);
							
							file << "Results columnwise: sent number patern and what was received, (decimal, binary, and event number). Last is the error rate." << endl;
							file << "Following results are for sending repeater " << sending_rep << ", receiving repeater: bottom left " << receiving_rep << "." << endl;
							for (uint u=0; u<64; u++){ //print patterns
								file << setw(2) << u << ":\t" << bino(u,6) << ":\tpttrn.\t|\t";
							}
							file << endl;
							
							for (uint s=0; s<64; s++){
								for (uint i=0; i<64; i++){
									bool fillflag=false;
									for (uint j=0; j<64; j++){
										if ((result[i][j]) && (!fillflag)) {
											file << setw (2) << j << ":\t" << bino(j,6) << ":\t" << setw(6) << result[i][j] << "\t|\t";
											result[i][j]=0;
											fillflag=true;
										}
									}
									if (!fillflag) file << "   \t       \t      \t|\t";
								}
								uint flag=0;
								for (uint g=0; g<64; g++) flag+=accumulate(result[g].begin(), result[g].end(), 0);
								if (!flag) s=64; //empty array => interrupt
								file << endl;
							}
							
							for (uint u=0; u<64; u++){ //print error rates
								file << "ER:\t" << setw(6) << setprecision(4) << errate[u] << "\t      \t|\t";;
							}
							file << "\n" << flush;
							file << "Total non-zero events recorded: " << totalevents << endl << flush;
							file << "Total error rate amounts to: " << totalrate*100 << "\%.\n\n";
							file.flush(); file.close();
							
							ss.str("../results/l1_bitflips/resultsFPGAinputShort.dat");
							file.open(ss.str().c_str(), fstream::app | fstream::out);
							file << "Sending repeater " << sending_rep << ", receiving repeater: left " << receiving_rep << ". ";
							file << "Error rate: " << totalrate*100 << "\%.\n";
							file.flush(); file.close();
						}
						
						///looping over bottom right receiving repeaters (just a copy of the above code)
						for(uint receiving_rep=sending_rep/2; receiving_rep<64; receiving_rep+=16){ //same as above
							cout << "Testing with receiving repeater: bottom right " << receiving_rep << endl;
							lcr->switch_connect(sending_rep*2+1, 128+receiving_rep*2, true); //switch on
							rca[rc_br]->reset();
							
							//accumulate experimental data
							for (uint i=0; i<64; i++) { //reser result vector
								result[i].clear();
								result[i].resize(64);
							}
							for (uint nnumber=1; nnumber<64; nnumber++){ ///looping over addresses
								uint chan=7-(sending_rep/4); //calculate channel number from sending repeater number
								jtag->FPGA_set_cont_pulse_ctrl(true, 1<<(chan&0x7), false, delay, 0x1aaaa, nnumber, nnumber, hicann_nr); //FPGA BEG
								usleep(3000);  //stabilizing

								cout << endl << endl << endl; //space for output
								uint count=0, loop=0, stopflag=0;
								while (loop<stopnumber){
									loop+=3;
									readout_repeater(rc_br, RepeaterControl::OUTWARDS, receiving_rep, times, nnums, true);
									//outer vector: sent numbers, inner vector: received numbers
									result[nnumber][nnums[0]]++;
									result[nnumber][nnums[1]]++;
									result[nnumber][nnums[2]]++;
									
									cout << "\033[" << count+2 << "A"; //go back count+2 lines
									cout << "Transmitting number " << nnumber << "." << endl;
									cout << "Accumulated events: " << endl;
									count=0;
									for (uint i=0; i<64; i++){
										if (result[nnumber][i]){
											cout << "Number \t" << i << ":\t" << result[nnumber][i] << "           " << endl;
											count++;
										}
									}
								}
							}
							lcr->switch_connect(sending_rep*2+1, 128+receiving_rep*2, false); //switch off
							
							//calculating error rates and writing a file
							vector <double> errate (64,0.0);
							uint totalevents=0, totalerrors=0;
							double totalrate=0;
							for (uint u=0; u<64; u++){ //zeros are neither errors nor real events!
								errate[u]=1.0-(((double)result[u][u])/((double)(accumulate(result[u].begin()+1, result[u].end(), 0))));
								totalevents+=accumulate(result[u].begin()+1, result[u].end(), 0);
								totalerrors+=accumulate(result[u].begin()+1, result[u].end(), 0)-result[u][u];
							}
							totalrate=(double)totalerrors/(double)totalevents;
							
							cout << "Writing output file" << endl << endl;
							ss.str("../results/l1_bitflips/resultsFPGAinputRaw.dat");
							file.open(ss.str().c_str(), fstream::app | fstream::out);
							
							file << "Results columnwise: sent number patern and what was received, (decimal, binary, and event number). Last is the error rate." << endl;
							file << "Following results are for sending repeater " << sending_rep << ", receiving repeater: bottom right " << receiving_rep << "." << endl;
							for (uint u=0; u<64; u++){ //print patterns
								file << setw(2) << u << ":\t" << bino(u,6) << ":\tpttrn.\t|\t";
							}
							file << endl;
							
							for (uint s=0; s<64; s++){
								for (uint i=0; i<64; i++){
									bool fillflag=false;
									for (uint j=0; j<64; j++){
										if ((result[i][j]) && (!fillflag)) {
											file << setw (2) << j << ":\t" << bino(j,6) << ":\t" << setw(6) << result[i][j] << "\t|\t";
											result[i][j]=0;
											fillflag=true;
										}
									}
									if (!fillflag) file << "   \t       \t      \t|\t";
								}
								uint flag=0;
								for (uint g=0; g<64; g++) flag+=accumulate(result[g].begin(), result[g].end(), 0);
								if (!flag) s=64; //empty array => interrupt
								file << endl;
							}
							
							for (uint u=0; u<64; u++){ //print error rates
								file << "ER:\t" << setw(6) << setprecision(4) << errate[u] << "\t      \t|\t";;
							}
							file << "\n" << flush;
							file << "Total non-zero events recorded: " << totalevents << endl << flush;
							file << "Total error rate amounts to: " << totalrate*100 << "\%.\n\n";
							file.flush(); file.close();
							
							ss.str("../results/l1_bitflips/resultsFPGAinputShort.dat");
							file.open(ss.str().c_str(), fstream::app | fstream::out);
							file << "Sending repeater " << sending_rep << ", receiving repeater: right " << receiving_rep << ". ";
							file << "Error rate: " << totalrate*100 << "\%.\n";
							file.flush(); file.close();
						}
					}
					spc->write_cfg(0x0ff00); //turn off DNC input
   					nc->beg_off(8); //turn all BEGs off
				} break;
				
				case 'U':{
					uint stopnumber, delay;
					vector < vector <uint> > result(64); //outer vector: sent numbers, inner vector: received numbers
					
					nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
							DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
					bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
					bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
					bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};
					vector<nc_merger> merg(mer, mer+23);
					vector<bool> slow(sl, sl+23);
					vector<bool> select(se, se+23);
					vector<bool> enable(en, en+23);

					vector<uint> times(3,0);
					vector<uint> nnums(3,0);					

					stringstream ss;
					string filename;
					fstream file;
	
					double vol_low, vol_high, voh_low, voh_high, vol_curr, voh_curr, vol_steps, voh_steps, offset;

					cout << "Enter number of events for statistics: " << flush;
					cin >> stopnumber;
					cout << "Enter delay between single events: " << flush;
					cin >> delay;

					cout << "Enter lowest Vol: " << flush;
					cin >> vol_low;
					cout << "Enter highest Vol: " << flush;
					cin >> vol_high;
					cout << "Enter number_of_steps-1 for Vol: " << flush;
					cin >> vol_steps;
					cout << "Enter lowest Voh: " << flush;
					cin >> voh_low;
					cout << "Enter highest Voh: " << flush;
					cin >> voh_high;
					cout << "Enter number_of_steps-1 for Voh: " << flush;
					cin >> voh_steps;

					spc->write_cfg(0x0ff00); //turn off DNC input to prevent in from interpreting it as config-packet

					rca[rc_l]->reset();
					rca[rc_r]->reset();
					rca[rc_tl]->reset();
					rca[rc_bl]->reset();
					rca[rc_tr]->reset();
					rca[rc_br]->reset();
					
					lcl->reset();
					lcr->reset();
					lctl->reset();
					lctr->reset();
					lcbl->reset();
					lcbr->reset();

					nc->nc_reset();
					nc->merger_set(merg, enable, select, slow);
					nc->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					nc->beg_configure(8, 0, 2000);
					nc->beg_on(8); //all BEGs fire to keep DLLs locked
					usleep(10000); //locking

					//setup DNC
					dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
					uint64_t dirs = (uint64_t)0x00<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					spc->write_cfg(0x0ffff); //turn on DNC input
					
					offset=0.011; //offset on the iBoard is about 11 mV
					
					for (uint vol_i=0; vol_i<vol_steps+1; vol_i++){ ///sweeping Vol
						vol_curr=vol_low+((vol_high-vol_low)/vol_steps)*((double) vol_i);
						cout << "Setting Vol to " << vol_curr << endl;
						board.setVoltage(SBData::vol, vol_curr-offset);
						
						for (uint voh_i=0; voh_i<voh_steps+1; voh_i++){ ///sweeping Voh
							double enderrate=0;
							vector<double> errorz;
						
							voh_curr=voh_low+((voh_high-voh_low)/voh_steps)*((double) voh_i);
							cout << "Setting Voh to " << voh_curr << endl;
							board.setVoltage(SBData::voh, voh_curr-offset);
							cout << "Voltages set, proceeding with tests..." << endl;
							
							ss.str("../results/l1_bitflips/resultsVoltageSweepShort.dat");
							file.open(ss.str().c_str(), fstream::app | fstream::out);
							file << endl << "Vol: " << vol_curr << ", Voh: " << voh_curr << "." << endl;
							file.flush(); file.close();
							
							///sending repeaters 0, 12 and 28 had problems on the chip, so we shall use them
							///to see if the behavior gets better with greated deltaV / common mode voltage
							for(uint sending_rep=0; sending_rep<32; sending_rep+=28){ ///looping over sending repeaters
								cout << "Testing with sending repeater " << sending_rep << endl;
								rca[rc_l]->reset();
								rca[rc_l]->conf_spl1_repeater(sending_rep, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //turn on
								
								///looping over bottom left receiving repeaters
								for(uint receiving_rep=sending_rep/2; receiving_rep<64; receiving_rep+=16){
									cout << "Testing with receiving repeater: bottom left " << receiving_rep << endl;
									lcl->switch_connect(sending_rep*2+1, receiving_rep*2, true); //switch on
									rca[rc_bl]->reset();
									
									//accumulate experimental data
									for (uint i=0; i<64; i++) { //reser result vector
										result[i].clear();
										result[i].resize(64);
									}
									for (uint nnumber=1; nnumber<64; nnumber++){ ///looping over addresses
										uint chan=7-(sending_rep/4); //calculate channel number from sending repeater number
										jtag->FPGA_set_cont_pulse_ctrl(true, 1<<(chan&0x7), false, delay, 0x1aaaa, nnumber, nnumber, hicann_nr); //FPGA BEG
										usleep(3000);  //stabilizing
										
										cout << endl << endl << endl; //space for output
										uint count=0, loop=0, stopflag=0;
										while (loop<stopnumber){
											loop+=3;
											readout_repeater(rc_bl, RepeaterControl::OUTWARDS, receiving_rep, times, nnums, true);
											//outer vector: sent numbers, inner vector: received numbers
											result[nnumber][nnums[0]]++;
											result[nnumber][nnums[1]]++;
											result[nnumber][nnums[2]]++;
											
											cout << "\033[" << count+2 << "A"; //go back count+2 lines
											cout << "Transmitting number " << nnumber << "." << endl;
											cout << "Accumulated events: " << endl;
											count=0;
											for (uint i=0; i<64; i++){
												if (result[nnumber][i]){
													cout << "Number \t" << i << ":\t" << result[nnumber][i] << "           " << endl;
													count++;
												}
											}
										}
									}
									lcl->switch_connect(sending_rep*2+1, receiving_rep*2, false); //switch off
									
									//calculating error rates and writing a file
									vector <double> errate (64,0.0);
									uint totalevents=0, totalerrors=0;
									double totalrate=0;
									for (uint u=1; u<64; u++){ //zeros are neither errors nor real events!
										errate[u]=1.0-(((double)result[u][u])/( (double) accumulate(result[u].begin()+1, result[u].end(), 0) ) );
										totalevents+=accumulate(result[u].begin()+1, result[u].end(), 0);
										totalerrors+=accumulate(result[u].begin()+1, result[u].end(), 0)-result[u][u];
									}
									totalrate=(double)totalerrors/(double)totalevents;
									
									cout << "Writing raw output file" << endl << endl;
									ss.str("");
									ss <<"../results/l1_bitflips/resultsVoltageSweepRaw.dat";
									filename = ss.str();
									file.open(filename.c_str(), fstream::app | fstream::out);
									
									file << "Vol: " << vol_curr << ", Voh: " << voh_curr << "." << endl;
									file << "Sending repeater " << sending_rep << ", receiving repeater: " << receiving_rep << "." << endl;
									for (uint u=0; u<64; u++){ //print patterns
										file << setw(2) << u << ":\t" << bino(u,6) << ":\tpttrn.\t|\t";
									}
									file << endl;
									
									for (uint s=0; s<64; s++){
										for (uint i=0; i<64; i++){
											bool fillflag=false;
											for (uint j=0; j<64; j++){
												if ((result[i][j]) && (!fillflag)) {
													file << setw (2) << j << ":\t" << bino(j,6) << ":\t" << setw(6) << result[i][j] << "\t|\t";
													result[i][j]=0;
													fillflag=true;
												}
											}
											if (!fillflag) file << "   \t       \t      \t|\t";
										}
										uint flag=0;
										for (uint g=0; g<64; g++) flag+=accumulate(result[g].begin(), result[g].end(), 0);
										if (!flag) s=64; //empty array => interrupt
										file << endl;
									}
									
									for (uint u=0; u<64; u++){ //print error rates
										file << "ER:\t" << setw(6) << setprecision(4) << errate[u] << "\t      \t|\t";;
									}
									file << "\n" << flush;
									file << "Total non-zero events recorded: " << totalevents << endl << flush;
									file << "Total error rate amounts to: " << totalrate*100 << "\%.\n\n";
									file.flush(); file.close();
									
									cout << "Writing short output file" << endl << endl;
									ss.str("../results/l1_bitflips/resultsVoltageSweepShort.dat");
									file.open(ss.str().c_str(), fstream::app | fstream::out);
									
									file << "Sending repeater " << sending_rep << ", receiving repeater: " << receiving_rep << ". ";
									file << "Error rate: " << totalrate*100 << "\%.\n";
									file.flush(); file.close();
									
									errorz.push_back(totalrate*100);
								} ///receiving repeater loop
							} ///sending repeater loop
							enderrate=accumulate(errorz.begin(), errorz.end(), 0)/(double)errorz.size();
							ss.str("../results/l1_bitflips/resultsVoltageSweepShort.dat");
							file.open(ss.str().c_str(), fstream::app | fstream::out);
							file << "Overall error rate with Vol=" << vol_curr << ", Voh=" << voh_curr << ": " << enderrate << "\%."<< endl;
							file.flush(); file.close();
						} ///voh loop
					} ///vol loop

					spc->write_cfg(0x0ff00); //turn off DNC input
   					nc->beg_off(8); //turn all BEGs off
				} break;
				
				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};


class LteeTmAKBSReviewY1 : public Listee<Testmode>{
public:         
	LteeTmAKBSReviewY1() : Listee<Testmode>(string("tmak_bsreview_y1"),string("contains all functionality for year 1 BrainScaleS review low level demo, modified")){};
	Testmode * create(){return new TmAKBSReviewY1();};
};
LteeTmAKBSReviewY1 ListeeTmAKBSReviewY1;
