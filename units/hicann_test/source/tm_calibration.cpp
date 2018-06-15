// Company         :   kip                      
// Author          :   Marc-Olivier Schwartz
// E-Mail          :   marcoliver.schwartz@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_calibration.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jan 18 11
// Last Change     :   Tue Apr 24 12    
// by              :   marco23
//------------------------------------------------------------------------
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
#include "hicann_cfg.h"

#include <tr1/tuple>

//#define USENR         0
#define fpga_master   1
#define DATA_TX_NR    10000

using namespace facets;
using namespace std;
using namespace std::tr1;

class TmCalibration : public Testmode{

public:
	// repeater control 
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	
    double starttime;
    double clkperiod_bio_dnc;
    double clkperiod_bio_fpga;
    double playback_begin_offset;
    
	static const double clkperiod_dnc; // in seconds
	static const double clkperiod_fpga; // in seconds
	typedef unsigned int l2_channel_t;
		
	uint hicann_nr, hicann_jtag_nr;

	// hardware infrastructure
	static const uint num_reticles = 2;
	static const uint num_dncs = 2;
	static const uint num_hicanns = 2;
	static const uint src_hicann = 0;
	static const uint tgt_hicann = 1;
	
	struct pulse_float_t
	{
		unsigned int id;
		double time;
		unsigned int chan;

		pulse_float_t() :
			id(0),
			time(0.0),
			chan(0)
		{};

		pulse_float_t(unsigned int set_id, double set_time, unsigned int set_chan) :
			id(set_id),
			time(set_time),
			chan(set_chan)
		{};

		bool operator< (const pulse_float_t &p) const { return ((this->time<p.time)||((this->time==p.time)&&(this->id<p.id)));}
		bool operator==(const pulse_float_t &p) const { return ((this->time==p.time)&&(this->id==p.id));}
	};
	
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
    
    void writePulses(
			const char *filename,
			std::map< std::tr1::tuple<l2_channel_t, l2_pulse_address_t>, std::vector<double> > & spiketrains
			)
	{
		// convert spiketrains to single list, sorted by time
		std::vector<pulse_float_t> pulselist;

		std::map< std::tr1::tuple<l2_channel_t, l2_pulse_address_t>, std::vector<double> >::iterator it_st= spiketrains.begin();
		std::map< std::tr1::tuple<l2_channel_t, l2_pulse_address_t>, std::vector<double> >::iterator it_st_end = spiketrains.end();

		for (;it_st != it_st_end; ++it_st )
		{
			std::tr1::tuple<l2_channel_t, l2_pulse_address_t> curr_event = it_st->first;
			std::vector<double> & times = it_st->second;

			// ignore wafer and FPGA ids for now
			//unsigned int curr_id = ((curr_addr.dnc_id&0x3)<<12) | ((curr_addr.hicann_id&0x7)<<9) | (curr_addr.address&0x1ff);
			unsigned int curr_id = (get<1>(curr_event)).data.integer & 0x3f;
			unsigned int curr_chan = get<0>(curr_event);

			for (unsigned int npulse=0;npulse<times.size();++npulse)
				pulselist.push_back(pulse_float_t(curr_id,times[npulse],curr_chan));
		}

		std::sort(pulselist.begin(),pulselist.end());

		FILE *fout = fopen(filename, "a");
        
        for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
        	fprintf(fout,"%f\t%d\t%d\n",pulselist[npulse].time, pulselist[npulse].chan, pulselist[npulse].id);

		fclose(fout);
    }
    
