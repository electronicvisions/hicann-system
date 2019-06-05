// testmode for pulse playback/trace

#include <time.h>

#include "pulse_float.h"

#include "common.h"
#include "hicann_ctrl.h"

#include "neuron_control.h" //neuron control class (merger, background generators)
#include "spl1_control.h"
#include "dnc_control.h"
#include "fpga_control.h"

#include "s2comm.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys_2fpga_arq.h"
#include "testmode.h"

#include "ethernet_software_if.h"

extern "C" {
#include <unistd.h>
}

//#define USENR         0
#define fpga_master   1
#define DATA_TX_NR    10000

using namespace std;
using namespace facets;

class TmL2Pulses : public Testmode{

  public:
    HicannCtrl  *hc;
    NeuronControl *nc; //neuron control
    SPL1Control *spc;
    DNCControl  *dc;
    S2C_JtagPhys2Fpga* local_comm_hs;

    double starttime;
    double clkperiod_bio_dnc;
    double clkperiod_bio_fpga;
    double playback_begin_offset;

    // test function
    bool test() 
    {
      bool success = true;

      this->starttime = 0.0; // set by sendPlaybackPulses
      this->clkperiod_bio_dnc = 4.0e-9*1.0e4*1.0e3; // in ms
      this->clkperiod_bio_fpga = 8.0e-9*1.0e4*1.0e3; // in ms
      this->playback_begin_offset = 5.0; // in ms

      hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
      nc  = &hc->getNC();
      spc = &hc->getSPL1Control();
      dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC

      bool hicann_loop = false;
	  bool nrnaddr_filter = false;
	  unsigned int pos_filter_mask = 0x1ff; // filter effectively disabled. reset value
	  unsigned int neg_filter_mask = 0x1ff; // filter effectively disabled. reset value
	  bool use_ddr2_pbtrace = false;
      bool use_pulse_gen = false;
	  bool block_trace_recording = false; // test for trace blocking
      unsigned int pulse_gen_isi = 1000;
      unsigned int experiment_count = 1;
      double multi_exp_timeshift_s = 5.0e-6; // shift of pulse start time between experiments

      local_comm_hs = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);

      if(!local_comm_hs){
        log(Logger::ERROR) << "Wrong Communication Class! Require at least S2C_JtagPhys2Fpga. EXIT";
        exit(EXIT_FAILURE);
      }

      bool is_k7fpga = local_comm_hs->is_k7fpga();

#ifndef NCSIM
      if (is_k7fpga)
      {
        cout << "Use HICANN loopback (1/0)? "; cin >> hicann_loop;
        use_ddr2_pbtrace = true;
        use_pulse_gen = false;
		cout << "Use neuron address filter (1/0)? ";
		cin >> nrnaddr_filter;
		if (nrnaddr_filter) {
			cout << "pos. filter mask (9bit hex, pulse filtered if at least one masked address bit "
				    "(0) is set: 0x";
			cin >> hex >> pos_filter_mask;
			cout << "neg. filter mask (9bit hex, pulse filtered if all masked address bits (0) are "
				    "unset: 0x";
			cin >> hex >> neg_filter_mask;
		}
	  }
      else
      {
      cout << "Use DDR2 playback/trace (1/0)? "; cin >> use_ddr2_pbtrace;
      cout << "Use HICANN loopback (1/0)? "; cin >> hicann_loop;
      cout << "Use FPGA pulse generator (1/0)? "; cin >> use_pulse_gen;
      if (use_pulse_gen)
      {
	cout << "FPGA pulse generator inter-spike-interval (clks): "; cin >> pulse_gen_isi;
      }
      }
#else
      if (is_k7fpga)
      {
        hicann_loop = true;
        use_ddr2_pbtrace = true;
        use_pulse_gen = false;
      }
      else
      {
      hicann_loop = true;
      use_ddr2_pbtrace = true;
      use_pulse_gen = false;
      pulse_gen_isi = 1000;
      }
#endif

