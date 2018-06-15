// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_ramtest.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   3/2009
//------------------------------------------------------------------------

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "s2comm.h"
#include "s2ctrl.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "testmode.h" 
#include "neuron_control.h" //neuron control class (merger, background generators)
#include "spl1_control.h"
#include "dnc_control.h"


// jtag
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys_2fpga_arq.h"

// hostal stuff
#include "pulse_float.h"
#include "ethernet_software_if.h"
#include "host_al_controller.h"

#include "ramtest.h"

using namespace facets;
using namespace std;

class TmPlaybackSwitchRamTest : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmjp_playback_srtest"; return ss.str();}

public:
    HicannCtrl  *hc;
	NeuronControl *nc; //neuron control
    SPL1Control *spc;
    DNCControl  *dc;

	ci_data_t rcvdata;
	ci_addr_t rcvaddr;


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {

		uint startaddr = 0;

#ifdef NCSIM
		uint maxaddr   = 5;
#else
		uint maxaddr   = 111;
#endif

		uint datawidth = 16;

		uint testdata = 0;
		rcvdata  = 0;
		rcvaddr  = 0;

		unsigned int pulses_per_config = 10;
		double pulse_isi = 1.0e-7; // seconds technical time
		double add_config_isi = 30.0e-7; // additional time before and after config packet
		double compare_eps = 8.0e-9; // one FPGA clock cycle tolerance

		bool hicann_loop = true;

		bool result = true;

#ifdef NCSIM
      Logger::AlterLevel al(Logger::DEBUG0);
#endif

		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		uint hicannr = 0;
		
        hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
        nc  = &hc->getNC();
        spc = &hc->getSPL1Control();
        dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC
		FPGAControl *fpga = (FPGAControl*) chip[0];

		// get hicann id from testmode
		unsigned int hicann_id = comm->jtag2dnc(hc->addr());

		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		fpga->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1);
		
		//jtag->SetARQTimings(0x4, 0x0c8, 0x032);
		
		// get ARQ comm pointer, if available
		S2C_JtagPhys2FpgaArq * arq_comm = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
		HostALController *hostal = arq_comm->getHostAL();

		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, true); // set reset
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, false); // unset reset


		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------
	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr(), false, true) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
	 	dbg(0) << "Init() ok" << endl;


		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		dbg(0)<<"start SwitchCtrl access..."<<endl;

		// test read on tag 1 -> why?
		//sc->read_cmd(0x4002, 0);
		
		std::vector<int> testdatas;

		// switchram write packets
		double curr_time = pulse_isi;
		std::vector<pulse_float_t> send_pulses;
		for(uint i=startaddr;i<=maxaddr;i++)
		{
			testdata = 0;
			while(testdata == 0)  // avoid testing against reset value
				testdata = rand()%(1<<datawidth);

			// write pulses
			std::vector<pulse_float_t> curr_pulses;
			for (unsigned int npulse=0;npulse<pulses_per_config;++npulse)
			{
				unsigned int curr_id =  (rand()%64) + arq_comm->dncid*(1<<12) + hicann_id*(1<<9);
				curr_pulses.push_back( pulse_float_t(curr_id, curr_time) );
				send_pulses.push_back( pulse_float_t(curr_id, curr_time) );
				curr_time += pulse_isi;
			}
			hostal->sendPulseList(curr_pulses,0.0);

			curr_time += add_config_isi;
			// write next config packet (write)
			uint64_t config_packet = ((uint64_t)(0x1e)<<44) | ((uint64_t)(i)<<32) | ((uint64_t)(testdata));
			hostal->sendPlaybackConfig(curr_time, config_packet, arq_comm->dncid, hicann_id);
			curr_time += pulse_isi+add_config_isi;


			testdatas.push_back(testdata);
		}

		// switchram read request packets
		curr_time += 100*pulse_isi; // wait a bit before sending read requests
		for(uint i=startaddr;i<=maxaddr;i++)
		{
			// write pulses
			std::vector<pulse_float_t> curr_pulses;
			for (unsigned int npulse=0;npulse<pulses_per_config;++npulse)
			{
				unsigned int curr_id =  (rand()%64) + arq_comm->dncid*(1<<12) + hicann_id*(1<<9);
				curr_pulses.push_back( pulse_float_t(curr_id, curr_time) );
				send_pulses.push_back( pulse_float_t(curr_id, curr_time) );
				curr_time += pulse_isi;
			}
			hostal->sendPulseList(curr_pulses,0.0);

			curr_time += add_config_isi;
			// write next config packet (read)
			uint64_t config_packet = ((uint64_t)(0x0e)<<44) | ((uint64_t)(i)<<32) | ((uint64_t)(0x80000000));
			hostal->sendPlaybackConfig(curr_time, config_packet, arq_comm->dncid, hicann_id);
			curr_time += pulse_isi+add_config_isi;
		}

		log(Logger::INFO) << "Number of pulses to be sent: " << send_pulses.size();