    bool stop_experiment(){
		// stop experiment
		log(Logger::INFO) << "Stopping experiment";
		jtag->FPGA_disable_pulsefifo();
		jtag->FPGA_disable_tracefifo();

		return true;
	}
	bool execute_experiment(float duration_in_s)
	{
		// Start systime counter if successful
		log(Logger::INFO) << "jd_mappingmaster: Enabling systime counter";
		unsigned int curr_ip = jtag->get_ip();
		S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
		if ( (curr_ip != 0) && (comm_ptr != NULL)) // if Ethernet-JTAG available, trigger via Ethernet (without timestamp offset), else use JTAG method (with offset...)
		{
			comm_ptr->trigger_systime(curr_ip,false); // stop first -> necessary when re-initializing
			jtag->HICANN_stop_time_counter();
			jtag->HICANN_start_time_counter();
			comm_ptr->trigger_systime(curr_ip,true);
			log(Logger::INFO) << "Systime counters started synchronously.";
		}
		else
			jtag->FPGA_set_systime_ctrl(1);

		// start experiment execution
		log(Logger::INFO) << "tm_l2pulses: Start: enabling fifos";
		jtag->FPGA_reset_tracefifo();
		jtag->FPGA_enable_tracefifo();
		//~ jtag->FPGA_enable_fifoloop();

		// test: send some pulses directly
		//for (unsigned int n=0;n<5;++n)
		//    jtag->FPGA_start_pulse_event(0,0xabc0+n);

		//~ jtag->FPGA_enable_pulsefifo();

	// TODO -- wait depending on the last entry in the pulselist
	// TODO -- have a progress bar

		// wait...
		log(Logger::INFO) << "jd_mappingmaster: Running experiment for " << duration_in_s << " seconds";
		 usleep(1000000*duration_in_s);

	//					cout << "Experiment running, press ENTER to interrupt";
	//					cin.ignore();
	//					cin.ignore();
		// stop experiment
		log(Logger::INFO) << "jd_mappingmaster: Stopping experiment";
		//~ jtag->FPGA_disable_pulsefifo();
		jtag->FPGA_disable_tracefifo();

		comm_ptr->trigger_systime(curr_ip,false);
		return true;
	}
	
	/// currently, transformation of timestamps back to pulse times outputs biological times in ms
	void read_trace_pulses(
				std::map< std::tr1::tuple<l2_channel_t, l2_pulse_address_t>, std::vector<double> > & spiketrains
			)
	{
		spiketrains.clear();

		unsigned int spk_count = 0;
		unsigned int cycle_count = 0;
		unsigned int prev_tstamp = 0;
		//cout<<"Try to get pules...."<<endl;
		while((!jtag->FPGA_empty_pulsefifo()))
		{
			uint64_t rdata;
			jtag->FPGA_get_trace_fifo(rdata);


			unsigned int curr_id = (rdata>>15)&0x1ff;
			l2_channel_t curr_chan =(rdata>>21)&0x7;
			unsigned int curr_tstamp = rdata&0x7fff;

			if ( (spk_count > 0) && ( prev_tstamp > curr_tstamp+0x2000 ) ) // overflow detected (tolerates unsorted pulse times up to a quarter of the timestamp counter range)
				cycle_count += 1;

			double curr_time = this->starttime + this->clkperiod_dnc*(curr_tstamp + cycle_count*0x8000);
			prev_tstamp = curr_tstamp;


			if (spk_count < 10)
				log(Logger::INFO) << "read_trace_pulses: received data " << (unsigned long)rdata << " -> id=" << curr_id << ", tstamp=" << curr_tstamp << ", time=" << curr_time;

			l2_pulse_address_t curr_address(curr_id);

			spiketrains[std::tr1::make_tuple(curr_chan, curr_address)].push_back( curr_time );

			++spk_count;

			if ( (rdata>>63) ) // second pulse available
			{
				rdata = rdata>>32;
				curr_id = (rdata>>15)&0x1ff; // ignore HICANN ID, because this is set in testmode
				curr_chan =(rdata>>21)&0x7;
				curr_tstamp = rdata&0x7fff;

				if ( (spk_count > 0) && ( prev_tstamp > curr_tstamp+0x2000 ) ) // overflow detected (tolerates unsorted pulse times up to a quarter of the timestamp counter range)
					cycle_count += 1;

				curr_time = this->starttime + this->clkperiod_dnc*(curr_tstamp + cycle_count*0x8000);
				prev_tstamp = curr_tstamp;

				l2_pulse_address_t curr_address_2(curr_id);

				spiketrains[std::tr1::make_tuple(curr_chan, curr_address_2)].push_back( curr_time );
		    ++spk_count;
			}
		}

		log(Logger::INFO) << "read_trace_spikes: " << spk_count << " spikes from " << spiketrains.size() << " addresses found";
	}


