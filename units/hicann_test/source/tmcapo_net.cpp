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

//#define USENR         0
#define fpga_master   1
#define DATA_TX_NR    10000

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

class TmCapoNet : public Testmode{

public:
	// repeater control 
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations
	
    double starttime;
    double clkperiod_bio_dnc;
    double clkperiod_bio_fpga;
    double playback_begin_offset;
		
	static const double clkperiod_dnc; // in seconds
	static const double clkperiod_fpga; // in seconds
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


	void writePulses(
			const char *filename,
			std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
			)
	{
		// convert spiketrains to single list, sorted by time
		std::vector<pulse_float_t> pulselist;

		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st= spiketrains.begin();
		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st_end = spiketrains.end();

		for (;it_st != it_st_end; ++it_st )
		{
			l2_pulse_address_t curr_addr = it_st->first;
			std::vector<double> & times = it_st->second;

			// ignore wafer and FPGA ids for now
			//unsigned int curr_id = ((curr_addr.dnc_id&0x3)<<12) | ((curr_addr.hicann_id&0x7)<<9) | (curr_addr.address&0x1ff);
			unsigned int curr_id = curr_addr.data.integer & 0x3fff;

			for (unsigned int npulse=0;npulse<times.size();++npulse)
				pulselist.push_back(pulse_float_t(curr_id,times[npulse]));
		}

		std::sort(pulselist.begin(),pulselist.end());

		FILE *fout = fopen(filename, "w");
        
        for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
        	fprintf(fout,"%f\t%d\n",pulselist[npulse].time*10000, pulselist[npulse].id);

		fclose(fout);
    }

	/// currently, transformation of timestamps back to pulse times outputs biological times in ms
	void read_trace_pulses(
				std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
			)
	{
		spiketrains.clear();

		unsigned int spk_count = 0;
		unsigned int cycle_count = 0;
		unsigned int prev_tstamp = 0;
		cout<<"Try to get pules...."<<endl;
		while((!jtag->FPGA_empty_pulsefifo()))
		{
			uint64_t rdata;
			jtag->FPGA_get_trace_fifo(rdata);


			unsigned int curr_id = (rdata>>15)&0x1ff;
			unsigned int curr_tstamp = rdata&0x7fff;

			if ( (spk_count > 0) && ( prev_tstamp > curr_tstamp+0x2000 ) ) // overflow detected (tolerates unsorted pulse times up to a quarter of the timestamp counter range)
				cycle_count += 1;

			double curr_time = this->starttime + this->clkperiod_dnc*(curr_tstamp + cycle_count*0x8000);
			prev_tstamp = curr_tstamp;


			if (spk_count < 10)
				log(Logger::INFO) << "read_trace_pulses: received data " << (unsigned long)rdata << " -> id=" << curr_id << ", tstamp=" << curr_tstamp << ", time=" << curr_time;

			l2_pulse_address_t curr_address(curr_id);

			spiketrains[curr_address].push_back( curr_time );

			++spk_count;

			if ( (rdata>>63) ) // second pulse available
			{
				rdata = rdata>>32;
				curr_id = (rdata>>15)&0x1ff; // ignore HICANN ID, because this is set in testmode
				curr_tstamp = rdata&0x7fff;

				if ( (spk_count > 0) && ( prev_tstamp > curr_tstamp+0x2000 ) ) // overflow detected (tolerates unsorted pulse times up to a quarter of the timestamp counter range)
					cycle_count += 1;

				curr_time = this->starttime + this->clkperiod_dnc*(curr_tstamp + cycle_count*0x8000);
				prev_tstamp = curr_tstamp;

				l2_pulse_address_t curr_address_2(curr_id);

				spiketrains[curr_address_2].push_back( curr_time );
		    ++spk_count;
			}
		}

		log(Logger::INFO) << "read_trace_spikes: " << spk_count << " spikes from " << spiketrains.size() << " addresses found";
	}