// perform experiment
      // settings for HICANN loopback
      if (hicann_loop)
	  {
        nc->dnc_enable_set(1,1,1,1,1,1,1,1);
        nc->loopback_on(0,1);
        nc->loopback_on(2,3);
        nc->loopback_on(4,5);
        nc->loopback_on(6,7);
        spc->write_cfg(0x055ff);

		if (!arq_comm->is_k7fpga())
		{
		  jtag->DNC_set_loopback(0x00);
		  uint64_t dirs = (uint64_t)0x55<<(comm->jtag2dnc(hc->addr())*8);
		  //uint64_t dirs = (uint64_t)0x55;
		  dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr (needs conversion from jtag to dnc address!)
		  jtag->DNC_set_timestamp_ctrl(0x00); // DNC timestamp
		}
      }
	  else
	  {
		// DNC loopback in Virtex5, FPGA loopback in Kintex7
		if (arq_comm->is_k7fpga())
		{
		  hostal->setFPGAPulseLoopback(true);
		  hostal->setPlaybackSystimeReplacing(true);
		}
		else
		{
		  // DNC loopback
		  //hostal->setFPGAPulseLoopback(true);
		  jtag->DNC_set_loopback(0xff); // DNC loopback
		  jtag->DNC_set_timestamp_ctrl(0x00); // DNC timestamp
		}
	  }
	  
	  comm->Flush();
	  
      log(Logger::INFO) << "Starting systime counters";
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
	    log(Logger::WARNING) << "bad s2comm pointer; did not initialize systime counter";
	  }


      // start experiment execution
      log(Logger::INFO) << "Starting experiment";
      
      hostal->startPlaybackTrace();

      double exp_time_us = 1e6*curr_time+1000.0;
      log(Logger::INFO) << "Running for " << exp_time_us << "us";
	  // wait...
	  #ifdef NCSIM
		 wait(exp_time_us,SC_US);
	  #else
		 usleep(int(exp_time_us+0.5e6));
	  #endif

        log(Logger::INFO) << "Stopping experiment";
        hostal->stopTrace();
		
		if (!arq_comm->is_k7fpga())
		{
		  for (size_t ndnc = 0; ndnc < 4; ++ndnc) {
			uint32_t sent_playback_count = jtag->GetSentPlaybackCount(ndnc);
			log(Logger::INFO) << "\tDNC " << ndnc << ":\t" << sent_playback_count;
		  }

		  unsigned int trace_pulse_count = jtag->GetTracePulseCount();
		  unsigned int trace_entry_count = jtag->GetTraceEntryCount();
		  log(Logger::INFO) << "Traced pulse count: " << trace_pulse_count << ", stored in " << trace_entry_count << " memory entries";
		}

		log(Logger::INFO) << "Received config packets: " << hostal->getReceivedHICANNConfigCount(arq_comm->dncid, hicann_id, 0);