	bool configureLayer2()
    {
			unsigned int hicann_jtag_nr = hc->addr();
			unsigned int hicann_nr      = jtag->pos_dnc-1-hicann_jtag_nr;

			// initialize
			log(Logger::INFO) << "Try Init() ..." ;
			if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
				log(Logger::INFO) << "ERROR: Init failed,abort..." << endl;
				return false;
			}
			log(Logger::INFO) << "Init() ok" << endl;

      printf("tm_calibration: Starting systime counters\n");
      unsigned int curr_ip = jtag->get_ip();
			
			S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(hc->GetCommObj());
			if (comm_ptr != NULL)
			{
	      comm_ptr->trigger_systime(curr_ip,false);
				jtag->HICANN_stop_time_counter();
				jtag->HICANN_start_time_counter();
      	comm_ptr->trigger_systime(curr_ip,true);
			}
			else
			{
			  printf("tm_l2pulses warning: bad s2comm pointer; did not initialize systime counter.\n");
			}


		// configure
        jtag->FPGA_reset_tracefifo();
        
	  nc->dnc_enable_set(1,0,1,0,1,0,1,0);
      spc->write_cfg(0x055ff);
			
			// setup DNC
      dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
	  uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
      dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

	  // enable trace
     jtag->FPGA_enable_tracefifo();

     // do your experiment here

    // stop trace
	jtag->FPGA_disable_tracefifo();

		return true;    
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
			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(1);
			fc[fg_num]->set_voltagewritetime(15);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(9);
			fc[fg_num]->set_pulselength(9);
			fc[fg_num]->set_fg_biasn(5);
			fc[fg_num]->set_fg_bias(8);
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

		jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 80, 0xaaaa, 0, 0, hicann_nr); //to lock the DLL(s)
		usleep(100000);
		jtag->FPGA_set_cont_pulse_ctrl(on, 1<<(chan&0x7), poisson, delay, 0x1aaaa, startn, stopn-startn, hicann_nr); //actual activity
	}
	
	void configure_hicann_bgen(uint on, uint delay, uint seed, bool poisson, bool receive, bool merge){
		nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
				DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
		bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
		bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
		//bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
		bool en[23] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
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

	bool test() {
		this->hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
		dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC

		hicann_jtag_nr = hc->addr();
		hicann_nr      = jtag->pos_dnc-1-hicann_jtag_nr;
		jtag->set_hicann_pos(hicann_jtag_nr);
		
		uint64_t jtagid;

		jtag->read_id(jtagid,jtag->pos_hicann);
		//cout << "HICANN ID: 0x" << hex << jtagid << endl;
		
		jtag->read_id(jtagid,jtag->pos_dnc);
		//cout << "DNC ID: 0x" << hex << jtagid << endl;
		
		jtag->read_id(jtagid,jtag->pos_fpga);
		//cout << "FPGA ID: 0x" << hex << jtagid << endl;

		//log(Logger::INFO) << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
			log(Logger::INFO) << "ERROR: Init failed,abort..." << endl;
			return false;
		}
			
		//log(Logger::INFO) << "Init() ok" << endl;

		nbc          = &hc->getNBC();
		nc           = &hc->getNC();
		fc[0]        = &hc->getFC_tl();
		fc[1]        = &hc->getFC_tr();
		fc[2]        = &hc->getFC_bl();
		fc[3]        = &hc->getFC_br();
		sc_top       = &hc->getSC_top();
		sc_bot       = &hc->getSC_bot();
		lcl          = &hc->getLC_cl();
		lcr          = &hc->getLC_cr();
		lctl         = &hc->getLC_tl();
		lcbl         = &hc->getLC_bl();
		lctr         = &hc->getLC_tr();
		lcbr         = &hc->getLC_br();
		rca[rc_l]    = &hc->getRC_cl();
		rca[rc_r]    = &hc->getRC_cr();
		rca[rc_tl]   = &hc->getRC_tl();
		rca[rc_bl]   = &hc->getRC_bl();
		rca[rc_tr]   = &hc->getRC_tr();
		rca[rc_br]   = &hc->getRC_br();
		spc          = &hc->getSPL1Control();

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

		// Configure neurons
		nbc->initzeros();

		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to neuron output line 0 and 1
		nbc->setOutputMux(6,4);
	
	
		nbc->setNeuronBigCap(1,1);

		nbc->setNeuronGl(0,0,0,0);
		nbc->setNeuronGladapt(0,0,0,0);
 
		nbc->setNeuronReset(1,1);
		
		nbc->configNeuronsGbl();

		//configure the floating gates
		setupFg();
		

		char c;
		bool cont=true;
		do{
//			cout << "you better know your options!" << endl;
//		cout << "Select test option:" << endl;
//			cout << "0: Reset everything (all rams)" << endl;
//			cout << "1: Send HICANN reset pulse (restart the testmode after reset!)" << endl;
//			cout << "2: JTAG reset" << endl;
//			cout << "3: Reset synapse drivers" << endl;
//			cout << "4: Reset synapse drivers and synapses in top half" << endl;
//			cout << "6: Turn all BEG off" << endl;
//			cout << "7: Reset all neurons" << endl;
//			cout << "8: Configure neuronbuilder MUXes manually" << endl;
//			cout << "9: Turn FPGA BEG off / cut the DNC->HICANN connection" << endl;
//			cout << "H: Write weights to all synapses" << endl;
//			cout << "f: Set PLL to 100 MHz output" << endl;
//			cout << "F: Set PLL to a custom value" << endl;
//			cout << "e: Error search in synaptic drivers" << endl;
//			cout << "s: Continously stimulate neuron" << endl;
//			cout << "c: Configure a neuron manually" << endl;
//			cout << "p: Program floating gates" << endl;
//			cout << "n: Activate single denmem" << endl;
//			cout << "l: Loopback FPGA-HICANN-FPGA" << endl;
//			cout << "L: Test FPGA BEG input on the neuron (scope needed)" << endl;
//			cout << "S: Send spike stimulus to neuron" << endl;
//			cout << "D: Activate FPGA BEG" << endl;
//			cout << "i: Activate synaptic input" << endl;
//			cout << "I: Set preout configuration for a driver" << endl;
//			cout << "j: Configure HICANN for jitter measurement (HICANNv2)" << endl;
//			cout << "r: Configure repeaters and crossbars" << endl;
//			cout << "R: Read out the neuron spikes via FPGA" << endl;
//			cout << "t: Read out repeater test input" << endl;
//			cout << "a: Activate neuron" << endl;
//			cout << "A: Activate neuron without using preout as trigger" << endl;
//			cout << "b: Turn on the BEG" << endl;
//			cout << "B: Configure BEG" << endl;
//			cout << "w: Set weight for a synapse" << endl;
//			cout << "W: Set decoder for a synapse" << endl;
//			cout << "g: Write an FG line" << endl;
//			cout << "z: Programm all floating gates to zero:" << endl;
//			cout << "x: Exit" << endl;
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

				case '6':{
					nc->beg_off(8);
				} break;

				case '7':{
					nc->beg_off(8);
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
					jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, hicann_nr); // stop the BEG
					nc->dnc_enable_set(0,0,0,0,0,0,0,0); //disable all DNC inputs
					spc->write_cfg(0x0); //disable spl1 channels
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

				case 'h':{
					uint neuron;
					//cout << "Which neuron?" << endl;
					cin >> neuron;
					
					uint delay;
					cout << "Which neuron?" << endl;
					cin >> delay;
					
					
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 2, 2);
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
					nbc->setNeuronGl(0,0,0,0);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(neuron,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					
					configure_hicann_bgen(1, delay, 200, 0, 0, 0);
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 8, 8);
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
					nbc->setNeuronGl(0,0,0,0);
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
								sc_top->drv_config(i, 0, 0, 0, 0, 0, 0, 1);
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

				case 's': {

					// cout <<"Enter peak current of square stimulus:";
					uint sval=0;
					cin >>dec>>sval;
					for(int fg_num = 0; fg_num < 4; fg_num += 1)
					{
						while(fc[fg_num]->isBusy());
			//			fc[fg_num]->initSquare(0,420);
		//				fc[fg_num]->initval(0,sval);
			//			fc[fg_num]->initSquare(0,0);	
						fc[fg_num]->initSquare(0,sval);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();

						fc[fg_num]->stimulateNeuronsContinuous(0);
					}

					cont=true;
					  } break;

				case 'q': {


					cout <<"Enter peak current of ramp stimulus:";
					uint sval=0;
					cin >>dec>>sval;
					for(int fg_num = 0; fg_num < 4; fg_num += 1)
					{
						while(fc[fg_num]->isBusy());
			//			fc[fg_num]->initSquare(0,420);
		//				fc[fg_num]->initval(0,sval);
			//			fc[fg_num]->initSquare(0,0);	
						fc[fg_num]->initRamp(0,sval);
						fc[fg_num]->set_pulselength(15);
						fc[fg_num]->write_biasingreg();

						fc[fg_num]->stimulateNeuronsContinuous(0);
					}

					cont=true;
					  } break;

				case 'S':{
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
					nbc->setNeuronGl(0,0,0,0);
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
						comm_ptr->trigger_systime(curr_ip,false);
						jtag->FPGA_reset_tracefifo();
					}
					else jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 400, 0xaaaa, 0, 0, hicann_nr);
					
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

				case 'P':{
					setupFg();
				
					int bank=0; // bank number, programming data is written to
					//write currents first as ops need biases before they work.
					log(Logger::INFO)<<"tm_jitter::writing currents ...";
					for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
						for(int fg_num = 0; fg_num < 3; fg_num += 2){	
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
						for(int fg_num = 0; fg_num < 3; fg_num += 2){	
							log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
							fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
							while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
							fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
							while(fc[fg_num]->isBusy());
							fc[fg_num]->program_cells(lineNumber,bank,1); //programm up
						}
						bank =( bank +1)%2; //bank is 1 or 0
					}

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
						fc[fg_num]->set_maxcycle(10);		
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

					bank=0; // bank number, programming data is written to
					//write currents first as ops need biases before they work.
					log(Logger::INFO)<<"tm_jitter::writing currents ...";
					for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
						for(int fg_num = 0; fg_num < 3; fg_num += 2){	
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
						for(int fg_num = 0; fg_num < 3; fg_num += 2){	
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
						
						for (int i = 1; i < 64; i++){
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
						comm_ptr->trigger_systime(curr_ip,false);
						
						cout << "From trace FIFO received:" << endl;	
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
						cout << "Last Received pulse: 0x" << hex << jtag_recpulse << endl;
						cout << "Received Neuron number: " << dec << ((jtag_recpulse>>15) & 0x3f) << endl;
						cout << "From channel: " << dec << ((jtag_recpulse>>21) & 0x7) << endl;
					}
					
					jtag->HICANN_set_test(0x0);
					
					if ((uint)((jtag_recpulse>>15)&0x3f) != (neuron&0x3f)) cout << "TESTFAIL: Packet(s) not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
					else cout << "TESTPASS: Transmission of pulse packet(s) via JTAG->HICANN(L1)->JTAG successful." << endl;
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

					jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 80, 0xaaaa, 0, 0, hicann_nr); //to lock the DLL(s)
					usleep(1000);
					jtag->FPGA_set_cont_pulse_ctrl(1, 1<<(chan&0x7), poisson, delay, 0x1aaaa, synaddr1, synaddr2-synaddr1, hicann_nr); //actual activity
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

				case 'n':{

					uint add=0;
				   
					cin>>add;
					
					uint current=0;
				   
					cin>>current;
					
					activate_neuron(add,add,current);
					cont = true;		
				}break;

				case 'I':{
					uint drv, p0, p1, p2, p3;
					cout << "Enter driver number: "; cin >> drv;
					cout << "Enter value for preout 0: "; cin >> p0;
					cout << "Enter value for preout 1: "; cin >> p1;
					cout << "Enter value for preout 2: "; cin >> p2;
					cout << "Enter value for preout 3: "; cin >> p3;
					sc_top->preout_set(drv, p0, p1, p2, p3);
				} break;

				case 'z':{
					
					for(int cntlr=0; cntlr<4;cntlr+=1)
					{
						log(Logger::INFO)<<"Init done, programming now";
						for(int i=0; i<24; i+=1){
							for(cntlr=0; cntlr<4; cntlr+=1){
								while(fc[cntlr]->isBusy());

								log(Logger::DEBUG2)<<"programing row " << i << endl;
								 
								// start state machine
								fc[cntlr]->program_cells(i,0,0);
							}
						}	
						for(int cntlr=0; cntlr<4;cntlr+=1)
						{
							fc[cntlr]->readout_cell(1,0);
						}
		
					}

				}

				break;

				case 'O':{
				uint val1, val2;
				// cout<<"Enter mux configuration for outputs: ";
				cin>>val1;
				cin>>val2;			
				nbc->setOutputMux(val1,val2);
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
								sc_top->drv_config(i,0, 0, 0, 0, 0, 0, 0);
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
		
					uint min_denmem, max_denmem;
					//cout << "Enter denmem range" << endl;
					//cout << "First denmem: " << flush;
					cin >> min_denmem;
					//cout << "Last denmem: " << flush;
					cin >> max_denmem;

					nbc->clear_sram();
					nbc->write_sram();
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,0,0);
					nbc->setNeuronGladapt(0,0,0,0);

					for(uint denmem=min_denmem; denmem<=max_denmem; denmem++){
						//	 if(!(denmem==10)) nbc->enable_firing(denmem); //number 11 does funny stuff when not calibrated
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

					std::map< std::tr1::tuple<l2_channel_t, l2_pulse_address_t>, std::vector<double> > spiketrain;
					std::map< std::tr1::tuple<l2_channel_t, l2_pulse_address_t>, std::vector<double> > spikein;

					execute_experiment(1);
					spc->write_cfg(0x00000);

					read_trace_pulses(spiketrain);
					writePulses("train", spiketrain);
					log(Logger::INFO) << "Done" << endl;
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
					uint min_denmem;
					cout << "begin: " << flush;
					cin >> min_denmem;
					
					uint max_denmem;
					cout << "end: " << flush;
					cin >> max_denmem;
					
					nbc->clear_sram();
					nbc->write_sram();
					nbc->setNeuronBigCap(1,1);
					nbc->setNeuronGl(0,0,0,0);
					nbc->setNeuronGladapt(0,0,0,0);
					
					for(uint denmem=min_denmem; denmem<=max_denmem; denmem++){
						//	 if(!(denmem==10)) nbc->enable_firing(denmem); //number 11 does funny stuff when not calibrated
						nbc->enable_firing(denmem); 
						nbc->set_spl1address(denmem, (denmem)%64);
					}
					//~ nbc->print_sram(std::cout);
					nbc->write_sram();

					nbc->setNeuronReset(1,1);
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					cout << "done" << endl;
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
				
				case 'L':{
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
					nbc->setNeuronGl(0,0,0,0);
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
						comm_ptr->trigger_systime(curr_ip,false);
						jtag->FPGA_reset_tracefifo();
					}
					else jtag->FPGA_set_cont_pulse_ctrl(1, 0x1, 0, 400, 0xaaaa, 0, 0, hicann_nr);
				} break;

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

const double TmCalibration::clkperiod_dnc = 4.0e-9;  // in seconds
const double TmCalibration::clkperiod_fpga = 8.0e-9; // in seconds

class LteeTmCalibration : public Listee<Testmode>{
public:         
	LteeTmCalibration() : Listee<Testmode>(string("tm_calibration"),string("Cali")){};
	Testmode * create(){return new TmCalibration();};
};
LteeTmCalibration ListeeTmCalibration;
