//------------------------------------------------------------------------

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#include "eth_utils.h"
#endif

#include "s2comm.h"
#include "s2ctrl.h"
#include "fpga_control.h"
#include "dnc_control.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

// jtag
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys_2fpga_arq.h"

//only if needed
#include "l1switch_control.h" //layer 1 switch control class
#include "synapse_control.h"
#include "syn_trans.h"

// hostal stuff
#include "ethernet_software_if.h"
#include "host_al_controller.h"

#include "ramtest.h"

using namespace facets;
using namespace std;

class TmARQBurnin : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_arq_burnin"; return ss.str();}
public:

	struct switchdata {
		uint hc;
		uint lc;
		uint addr;
		uint data;
		switchdata(uint h, uint l, uint a, uint d):hc(h), lc(l), addr(a), data(d){}
	};
		
	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {

		uint testhc;
		uint testl1c;
		uint testaddr;
		uint testdata;

		int error = 0;
		int ignore = 0;
		int ok = 0;

		ci_data_t rcvdata;
		ci_addr_t rcvaddr;

		bool result = true;


		uint num_hicanns = 1;
		uint num_writes = 0;
		bool add_pulses = false;
		bool use_pulse_gen = false;
		unsigned int pulse_gen_isi = 1000;

		vector<HicannCtrl*>            hicann_hs, hicann_jtag;
		vector<vector<vector<uint> > > switches;
		vector<vector<vector<uint32_t> > > luts;

		std::vector<pulse_float_t> send_pulses;
		std::vector<pulse_float_t> rec_pulses;

		S2C_JtagPhys2FpgaArq* local_comm_hs;
		S2C_JtagPhys* s2c_jtag;
		Stage2Comm *local_comm;
		
		bitset<8> used_hicanns;
		double read_ratio(0.5);


#ifndef NCSIM
		cout << "How many HICANNs (1-8)? "; cin >> num_hicanns;
		cout << "How many writes? "; cin >> num_writes;
		cout << "Which read ratio? "; cin >> read_ratio;
		cout << "Concurrent pulse stimulation (1/0)? "; cin >> add_pulses;
		if(add_pulses){
			cout << "Use FPGA pulse generator (1/0)? "; cin >> use_pulse_gen;
			if(use_pulse_gen) {
				cout << "FPGA pulse generator inter-spike-interval (clks): "; cin >> pulse_gen_isi;
			}
		}
cout << "test0" << endl << flush;
#else
		num_hicanns = 1;
		num_writes = 200000;
		add_pulses = false;
#endif



#ifdef NCSIM
      std::cout << "Starting testcase tm_arq_burnin at time " << int(sc_simulation_time()) << "ns" << std::endl;
      Logger::AlterLevel al(Logger::DEBUG0);
/*
      eth_dbg::setDbgLevel(ETH_COMPONENT_MAC, 10);
      eth_dbg::setDbgLevel(ETH_COMPONENT_ARP, 10);
      eth_dbg::setDbgLevel(ETH_COMPONENT_IPV4, 10);
      eth_dbg::setDbgLevel(ETH_COMPONENT_UDP, 10);
*/
#endif


		// ----------------------------------------------------
		// generate all HICANN objects, locally:
		// ----------------------------------------------------

		// high-speed
		for(uint i=0; i<8; i++){
			if ((i<num_hicanns) && (i+jtag->pos_hicann < 8))
				used_hicanns[comm->jtag2dnc(i + jtag->pos_hicann)] = true;
		}
		for(uint hc=0; hc<num_hicanns; hc++){
			hicann_hs.push_back( new HicannCtrl(comm,hc) );
			vector<vector<uint> > tmpall;
			for(uint lc=0; lc<6; lc++){
				vector<uint> tmp(112,0);
				tmpall.push_back(tmp);
			}
			switches.push_back(tmpall);

			vector<vector<uint32_t> > tmpall_1;
			for(uint block=0; block<2; block++){
				vector<uint32_t> tmp(6,0);
				tmpall_1.push_back(tmp);
			}
			luts.push_back(tmpall_1);
		}
		
		// JTAG only
		s2c_jtag   = new S2C_JtagPhys(comm->getConnId(), jtag, false);
		local_comm = static_cast<Stage2Comm*>(s2c_jtag);
		for(uint hc=0; hc<num_hicanns; hc++){
			hicann_jtag.push_back( new HicannCtrl(local_comm,hc) );
		}


		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		FPGAControl *fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1); // "DNC" reset

		uint arbiterdelay = 10; // max