	void write_playback_pulses(
			std::map< l2_pulse_address_t, std::vector<double> > spiketrains  //!< an input spike train in hw time
		)
	{

		// setup pulse channels
		jtag->FPGA_reset_tracefifo();

		double playback_begin_offset = 500e-9; // in seconds, offset time of between start of experiment and first pulse

		double fpgahicann_delay = 400e-9; // in seconds, denotes the transmission delay FPGA-HICANN

		// convert spiketrains to single list, sorted by time
		std::vector<pulse_float_t> pulselist;

		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st= spiketrains.begin();
		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st_end = spiketrains.end();

	    for (;it_st != it_st_end; ++it_st )
	    {
			l2_pulse_address_t curr_addr = it_st->first;
			std::vector<double> & times = it_st->second;
		// ignore all IDs from HICANN upwards, because used only for single HICANN experiments - ID is set in testmode
			//unsigned int curr_id = ((this->hicann_nr&0x7)<<9) | (curr_addr.address&0x1ff);
			unsigned int curr_id = curr_addr.data.integer & 0xfff ;

			for (unsigned int npulse=0;npulse<times.size();++npulse)
				pulselist.push_back(pulse_float_t(curr_id,times[npulse]));
	    }

		std::sort(pulselist.begin(),pulselist.end());

		log(Logger::DEBUG0) << "tmcapo_net::write_playback_pulses: Send " << pulselist.size() << " pulses:";
		for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
			log(Logger::INFO) << "  id=" << pulselist[npulse].id << ", time=" <<pulselist[npulse].time;


		if (pulselist.size())
		{
			this->starttime = pulselist[0].time - fpgahicann_delay - playback_begin_offset; // always start first pulse at clk cycle 0 + offset
			int prev_reltime = 1; // account for delay of 1 clk during playback fifo release
			unsigned int discarded_count = 0;

			for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
			{
				double curr_time = pulselist[npulse].time - starttime;
				int curr_cycle = (int)((curr_time - fpgahicann_delay) / this->clkperiod_fpga + 0.5);
				int curr_tstamp = (int)(fpgahicann_delay / this->clkperiod_dnc + 0.5); // write only FPGA-HICANN delay, because FPGA release time is added in playback FIFO

				if ((prev_reltime+2 > curr_cycle) && (npulse>0)) // ensure minimum of two clk cycles between release times
				{
					curr_cycle = prev_reltime + 2;
					if (curr_cycle > curr_tstamp) // release time too late for pulse to reach DNC before timestamp expires -> throw away
					{
						++discarded_count;
						continue;
					}
				}

				unsigned int hicann_id = (pulselist[npulse].id >> 9) & 0x7;
				unsigned int neuron_id = pulselist[npulse].id & 0x1ff;
				unsigned int pulse_data = (hicann_id<<24) | (neuron_id << 15) | (curr_tstamp & 0x7fff );
				unsigned int extra_pulse = 0;

				log(Logger::DEBUG0) << "tmcapo_net: fill data: FPGA delta time: "<< curr_cycle-prev_reltime << ", HICANN " << hicann_id << ", pulse: " << pulse_data << ", extra: " << extra_pulse;

				jtag->FPGA_fill_playback_fifo(false, (curr_cycle-prev_reltime)<<1, hicann_id, pulse_data, extra_pulse);

				prev_reltime = curr_cycle;
			}

		}

	}
	bool execute_experimentcont()
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
		//jtag->FPGA_enable_tracefifo();
		jtag->FPGA_enable_fifoloop();

		// test: send some pulses directly
		//for (unsigned int n=0;n<5;++n)
		//    jtag->FPGA_start_pulse_event(0,0xabc0+n);

		jtag->FPGA_enable_pulsefifo();

	// TODO -- wait depending on the last entry in the pulselist
	// TODO -- have a progress bar

		// wait...
		log(Logger::INFO) << "Running experiment";
		return true;
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
		//jtag->FPGA_enable_tracefifo();
		jtag->FPGA_enable_fifoloop();

		// test: send some pulses directly
		//for (unsigned int n=0;n<5;++n)
		//    jtag->FPGA_start_pulse_event(0,0xabc0+n);