// read out / compare config packets


		for(uint i=startaddr;i<=maxaddr;i++){
			testdata = testdatas.at(i);

					if ( hostal->getReceivedHICANNConfigCount(arq_comm->dncid, hicann_id, 0) )
					{
						uint64_t curr_config = hostal->getReceivedHICANNConfig(arq_comm->dncid, hicann_id, 0);
						log(Logger::DEBUG0) << "Received HICANN config packet: " << std::hex << curr_config;
						rcvaddr = (curr_config>>32) & 0xff;
						rcvdata = (curr_config) & 0xffff;
					}
					else
					{
						rcvaddr = 0;
						rcvdata = 0;
					}
			
					log(Logger::INFO) << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
			
					if(i==rcvaddr && testdata==rcvdata)
						log(Logger::INFO) << "  ---> OK :-)";
					else{
						log(Logger::INFO) << "  ---> ERROR :-(";
						result = false;
//						break;
					}
		}

		log(Logger::INFO) << "Config packets in buffer after reading out all expected packets: " << hostal->getReceivedHICANNConfigCount(arq_comm->dncid, hicann_id, 0);

		if((arq_comm) && (!arq_comm->is_k7fpga())){
			for(uint i=0; i<10; i++){
				// ARQ counters for infering state of ARQ / high-speed links
				jtag_cmdbase::arqcounters_t cntrs = jtag->GetARQCounters();
				uint32_t rcounter = cntrs.first, wcounter = cntrs.second;
				std::cout << "ARQ read (from HW) counter: " << dec <<
					(rcounter & 0x7fffffff)
					<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
					<< "ARQ write (to HW) counter: " <<
					(wcounter & 0x7fffffff)
					<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)"
					<< std::endl;
				jtag_cmdbase::arqdebugregs_t dregs = jtag->GetARQNetworkDebugReg();
				std::cout << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second  << std::endl;
			}
		}

		

		// read out / compare traced pulses
		std::vector<pulse_float_t> rec_pulses;
		
		// read traced pulses
		hostal->startTraceRead();
		
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
				wait(200,SC_US);
			#else
				usleep( 10000.0 );
			#endif
		}
		
		 log(Logger::INFO) << "Read pulse count: " << rec_pulses.size();
		
		// compare sent and received pulses

		unsigned int pos1 = 0;
		unsigned int pos2 = 0;
		unsigned int only1 = 0;
		unsigned int only2 = 0;
		bool fine_up_to_now = true;

		unsigned int id_off = 0;
		if (hicann_loop)
			id_off = 64;

		while ( (pos1<send_pulses.size()) &&  (pos2<rec_pulses.size()) )
		{
			if (send_pulses[pos1].id+id_off == rec_pulses[pos2].id)
			{
				if ( (!hicann_loop) && ( fabs(send_pulses[pos1].time-rec_pulses[pos2].time) > compare_eps ) )
					log(Logger::INFO) << "Pulse " << pos1 << " with id=" << send_pulses[pos1].id << ": timestamps differ: sent: " << 1e9*send_pulses[pos1].time << "ns, "
					                  <<  "received: " << 1e9*rec_pulses[pos2].time << "ns";
				
				pos1 += 1;
				pos2 += 1;
			}
			else
			{
				if (fine_up_to_now)
				{
					log(Logger::INFO) <<  "First ID mismatch at pos " << pos1;
					fine_up_to_now = false;
				}

				// try searching for a match in both directions - take minimum
				unsigned int match1 = 0;
				while ( (pos1+match1 < send_pulses.size()) && (send_pulses[pos1+match1].id+id_off != rec_pulses[pos2].id))
					match1 += 1;

				unsigned int match2 = 0;
				while ( (pos2+match2 < rec_pulses.size()) && (send_pulses[pos1].id+id_off != rec_pulses[pos2+match2].id))
					match2 += 1;

				if (match1 < match2)
				{
					only1 += match1;
					pos1 += match1;
					log(Logger::INFO) << "At (sent) pos " << pos1 << ": Found " << match1 << " spikes only in sent spikes";
				}
				else
				{
					only2 += match2;
					pos2 += match2;
					log(Logger::INFO) << "At (sent) pos " << pos1 << ": Found " << match2 << " spikes only in received spikes";
				}
			}
		}

		only1 += send_pulses.size()-pos1;
		only2 += rec_pulses.size()-pos2;

		log(Logger::INFO) << "Spikes sent only: " << only1;
		log(Logger::INFO) << "Spikes received only:" << only2;

		if (!fine_up_to_now || (only1 > 0) || (only2 > 0) )
			result = false;

		if (result)
			log(Logger::INFO) << "TEST PASSED";
		else
			log(Logger::INFO) << "TEST FAILED";


		// convert time base to ms biol. time
		for (unsigned int npulse=0;npulse<send_pulses.size();++npulse)
			send_pulses[npulse].time *= 1.0e3*1.0e4;

		for (unsigned int npulse=0;npulse<rec_pulses.size();++npulse)
		{
			rec_pulses[npulse].time *= 1.0e3*1.0e4;
			rec_pulses[npulse].id -= id_off;
		}

		this->writeTracePulses("srtest_sentspikes.txt", send_pulses);
		this->writeTracePulses("srtest_recspikes.txt", rec_pulses);
		
//  		// ----------------------------------------------------
//  		// shutdown any ramaining arq activity in HICANN...
//  		// ----------------------------------------------------
//  		if (hc_jtag->GetCommObj()->Init(hc_jtag->addr()) != Stage2Comm::ok) {
//  			dbg(0) << "ERROR: Init failed, abort" << endl;
//  			return 0;
//  		}
// 
		return result;
	}



    bool writeTracePulses(const char *filename, std::vector<pulse_float_t> pulselist)
	{
      FILE *fout = fopen(filename, "w");
	  
	  for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
	    fprintf(fout, "%.2f\t%d\n",pulselist[npulse].time,pulselist[npulse].id);
	  
	  fclose(fout);
	  
	  return true;
    }

};


class LteeTmPlaybackSwitchRamTest : public Listee<Testmode>{
public:
	LteeTmPlaybackSwitchRamTest() : Listee<Testmode>(string("tmjp_playback_srtest"), string("ramtest of L1 switch memory via FPGA playback with intermediate pulses")){};
	Testmode * create(){return new TmPlaybackSwitchRamTest();};
};

LteeTmPlaybackSwitchRamTest ListeeTmPlaybackSwitchRamTest;