#ifndef NCSIM          
		std::vector<boost::program_options::basic_option<char> >::iterator it;
		for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("arbiterdelay").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> arbiterdelay;
				break;
			}
		}
#endif
		// -> AG: WHY all f's??? comm->jtag->SetARQTimings(arbiterdelay, -1ul, -1ul);
		comm->jtag->SetARQTimings(arbiterdelay, 0x0c8, 0x01f);

		unsigned int curr_ip = comm->jtag->get_ip();

		// toggle arq reset one more time to update arq timings
		Stage2Comm::set_fpga_reset(curr_ip, false, false, false, false, true);
		Stage2Comm::set_fpga_reset(curr_ip, false, false, false, false, false);


		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		log(Logger::INFO) << "HICANN ID: 0x" << hex << jtagid << endl;


		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------
		
	 	dbg(0) << "Try Init() ..." ;
		local_comm_hs = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
		
		if(local_comm_hs){
			if (local_comm_hs->Init(used_hicanns) != Stage2Comm::ok) {
			 	dbg(0) << "ERROR: Init failed, abort" << endl;
				return 1;
			}
		 	dbg(0) << "Init() ok" << endl;
		} else {
			dbg(0) << "Wrong Comm Class!!! EXIT" << endl;
			exit(EXIT_FAILURE);
		}


		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		dbg(0)<<"start SwitchCtrl access..."<<endl;