      // ----------------------------------------------------
      // reset test logic and FPGA
      // ----------------------------------------------------
#ifndef NCSIM
      FPGAControl *fpga = (FPGAControl*) chip[0];
      fpga->reset();

      jtag->reset_jtag();
#endif

      if (!is_k7fpga)
      jtag->FPGA_set_fpga_ctrl(0x1); // "DNC" reset

      uint64_t jtagid=0xf;
      jtag->read_id(jtagid,jtag->pos_hicann);
      log(Logger::INFO) << "HICANN ID: 0x" << hex << jtagid << endl;

      if (!is_k7fpga) // design version read out via I2C in Kintex7 system
      log(Logger::INFO) << "Design Version: 0x" << hex << jtag->GetFPGADesignVersion() << endl;


      // ----------------------------------------------------
      // Initialize high-speed communication
      // ----------------------------------------------------

	if (local_comm_hs->Init(hc->addr()) != Stage2Comm::ok) {
	  log(Logger::ERROR) << "Init failed, abort";
	  return 1;
	}
	log(Logger::INFO) << "Init() ok";

      std::vector<pulse_float_t> send_pulses;
      std::vector<pulse_float_t> rec_pulses;

      if (!use_pulse_gen)
	this->readPulses("pynn_spikes.txt", send_pulses);


      // do systime initialisation right at the start
      log(Logger::INFO) << "Starting systime counters";
      unsigned int curr_ip = jtag->get_ip();

      local_comm_hs->trigger_systime(curr_ip,false);
      jtag->HICANNv2_reset_all_time_counters();
      local_comm_hs->trigger_systime(curr_ip,true);

