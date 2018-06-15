// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_hardwaretest.cpp                
// Project Name    :   p_brainscales
// Subproject Name :   s_hicann2            
//                    			
// Create Date     :   May 29 12
// Last Change     :   May 29 12    
// by              :   akononov
//------------------------------------------------------------------------
#include "common.h"
#include "testmode.h" //Testmode and Lister classes
#include "s2c_jtagphys_2fpga.h" //jtag-fpga library

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

#define fpga_master   1

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

class TmHardwareTest : public Testmode{

public:
	//repeater locations
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	enum fc_loc {fc_tl=0, fc_bl, fc_tr, fc_br}; //FGControl locations
	static const int TAG = 0;

	uint hicann_nr, hicann_jtag_nr;

	HicannCtrl *hc; 
	FGControl *fc[4];
	NeuronBuilderControl *nbc;
	NeuronControl *nc;
	RepeaterControl *rca[6]; //array of the 6 repeater blocks
	SynapseControl *sc_top, *sc_bot;
	L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;
	
	//DNC stuff
	SPL1Control *spc;
	DNCControl  *dc;
	FPGAControl *fpc;

	//generate an SPL1 event (27 bits) for HICANN 0
	uint64_t gen_spl1(uint hicann, uint channel, uint number, uint time){
		return ((hicann & 0x7) << 24) | ((channel & 0x7) << 21) | ((number & 0x3f) << 15) | (time & 0x7fff);
	}
	
	//generate an FPGA event packet (64 bits)
	uint64_t gen_fpga(uint64_t event1, uint64_t event2){
		return 0x8000000000000000 | (event2 << 32) | event1;
	}
    
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
	