		jtag->FPGA_enable_pulsefifo();

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
		jtag->FPGA_disable_pulsefifo();
		jtag->FPGA_disable_tracefifo();

		comm_ptr->trigger_systime(curr_ip,false);
		return true;
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


	bool test() {
		this->hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
		dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC

		hicann_jtag_nr = hc->addr();
		hicann_nr      = jtag->pos_dnc-1-hicann_jtag_nr;
		jtag->set_hicann_pos(hicann_jtag_nr);
		
		uint64_t jtagid;

		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;
		
		jtag->read_id(jtagid,jtag->pos_dnc);
		cout << "DNC ID: 0x" << hex << jtagid << endl;
		
		jtag->read_id(jtagid,jtag->pos_fpga);
		cout << "FPGA ID: 0x" << hex << jtagid << endl;

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

		set_pll(2,1); //pll to 100 MHz as default value

		nbc->setOutputMux(1,4); //set default do be ready for neuron 0 and preout
		
		nbc->setOutputEn(true,true);

		
		for(uint denmem=0; denmem<32; denmem++){//only first first(last) spl1
		//	 if(!(denmem==10)) nbc->enable_firing(denmem); //number 11 does funny stuff when not calibrated
			 nbc->enable_firing(denmem); 
			 nbc->set_spl1address(denmem, denmem+1);
			 /*if(denmem%8<4)
				 nbc->set_spl1address(denmem,2);
			 else
				 nbc->set_spl1address(denmem,0);*/
		}
		nbc->write_sram();
		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "0: Reset everything (all rams)" << endl;
			cout << "1: Send HICANN reset pulse (restart the testmode after reset!)" << endl;
			cout << "2: JTAG reset" << endl;
		//	cout << "3: Set PLL frequency on the HICANN" << endl;
			cout << "4: Test switch memory of currently selected HICANN" << endl;
		//	cout << "5: Loopback test with FIFO/BG/Rx" << endl;
			cout << "6: Turn FPGA BEG off" << endl;
			cout << "7: Turn HICANN BEG off" << endl;
			cout << "f: Test FPGA BEG input on the neuron (scope needed)" << endl;
			cout << "h: Test inner-HICANN neuron input (scope needed)" << endl;
			cout << "F: Adjust FPGA BEG" << endl;
			cout << "H: Adjust HICANN BEG" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "P: Spike out" << endl;
			cout << "c: programm local neuron builder parameters" << endl;
			cout << "C: config global neuron parameter" << endl;
			cout << "n: set dedicated neuron to output" << endl;
			cout << "a: create network and activate l1" << endl;
			cout << "O: Set Output Mux values on HICANN" << endl;
		//	cout << "o: Deactivate analog outputs on HICANN" << endl;
			cout << "r: ARQ reset (NOT AVAIABLE ANYMORE!)" << endl;
			cout << "s: stop experiment" << endl;
			cout << "S: Set synapse parameters" <<endl;
		//	cout << "l: Loopback-channel collision check" << endl;
			cout << "u: soft FPGA reset (via Ethernet)" << endl;
			cout << "x: Exit" << endl;
			cin >> c;
			
			switch(c){

				case '0':{
					cout << "resetting everything" << endl;
					cout << "resetting synapses" << endl;
					sc_top->reset_all();
					sc_bot->reset_all();
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
					set_pll(2,1); //set pll to 100 MHz
				} break;

				case '2':{
					cout << "resetting test logic" << endl;
					jtag->reset_jtag();
					set_pll(2,1); //set pll to 100 MHz
					nbc->setOutputMux(1,4); //set default do be ready for neuron 0 and preout
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
				
				case '6':{
					jtag->FPGA_reset_tracefifo();
					jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, hicann_nr);
					spc->write_cfg(0x0ff00);
				} break;
				
				case '7':{
					nc->beg_off(8);
				} break;

				case '8':{
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

					neuron = 43;
					
					/// fill the fifo and start it
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
					
					jtag->HICANN_set_test(0x0);
					
					if ((uint)((jtag_recpulse>>15)&0x3f) != (neuron&0x3f)) cout << "TESTFAIL: Packet(s) not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
					else cout << "TESTPASS: Transmission of pulse packet(s) via JTAG->HICANN(L1)->JTAG successful." << endl;
				} break;
				case 'f':{
					vector<uint> weights(32,0xffffffff), address(32,0); //one row, all weights max, decoder 0
					bool loopfifo = false;
					uint neuron = 0;
					uint64_t event;
					
					//cout << "Which neuron?" << endl;
					//cin >> neuron;
					//cout << "For loopfifo enter 1, for BEG enter 0: ";
					//cin >> loopfifo;
					
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
					

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 8, 8);
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
					nbc->setNeuronBigCap(0,0);
					nbc->setNeuronGl(0,0,0,0);
					nbc->setNeuronGladapt(0,0,0,0);
					nbc->setNeuronReset(1,1);

					activate_neuron(neuron,0,0);
					
					nbc->setSpl1Reset(0); //this may be not necessary
					nbc->setSpl1Reset(1);
					nbc->configNeuronsGbl();
					
					configure_hicann_bgen(1, 500, 200, 0, 0, 0);
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 0, 0, 8, 8);
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
				/*	if (poisson) {
						cout << "Enter seed: ";
						cin >> seed;
						for(int i=0; i<8; i++){
							nc->beg_off(i);
							nc->set_seed(seed+5*i);
							nc->beg_on(i);
						}
					}*/
				}break;