//		srand ( time(NULL) ); // use individual seed for each run
        #ifdef NCSIM
            srand ( 100 );
        #else
    		srand ( randomseed ); // use individual seed for each run
        #endif

		// ok, get some debug registers multiple times (to be sure ;))
		//{
			//jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
			//log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		//}
		//{
			//jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
			//log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		//}
		//{
			//jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
			//log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		//}
		//{
			//jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
			//log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		//}
		//{
			//jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
			//log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		//}

		//{
			//// get all arq seq numbers from all (4) segments
			//for (int ii=0; ii < 4; ii++) {
				//jtag_cmdbase::arqdout_t dout = local_comm_hs->jtag->GetARQDout(ii);
				//log(Logger::DEBUG0) << "ARQ Sequence counters before experiment (ARQDout): " << " " << dec
					//<< dout[0] << " " << dout[1] << " " << dout[2] << " " << dout[3] << " "
					//<< dout[4] << " " << dout[5] << " " << dout[6] << " " << dout[7];
			//}
			//// ok, get arq packet counters (recv from upper layer, sent to upper layer)
			//jtag_cmdbase::arqcounters_t cntrs = local_comm_hs->jtag->GetARQCounters();
			//uint32_t rcounter = cntrs.first, wcounter = cntrs.second;
			//log(Logger::DEBUG0) << "ARQ read (from HW) counter: " << dec <<
				//(rcounter & 0x7fffffff)
				//<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
				//<< "ARQ write (to HW) counter: " <<
				//(wcounter & 0x7fffffff)
				//<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)";
			//jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
			//log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
		//}


		vector<switchdata> exp_read_results;
		double tag_ratio(0.5);
		bool read;
		bool tag;
		
		// initialize STDP LUTs with 0s:
		for(uint hc=0; hc<num_hicanns; hc++)
			for(uint lut=0; lut<6; lut++)
				for(uint block=0; block<2; block++)
					hicann_hs[hc]->getSC(block).write_data(SynapseControl::sc_lut+lut, (unsigned long)0);

		// flush
		comm->Flush();

		// write pulses if required
		// to do: extend to more than one HICANN
		HostALController *hostal = NULL;
        #ifdef NCSIM
          // need always in simulation
 		  S2C_JtagPhys2FpgaArq * arq_comm = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
		  assert(arq_comm);
		  hostal = arq_comm->getHostAL();
        #endif

		if (add_pulses)
		{
            #ifndef NCSIM
			  S2C_JtagPhys2FpgaArq * arq_comm = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
			  assert(arq_comm);
			  hostal = arq_comm->getHostAL();
            #endif

			if(!use_pulse_gen){
	          this->readPulses("pynn_spikes.txt", send_pulses);
			  // convert time base from ms biological time to seconds technical time
			  // move address space to selected DNC 
			  for (unsigned int npulse=0;npulse<send_pulses.size();++npulse) {
			    send_pulses[npulse].time *= 1.0e-3*1.0e-4;
			    send_pulses[npulse].id += (1<<9)*comm->jtag2dnc(hicann_hs[0]->addr());
			    send_pulses[npulse].id += (1<<12)*arq_comm->dncid;
			  }
				hostal->sendPulseList(send_pulses,0.0);
			}

			// loopback settings
			NeuronControl *nc  = &hicann_hs[0]->getNC();
			SPL1Control *spc = &hicann_hs[0]->getSPL1Control();
			DNCControl  *dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC
			jtag->DNC_set_loopback(0x00);
			nc->dnc_enable_set(1,1,1,1,1,1,1,1);
			nc->loopback_on(0,1);
			nc->loopback_on(2,3);
			nc->loopback_on(4,5);
			nc->loopback_on(6,7);
			spc->write_cfg(0x055ff);
			//uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
			uint64_t dirs = (uint64_t)0x55;
			dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr
			jtag->DNC_set_timestamp_ctrl(0x00); // DNC timestamp
			comm->Flush();

			// start experiment
      log(Logger::INFO) << "Starting systime counters";
			arq_comm->trigger_systime(curr_ip,false);
			jtag->HICANN_stop_time_counter();
			jtag->HICANN_start_time_counter();
			arq_comm->trigger_systime(curr_ip,true);

			if(use_pulse_gen) {
				// enable bg gen in FPGA
	    	jtag->FPGA_set_cont_pulse_ctrl(1, 0x55, 0, pulse_gen_isi, 100, 1, 63, comm->jtag2dnc(hicann_hs[0]->addr()));
			}

      log(Logger::INFO) << "Starting playback/trace memory";
			hostal->startPlaybackTrace();
			comm->Flush();
		}
		
		// write random values to available switch matrices
		for(uint w=0; w<num_writes; w++){
			if(!(w%10000)) log(Logger::INFO) << "transmitted " << dec << w << " packets";
			testhc   = rand() % num_hicanns;
			//testl1c  = [&](){if (w>=num_writes/2) return 1; else return 0;}();
			testl1c  = rand() % 6;
			testdata = 0;
			read     = (((double)rand()/(double)RAND_MAX) < read_ratio) ? true : false;
			tag      = (((double)rand()/(double)RAND_MAX) < tag_ratio) ? true : false;
			if(!tag){
				if(testl1c < 2){
					testaddr = rand() % 64;
					if(read){
						switch(testl1c){
							case 0:hicann_hs[testhc]->getLC_cl().read_cfg(testaddr);break;
							case 1:hicann_hs[testhc]->getLC_cr().read_cfg(testaddr);break;
						}
						exp_read_results.push_back(switchdata(testhc, testl1c, testaddr, switches[testhc][testl1c][testaddr]));
						log(Logger::INFO) << "Read answer " << dec << exp_read_results.size()-1 << " produced at packet " << w << " for "
							<< "HICANN " << exp_read_results.back().hc
							<< ", L1 " << exp_read_results.back().lc
							<< ", test 0x" << hex << exp_read_results.back().addr
							<< ", 0x" << exp_read_results.back().data;
					} else {
						while(testdata == 0)  // avoid testing against reset value
							testdata = rand()%(1<<4); // 4 entries per address
						switch(testl1c){
							case 0:hicann_hs[testhc]->getLC_cl().write_cfg(testaddr, testdata);break;
							case 1:hicann_hs[testhc]->getLC_cr().write_cfg(testaddr, testdata);break;
						}
						switches[testhc][testl1c][testaddr] = testdata;
						log(Logger::INFO) << "write: HICANN " << testhc << " L1 " << testl1c << " text 0x" << hex << testaddr << " 0x" << switches[testhc][testl1c][testaddr] << endl;
					}
				} else {
					testaddr = rand() % 112;
					if(read){
						switch(testl1c){
							case 2:hicann_hs[testhc]->getLC_tl().read_cfg(testaddr);break;
							case 3:hicann_hs[testhc]->getLC_bl().read_cfg(testaddr);break;
							case 4:hicann_hs[testhc]->getLC_tr().read_cfg(testaddr);break;
							case 5:hicann_hs[testhc]->getLC_br().read_cfg(testaddr);break;
						}
						exp_read_results.push_back(switchdata(testhc, testl1c, testaddr, switches[testhc][testl1c][testaddr]));
						log(Logger::INFO) << "Read answer " << dec << exp_read_results.size()-1 << " produced at packet " << w << " for "
							<< "HICANN " << exp_read_results.back().hc
							<< ", L1 " << exp_read_results.back().lc
							<< ", test 0x" << hex << exp_read_results.back().addr
							<< ", 0x" << exp_read_results.back().data;
					} else {
						while(testdata == 0)  // avoid testing against reset value
							testdata = rand()%(1<<16); // 16 entries per address
						switch(testl1c){
							case 2:hicann_hs[testhc]->getLC_tl().write_cfg(testaddr, testdata);break;
							case 3:hicann_hs[testhc]->getLC_bl().write_cfg(testaddr, testdata);break;
							case 4:hicann_hs[testhc]->getLC_tr().write_cfg(testaddr, testdata);break;
							case 5:hicann_hs[testhc]->getLC_br().write_cfg(testaddr, testdata);break;
						}
						switches[testhc][testl1c][testaddr] = testdata;
						log(Logger::INFO) << "write: HICANN " << testhc << " L1 " << testl1c << " test 0x" << hex << testaddr << " 0x" << switches[testhc][testl1c][testaddr] << endl;
					}
				}
			} else {
				size_t block = (rand() < RAND_MAX/2) ? 1 : 0;
				testaddr = rand() % 6;
				if(read){
					hicann_hs[testhc]->getSC(block).read_cmd(static_cast<ci_addr_t>(SynapseControl::sc_lut+testaddr), Stage2Comm::synramdelay);
					exp_read_results.push_back(switchdata(testhc, 6+block, testaddr, luts[testhc][block][testaddr]));
					log(Logger::INFO) << "Read answer " << dec << exp_read_results.size()-1 << " produced at packet " << w << " for "
						<< "HICANN " << exp_read_results.back().hc
						<< ", L1 " << exp_read_results.back().lc
						<< ", test 0x" << hex << exp_read_results.back().addr
						<< ", 0x" << exp_read_results.back().data;
				} else {
					testdata = rand(); // all lut entries are 31 bit wide (8 4-bit luts)
					hicann_hs[testhc]->getSC(block).write_data(static_cast<ci_addr_t>(SynapseControl::sc_lut+testaddr), (unsigned long)testdata);
					luts[testhc][block][testaddr] = testdata;
					log(Logger::INFO) << "write: HICANN " << testhc << " L1 " << 6+block << " test 0x" << hex << testaddr << " 0x" << luts[testhc][block][testaddr] << endl;
				}
			}
            
            #ifdef NCSIM
              // fill receive buffer periodically in simulation -> avoid buffer overflow
              hostal->getReceivedHICANNConfigCount(0, 0, 0);
            #endif
            

			// read out debug counters periodically to monitor connection quality / upcoming connection problems -> do only if ARQ counters are unexpectedly high
			if (false) {
			//if ( (w != 0) && ((w%20000) == 0)) {
                log(Logger::DEBUG0) << "Checking HICANN ARQ state after " << w << " of " << num_writes << " packets";
                #ifdef NCSIM
                  log(Logger::DEBUG0) << "...at simulation time " << (int)(sc_simulation_time()) << "ns";
                #endif
	 			jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
				log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;

				if (dregs.first > 2*num_writes || dregs.second > 2*num_writes)
				{
					unsigned int ntrial=0;
					unsigned int max_trial = 20;
					for (ntrial = 0;ntrial < max_trial;++ntrial)
					{
						jtag_cmdbase::arqcounters_t cntrs = local_comm_hs->jtag->GetARQCounters();
			 			uint32_t rcounter = cntrs.first, wcounter = cntrs.second;
						log(Logger::DEBUG0) << "status after " << dec << w << " packets:";
						log(Logger::DEBUG0) << "ARQ read (from HW) counter: " << dec <<
							(rcounter & 0x7fffffff)
							<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
							<< "ARQ write (to HW) counter: " <<
							(wcounter & 0x7fffffff)
							<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)";
			 			jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
						log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
						
						if (wcounter & 0x80000000) // no data to write by ARQ
							break;
					}
				
					// do test transmissions on all tested hicanns
					usleep(1000);
					
					// -> first read out some packets in FPGA register to see if some HICANNs are sending continuously
					log(Logger::DEBUG0) << "Monitoring incoming packets (beware: data and HICANN ID may not always be triggered by the same packet):";
					for (unsigned int ntest=0;ntest<20;++ntest)
					{
						uint64_t rec_data = 0;
						uint64_t rec_channel = 0;
						jtag->FPGA_get_rx_data(rec_data);
						jtag->FPGA_get_rx_channel(rec_channel);
						log(Logger::DEBUG0) << "Packet " << ntest << ": HICANN=" << dec << (rec_channel&0x7) << ", data=0x" << hex << rec_data;
					}
					
					unsigned int trans_count = 100;
					for (size_t hicann_nr = 0; hicann_nr < used_hicanns.size(); hicann_nr++)
					{
						if (!used_hicanns[hicann_nr]) continue;

						unsigned int trans_errors = local_comm_hs->test_transmission(hicann_nr, trans_count);
						log(Logger::DEBUG0) << "Transmission test after " << w << " packets on HICANN (DNCchannel) " << hicann_nr << ": " << trans_errors << " errors on " << trans_count << " transmissions";

					}

					if (ntrial == max_trial)
					{
						log(Logger::DEBUG0) << "ARQ seems blocked - stop test";
						return 1;
					}
					
					usleep(1000);

					log(Logger::DEBUG0) << *local_comm_hs;
 				}
			}

			if (w == num_writes/2) {
				// optional: break it (reset hicann)!
				//assert(local_comm->Init(hicann_jtag[testhc]->addr()) == Stage2Comm::ok);
			}
		}

		// flush remaining packets
		comm->Flush();