      if (use_ddr2_pbtrace)
      {
	S2C_JtagPhys2FpgaArq * arq_comm = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
	assert(arq_comm);
	HostALController *hostal = arq_comm->getHostAL();
	double exp_time_s = 1.0;

	#ifdef NCSIM
		exp_time_s = 100e-6;
	#endif

	if (!use_pulse_gen)
	{
	  // convert time base from ms biological time to seconds technical time
	  // move address space to selected DNC 
	  for (unsigned int npulse=0;npulse<send_pulses.size();++npulse)
	  {
	    send_pulses[npulse].time *= 1.0e-3*1.0e-4;
	    send_pulses[npulse].id += (1<<9)*comm->jtag2dnc(hc->addr());
	    send_pulses[npulse].id += (1<<12)*arq_comm->dncid;
	  }

	  #ifdef NCSIM
            exp_time_s = send_pulses.back().time + 400e-6;
	  #else
	    exp_time_s = send_pulses.back().time + 0.1;
      #endif
	}

	for (unsigned int nexperiment=0;nexperiment<experiment_count;++nexperiment)
	{
	  // clean-up from previous experiment run
	  if (nexperiment > 0)
	  {
	    rec_pulses.clear();
	    hostal->reset();

	    for (unsigned int npulse=0;npulse<send_pulses.size();++npulse)
	      send_pulses[npulse].time += multi_exp_timeshift_s;
	  }

	  if (block_trace_recording)
	    hostal->addPlaybackFPGAConfig(0, false, false, false, true);

	  if (!use_pulse_gen)
	  {
	    hostal->sendPulseList(send_pulses,0.0);
	    log(Logger::INFO) << "Sent " << send_pulses.size() << " pulses";
	  }

	  // send end-of-data marker to playback
	  // -> wait 100 FPGA clks after last pulse, activate trace stop and read-out
	  hostal->addPlaybackFPGAConfig(uint64_t(send_pulses.back().time*1e9/8.0)+100, true, true, true, false);
	  hostal->flushPlaybackPulses();

	  // wait for playback to acknowledge end of data
	  while (hostal->getPlaybackEndAddress() == 0)
	  {
		  #ifdef NCSIM
			wait(50.0,SC_US);
		  #else
			usleep(100000);
		  #endif
	  }

	  log(Logger::INFO) << "Received playback end address: " << hostal->getPlaybackEndAddress() << std::endl;

	  // debug: replace timestamps from playback memory by current systime
	  hostal->setPlaybackSystimeReplacing(true);

	  this->executeExperiment(
		  hicann_loop, use_pulse_gen, pulse_gen_isi, exp_time_s, pos_filter_mask, neg_filter_mask,
		  hostal);

	  // wait for experiment to finish (not really necessary, just to avoid read attempts that would fail anyway)
	  #ifdef NCSIM
		wait(1e6*exp_time_s,SC_US);
	  #else
		usleep(int(1e6*exp_time_s));
	  #endif

	  // read traced pulses
	  unsigned int trial_count = 0;

	  for(;;)
	  {
	    std::vector<pulse_float_t> curr_rec_pulses = hostal->getFPGATracePulses();
	    if (curr_rec_pulses.size() == 0)
	    {
	      trial_count += 1;

	      if (trial_count >= 10)
		break;
	    }

	    for (unsigned int npulse=0;npulse<curr_rec_pulses.size();++npulse)	
	      rec_pulses.push_back(curr_rec_pulses[npulse]);

#ifdef NCSIM
            wait(40,SC_US);
#else
	    usleep( 10000.0 );
#endif
	  }

	  log(Logger::INFO) << "Read pulse count: " << rec_pulses.size();

	  // convert back time base and DNC address space
	  for (unsigned int npulse=0;npulse<rec_pulses.size();++npulse)
	  {
	    rec_pulses[npulse].time *= 1.0e3*1.0e4;
	    rec_pulses[npulse].id -= (1<<9)*comm->jtag2dnc(hc->addr());
	    rec_pulses[npulse].id -= (1<<12)*arq_comm->dncid;
	    if(hicann_loop) // Neuron number is incremented by 64 due to HICANN loopback
	      rec_pulses[npulse].id -= 64;
	  }

          if (experiment_count > 1)
          {
            char tracefname[80];
            sprintf(tracefname,"trace_spikes_run%d.txt",nexperiment);
            this->writeTracePulses(tracefname,rec_pulses);
          }

          // check success
          // read out of playback/trace pulse count from FPGA not yet supported for Kintex7
		  if (!is_k7fpga)
          {
	  for (size_t ndnc = 0; ndnc < 4; ++ndnc) {
	    uint32_t sent_playback_count = jtag->GetSentPlaybackCount(ndnc);
	    log(Logger::INFO) << "\tDNC " << ndnc << ":\t" << sent_playback_count;
	  }

	  unsigned int trace_pulse_count = jtag->GetTracePulseCount();
	  unsigned int trace_entry_count = jtag->GetTraceEntryCount();
	  log(Logger::INFO) << "Traced pulse count: " << trace_pulse_count << ", stored in " << trace_entry_count << " memory entries";

	  if( trace_pulse_count != rec_pulses.size() ) {
	    log(Logger::ERROR) << "Mismatch between jtag->GetTracePulseCount() = "
	      << trace_pulse_count
	      << " and rec_pulses.size() = "
                << rec_pulses.size();
	    success = false;
	  }
          }


	  if( !use_pulse_gen && (rec_pulses.size() != send_pulses.size()) ) {
	    log(Logger::ERROR) << "Mismatch between number of sent pulses = "
	      << send_pulses.size()
	      << " and number of received pulses "
	      << rec_pulses.size();
	    success = false;
	  } else if( use_pulse_gen && (rec_pulses.size() == 0 ) ) {
	    log(Logger::ERROR) << "No events received from pulse generator";
	    success = false;
	  }
	}
      }
      else
      {
	if (!use_pulse_gen)
	  this->sendPlaybackPulses(send_pulses);

	log(Logger::INFO) << "Sent " << send_pulses.size() << " pulses";

	this->executeExperiment(
		hicann_loop, use_pulse_gen, pulse_gen_isi, 1.0, pos_filter_mask, neg_filter_mask);

	this->getTracePulses(rec_pulses);
      }

      log(Logger::INFO) << "Traced " << rec_pulses.size() << " pulses";
      this->writeTracePulses("trace_spikes.txt",rec_pulses);