	//sets the repeater frequency (use 2,1 to get 100 MHz)
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
	}
	
	void setupFG(){
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
	
	void programFG(){
		int bank=0; // bank number, programming data is written to
		//write currents first as ops need biases before they work.
		cout << "tm_hardwaretest::writing currents" << flush;
		for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
			cout << "." << flush;
			for(int fg_num = 0; fg_num < 4; fg_num += 1){	
				fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
				while(fc[fg_num]->isBusy());
				fc[fg_num]->program_cells(lineNumber,bank,1); //programm up
			}
			bank =(bank +1)%2; //bank is 1 or 0
		}
		cout << endl << "tm_hardwaretest::writing voltages" << flush;
		for(uint lineNumber = 0; lineNumber < 24; lineNumber += 2){
			cout << "." << flush;
			for(int fg_num = 0; fg_num < 4; fg_num += 1){	
				fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
				while(fc[fg_num]->isBusy());
				fc[fg_num]->program_cells(lineNumber,bank,1); //programm up
			}
			bank =(bank +1)%2; //bank is 1 or 0
		}
		cout << endl;
	}

	bool test_switchram(uint runs) {
		int mem_size;
		ci_data_t rcvdata;
		ci_addr_t rcvaddr;

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

	bool initConnectionFPGA(){
		//Reset Setup to get defined start point
		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1);
		//jtag->DNC_lvds_pads_en(~(0x100+(1<<hicann_nr)));
		
		//Stop all HICANN channels and start DNCs FPGA channel
		jtag->DNC_start_link(0x100);
		//Start FPGAs DNc channel
		jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x100);

		unsigned char state1 = 0;
		uint64_t state2 = 0;
		unsigned int break_count = 0;

		printf("Start FPGA-DNC initialization: \n");

		while ((((state1 & 0x1) == 0)||((state2 & 0x1) == 0)) && break_count<10) {
			usleep(900000);
			jtag->DNC_read_channel_sts(8,state1);
			printf("DNC Status = 0x%02hx\t" ,state1 );

			jtag->FPGA_get_status(state2);
			printf("FPGA Status = 0x%02hx RUN:%i\n" ,(unsigned char)state2,break_count);
			++break_count;
		}
		
		if (break_count == 10) {
			this->dnc_shutdown();
			this->hicann_shutdown();
			jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
			printf ("ERROR in FPGA-DNC initialization --> EXIT\n");
			return false;
		}
		state1 = 0;
		state2 = 0;
		break_count = 0;

		printf("Start DNC-HICANN initialization: \n");
		jtag->DNC_set_lowspeed_ctrl(0xff); // ff = lowspeed
		jtag->DNC_set_speed_detect_ctrl(0x00); // 00 = no speed detect
		jtag->HICANN_set_link_ctrl(0x061);
        
		while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && break_count < 10) {
			jtag->HICANN_stop_link();
			jtag->DNC_stop_link(0x0ff);
			jtag->HICANN_start_link();
			jtag->DNC_start_link(0x100 + 0xff);

			usleep(90000);
			jtag->DNC_read_channel_sts(hicann_nr,state1);
			printf("DNC Status = 0x%02hx\t" ,state1 );
			jtag->HICANN_read_status(state2);
			printf("HICANN Status = 0x%02hx\n" ,(unsigned char)state2 );
			++break_count;
		}

		if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41)) {
			printf("Sucessfull init DNC-HICANN connection \n");
		} else {
			printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,(unsigned char)state2);
			this->dnc_shutdown();
			this->hicann_shutdown();
			jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
			return 0;
		}

		printf("Successfull FPGA-DNC-HICANN initialization\n");
		return true;
	}

	void dnc_shutdown(){
		if(!jtag){
			log(Logger::ERROR) << "Stage2Comm::dnc_shutdown: Object jtag not set, abort!";
			exit(EXIT_FAILURE);
		}
		jtag->DNC_set_GHz_pll_ctrl(0xe);
		jtag->DNC_stop_link(0x1ff);
		jtag->DNC_set_lowspeed_ctrl(0x00);
		jtag->DNC_set_loopback(0x00);
		jtag->DNC_lvds_pads_en(0x1ff);
		jtag->DNC_set_speed_detect_ctrl(0x0);
		jtag->DNC_set_GHz_pll_ctrl(0x0);
	}

	void hicann_shutdown(){
		if(!jtag){
			log(Logger::ERROR) << "Stage2Comm::hicann_shutdown: Object jtag not set, abort!";
			exit(EXIT_FAILURE);
		}
		jtag->HICANN_set_reset(1);
		jtag->HICANN_set_GHz_pll_ctrl(0x7);
		jtag->HICANN_set_link_ctrl(0x141);
		jtag->HICANN_stop_link();
		jtag->HICANN_lvds_pads_en(0x1);
		jtag->HICANN_set_reset(0);
		jtag->HICANN_set_GHz_pll_ctrl(0x0);
	}


	bool runFpgaBenchmark(unsigned int runtime_s) {
		bool failflag=false;
		uint64_t read_data_dnc = 0;
		unsigned char read_crc_dnc = 0;
		unsigned char read_crc_dnc_dly = 0;
		unsigned int total_crc_dnc = 0;
		uint64_t read_data_fpga = 0;
		unsigned char read_crc_fpga = 0;
		unsigned char read_crc_fpga_dly = 0;
		unsigned int total_crc_fpga = 0;
		printf("tm_l2bench: Starting benchmark transmissions\n");
		
		// test DNC loopback
		jtag->DNC_set_loopback(0xff); // DNC loopback
		// setup DNC
		dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
		uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
		dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

		//jtag->FPGA_set_cont_pulse_ctrl(enable, channel, poisson?, delay, seed, nrn_start, char nrn_end, hicann_nr)
		jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 20, 100, 1, 15, hicann_nr);

		unsigned int starttime = (unsigned int)time(NULL);
		unsigned int runtime = (unsigned int)time(NULL);
		
		printf("BER FPGA: %e\tError FPGA: %i\t BER DNC: %e\tError DNC: %i\t Time:(%i:%02i:%02i)",(double(total_crc_fpga)/(double(100))),
					total_crc_fpga,(double(total_crc_dnc)/(double(100))),total_crc_dnc,(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60);
		// wait...
		#ifdef NCSIM
			for(unsigned i=0;i<runtime_s;++i) {
				wait(100,SC_US);
				runtime = (unsigned int)time(NULL);

				//Read DNC crc counter
				jtag->DNC_set_channel(8+hicann_nr);
				jtag->DNC_read_crc_count(read_crc_dnc);
				jtag->DNC_get_rx_data(read_data_dnc);
				if(read_crc_dnc != read_crc_dnc_dly) {
					total_crc_dnc += ((unsigned int)read_crc_dnc+0x100-(unsigned int)read_crc_dnc_dly)&0xff;
				}
				read_crc_dnc_dly = read_crc_dnc;

				//Read FPGA crc counter
				jtag->FPGA_get_crc(read_crc_fpga);
				jtag->FPGA_get_rx_data(read_data_fpga);
				if(read_crc_fpga != read_crc_fpga_dly) {
					total_crc_fpga += ((unsigned int)read_crc_fpga+0x100-(unsigned int)read_crc_fpga_dly)&0xff;
				}
				read_crc_fpga_dly = read_crc_fpga;

				printf("BER FPGA: %.1e\tError FPGA: %i\t BER DNC: %.1e\tError DNC: %i\t Time:(%i:%02i:%02i) Data (FPGA:0x%0X)(DNC:0x%0X)\n",(double(total_crc_fpga)/(double(50000*i+1)*double(100))),
					total_crc_fpga,(double(total_crc_dnc)/(double(50000*i)*double(100))),total_crc_dnc,(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_fpga),static_cast<unsigned int>(read_data_dnc));
				fflush(stdout);
				if (total_crc_fpga || total_crc_dnc) failflag=true;
			}
		#else
			int i=0;
			while((runtime-starttime)< runtime_s) {
				++i;
				usleep(1000);
				runtime = (unsigned int)time(NULL);

				//Read DNC crc counter
				jtag->DNC_set_channel(8+hicann_nr);
				jtag->DNC_read_crc_count(read_crc_dnc);
				jtag->DNC_get_rx_data(read_data_dnc);
				if(read_crc_dnc != read_crc_dnc_dly) {
					total_crc_dnc += ((unsigned int)read_crc_dnc+0x100-(unsigned int)read_crc_dnc_dly)&0xff;
				}
				read_crc_dnc_dly = read_crc_dnc;

				//Read FPGA crc counter
				jtag->FPGA_get_crc(read_crc_fpga);
				jtag->FPGA_get_rx_data(read_data_fpga);
				if(read_crc_fpga != read_crc_fpga_dly) {
					total_crc_fpga += ((unsigned int)read_crc_fpga+0x100-(unsigned int)read_crc_fpga_dly)&0xff;
				}
				read_crc_fpga_dly = read_crc_fpga;
				
				if ((i%50) == 0) {
					printf("\rBER FPGA: %.1e Error FPGA: %i BER DNC: %.1e Error DNC: %i Time:(%i:%02i:%02i) Data (FPGA:0x%08X)(DNC:0x%08X)",
						(double(total_crc_fpga)/(double(6710886*(runtime-starttime)*128))),	total_crc_fpga,
						(double(total_crc_dnc)/(double(6710886*(runtime-starttime)*128))),total_crc_dnc,
						(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
						static_cast<unsigned int>(read_data_fpga),static_cast<unsigned int>(read_data_dnc));
					fflush(stdout);
				}
				if ((runtime_s%5000) == 0) printf("\n");
				if (total_crc_fpga || total_crc_dnc) failflag=true;
			}
		#endif
		
		// stop experiment
		printf("\ntm_l2bench: Stopping benchmark transmissions\n");
		jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
		
		//results:
		printf("*** Result FPGA-DNC connection BER ***\n");
		printf("FPGA: BER : %.1e\t Error : %i\n",(double(total_crc_fpga)/(double(6710886*runtime_s*128))), total_crc_fpga);
		printf("DNC : BER : %.1e\t Error : %i\n",(double(total_crc_dnc)/(double(6710886*runtime_s*128))), total_crc_dnc);
		
		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 0;
		}
		jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC FPGAif delay values:\n");
		jtag->printDelayFPGA(vec_out);
		return failflag;
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
	
	bool there_are_spikes(uint repeater, bool silent){
		vector<uint> times1(3,0), times2(3,0), nnums1(3,0), nnums2(3,0);
		readout_repeater(rc_l, RepeaterControl::INWARDS, repeater, times1, nnums1, silent);
		readout_repeater(rc_l, RepeaterControl::INWARDS, repeater, times2, nnums2, silent);
		bool flag=false;
		for (int n=0; n<3; n++){
			if (times1[n]!=times2[n]) flag=true;
		}
		return flag;
	}
	
	bool test() {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::INFO) << "ERROR: object 'chip' in testmode not set, abort" << flush;
			return true;
		}
		if (!jtag) {
		 	log(Logger::INFO) << "ERROR: object 'jtag' in testmode not set, abort" << flush;
			return true;
		}
		
		hc  = (HicannCtrl*)  chip[FPGA_COUNT+DNC_COUNT+0];
		dc  = (DNCControl*)  chip[FPGA_COUNT]; // use DNC
		fpc = (FPGAControl*) chip[0]; // use FPGA
		
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		nbc        = &hc->getNBC();
		nc         = &hc->getNC();
		fc[fc_tl]  = &hc->getFC_tl();
		fc[fc_tr]  = &hc->getFC_tr();
		fc[fc_bl]  = &hc->getFC_bl();
		fc[fc_br]  = &hc->getFC_br();
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
		
		hicann_jtag_nr = hc->addr();
		hicann_nr = jtag->pos_dnc-1-hicann_jtag_nr;
		
		cout << "Pulse FPGA reset..." << endl;
		unsigned int curr_ip = jtag->get_ip();
		if (comm != NULL) {
			comm->set_fpga_reset(curr_ip, true, true, true, true, true); // set reset
			usleep(100000); // wait: optional, to see LED flashing - can be removed
			comm->set_fpga_reset(curr_ip,false, false, false, false, false); // release reset
		} else {
			printf("tm_l2pulses warning: bad s2comm pointer; did not perform soft FPGA reset.\n");
		}

		cout << "Reset JTAG..." << endl;
		jtag->reset_jtag();

		cout << "Pulse DNC/HICANN reset..." << endl;
		jtag->FPGA_set_fpga_ctrl(1);
		
		log(Logger::INFO) << "Try Init() ..." << Logger::flush;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
			log(Logger::INFO) << "ERROR: Init failed, abort..." << flush;
			return true;
		}
	 	log(Logger::INFO) << "Init() ok" << flush;

		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << dec << endl;
	
		setupFG(); //setup floating gate controller

		char c;
		uint mode=0; //not chosen yet
		
		cout << "Select test option:" << endl;
		cout << "0: Reset all RAMs in HICANN and verify" << endl;
		cout << "s: Switch RAM test" << endl;
		cout << "r: HICANN-BEG and Repeater function test" << endl << endl;
		cout << "GBit must be enabled for the following (-bje2f option)" <<endl;
		cout << "k: Loopback FPGA-DNC-FPGA" << endl;
		cout << "l: Loopback FPGA-HICANN-FPGA: single pulse" << endl;
		cout << "m: Loopback FPGA-HICANN-FPGA: FPGA BEG" << endl;
		cout << "n: Loopback FPGA-HICANN-FPGA: FIFO" << endl << endl;
		cout << "Proper xml-file required for the following (-c ../conf/akononov/FGparam_concatenation_test.xml option)" <<endl;
		cout << "i: Verify synaptic input, internal BEG" << endl;
		cout << "x: Exit" << endl;
		cin >> c;
		
		switch(c){
			case '0':{
				ci_addr_t addr=0;
				ci_data_t data=0;
				bool failflag=false;
				cout << "Resetting everything" << endl;
				cout << "Resetting crossbars and switches" << endl;
				lcl->reset();
				for (uint i=0; i<64; i++){
					lcl->read_cfg(i);
					lcl->get_read_cfg(addr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
				lcr->reset();
				for (uint i=0; i<64; i++){
					lcr->read_cfg(i);
					lcr->get_read_cfg(addr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
				
				lctl->reset();
				for (uint i=0; i<112; i++){
					lctl->read_cfg(i);
					lctl->get_read_cfg(addr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
				
				lctr->reset();
				for (uint i=0; i<112; i++){
					lctr->read_cfg(i);
					lctr->get_read_cfg(addr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
									
				lcbl->reset();
				for (uint i=0; i<112; i++){
					lcbl->read_cfg(i);
					lcbl->get_read_cfg(addr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
				
				lcbr->reset();
				for (uint i=0; i<112; i++){
					lcbr->read_cfg(i);
					lcbr->get_read_cfg(addr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
									
				cout << "Resetting repeater blocks" << endl;
				rca[rc_l]->reset();
				for (uint i=0; i<32; i++) if (rca[rc_l]->read_data(i)) failflag=true;
				if (failflag) {cout << "Failed" << endl; return true;}
									
				rca[rc_tl]->reset();
				for (uint i=0; i<64; i++) if (rca[rc_tl]->read_data(i)) failflag=true;
				if (failflag) {cout << "Failed" << endl; return true;}
									
				rca[rc_tr]->reset();
				for (uint i=0; i<64; i++) if (rca[rc_tr]->read_data(i)) failflag=true;
				if (failflag) {cout << "Failed" << endl; return true;}
									
				rca[rc_bl]->reset();
				for (uint i=0; i<64; i++) if (rca[rc_bl]->read_data(i)) failflag=true;
				if (failflag) {cout << "Failed" << endl; return true;}
									
				rca[rc_br]->reset();
				for (uint i=0; i<64; i++) if (rca[rc_br]->read_data(i)) failflag=true;
				if (failflag) {cout << "Failed" << endl; return true;}
									
				rca[rc_r]->reset();
				for (uint i=0; i<32; i++) if (rca[rc_r]->read_data(i)) failflag=true;
				if (failflag) {cout << "Failed" << endl; return true;}
									
				cout << "Resetting neuron builder" << endl;
				nbc->initzeros();
				ci_addr_t naddr = 0;
				for (uint i=0; i<512; i++){
					nbc->read_data(i);
					nbc->get_read_data(naddr, data);
					if (!(data==0xbfffc0 || data==0x17fffc0 || data==0x1400000)) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
									
				cout << "Resetting neuron control" << endl;
				nc->nc_reset();
				for (uint i=0; i<20; i++){
					nc->read_data(i);
					nc->get_read_data(naddr, data);
					if (data) failflag=true;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
									
				cout << "Resetting synapse drivers and top synapses" << flush;
				vector<uint> datavec(32,0);
				sc_top->reset_all();
				for (uint i=0; i<224; i++){
					sc_top->read_row(i, datavec, false); //weights
					for (uint j=0; j<32; j++) if (datavec[j]) failflag=true;
					if (!i%8) cout << "." << flush;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
				for (uint i=0; i<224; i++){
					sc_top->read_row(i, datavec, true); //decoder
					for (uint j=0; j<32; j++) if (datavec[j]) failflag=true;
					if (!i%8) cout << "." << flush;
				}
				cout << endl;
				if (failflag) {cout << "Failed" << endl; return true;}
				
				cout << "Resetting synapse drivers and bottom synapses" << flush;
				sc_bot->reset_all();
				for (uint i=0; i<224; i++){
					sc_bot->read_row(i, datavec, false); //weights
					for (uint j=0; j<32; j++) if (datavec[j]) failflag=true;
					if (!i%8) cout << "." << flush;
				}
				if (failflag) {cout << "Failed" << endl; return true;}
				for (uint i=0; i<224; i++){
					sc_bot->read_row(i, datavec, true); //decoder
					for (uint j=0; j<32; j++) if (datavec[j]) failflag=true;
					if (!i%8) cout << "." << flush;
				}
				cout << endl;
				if (failflag) {cout << "Failed" << endl; return true;}
			} return false; //Success

			case 's':{
				bool result;
				cout << "Testing Switch RAM... " << flush;
				result = test_switchram(2);
				return !result;
			}

			case 'r':{
				set_pll(4,1);

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
				
				configure_hicann_bgen(1, 330, 200, 0, 0, 0);
				
				lcl->switch_connect(1,0,1);
				usleep(10000);
				cout << "Events after the BEG:" << endl;
				readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
				readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
				readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
			
				if(there_are_spikes(28, 0)) {cout << "BEG test successful" << endl; return false;}
				else {cout << "BEG Test failed" << endl; return true;}
			}

			case 'k':{
				bool result=true;
				if (!initConnectionFPGA()){
					cout << "Stop FPGA benchmark : INIT ERROR" << endl;
					return true;
				}
				unsigned int runtime_s = 10; // sec
				result=runFpgaBenchmark(runtime_s);
				
				//Shutdown interfaces
				dnc_shutdown();
				hicann_shutdown();
				jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
				cout << "Finished benchmark of FPGA-DNC connection" << endl;
				return result;
			}
			
			case 'l':{
				if(!mode){
					cout << "Testing Loopback in single pulse mode" << endl;
					mode=1;
				}
			}
			
			case 'm':{
				if(!mode){
					cout << "Testing Loopback in BEG mode" << endl;
					mode=2;
				}
			}
			
			case 'n':{
				if(!mode){
					cout << "Testing Loopback in FIFO mode" << endl;
					mode=3;
				}
				
				uint neuron=21;
				uint64_t jtag_recpulse = 0;
				bool result, failflag=false;
				
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
				//clear routing memory
				for(int addr=0;addr<64;++addr)
					jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
				for(int addr=0;addr<64;++addr)
					jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3
				
				/// fill the fifo and start it
				if(mode==3) {
					uint64_t event;
					cout << "current HICANN JTAG ID: " << 7-hicann_nr << endl;
					cout << "current HICANN DNC ID: " << hicann_nr << endl;
					
					jtag->FPGA_reset_tracefifo();
					
					for (int i = 1; i < 1000; i++){
						event=gen_spl1(hicann_nr, 0, neuron, 150*2*i);
						jtag->FPGA_fill_playback_fifo(false, 200, hicann_nr, event, 0);
					}
					cout << endl << "Last event written: " << hex << event << dec << endl;
					cout << "Starting systime counters..." << endl;
					unsigned int curr_ip = jtag->get_ip();
					S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
					if (comm_ptr != NULL){
						comm_ptr->trigger_systime(curr_ip,false);
						jtag->HICANN_stop_time_counter();
						jtag->HICANN_start_time_counter();
						comm_ptr->trigger_systime(curr_ip,true);
					}
					else  printf("tm_l2pulses warning: bad s2comm pointer; did not initialize systime counter.\n");
					// start experiment execution
					cout << "Starting experiment..." << endl;
					jtag->FPGA_enable_tracefifo();
					jtag->FPGA_enable_pulsefifo();
					usleep(10000);
					// stop experiment
					jtag->FPGA_disable_pulsefifo();
					jtag->FPGA_disable_tracefifo();
					
					// disable sys_Start
					comm_ptr->trigger_systime(curr_ip,false);
					uint nnum=0, hic=0, chan=0;
					
					uint i=0;
					while(!jtag->FPGA_empty_pulsefifo()) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						nnum=(jtag_recpulse>>15) & 0x3f;
						hic=(jtag_recpulse>>24) & 0x7;
						chan=(jtag_recpulse>>21) & 0x7;
						if (nnum!=neuron) failflag=true;
					}
					if (nnum!=neuron) failflag=true;
					cout << i << " events recieved" << endl;
					
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x0ff00);
				}
				
				/// activate background generator
				if(mode==2) {
					uint channel=0, period=500;
					jtag->FPGA_reset_tracefifo();
					jtag->FPGA_set_cont_pulse_ctrl(1, 1<<channel, 0, period, 0, neuron, neuron, hicann_nr);
					jtag->FPGA_enable_tracefifo();
					usleep(1000);
					jtag->FPGA_disable_tracefifo();
					jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
					uint nnum=0, hic=0, chan=0;
					
					uint i=0;
					while(!jtag->FPGA_empty_pulsefifo()) {
						i++;
						jtag->FPGA_get_trace_fifo(jtag_recpulse);
						nnum=(jtag_recpulse>>15) & 0x3f;
						hic=(jtag_recpulse>>24) & 0x7;
						chan=(jtag_recpulse>>21) & 0x7;
						if (nnum!=neuron) failflag=true;
					}
					if (nnum!=neuron) failflag=true;
					cout << i << " events recieved" << endl;
					
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x0ff00);
				}
				/// send single pulse
				if(mode==1) {
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
					uint nnum=(jtag_recpulse>>15) & 0x3f;
					if (nnum!=neuron) failflag=true;
				}
				
				jtag->HICANN_set_test(0x0);
				
				
				if (failflag) {cout << "Test failed" << endl; return true;}
				else{cout << "Test successful" << endl;	return false;}
			}

			case 'i':{
				uint neuron=0;
				
				set_pll(2,1);
				setupFG();
				programFG();
				set_pll(3,1);
				
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
				rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
				
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
				
				configure_hicann_bgen(1, 340, 200, 0, 1, 0);
				
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
				
				usleep(2000000);
				nbc->setOutputEn(false,false);
				
				if(there_are_spikes(28, 0)) {cout << "BEG test successful" << endl;}
				else {cout << "BEG Test failed" << endl; return true;}
				
				if(there_are_spikes(0, 0)) {cout << "Test successful" << endl; return false;}
				else {cout << "Test failed" << endl; return true;}
			}

			default :{return true;}
		}
	return true;
	};
};


class LteeTmHardwareTest : public Listee<Testmode>{
public:         
	LteeTmHardwareTest() : Listee<Testmode>(string("tm_hardwaretest"), string("Hardware tests with return values")){};
	Testmode * create(){return new TmHardwareTest();};
};
LteeTmHardwareTest ListeeTmHardwareTest;