//		log(Logger::DEBUG0) << "1s to be sure...";
//#ifdef NCSIM
//		wait(500,SC_US);
//#else
//		usleep(15000);
//#endif

// 		{
// 			// get all arq seq numbers from all (4) segments
// 			for (int ii=0; ii < 4; ii++) {
// 				jtag_cmdbase::arqdout_t dout = local_comm_hs->jtag->GetARQDout(ii);
// 				log(Logger::DEBUG0) << "ARQ Sequence counters before experiment (ARQDout): " << " " << dec
// 					<< dout[0] << " " << dout[1] << " " << dout[2] << " " << dout[3] << " "
// 					<< dout[4] << " " << dout[5] << " " << dout[6] << " " << dout[7];
// 			}
// 
// 			jtag_cmdbase::arqcounters_t cntrs = local_comm_hs->jtag->GetARQCounters();
// 			uint32_t rcounter = cntrs.first, wcounter = cntrs.second;
// 			log(Logger::DEBUG0) << "ARQ read (from HW) counter: " << dec <<
// 				(rcounter & 0x7fffffff)
// 				<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
// 				<< "ARQ write (to HW) counter: " <<
// 				(wcounter & 0x7fffffff)
// 				<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)";
// 			jtag_cmdbase::arqdebugregs_t dregs = local_comm_hs->jtag->GetARQNetworkDebugReg();
// 			log(Logger::DEBUG0) << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
// 		}


 		// ----------------------------------------------------
 		// verify read back data
 		// ----------------------------------------------------
		for(uint i=0; i<exp_read_results.size(); i++){
			try {
			ci_payload res;
 			switch(exp_read_results[i].lc){
 				case 0:hicann_hs[exp_read_results[i].hc]->getLC_cl().get_read_cfg(rcvaddr, rcvdata);break;
 				case 1:hicann_hs[exp_read_results[i].hc]->getLC_cr().get_read_cfg(rcvaddr, rcvdata);break;
 				case 2:hicann_hs[exp_read_results[i].hc]->getLC_tl().get_read_cfg(rcvaddr, rcvdata);break;
 				case 3:hicann_hs[exp_read_results[i].hc]->getLC_bl().get_read_cfg(rcvaddr, rcvdata);break;
 				case 4:hicann_hs[exp_read_results[i].hc]->getLC_tr().get_read_cfg(rcvaddr, rcvdata);break;
 				case 5:hicann_hs[exp_read_results[i].hc]->getLC_br().get_read_cfg(rcvaddr, rcvdata);break;
 				case 6:hicann_hs[exp_read_results[i].hc]->getSC(0).get_data(&res); rcvaddr = res.addr&0xf; rcvdata = res.data; break;
 				case 7:hicann_hs[exp_read_results[i].hc]->getSC(1).get_data(&res); rcvaddr = res.addr&0xf; rcvdata = res.data; break;
 			}
			} catch (std::underflow_error& e) {
				log(Logger::ERROR) << " got underflow error" << endl;
				break;
			}

			stringstream message;
			bool thisoneok = true;

			if(exp_read_results[i].lc < 6){
	 			if(!(exp_read_results[i].addr==rcvaddr && exp_read_results[i].data==rcvdata)){
					if (exp_read_results[i].data==0) {
						ignore++;
						message << " IGNORED :-/";
					} else {
						error++;
						message << " (read answer nr "<< dec << i << ") ERROR :-(";
					}
 					result = false;
					thisoneok = false;
 				} else {
 					ok++;
 					message << " OK :-)";
 				}
			} else {
	 			if(exp_read_results[i].data != rcvdata){
 					error++;
					message << " (read answer nr "<< dec << i << ") ERROR :-(";
 					result = false;
					thisoneok = false;
 				} else {
 					ok++;
 					message << " OK :-)";
 				}
			}



 			if(thisoneok)
				log(Logger::WARNING) << "Read No. " << i << ", HICANN " << exp_read_results[i].hc << ", L1 " << exp_read_results[i].lc << hex << ", test \t0x" << exp_read_results[i].addr << ", \t0x" << exp_read_results[i].data << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": " << message.str();
			else
				log(Logger::ERROR) << "Read No. " << i << ", HICANN " << exp_read_results[i].hc << ", L1 " << exp_read_results[i].lc << hex << ", test \t0x" << exp_read_results[i].addr << ", \t0x" << exp_read_results[i].data << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": " << message.str();

			//if (error != 0){
				//return 1;
			//}

		}
	
		log(Logger::INFO) << "errors:  " << dec << error;
		log(Logger::INFO) << "ignored:  " << dec << ignore;
		log(Logger::INFO) << "ok:  " << dec << ok;

		// test pulse result
		if (add_pulses) {
			if(use_pulse_gen) {
				// disable bg gen in FPGA
	    	jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, comm->jtag2dnc(hicann_hs[0]->addr()));
			}

			hostal->stopTrace();

			for (size_t ndnc = 0; ndnc < 4; ++ndnc) {
				uint16_t count_current_dnc = jtag->GetSentPlaybackCount(ndnc);
				log(Logger::INFO) << "\tDNC " << ndnc << ":\t" << count_current_dnc;
			}

			unsigned int trace_pulse_count = jtag->GetTracePulseCount();
			unsigned int trace_entry_count = jtag->GetTraceEntryCount();
			log(Logger::INFO) << "Traced pulse count: " << trace_pulse_count << ", stored in " << trace_entry_count << "memory entries";


			// read traced pulses
			hostal->startTraceRead();
		
			unsigned int trial_count = 0;
			for(;;) {
				std::vector<pulse_float_t> curr_rec_pulses = hostal->getFPGATracePulses();
				if (curr_rec_pulses.size() == 0) {
					trial_count += 1;
					if (trial_count >= 10) break;
				}

				for (unsigned int npulse=0;npulse<curr_rec_pulses.size();++npulse)	
					rec_pulses.push_back(curr_rec_pulses[npulse]);

				#ifndef NCSIM
					usleep( 10000.0 );
				#endif
			}
			log(Logger::INFO) << "Read pulse count: " << rec_pulses.size();
		
			// convert back time base and DNC address space
			for (unsigned int npulse=0;npulse<rec_pulses.size();++npulse) {
			  rec_pulses[npulse].time *= 1.0e3*1.0e4;
			  rec_pulses[npulse].id -= (1<<9)*comm->jtag2dnc(hicann_hs[0]->addr());
			  rec_pulses[npulse].id -= (1<<12)*local_comm_hs->dncid;
				rec_pulses[npulse].id -= 64; // Neuron number is incremented by 64 due to HICANN loopback
			}
		
			log(Logger::INFO) << "Traced " << rec_pulses.size() << " pulses";
			this->writeTracePulses("trace_spikes.txt",rec_pulses);
		}
	