				case 'p':{
					setupFg();

					int bank=0; // bank number, programming data is written to
					//write currents first as ops need biases before they work.
					log(Logger::INFO)<<"tm_capo::writing currents ...";
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
					log(Logger::INFO)<<"tm_capo::writing voltages ...";
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
//use shorter programming time for second write
					/*
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						fc[fg_num]->set_maxcycle(10);		
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
					//Programming a second time
					log(Logger::INFO)<<"tm_capo::programming a second time ...";

					for( uint lineNumber = 0; lineNumber < 24; lineNumber += 1){ //starting at line 1 as this is the first current!!!!!
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
					}*/
				}break;
				case 'A':{ //send spikes from file using fpga 
					
					vector<uint> weights(32,0xffffffff), address(32,0x44444444); //one row, all weights max, decoder 0
					uint linecount=0, maxlockpulse;
					std::string buffer;
				

					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->configure();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
				
					
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 1, 1, 8, 8);
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

					cout << "HICANN number: "  << hicann_nr <<endl;
					nc->nc_reset();
					
					//configure_hicann_bgen(1, 1000, 0xaa, 0, 0, 0); //start BEG
					nc->dnc_enable_set(1,1,1,1,1,1,1,1);
					spc->write_cfg(0x0ffff);

/*
					//configure_hicann_bgen(1, 300, 0xaa, 0, 0, 0); //start BEG
				//	nc->nc_reset();
					nc->loopback_off(8);	
					//nc->loopback_on(0,1); //return pulses to dnc for debugging
					jtag->FPGA_reset_tracefifo();
					dc->setTimeStampCtrl(0x0);	
					spc->write_reset();
				
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
*/
					std::map< l2_pulse_address_t, std::vector<double> > spikein;


					spikein[l2_pulse_address_t(4)] = std::vector<double>(500,1e-5);
					spikein[l2_pulse_address_t(0)] = std::vector<double>(500,1e-5);
					
				/*	for (int i = 1; i < 200; i++){ //DLL locking events
						spikein[l2_pulse_address_t(0)][linecount]=0.000001*i;
						cout<<spikein[l2_pulse_address_t(0)][linecount]<<endl;
						linecount++;
					} */
					///read in the input spike data
					cout << "Reading spikes.txt file..." << endl;
					uint times[16000];
					ifstream file("spikes.txt");
					while(!file.eof()){ //read in the spike times (in FPGA cycles)
						getline(file,buffer);
						spikein[l2_pulse_address_t(4)][linecount]=atof(buffer.c_str())/10000 +0.0002;
						cout<<spikein[l2_pulse_address_t(4)][linecount]<<endl;
						linecount++;
					}
					file.close();