      return success;
    }




    unsigned int readPulses(const char *filename, std::vector<pulse_float_t> &pulselist)
    {
      if (access(filename, F_OK ) == -1) {
        std::cerr << "File " << filename << " does not exist!" << endl;
        exit(EXIT_FAILURE);
      }

      FILE *fin = fopen(filename, "r");

      pulselist.clear();

      char curr_line[80];
      while ( fgets(curr_line, 80, fin) )
      {
	if (curr_line[0] == '#') // comment line
	  continue;

	double curr_id = 0;
	double curr_time = 0.0;
	if ( sscanf(curr_line,"%lf %lf",&curr_time,&curr_id) != 2)
	{
	  log(Logger::WARNING) << "readPulses warning: Ignoring undecodable line " << curr_line;
	  continue;
	}

	pulselist.push_back( pulse_float_t((unsigned int)(curr_id),curr_time) );
      }


      fclose(fin);

      return pulselist.size();
    }

    bool writeTracePulses(const char *filename, std::vector<pulse_float_t> pulselist)
    {
      FILE *fout = fopen(filename, "w");

      for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
	fprintf(fout, "%.2f\t%d\n",pulselist[npulse].time,pulselist[npulse].id);

      fclose(fout);

      return true;
    }

    void sendPlaybackPulses(std::vector<pulse_float_t> &pulselist, double fpgahicann_delay_bioms = 5.0)
    {
      jtag->FPGA_reset_tracefifo();
      if (pulselist.size())
      {
	this->starttime = pulselist[0].time - fpgahicann_delay_bioms - playback_begin_offset; // always start first pulse at clk cycle 0 + offset
	int prev_reltime = 0;
	unsigned int discarded_count = 0;

	//printf("resolution: %lf\n", this->clkperiod_bio_fpga);

	for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
	{
	  double curr_time = pulselist[npulse].time - starttime;
	  int curr_cycle = (int)((curr_time - fpgahicann_delay_bioms) / this->clkperiod_bio_fpga + 0.5);
	  int curr_tstamp = (int)(fpgahicann_delay_bioms / this->clkperiod_bio_dnc + 0.5); // write only FPGA-HICANN delay, because FPGA release time is added in playback FIFO

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
	  unsigned int pulse_data = ((comm->jtag2dnc(hc->addr()) & 0x7) << 24) + ((neuron_id & 0x1ff) << 15) + (curr_tstamp & 0x7fff );
	  unsigned int extra_pulse = 0;

	  //printf("sendPlaybackPulses: fill data: FPGA delta time: %X, HICANN %d, pulse: %06X, extra: %06X\n",curr_cycle-prev_reltime, hicann_id, pulse_data, extra_pulse);


	  jtag->FPGA_fill_playback_fifo(false, (curr_cycle-prev_reltime)<<1, hicann_id, pulse_data, extra_pulse);

	  prev_reltime = curr_cycle;
	}

      }
    }

	void executeExperiment(
	    bool hicann_loop,
	    bool use_pulse_gen,
	    unsigned int pulse_gen_isi,
	    double exp_time_s,
	    unsigned int pos_filter_mask,
	    unsigned int neg_filter_mask,
	    HostALController* hostal = NULL)
	{
      log(Logger::INFO) << "Starting experiment method";

      S2C_JtagPhys2Fpga *local_comm_hs = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);
      assert(local_comm_hs);

	  jtag->K7FPGA_set_hicannif(comm->jtag2dnc(hc->addr())); // highspeed interface number
	  jtag->K7FPGA_set_neuron_addr_filter(pos_filter_mask, neg_filter_mask);

	  // settings for HICANN loopback without timestamp handling
      if (hicann_loop)
      {
	nc->dnc_enable_set(1,1,1,1,1,1,1,1);
	nc->loopback_on(0,1);
	nc->loopback_on(2,3);
	nc->loopback_on(4,5);
	nc->loopback_on(6,7);
	spc->write_cfg(0x055ff);

		if (!local_comm_hs->is_k7fpga())
        {
          jtag->DNC_set_loopback(0x00);
	uint64_t dirs = (uint64_t)0x55<<(comm->jtag2dnc(hc->addr())*8);
	dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr (needs conversion from jtag to dnc address!)
	jtag->DNC_set_timestamp_ctrl(0x00); // DNC timestamp
      }
      }
      else
      {
        // DNC loopback in Virtex5, FPGA loopback in Kintex7
        if (local_comm_hs->is_k7fpga())
        {
          if (hostal != NULL)
          {
            hostal->setFPGAPulseLoopback(true);
            hostal->setPlaybackSystimeReplacing(true);
          }
        }
      else
      {
	jtag->DNC_set_loopback(0xff); // DNC loopback
	jtag->DNC_set_timestamp_ctrl(0x00); // DNC timestamp
      }	
      }        

      comm->Flush();

      if (use_pulse_gen)
      {
	//jtag->FPGA_set_cont_pulse_ctrl(enable, channel, poisson?, delay, seed, nrn_start, char nrn_end)
	jtag->FPGA_set_cont_pulse_ctrl(1, 0x01, 0, pulse_gen_isi, 100, 1, 31, comm->jtag2dnc(hc->addr()));
      }


      comm->Flush();

      // start experiment execution
      log(Logger::INFO) << "Starting experiment";

      if (hostal != NULL)
      {

        // ugly: directly send UDP packet for experiment start trigger, to make c/1939 independent of new, separated systime/experiment start (c/2018)
        EthernetSoftwareIF eth_if;
        eth_if.init(1800);
        unsigned int fpga_ip = jtag->get_ip();

        // stop experiment (unset experiment bit, to be sure)
        uint32_t pck = 0x55000004;
        pck = htonl(pck);
        eth_if.sendUDP(fpga_ip, 1800, &pck, 4);

        // start experiment (generate rising edge on experiment bit)
        pck = 0x55000005;
        pck = htonl(pck);
        eth_if.sendUDP(fpga_ip, 1800, &pck, 4);

        eth_if.end();
      }
      else
      {
	jtag->FPGA_enable_pulsefifo();

	// wait...
#ifdef NCSIM
	wait(500,SC_US);
#else
	usleep(2000000);
#endif

	// stop experiment
	log(Logger::INFO) << "Stopping experiment";
	jtag->FPGA_disable_pulsefifo();
	jtag->FPGA_disable_tracefifo();
      }

      if (use_pulse_gen)
	jtag->FPGA_set_cont_pulse_ctrl(0, 0xff, 0, 2000, 100, 1, 15, comm->jtag2dnc(hc->addr()));
    }

    void getTracePulses(std::vector<pulse_float_t> &pulselist)
    {
      pulselist.clear();
      uint64_t prev_entry = 1;
      while((!jtag->FPGA_empty_pulsefifo()))
      {
	uint64_t rdata; 
	jtag->FPGA_get_trace_fifo(rdata);

	//printf("getTracePulses: received data %016lX\n",rdata);

	if ( (rdata==0) &&(prev_entry==0) )
	{
	  printf("Error when reading trace memory: I only get zeros. Stop.\n");
	  break;
	}

	unsigned int curr_id = (rdata>>15)&0xfff;
	unsigned int curr_tstamp = rdata&0x7fff;
	double curr_time = this->starttime + this->clkperiod_bio_dnc*curr_tstamp;
	pulselist.push_back( pulse_float_t(curr_id,curr_time) );

	if ( (rdata>>63) ) // second pulse available
	{
	  rdata = rdata>>32;
	  curr_id = (rdata>>15)&0xfff;
	  curr_tstamp = rdata&0x7fff;
	  curr_time = this->starttime + this->clkperiod_bio_dnc*curr_tstamp;
	  pulselist.push_back( pulse_float_t(curr_id,curr_time) );
	}

	prev_entry = rdata;
      }
    }

};

class LteeTmL2Pulses : public Listee<Testmode>{
  public:
    LteeTmL2Pulses() : Listee<Testmode>(string("tm_l2pulses"),string("playback/trace pulses via L2 from/to file")){};
    Testmode * create(){return new TmL2Pulses();};
};

LteeTmL2Pulses ListeeTmL2Pulses;