// 		// ----------------------------------------------------
// 		// loop over all HICANNs for JTAG readback
// 		// ----------------------------------------------------
// 		for(uint hc=0; hc<num_hicanns; hc++){
// 
// 			// Initialize JTAG only comm for readback: ARQ reset and other stuff
// 			if (local_comm->Init(hicann_jtag[hc]->addr()) != Stage2Comm::ok) {
// 				dbg(0) << "ERROR: Init failed, abort" << endl;
// 				return 0;
// 			}
// 			for(uint t=0;t<2;t++) hicann_jtag[hc]->tags[t]->arq.Reset();
// 			// read back via JTAG
// 			for(uint l1c=0; l1c<6; l1c++){
// 				for(uint a=0; a<112; a++){
// 					if(!(l1c<2 && a>=64)){ // crossbars on no. 0 and 1 are valid only up to 64 addresses!
// 						testdata = switches[hc][l1c][a];
// 						if(testdata != 0){ // only compare memories that have been actually written...
// 							switch(l1c){
// 								case 0:hicann_jtag[hc]->getLC_cl().read_cfg(a);
// 								       hicann_jtag[hc]->getLC_cl().get_read_cfg(rcvaddr, rcvdata);break;
// 								case 1:hicann_jtag[hc]->getLC_cr().read_cfg(a);
// 								       hicann_jtag[hc]->getLC_cr().get_read_cfg(rcvaddr, rcvdata);break;
// 								case 2:hicann_jtag[hc]->getLC_tl().read_cfg(a);
// 								       hicann_jtag[hc]->getLC_tl().get_read_cfg(rcvaddr, rcvdata);break;
// 								case 3:hicann_jtag[hc]->getLC_bl().read_cfg(a);
// 								       hicann_jtag[hc]->getLC_bl().get_read_cfg(rcvaddr, rcvdata);break;
// 								case 4:hicann_jtag[hc]->getLC_tr().read_cfg(a);
// 								       hicann_jtag[hc]->getLC_tr().get_read_cfg(rcvaddr, rcvdata);break;
// 								case 5:hicann_jtag[hc]->getLC_br().read_cfg(a);
// 								       hicann_jtag[hc]->getLC_br().get_read_cfg(rcvaddr, rcvdata);break;
// 							}
// 							cout << "HICANN " << hc << ", L1 " << l1c << hex << ", test \t0x" << a << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
// 							if(!(a==rcvaddr && testdata==rcvdata)){
// 								error++;
// 								cout << " ERROR :-(" << endl;
// 								result = false;
// 							} else {
// 								ok++;
// 								cout << " OK :-)" << endl;
// 							}
// 						}
// 					}
// 				}
// 			}
// 
// 			log(Logger::INFO) << "errors:  " << dec << error;
// 			log(Logger::INFO) << "ok:  " << dec << ok;
// 
// 		
// 			// shutdown any rameining arq activity in HICANN...
// 			if (local_comm->Init(hicann_jtag[hc]->addr()) != Stage2Comm::ok) {
// 				dbg(0) << "ERROR: Init failed, abort" << endl;
// 				return 0;
// 			}
// 			for(uint t=0;t<2;t++) hicann_jtag[hc]->tags[t]->arq.Reset();
// 		
// 		}
// 
		log(Logger::INFO) << *local_comm_hs;

		return !result; // shell wants 0 for success
	}