					std::map< l2_pulse_address_t, std::vector<double> > spiketrain;

				//	spikein[u2_pulse_address_t(10)] = std::vector<double>(100,1e-5);
					write_playback_pulses(spikein);
					execute_experimentcont();
					
				//	spikein[l2_pulse_address_t(10)] = std::vector<double>(100,1e-5);
				//	write_playback_pulses(spikein);

					//read_trace_pulses( spiketrain);
				//	cout<<spiketrain;
					//writePulses("train", spiketrain);

				} break;
				
				case '9':{ //send spikes from file using fpga 
					
					uint linecount=0, maxlockpulse;
					std::string buffer;
					
					//configure_hicann_bgen(1, 300, 0xaa, 0, 0, 0); //start BEG
					nc->nc_reset();
					nc->loopback_off(8);	
					nc->loopback_on(0,1); //return pulses to dnc for debugging
					jtag->FPGA_reset_tracefifo();
					dc->setTimeStampCtrl(0x0);	
					nc->dnc_enable_set(1,0,1,0,1,0,1,0);
					spc->write_cfg(0x055ff);
					spc->write_reset();
				
					//rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)

					uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
					dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
					std::map< l2_pulse_address_t, std::vector<double> > spikein;
					spikein[l2_pulse_address_t(2)] = std::vector<double>(500,1e-5);
					spikein[l2_pulse_address_t(0)] = std::vector<double>(500,1e-5);
					
					for (int i = 1; i < 200; i++){ //DLL locking events
						spikein[l2_pulse_address_t(0)][linecount]=0.000001*i;
						cout<<spikein[l2_pulse_address_t(0)][linecount]<<endl;
						linecount++;
					}
					///read in the input spike data
					cout << "Reading spikes.txt file..." << endl;
					uint times[16000];
					ifstream file("spikes.txt");
					while(!file.eof()){ //read in the spike times (in FPGA cycles)
						getline(file,buffer);
						spikein[l2_pulse_address_t(2)][linecount]=atof(buffer.c_str())/10000 +0.002;
						cout<<spikein[l2_pulse_address_t(2)][linecount]<<endl;
						linecount++;
					}
					file.close();

					std::map< l2_pulse_address_t, std::vector<double> > spiketrain;

				//	spikein[l2_pulse_address_t(10)] = std::vector<double>(100,1e-5);
					write_playback_pulses(spikein);
					//sc_top->configure();
					execute_experiment(1);
					
				//	spikein[l2_pulse_address_t(10)] = std::vector<double>(100,1e-5);
				//	write_playback_pulses(spikein);

					read_trace_pulses( spiketrain);
				//	cout<<spiketrain;
					writePulses("train", spiketrain);

				} break;
				case 'P':{ // Print spikes
					nc->loopback_off(8);	
					jtag->FPGA_reset_tracefifo();
					spc->write_cfg(0x7eff);
					dc->setTimeStampCtrl(0x0);	
					dc->setDirection(0x0);
					nc->dnc_enable_set(0,0,0,0,0,0,0,0);
					spc->write_reset();
				

					std::map< l2_pulse_address_t, std::vector<double> > spiketrain;
					std::map< l2_pulse_address_t, std::vector<double> > spikein;

				//	spikein[l2_pulse_address_t(10)] = std::vector<double>(100,1e-5);
				//	write_playback_pulses(spikein);
					execute_experiment(1);
					
				//	spikein[l2_pulse_address_t(10)] = std::vector<double>(100,1e-5);
				//	write_playback_pulses(spikein);

					read_trace_pulses( spiketrain);
				//	cout<<spiketrain;
					writePulses("train", spiketrain);
					
					jtag->FPGA_set_fpga_ctrl(1);
					
					jtag->reset_jtag();

					jtag->set_hicann_pos(hicann_jtag_nr);
					set_pll(2,1); //set pll to 100 MHz
					nbc->setOutputMux(1,4); //set default do be ready for neuron 0 and preout
					nbc->setOutputEn(true,true);
					
					for(uint denmem=0; denmem<32; denmem++){//only first first(last) spl1
					//	 if(!(denmem==10)) nbc->enable_firing(denmem); //number 11 does funny stuff when not calibrated
						 nbc->enable_firing(denmem); 
						 nbc->set_spl1address(denmem, denmem+1);
						 /*if(denmem%8<4)
							 nbc->set_spl1address(denmem,2);
						 else
							 nbc->set_spl1address(denmem,0);*/
					}
					nbc->write_sram();

				} break;
				case 'c':{
					//setup neuron builder for building small networks 
					//all neurons are programmed as firing finaly ... at the beginning, only one neuron is selected	 
					for(uint denmem=0; denmem<32; denmem++){//only first first(last) spl1
					//	 if(!(denmem==10)) nbc->enable_firing(denmem); //number 11 does funny stuff when not calibrated
						 nbc->enable_firing(denmem); 
						 nbc->set_spl1address(denmem, denmem+1);
						 /*if(denmem%8<4)
							 nbc->set_spl1address(denmem,2);
						 else
							 nbc->set_spl1address(denmem,0);*/
					}
					nbc->write_sram();
					//nbc->print_sram(cout);
				}break;
				case 'n':{
					uint denmemNr = 0;
					cout<<"Enter denmemnumber: ";
					cin>>denmemNr;
					nbc->set_aout(denmemNr);
					nbc->write_sram();
					//nbc->print_sram(cout);
				}break;
				case 'a':{


					vector<uint> weights(32,0xffffffff); //all weights max
					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_bot->reset_drivers(); //reset bottom synapse block drivers
					sc_top->reset_dll();
					rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
					//rca[rc_tl]->configure(); //do this before locking the driver's DLL (resets the DLLs)

					for (uint u=0; u<6; u++) rca[u]->reset();
					lcl->reset(); //reset left crossbar
					lcr->reset();
					lctl->reset(); //reset top left select switch
					lctr->reset();
					lcbl->reset();
					lcbr->reset();
					configure_hicann_bgen(1, 500, 0xaa, 1, 1, 1); //start BEG ... before reseting drivers !
					rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 0 to transmit SPL1 data on line 1
					rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true); //configure sending repeater 28 to transmit BEG data on line 57
					lctl->switch_connect(0,28,1); //to driver 0 ... for background connection

					lctl->switch_connect(16,28,1); //to driver 0 ... try second background connection

					lcl->switch_connect(57,28,1); //background

					lcl->switch_connect(1,0,1); //to the upper HICANN

					//layer one pulses are connected to upper most syndriver 
					
					vector<uint> weightsfb(32,0xffffff00); //all weights max
					vector<uint> address220(32,0x55331177); //one row, all weights max, decoder 0
					vector<uint> address221(32,0x66442277); //one row, all weights max, decoder 0
					lctl->switch_connect(110,0,1); //to last driver feedback from network


					sc_top->reset_drivers(); //reset top synapse block drivers
					sc_top->drv_config(220, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(220, 0, 0, 0, 0);
					sc_top->drv_set_gmax(220, 1, 1, 15, 15); //has been 110 before
					sc_top->write_weight(220, weightsfb);
					sc_top->write_weight(221, weightsfb);
					sc_top->write_decoder(220, address220, address221);

					vector<uint> address216(32,0xddbb9977); //one row, all weights max, decoder 0
					vector<uint> address217(32,0xeeccaa77); //one row, all weights max, decoder 0
					
					sc_top->drv_config(216, 1, 0, 1, 0, 1, 0, 1);
					sc_top->preout_set(216, 0, 0, 0, 0);
					sc_top->drv_set_gmax(216, 1, 1, 15, 15); //has been 110 before
					sc_top->write_weight(216, weightsfb);
					sc_top->write_weight(217, weightsfb);
					sc_top->write_decoder(216, address216, address217);

					sc_top->drv_config(212, 1, 0, 1, 0, 1, 0, 1);
					sc_top->preout_set(212, 1, 1, 1, 1);
					sc_top->drv_set_gmax(212, 1, 1, 15, 15); //has been 110 before
					sc_top->write_weight(212, weightsfb);
					sc_top->write_weight(213, weightsfb);
					sc_top->write_decoder(212, address220, address220);

					sc_top->drv_config(208, 1, 0, 1, 0, 1, 0, 1);
					sc_top->preout_set(208, 1, 1, 1, 1);
					sc_top->drv_set_gmax(208, 1, 1, 15, 15); //has been 110 before
					sc_top->write_weight(208, weightsfb);
					sc_top->write_weight(209, weightsfb);
					sc_top->write_decoder(220, address216, address217);
					
					//feeding Background stimulus
					vector<uint> addressstim(32,0x44444400); //stimulate only first two neurons
					//vector<uint> addressstim(32,0x00000000); //stimulate all neurons .... debug
					sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(0, 0, 0, 0, 0);
					sc_top->drv_set_gmax(0, 1, 1, 15, 15);
					sc_top->write_weight(0, weights);
					sc_top->write_weight(1, weights);
					sc_top->write_decoder(0, addressstim, addressstim);
					
					//try to work with two synapsedrivers...
					sc_top->drv_config(16, 1, 0, 1, 0, 1, 1, 0);
					sc_top->preout_set(16, 0, 0, 0, 0);
					sc_top->drv_set_gmax(16, 1, 1, 15, 15);
					sc_top->write_weight(32, weights);
					sc_top->write_weight(33, weights);
					sc_top->write_decoder(32, addressstim, addressstim);

					sc_top->drv_config(12, 1, 0, 1, 0, 1, 0, 1);
					sc_top->preout_set(12, 0, 0, 0, 0);
					sc_top->drv_set_gmax(16, 1, 1, 15, 15);
					sc_top->write_weight(32, weights);
					sc_top->write_weight(33, weights);
					sc_top->write_decoder(32, addressstim, addressstim);
/*
					sc_bot->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
					sc_bot->preout_set(0, 0, 0, 0, 0);
					sc_bot->drv_set_gmax(0, 0, 0, 15, 15);
					sc_bot->drv_config(220, 1, 0, 1, 0, 1, 1, 0);
					sc_bot->preout_set(220, 0, 0, 0, 0);
					sc_bot->drv_set_gmax(110, 0, 0, 15, 15);
					sc_bot->write_weight(0, weightsstim);
					sc_bot->write_weight(1, weightsstim);
					sc_bot->write_decoder(0, addressstim, addressstim);
					sc_bot->write_weight(220, weightsfb);
					sc_bot->write_weight(221, weightsfb);
					sc_bot->write_decoder(220, addressfb, addressfb); */

//					configure_hicann_bgen(1, 500, 0xaa, 1, 1, 0); //start BEG
					nbc->setSpl1Reset(0);
					nbc->setSpl1Reset(1);
					nbc->setNeuronReset(1,1);
					activate_neuron(0,0,0);
					
					sc_top->configure();
					//sc_top->reset_dll();
					cout << "Events after the BEG:" << endl;
					usleep(1000);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 14);
					cout << "Events from neurons:" << endl;
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);

					for(int rep=0;rep<32;rep++){
						readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
						readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
						readout_repeater(rc_bl, RepeaterControl::OUTWARDS, 0);
					}


				} break;
				case 'C':{
					uint value1=0;
					uint value2=0;

					cout<<"Enter gl current mirror switches:"<<endl;
					cout<<"slow=";
					cin>>value1;
					cout<<"fast=";
					cin>>value2;
					nbc->setNeuronGl(value1, value1, value2, value2);
					cout<<"Enter gladapt current mirror switches:"<<endl;
					cout<<"slow=";
					cin>>value1;
					cout<<"fast=";
					cin>>value2;
					nbc->setNeuronGladapt(value1, value1, value2, value2);
					cout<<"Enter radapt current mirror switches:"<<endl;
					cout<<"slow=";
					cin>>value1;
					cout<<"fast=";
					cin>>value2;
					nbc->setNeuronRadapt(value1, value1, value2, value2);
					cout<<"Use big caps?";
					cin>>value1;
					nbc->setNeuronBigCap(value1, value1);	
					nbc->configNeuronsGbl();

					cont = true;		
				}break;

				case 'o':{
					nbc->setOutputEn(false, false);
				} break;

				case 'S':{
	//use bank 1 to keep neuron stimulus data
					uint bank=1;
					uint fg_num;
					float syn_tc;
					std::vector<int> val;
					uint syn_tc_int;
					cout<<"Enter syntc parameter:";
					cin>>syn_tc;
					syn_tc_int=(syn_tc/1.8)*1023;
					cout<<"DAC value:"<<syn_tc_int<<endl;
					uint lineNumber=14;
					log(Logger::INFO)<<"tm_neuron:: parameter...";
						log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
					for( fg_num = 0; fg_num < 4; fg_num += 1) //using left block only ignore right block
					{	
						lineNumber = 14 - (fg_num%2) * 2;
						//generate data vector from input and xmlfile
						val = conf->getFgLine((fg_loc)fg_num, lineNumber);
						for(int i=1; i<val.size(); i++){
							val[i] = syn_tc_int;
						}
						log(Logger::DEBUG2)<<"Fg_number"<<fg_num;

						fc[fg_num]->initArray(bank, val);//use two banks -> values can be written although fg is still working
						while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
						fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
					}
					
					for( fg_num = 0; fg_num < 4; fg_num += 1){
						lineNumber = 14 - (fg_num%2) * 2;
						while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
						fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
					}
		
					for( fg_num = 0; fg_num < 4; fg_num += 1){
						lineNumber = 14 - (fg_num%2) * 2;
						while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
						fc[fg_num]->stimulateNeuronsContinuous(0);
					}
				}break;
				
				case 'O':{
					
					uint op1=0;
				    uint op2=0;

					cout<<"Op1:";
					cin>>op1;
					cout<<"Op2:";
					cin>>op2;

					nbc->setOutputMux(op1,op2);
					nbc->setOutputEn(true,true);

					cont = true;		
				}break;
				case 's':{
					stop_experiment();
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
					nc->beg_on(7); ///AUSKOMMENTIEREN
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
					usleep(100000);
					jtag->FPGA_enable_fifoloop();
					jtag->FPGA_enable_pulsefifo();
					//~ nc->beg_on(7); ///EINKOMMENTIEREN
					// stop experiment
					jtag->FPGA_disable_pulsefifo();
					jtag->FPGA_disable_fifoloop();
					jtag->FPGA_disable_tracefifo();
					
					// disable sys_Start
					comm_ptr->trigger_systime(curr_ip,false);
					
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
					spc->write_cfg(0x0ff00);
				} break;

				case 'u':
					{
						cout << "soft FPGA reset (via Ethernet)" << endl;
						unsigned int curr_ip = jtag->get_ip();
						if (comm != NULL) {
							comm->set_fpga_reset(curr_ip, true, true, true, true, true); // set reset
#ifdef NCSIM
#else
							usleep(1000000); // wait: optional, to see LED flashing - can be removed
#endif
							comm->set_fpga_reset(curr_ip, false, false, false, false, false); // release reset
						} else {
							printf("tm_l2pulses warning: bad s2comm pointer; did not perform soft FPGA reset.\n");
						}
					}
					break;

				case 'x':{
					cont=false;
				} break;
			}
		}while(cont);
		return true;
	};
};

const double TmCapoNet::clkperiod_dnc = 4.0e-9;  // in seconds
const double TmCapoNet::clkperiod_fpga = 8.0e-9; // in seconds

const uint TmCapoNet::num_reticles;
const uint TmCapoNet::num_dncs;
const uint TmCapoNet::num_hicanns;
const uint TmCapoNet::src_hicann;
const uint TmCapoNet::tgt_hicann;


class LteeTmCapoNet : public Listee<Testmode>{
public:         
	LteeTmCapoNet() : Listee<Testmode>(string("tm_capo_net"),string("contains all functionality for year 1 BrainScaleS review low level demo, modified")){};
	Testmode * create(){return new TmCapoNet();};
};
LteeTmCapoNet ListeeTmCapoNet;