unsigned int readPulses(const char *filename, std::vector<pulse_float_t> &pulselist) {
	FILE *fin = fopen(filename, "r");
	pulselist.clear();

	char curr_line[80];
	while ( fgets(curr_line, 80, fin) ) {
		if (curr_line[0] == '#') // comment line
			continue;

		double curr_id = 0;
		double curr_time = 0.0;

		if ( sscanf(curr_line,"%lf %lf",&curr_time,&curr_id) != 2) {
			log(Logger::WARNING) << "readPulses warning: Ignoring undecodable line " << curr_line;
			continue;
		}

		pulselist.push_back( pulse_float_t((unsigned int)(curr_id),curr_time) );
	}

	fclose(fin);
	return pulselist.size();
}

bool writeTracePulses(const char *filename, std::vector<pulse_float_t> pulselist) {
	FILE *fout = fopen(filename, "w");

	for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
		fprintf(fout, "%.2f\t%u\n",pulselist[npulse].time,pulselist[npulse].id);

	fclose(fout);

	return true;
}

};


class LteeTmARQBurnin : public Listee<Testmode>{
public:
	LteeTmARQBurnin() : Listee<Testmode>(string("tm_arq_burnin"), string("ARQ long-term test using switch memories of up to 8 HICANNs")){};
	Testmode * create(){return new TmARQBurnin();};
};

LteeTmARQBurnin ListeeTmARQBurnin;

