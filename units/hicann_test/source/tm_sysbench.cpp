// testmode for system reset, l2 communication measure and initial hardware tests
// benchmark functions copied from tm_l2bench

#include <time.h>

#include "common.h"
#ifdef NCSIM
#include "systemc.h"
#endif

#include "testmode.h" //Testmode and Lister classes
#include "ramtest.h"
#include "syn_trans.h"

#include "s2comm.h"

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

#include "pulse_float.h"

#define fpga_master   1
using namespace std;
using namespace facets;

//binary output in stream
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


class TmSysBench : public Testmode{

public:
	int mem_size;
	ci_data_t rcvdata;
	ci_addr_t rcvaddr;
	uint error;
	uint rseed;

	enum rc_loc {rc_l=0, rc_r=1, rc_tl=2, rc_bl=3, rc_tr=4, rc_br=5}; //repeater locations
	enum lc_loc {lc_l=0, lc_r=1, lc_tl=2, lc_bl=3, lc_tr=4, lc_br=5}; //switch matrix locations
	enum fc_loc                 {fc_tl=0, fc_bl=1, fc_tr=2, fc_br=3}; //fg controller locations
	enum sc_loc                                    {sc_t=0,  sc_b=1}; //synapse controller locations

	enum rc_regs {rc_config=0x80,rc_status=0x80,rc_td0=0x81,rc_td1=0x87,rc_scfg=0xa0};
	enum rc_srambits {rc_touten=0x80,rc_tinen=0x40,rc_recen=0x20,rc_dir=0x10,rc_ren1=8,rc_ren0=4,rc_len1=2,rc_len0=1};
	enum rc_config
	{rc_fextcap1=0x80,rc_fextcap0=0x40,rc_drvresetb=0x20,rc_dllresetb=0x10,rc_start_tdi1=8,rc_start_tdi0=4,rc_start_tdo1=2,rc_start_tdo0=1};

	enum teststat {pass, fail, noexec};
	
	friend std::ostream & operator <<(std::ostream &os, teststat ts){
		os<<(ts==pass?"pass":(ts==fail?"fail":"not executed"));
		return os;
	}
	
	friend std::fstream & operator <<(std::fstream &os, teststat ts){
		os<<(ts==pass?"pass":(ts==fail?"fail":"not executed"));
		return os;
	}
						
	teststat test_id, test_fpga_initall, test_fpga_init1, test_fpga_init2, test_fpga_init3, test_fpga_dnc, test_dnc_hicann, test_spl1_fpga,
	         test_sw, test_rep, test_fg, test_synblk0, test_synblk1;


	S2C_JtagPhys* s2c_jtag;

	HicannCtrl  *hc;
	DNCControl  *dc;
	FPGAControl *fpc;

	FGControl *fc[4];
	NeuronBuilderControl *nbc;
	NeuronControl *nc;
	RepeaterControl *rc[6];
	SynapseControl *sc[2];
	L1SwitchControl *lc[6];
	SPL1Control *spc;

	uint num_hicanns, hicann_nr, hicann_jtag_nr, reticle_nr;
	double fpga_dnc_errate, dnc_hicann_errate, spl1_fpga_errate;

	uint test_sw_errors, test_repeaterram_errors, test_fgram_errors;
	uint initall_res;


	// test function
	
	// #########################################################
	// to enable MPI, configure waf with
	// CC=mpicc CXX=mpicxx ./waf configure --with-mpi
	// #########################################################
	
	bool test() 
	{
		s2c_jtag = dynamic_cast<S2C_JtagPhys*>(comm);
		
		if(!s2c_jtag){
			dbg(0) << "Wrong Comm Class!!! Must use -bje (S2C_JtagPhys)! - EXIT" << endl;
			exit(EXIT_FAILURE);
		}

		rseed = 0;
		
		uint runs;
		uint jtag_loops;

#ifdef MPIMODE
		world.barrier();
		if (MPIMASTER) {
#endif
			cout << "What Reticle are you running on?: ";
			cin >> reticle_nr;
#ifdef MPIMODE
		}
		mpi::broadcast(world, reticle_nr, 0);
		reticle_nr = reticle_nr + (comm->getConnId().get_jtag_port() - 1700);
#endif

		hc  = (HicannCtrl*)  chip[FPGA_COUNT+DNC_COUNT+0];
		dc  = (DNCControl*)  chip[FPGA_COUNT]; // use DNC
		fpc = (FPGAControl*) chip[0]; // use FPGA

		nbc       = &hc->getNBC();
		nc        = &hc->getNC();
		fc[fc_tl] = &hc->getFC_tl();
		fc[fc_tr] = &hc->getFC_tr();
		fc[fc_bl] = &hc->getFC_bl();
		fc[fc_br] = &hc->getFC_br();
		sc[sc_t]  = &hc->getSC_top();
		sc[sc_b]  = &hc->getSC_bot();
		lc[lc_l]  = &hc->getLC_cl();
		lc[lc_r]  = &hc->getLC_cr();
		lc[lc_tl] = &hc->getLC_tl();
		lc[lc_bl] = &hc->getLC_bl();
		lc[lc_tr] = &hc->getLC_tr();
		lc[lc_br] = &hc->getLC_br();
		rc[rc_l]  = &hc->getRC_cl();
		rc[rc_r]  = &hc->getRC_cr();
		rc[rc_tl] = &hc->getRC_tl();
		rc[rc_bl] = &hc->getRC_bl();
		rc[rc_tr] = &hc->getRC_tr();
		rc[rc_br] = &hc->getRC_br();
		spc       = &hc->getSPL1Control();

		char c;
		bool cont=true;
		do {

#ifdef MPIMODE
			world.barrier();
			cout << "Running in MPI mode!" << endl;
			if (MPIMASTER) {
#endif
				cout << "Select test option:" << endl;
				cout << "1: Test JTAG chains " << endl;
				cout << "2: Reset all Floating Gates to zero" << endl;
				cout << "3: Test high-speed connections " << endl;
				cout << "4: Functional HICANN tests (digital only)" << endl;
				cout << "x: exit" << endl;

				cin >> c;
				cout << endl;
#ifdef MPIMODE
			}
			mpi::broadcast(world, c, 0);
#endif
        
			switch(c) {

				case '1':{  //Test JTAG on all active reticles (with bypass)
					
					jtag_loops = 1000000;
					
					srand(rseed);
					// first reset everything...
					reset_all();

					//jtag-loop parameters
					unsigned long failure=0;
					unsigned long trun=0;
					uint64_t tin, tout;
							
					stringstream ss;
					fstream file;
					ss.str("");
					ss << "./results_wafer2/sysbench/results_jtag_reticle_" << reticle_nr << ".dat";
					string filename = ss.str();
					file.open(filename.c_str(), fstream::out);

					file << "JTAG-bypass test parameters:" << endl;
					file << "Powernumber: " << dec << reticle_nr << endl;
#ifdef MPIMODE
					file << "mpi #processes: " << world.size() <<endl;
#else
					file << "mpi #processes: --"<< endl;
#endif		
					file << "Loops: " << jtag_loops << endl <<endl;
					file << "Failures: (run) testinput,testoutput" <<endl;
							
					for(trun =0; trun <jtag_loops; trun++){ 	//repeat jtagchaintest
						
						tin = (((uint64_t)(rand()%0x3fffff))<<32) + (rand()%0xffffffff);
						tout=0;
						comm->jtag->bypass<uint64_t>(tin, 64, tout);
						tout = tout >> comm->jtag->pos_fpga+1;
						
						//write in file, if failure
						if (tin != tout){
							failure++;
							file << "(" << dec << trun << ") 0x" << hex << tin << ", 0x" << hex <<tout << endl;
						}

						//cancel jtag-bypass test
						if (trun > 100 && failure > (0.95*trun)){
							trun++;
							break;
						}
						
						if(trun%10000 == 0){
#ifdef MPIMODE
							cout << "MPI instance " << world.rank() << ": " << trun << "loops" << endl;
#else
							cout << trun << "loops" << endl;
#endif		
						}
					}
					
					file << "result [failure(loops)]: " << dec <<failure <<"("<< dec << trun << ")"<< endl;
					file.flush();
					file.close();	
					
					cout << "failure: " << dec << failure <<endl;
					cout << "loops: " << dec << trun <<endl;
					cout << "=================================" <<endl;
					cout << endl;
				} break;

				case '2':{  //Test JTAG on all active reticles (with bypass)
					
					stringstream ss;
					fstream file;
					ss.str("");
					ss << "./results_wafer2/sysbench/results_fgzero_reticle_" << reticle_nr << ".dat";
					string filename = ss.str();
					file.open(filename.c_str(), fstream::out);

					for(uint hc_jtnr=0; hc_jtnr<8; hc_jtnr++){
						hicann_jtag_nr = hc_jtnr;
						hicann_nr      = 7-hc_jtnr;
						jtag->set_hicann_pos(hicann_jtag_nr);
						hc->setAddr(hc_jtnr);

						// first reset everything...
						reset_all();
						hc->GetCommObj()->Init(hc->addr());
					
						file << "Write FGs of JTAG-HICANN " << hc_jtnr << " to zero result: " << (write_fg_zeros() ? "pass" : "fail") << endl;
						
					}

					file.flush();
					file.close();
				} break;

				case '3':{
					// first reset everything...
					reset_all();

					// ####################################################################################
					cout << "Verifying JTAG IDs..." << endl;
					uint64_t jtagid;
					test_id = pass;

					//cout << "How many runs?: ";
					//cin >> runs;
					runs = 1;

					jtag->read_id(jtagid,jtag->pos_hicann);
					cout << "HICANN ID: 0x" << hex << jtagid << endl;
					if (jtagid != 0x14849434) test_id = fail;

					jtag->read_id(jtagid,jtag->pos_dnc);
					cout << "DNC ID: 0x" << hex << jtagid << endl;
					if (jtagid != 0x1474346f) test_id = fail;

					jtag->read_id(jtagid,jtag->pos_fpga);
					cout << "FPGA ID: 0x" << hex << jtagid << endl;
					if (jtagid != 0xbabebeef) test_id = fail;


					// ####################################################################################
					for(uint truns=0; truns<runs; truns++){

						srand(rseed);
			
						cout << endl << "********** TEST RUN " << truns << ": **********" << endl << endl;

						// ####################################################################################
						cout << "Testing Init of all connections simulataneously..." << endl;
						reset_all();
						initall_res = initAllConnections(true);
						test_fpga_initall = (initall_res == 0x1ff) ? pass : fail;
						//Shutdown interfaces
						dnc_shutdown();
						for(uint hc_jtnr=0; hc_jtnr<8; hc_jtnr++){
					 		jtag->set_hicann_pos(hc_jtnr);
							hicann_shutdown();
						}
						jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
						cout << "Init all connections: 0x" << hex << initall_res << ": " << test_fpga_initall << endl;
			

						// ####################################################################################
						for(uint hc_jtnr=0; hc_jtnr<8; hc_jtnr++){
							hicann_jtag_nr = hc_jtnr;
							hicann_nr      = 7-hc_jtnr;
					 		jtag->set_hicann_pos(hicann_jtag_nr);
							hc->setAddr(hc_jtnr);

					 		cout << endl << "Using DNC channel " << hicann_nr << endl;
					 		cout << "HICANN JTAG position " << hicann_jtag_nr << endl;
		
							test_fpga_init1 = noexec;
							test_fpga_init2 = noexec;
							test_fpga_init3 = noexec;
							test_fpga_dnc = noexec;
							test_dnc_hicann = noexec;
							test_spl1_fpga = noexec;

							reset_all();

							// ####################################################################################
							unsigned int runtime_s = 20; // sec
							test_fpga_init1 = pass;
							test_fpga_init2 = pass;
							test_fpga_init3 = pass;

							cout << "Testing FPGA-DNC connection..." << endl;
							reset_all();
							if ( !initConnectionFPGA() ) {
								cout << "Stop FPGA benchmark : INIT ERROR" << endl;
								test_fpga_init1 = fail;
								//continue;
							} else {
								test_fpga_dnc = runFpgaBenchmark(runtime_s) ? pass : fail;
								//Shutdown interfaces
								dnc_shutdown();
								hicann_shutdown();
								jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
								cout <<  "Finished benchmark of FPGA-DNC connection" << endl;
							}
	
							// ####################################################################################
							cout << "Testing DNC-HICANN connection..." << endl;
							reset_all();
							if ( !initConnectionFPGA() ) {
								cout << "Stop DNC-HICANN benchmark : INIT ERROR" << endl;
								test_fpga_init2 = fail;
								//continue;
							} else {
								test_dnc_hicann = runHicannBenchmark(runtime_s) ? pass : fail;
								//Shutdown interfaces
								dnc_shutdown();
								hicann_shutdown();
								jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
								cout <<  "Finished benchmark of DNC-HICANN connection" << endl;
							}

							// ####################################################################################
							cout << "Testing FPGA-DNC-HICANN L2 loopback..." << endl;
							reset_all();
							if ( !initConnectionFPGA() ) {
								cout << "Stop full benchmark : INIT ERROR" << endl;
								test_fpga_init3 = fail;
								//continue;
							} else {
								test_spl1_fpga = runFullBenchmark(runtime_s) ? pass : fail;
								//Shutdown interfaces
								dnc_shutdown();
								hicann_shutdown();
								jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
								cout << "Finished benchmark of FPGA-DNC-HICANN connection" << endl;
							}

							write_summary_hs(truns);
				    }
			    	rseed++;
					}
				} break;

				case '4':{
					srand(rseed);
					
					// ####################################################################################
					// all communication devices now reset and not initialized, therefore call the
					// standard Init() procedure after all resets, below!

					// ####################################################################################
					for(uint hc_jtnr=0; hc_jtnr<8; hc_jtnr++){
						hicann_jtag_nr = hc_jtnr;
						hicann_nr      = 7-hc_jtnr;
						jtag->set_hicann_pos(hicann_jtag_nr);
						hc->setAddr(hc_jtnr);

						cout << endl << "Using DNC channel " << hicann_nr << endl;
						cout << "HICANN JTAG position " << hicann_jtag_nr << endl;
		
						reset_all();
						hc->GetCommObj()->Init(hc->addr());

						test_sw = noexec;
						test_rep = noexec;
						test_fg = noexec;
						test_synblk0 = noexec;
						test_synblk1 = noexec;

						// ####################################################################################
						// now reset HICANN custom SRAMs...
						cout << "resetting everything" << endl;
						cout << "resetting synapse drivers" << endl;
						sc[sc_t]->reset_drivers();
						sc[sc_b]->reset_drivers();
						// switch configuration is reset to zero upon reset!
						cout << "resetting repeater blocks" << endl;
						rc[rc_l]->reset();
						rc[rc_tl]->reset();
						rc[rc_tr]->reset();
						rc[rc_bl]->reset();
						rc[rc_br]->reset();
						rc[rc_r]->reset();
						cout << "resetting neuron builder" << endl;
						nbc->initzeros();
						cout << "resetting neuron control" << endl;
						nc->nc_reset();

						// ####################################################################################
						// perform digital tests
						uint loops = 6;
						uint synramloops = 2;

		    		cout << "Testing   Switch ram... " << flush;
						reset_all();
						hc->GetCommObj()->Init(hc->addr());
						test_sw_errors = test_switchram(loops);
						test_sw = (test_sw_errors==0) ? pass : fail;
						cout << test_sw << ", errors: " << test_sw_errors << endl;

						cout << "Testing Repeater ram... " << flush;
						reset_all();
						hc->GetCommObj()->Init(hc->addr());
						test_repeaterram_errors = test_repeaterram(loops);
						test_rep = (test_repeaterram_errors==0) ? pass : fail;
						cout << test_rep << ", errors: " << test_repeaterram_errors << endl;

						cout << "Testing Floating Gate Controller ram... " << flush;
						reset_all();
						hc->GetCommObj()->Init(hc->addr());
						test_fgram_errors = test_fgram(loops);
						test_fg = (test_fgram_errors==0) ? pass : fail;
						cout << test_fg << ", errors: " << test_fgram_errors << endl;

		    		// ####################################################################################
	 					// skip synapse memory tests in case previous tests all failed
						// since everything else worked so far, we do only one reset, here.
						// -> "Identical no. of Resets" is also required for keeping several MPI instances synced wrt. their reset sequences
						reset_all();
						hc->GetCommObj()->Init(hc->addr());
	 					if(test_spl1_fpga==fail || test_sw==fail || test_rep==fail || test_fg==fail){
	 						cout << "Skipping time consuming synapse memory test due to previous errors..." << endl;
	 						write_summary_func();
	 					} else {
	 						cout << "Testing  Syn Block 0... " << flush;
	 						test_synblk0 = test_block(hc, 0, loops, synramloops, loops) ? pass : fail;
	 						cout << test_synblk0 << endl;
	
	 						cout << "Testing  Syn Block 1... " << flush;
	 						test_synblk1 = test_block(hc, 1, loops, synramloops, loops) ? pass : fail;
	 						cout << test_synblk1 << endl;
							write_summary_func();
	 					}

		    		// ####################################################################################
						// re-reset everything...
						reset_all();
						hc->GetCommObj()->Init(hc->addr());
						cout << "resetting everything" << endl;
						cout << "resetting synapse drivers" << endl;
						sc[sc_t]->reset_drivers();
						sc[sc_b]->reset_drivers();
						// switch configuration is reset to zero upon reset!
						cout << "resetting repeater blocks" << endl;
						rc[rc_l]->reset();
						rc[rc_tl]->reset();
						rc[rc_tr]->reset();
						rc[rc_bl]->reset();
						rc[rc_br]->reset();
						rc[rc_r]->reset();
						cout << "resetting neuron builder" << endl;
						nbc->initzeros();
						cout << "resetting neuron control" << endl;
						nc->nc_reset();

						// ####################################################################################
					}
					
				} break;

				case 'x': cont = false; 
			}

		} while(cont);

		return true;
	}


	void reset_all() {
		unsigned int curr_ip = jtag->get_ip();
#ifdef MPIMODE
		world.barrier();
		if (MPIMASTER) {
#endif
			cout << "Pulse FPGA reset..." << endl;
			if (comm != NULL) {
				comm->set_fpga_reset(curr_ip, true, true, true, true, true); // set reset
				usleep(100);
				comm->set_fpga_reset(curr_ip, false, false, false, false, false); // release reset
			} else {
				printf("tm_sysbench error: bad s2comm pointer; did not perform soft FPGA reset.\n");
			}
#ifdef MPIMODE
		} else {
			cout << "Instance no. " << world.rank() << ": Waiting for FPGA reset..." << endl;
		}
		world.barrier();
#endif

		// JTAG reset performed for each reticle, independently
		cout << "Reset JTAG..." << endl;
		jtag->reset_jtag();
	
		// DNC reset performed for each reticle, independently
		cout << "Pulse DNC reset..." << endl;
		jtag->FPGA_set_fpga_ctrl(1);

		// DNC and HICANN reset need to be synched...
#ifdef MPIMODE
		world.barrier();
		if (MPIMASTER) {
#endif
			// leave FPGA ARQ disabled!
			comm->set_fpga_reset(curr_ip, false, false, false, false, /*ARQ:*/ true);
			
			cout << "Pulse Wafer *2* (i.e. HICANN) reset..." << endl;
			// wafer 2 reset control currently on raspeval-001, IP: 129.206.176.202
			comm->ResetWafer(FPGAConnectionId::IPv4::from_string("129.206.176.202"));

#ifdef MPIMODE
		} else {
			cout << "Instance no. " << world.rank() << ": Waiting for DNC/HICANN reset..." << endl;
		}
		world.barrier();
		
		// wait for HICANNs to recover from reset and "desync" instances:
		usleep(500000 * (world.rank()+1));
#endif

		// this became necessary since without the DNC BER on DNC 0 during runFPGABEnchmark was very high!
		// DNC reset performed for each reticle, independently
		cout << "Pulse DNC reset..." << endl;
		jtag->FPGA_set_fpga_ctrl(1);
	}
	

	// reset sw ARQ and ARQ in HICANN:
	void reset_arq(uint hicann_jtag_addr) {
		for(uint j=0; j<comm->link_layers[hicann_jtag_addr].size(); j++)
			comm->link_layers[hicann_jtag_addr][j].arq.Reset();

		// jtag stuff below this line
		jtag->set_hicann_pos(hicann_jtag_addr);

		log(Logger::DEBUG0) << "tm_l2bench:: Asserting ARQ reset for HICANN no. " << hicann_nr << " via JTAG..." << flush;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		jtag->HICANN_arq_write_ctrl(0x0, 0x0);
	}

	void dnc_shutdown() {
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

	void hicann_shutdown() {
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

	bool initConnectionFPGA()
	{
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

	// ********** test methods **********

	void write_summary_hs(uint run){
		stringstream ss;
		fstream file;
		ss.str("");
		ss << "./results_wafer2/sysbench/results_highspeed_reticle_" << reticle_nr << ".dat";
		string filename = ss.str();
		file.open(filename.c_str(), fstream::app | fstream::out);
		
		cout << endl << "########## SUMMARY run " << run << ": ##########" << endl;
		cout << "JTAG ID:                " << test_id << endl;
		cout << "FPGA Init1:             " << test_fpga_init1 << endl;
		cout << "FPGA Init2:             " << test_fpga_init2 << endl;
		cout << "FPGA Init3:             " << test_fpga_init3 << endl;
		cout << "FPGA Init All Conns:    " << test_fpga_initall << endl;
		cout << "FPGA-DNC Bench:         " << test_fpga_dnc << endl;
		cout << "DNC-HICANN Bench:       " << test_dnc_hicann << endl;
		cout << "SPL1 loopback via FPGA: " << test_spl1_fpga << endl;

		file << endl << "############ HICANN " << hicann_jtag_nr << " #############" << endl;
		file << "########## SUMMARY run " << run << ": ##########" << endl;
		file << "JTAG ID:                " << test_id << endl;
		file << "FPGA Init All Conns:    " << test_fpga_initall << endl;
		file << "FPGA Init1:             " << test_fpga_init1 << endl;
		file << "FPGA Init2:             " << test_fpga_init2 << endl;
		file << "FPGA Init3:             " << test_fpga_init3 << endl;
		file << "FPGA-DNC Bench:         " << test_fpga_dnc << endl;
		file << "DNC-HICANN Bench:       " << test_dnc_hicann << endl;
		file << "SPL1 loopback via FPGA: " << test_spl1_fpga << endl;
		if (test_fpga_initall==fail) file << "Init all conn. result:    " << initall_res << endl;
		if (test_fpga_dnc==fail)     file << "FPGA-DNC Error Rate:      " << fpga_dnc_errate << " (per second)" << endl;
		if (test_dnc_hicann==fail)   file << "DNC-HICANN Error Rate:    " << dnc_hicann_errate << " (per second)" << endl;
		if (test_spl1_fpga==fail)    file << "SPL1 loopback Error Rate: " << spl1_fpga_errate << " (per second)" << endl;
		file.flush(); file.close();
	}

	void write_summary_func(){
		stringstream ss;
		fstream file;
		ss.str("");
		ss << "./results_wafer2/sysbench/results_functional_reticle_" << reticle_nr << ".dat";
		string filename = ss.str();
		file.open(filename.c_str(), fstream::app | fstream::out);
		
		cout << endl << "############ HICANN " << hicann_jtag_nr << " #############" << endl;
		cout << endl << "########## SUMMARY functional tests ##########" << endl;
		cout << "Switch config memory:   " << test_sw << "\tErrors: " << test_sw_errors << endl;
		cout << "Repeater config memory: " << test_rep << "\tErrors: " << test_repeaterram_errors << endl;
		cout << "FG controller memory:   " << test_fg << "\tErrors: " << test_fgram_errors << endl;
		cout << "Synapse Block 0 memory: " << test_synblk0 << endl;
		cout << "Synapse Block 1 memory: " << test_synblk1 << endl;

		file << endl << "############ HICANN " << hicann_jtag_nr << " #############" << endl;
		file << "########## SUMMARY functional tests ##########" << endl;
		file << "Switch config memory:   " << test_sw << "\tErrors: " << test_sw_errors << endl;
		file << "Repeater config memory: " << test_rep << "\tErrors: " << test_repeaterram_errors << endl;
		file << "FG controller memory:   " << test_fg << "\tErrors: " << test_fgram_errors << endl;
		file << "Synapse Block 0 memory: " << test_synblk0 << endl;
		file << "Synapse Block 1 memory: " << test_synblk1 << endl;
		file.flush(); file.close();
	}

	bool write_fg_zeros(void) {
		//write currents first as ops need biases before they work.
#ifdef MPIMODE
		cout << "Intance " << world.rank() << ": reseting all floating gates to zero ..." << endl;
#else
		cout << "reseting all floating gates to zero ..." << endl;
#endif
		for(uint fg_num=0; fg_num<4; fg_num++) {
			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(63);
			fc[fg_num]->set_voltagewritetime(63);
			fc[fg_num]->set_readtime(2);
			fc[fg_num]->set_acceleratorstep(1);
			fc[fg_num]->set_pulselength(15);
			fc[fg_num]->set_fg_biasn(15);
			fc[fg_num]->set_fg_bias(15);
			if(fc[fg_num]->write_biasingreg() == 0) return false;
			if(fc[fg_num]->write_operationreg() == 0) return false;
			fc[fg_num]->initzeros(0);
		}
		for(uint lineNumber=0; lineNumber<24; lineNumber++){ //starting at line 1 as this is the first current!!!!!	
			for(uint fg_num=0; fg_num<4; fg_num++){
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore
				if(fc[fg_num]->program_cells(lineNumber,0,0) == 0) return false; //programm down
			}
		}
#ifdef MPIMODE
		cout << "Intance " << world.rank() << ": done resetting fg to zero" << endl;
#else
		cout << "done resetting fg to zero" << endl;
#endif
		return true;
	}

	uint test_switchram(uint runs) {
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
				case 0:l1=lc[lc_l];mem_size=64;rmask=15;break;
				case 1:l1=lc[lc_r];mem_size=64;rmask=15;break;
				case 2:l1=lc[lc_tl];mem_size=112;rmask=0xffff;break;
				case 3:l1=lc[lc_bl];mem_size=112;rmask=0xffff;break;
				case 4:l1=lc[lc_tr];mem_size=112;rmask=0xffff;break;
				case 5:l1=lc[lc_br];mem_size=112;rmask=0xffff;break;
				}

				srand(rseed);
				uint tdata[0x80];
				for(int i=0;i<mem_size;i++)
					{
						tdata[i]=rand() & rmask;
						if(l1->write_cmd(i,tdata[i],0) == 0) return 9999;
					}

				srand(rseed);
				for(int i=0;i<mem_size;i++)
				{
					if(l1->read_cmd(i,0) == 0) return 9999;
					if(l1->get_data(rcvaddr,rcvdata) == 0) return 9999;
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
		return error;
	}


	uint test_repeaterram(uint runs) {
		RepeaterControl *rctl;

		// test repeater sram
		cout << endl << "########## TEST REPEATER SRAM ##########" << endl;

		uint error=0;
		uint lerror=0;
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<6;r++){
				switch(r) {
				case 0 : rctl = rc[rc_l];  mem_size=32; break;
				case 1 : rctl = rc[rc_r];  mem_size=32; break;
				case 2 : rctl = rc[rc_tl]; mem_size=64; break;
				case 3 : rctl = rc[rc_bl]; mem_size=64; break;
				case 4 : rctl = rc[rc_tr]; mem_size=64; break;
				default: rctl = rc[rc_br]; mem_size=64; break;
        }
        
				cout<<"test repeater sram:" << r << endl;

				srand(rseed);
				const uint rmask=0x5f; //always keep recen and touten zero!
				uint tdata[0x40];
				for(int i=0;i<mem_size;i++){
					tdata[i]=rand()%256 & rmask;
					if(rctl->write_data(i,tdata[i]) == 0) return 9999;
				}

				srand(rseed);
				for(int i=0;i<mem_size;i++){
					rcvdata = rctl->read_data(i);
					if(rcvdata == 0xffffffff) return 9999;
					if(rcvdata != tdata[i]){
						error++;
						lerror++;
						cout << hex << "radr \t0x"<<i<<", \t0b"  << bino(rcvdata,8) << "	data written: 0b" <<
							bino(tdata[i],8)<<endl;
					}
				}
			}
			cout <<"Repeater SRAM test loop " << l << ", errors: " << lerror << ", total errors: " << error << endl;
		}
		cout << endl;
		return error;
	}


	uint test_fgram(uint runs) {
		// full floating gate controller memory test
		cout << endl << "########## TEST FLOATING GATE CONTROLLEr MEMORY ##########" << endl;
		
		uint error=0;
		uint lerror=0;
		FGControl *fctl;
		uint rmask; 
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<4;r++){
				switch (r){
				case 0:fctl=fc[fc_tl];mem_size=65;rmask=0xfffff;break;
				case 1:fctl=fc[fc_bl];mem_size=65;rmask=0xfffff;break;
				case 2:fctl=fc[fc_tr];mem_size=65;rmask=0xfffff;break;
				case 3:fctl=fc[fc_br];mem_size=65;rmask=0xfffff;break;
				}

				for(uint bank=0;bank<2;bank++){
					srand(rseed);
					uint tdata[0x100];
					for(int i=0+bank*128;i<mem_size+bank*128;i++)
						{
							tdata[i]=rand() & rmask;
							if(fctl->write_cmd(i,tdata[i],0) == 0) return 9999;
						}

					srand(rseed);
					for(int i=0+bank*128;i<mem_size+bank*128;i++)
					{
						if(fctl->read_cmd(i,0) == 0) return 9999;
						if(fctl->get_data(rcvaddr,rcvdata) == 0) return 9999;
						if(rcvdata != tdata[i]){
							error++;
							lerror++;
							cout << hex << "ERROR: bank \t" << bank << ", row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<
								bino(tdata[i],16)<<endl;
						}
					}
					cout <<"FG controller ram test loop " << l << " errors: " << lerror << ", total errors: " << error << endl;
				}
			}
			cout << endl;
		}
		return error;
	}

	bool test_block(HicannCtrl* hc, int block, int reg_loops, int synapse_ramtest_loops, int syndrv_ramtest_loops)
    {
        struct {
            bool cfg;
            bool creg;
            bool lut;
            bool synrst;
            bool syn_weight;
            bool syn_weight_encr;
            bool syn_dec;
            bool syn_dec_encr;
            bool syndrv_sram;
            bool syndrv_sram_ext;
        } test_report = { true, true, true, true, true, true, true, true, true, true };

        static const int syn_ramtest_start = 0;
        static const int syn_ramtest_stop = 223;
        static const int syndrv_ramtest_start = Syn_trans::syndrv_ctrl;
        static const int syndrv_ramtest_stop = Syn_trans::syndrv_gmax + 0xff;
        static const int syndrv_block_size = 224;
        //static const int syndrv_ramtest_stop = Syn_trans::syndrv_ctrl + 3;

        assert((syn_ramtest_start >= 0) && (syn_ramtest_start <= 223));
        assert((syn_ramtest_start >= 0) && (syn_ramtest_stop <= 223));
        assert(syn_ramtest_start <= syn_ramtest_stop);

				cout << endl << "########## TEST STDP CTRL ##########" << endl;

				CtrlModule *rc = static_cast<CtrlModule*>(sc[block]);

        bool result = true;
        Syn_trans syn(rc);
        Syn_trans::Config_reg cfg;
        Syn_trans::Control_reg creg;
        Syn_trans::Lut_data lut_a, lut_b, lut_ab;
        Syn_trans::Syn_corr synrst;

        // check config register
        test_report.cfg = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Config_reg cfg_check;
            cfg.randomize();
            syn.set_config_reg(cfg);
            cfg_check = syn.get_config_reg();

            if( !(cfg == cfg_check ) ) {
                cout << "read/write check of config register failed" << endl;
                result = false;
                test_report.cfg = false;
            }
        }
				cout << "Random test of config register (" << reg_loops << "x) completed" << endl << endl;

        cfg.dllresetb[0] = false;  // turning on DLL-resets
        cfg.dllresetb[1] = false;
        cfg.gen[0] = true;
        cfg.gen[1] = true;
        cfg.gen[2] = true;
        cfg.gen[3] = true;
        cfg.pattern[0] = Syn_trans::CORR_READ_CAUSAL;
        cfg.pattern[1] = Syn_trans::CORR_READ_ACAUSAL;
        cfg.predel = 0xf;
        cfg.endel = 0xf;
        cfg.oedel = 0xf;
        cfg.wrdel = 0x2;

        syn.set_config_reg(cfg);


        // check control register
        test_report.creg = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Control_reg creg_check;

            creg.randomize();
            creg.cmd = Syn_trans::DSC_IDLE;
            syn.set_control_reg(creg);
            creg_check = syn.get_control_reg();

            if( !(creg == creg_check) ) {
                cout << "read/write check of control register failed" << endl;
                cout << "  got: 0x" << hex << creg_check.pack()
                   << "  expected: 0x" << creg.pack() << dec << endl;
                result = false;
                test_report.creg = false;
            }
        }
				cout << "Random test of control register (" << reg_loops << "x) completed" << endl << endl;

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.encr = false;
        creg.continuous = false;
        creg.newcmd = true;
        creg.adr = 0;
        creg.lastadr = 224;
        creg.sel = 0;
        creg.without_reset = true;
        creg.sca = true;
        creg.scc = true;

        syn.set_control_reg(creg);


        // check LUT registers
        test_report.lut = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Lut_data lut_a_check, lut_b_check, lut_ab_check;

            for(int j=0; j<16; j++) {
                lut_a[j] = random() & 0xf;
                lut_b[j] = random() & 0xf;
                lut_ab[j] = random() & 0xf;
            }

            syn.set_lut(lut_a, 0);
            syn.set_lut(lut_b, 1);
            syn.set_lut(lut_ab, 2);

            syn.get_lut(lut_a_check, 0);
            syn.get_lut(lut_b_check, 1);
            syn.get_lut(lut_ab_check, 2);

            for(int j=0; j<16; j++) {
                if( !(lut_a[j] == lut_a_check[j]) 
                    && !(lut_b[j] == lut_b_check[j])
                    && !(lut_ab[j] == lut_ab_check[j]) )
                {
                    cout << "Read/write test of LUT failed at index " << dec << j << endl;
                    result = false;
                    test_report.lut = false;
                }
            }
        }
				cout << "Random test of STDP LUTs (" << reg_loops << "x) completed" << endl << endl;

        for(int j=0; j<16; j++) {
            lut_a[j] = 0xf;
            lut_b[j] = 0x0;
            lut_ab[j] = j;
        }

        syn.set_lut(lut_a, 0);
        syn.set_lut(lut_b, 1);
        syn.set_lut(lut_ab, 2);


        // check SYNRST register
        test_report.synrst = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Syn_corr synrst_check;

            for(int i=0; i<2; i++) {
                for(int j=0; j<4; j++) {
                    synrst[j][i] = random() & 0xff;
                }
            }

            syn.set_synrst(synrst);
            syn.get_synrst(synrst_check);

            for(int i=0; i<2; i++) {
                for(int j=0; j<4; j++) {
                    if( synrst[j][i] != synrst_check[j][i] ) {
                        cout << "Read/write test of SYNRST failed at pattern " << dec << i
                           << " slice " << j << endl;
                        test_report.synrst = false;
                    }
                }
            }
        }
        cout << "Random test of correlation reset register (" << reg_loops << "x) completed" << endl << endl;
        

        cout << "Performing random correlation reset commands (" << reg_loops << "x)..." << endl;

        // reset correlation command test
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Syn_corr synrst_check;

            for(int i=0; i<2; i++) {
                for(int j=0; j<4; j++) {
                    synrst[j][i] = random() & 0xff;
                }
            }

            syn.set_synrst(synrst);

            creg.cmd = Syn_trans::DSC_START_READ;
            creg.adr = 2;
            syn.set_control_reg(creg);
            syn.wait_while_busy();

            creg.cmd = Syn_trans::DSC_RST_CORR;
            syn.set_control_reg(creg);
            syn.wait_while_busy();

            creg.cmd = Syn_trans::DSC_CLOSE_ROW;
            syn.set_control_reg(creg);
            syn.wait_while_busy();
        }

        
        creg.cmd = Syn_trans::DSC_IDLE;
        creg.adr = 0;
        syn.set_control_reg(creg);

        for(int i=0; i<2; i++) {
            for(int j=0; j<4; j++) {
                synrst[j][i] = 0xff;
            }
        }

        syn.set_synrst(synrst);

        cout << "Ramtest on four locations of every syndriver SRAM block" << endl;

        test_report.syndrv_sram_ext = ramtest(rc, Syn_trans::syndrv_ctrl,
                Syn_trans::syndrv_ctrl+0xdf, true);

        test_report.syndrv_sram_ext = test_report.syndrv_sram_ext
            && ramtest(rc, Syn_trans::syndrv_drv,
                Syn_trans::syndrv_drv+0xdf, true);
        
        test_report.syndrv_sram_ext = test_report.syndrv_sram_ext 
            && ramtest(rc, Syn_trans::syndrv_gmax,
                Syn_trans::syndrv_gmax+0xdf, true);

        cout << "Starting synapse weights ramtest (" << synapse_ramtest_loops << "x)..." << endl;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_weight = syn_ramtest(syn, 
                    syn_ramtest_start, 
                    syn_ramtest_stop)
                && test_report.syn_weight;

        cout << "Starting synapse decoder addresses ramtest (" << synapse_ramtest_loops << "x)..." << endl;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_dec = syn_ramtest_dec(syn,
                    syn_ramtest_start,
                    syn_ramtest_stop) 
                && test_report.syn_dec;

        // read again with correlation activated (only one run each)
				uint corr_runs = 0;
        cout << "Starting synapse weights ramtest with correlation readout (" << corr_runs << "x)..." << endl;
        for(int i=0; i<corr_runs; i++)
            test_report.syn_weight_encr = syn_ramtest(syn, syn_ramtest_start, syn_ramtest_stop, true)
                && test_report.syn_weight_encr;

        cout << "Starting synapse decoder addresses ramtest with correlation readout (" << corr_runs << "x)..." << endl;
        for(int i=0; i<corr_runs; i++)
            test_report.syn_dec_encr = syn_ramtest_dec(syn, syn_ramtest_start, syn_ramtest_stop, true)
                && test_report.syn_dec_encr;

        cout << "Starting synapse driver SRAM ramtest on CTRL block (" << syndrv_ramtest_loops << "x)..." << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = ramtest(rc, Syn_trans::syndrv_ctrl,
                    Syn_trans::syndrv_ctrl+syndrv_block_size-1, true)
                && test_report.syndrv_sram;

        cout << "Starting synapse driver SRAM ramtest on DRV block (" << syndrv_ramtest_loops << "x)..." << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = test_report.syndrv_sram && ramtest(rc, Syn_trans::syndrv_drv,
                    Syn_trans::syndrv_drv+syndrv_block_size-1, true)
                && test_report.syndrv_sram;
        
        cout << "Starting synapse driver SRAM ramtest on GMAX block (" << syndrv_ramtest_loops << "x)..." << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = test_report.syndrv_sram && ramtest(rc, Syn_trans::syndrv_gmax,
                    Syn_trans::syndrv_gmax+syndrv_block_size-1, true)
                && test_report.syndrv_sram;


        // run one update cycle of auto_update
        creg.cmd = Syn_trans::DSC_AUTO;
        creg.sel = 0;
        creg.encr = true;
        creg.adr = 0;
        creg.lastadr = 2;
        creg.continuous = 0;

        syn.set_control_reg(creg);
        syn.wait_while_busy();

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.lastadr = 223;
        syn.set_control_reg(creg);


        result = test_report.cfg 
            && test_report.creg 
            && test_report.lut 
            && test_report.synrst
            && test_report.syn_weight
            && test_report.syn_weight_encr
            && test_report.syn_dec
            && test_report.syn_dec_encr
            && test_report.syndrv_sram
            && test_report.syndrv_sram_ext;

        cout << "<<<< STDP report >>>>"
            << dec
            << "\nreg_loops         = " << reg_loops
            << "\nstart_row         = " << syn_ramtest_start << " (full: 0)"
            << "\nstop_row          = " << syn_ramtest_stop << " (full: 223)"
            << "\nsyndrv_block_size = " << syndrv_block_size << " (full: 224)"
            << "\n----" 
            << "\ncfg:.........." << (test_report.cfg ? "ok" : "failed")
            << "\ncreg:........." << (test_report.creg ? "ok" : "failed")
            << "\nlut:.........." << (test_report.lut ? "ok" : "failed")
            << "\nsynrst:......." << (test_report.synrst ? "ok" : "failed")
            << "\nsyn weights:.." << (test_report.syn_weight ? "ok" : "failed")
            << "\n +with encr:.." << (test_report.syn_weight_encr ? "ok" : "failed")
            << "\nsyn decoders:." << (test_report.syn_dec ? "ok" : "failed")
            << "\n +with encr:.." << (test_report.syn_dec_encr? "ok" : "failed")
            << "\nsyndrv sram:.." << (test_report.syndrv_sram ? "ok" : "failed")
            << "\n +ext:........" << (test_report.syndrv_sram_ext ? "ok" : "failed")
            << endl << endl;

        return result;
    }

    bool ramtest(CtrlModule* rc, 
        int low_addr, 
        int high_addr,
      	bool two_reads_mode = false)
    {
        std::map<int,uint> test_data;
        bool rv = true;

        cout << "***Starting ramtest from " << low_addr << " to " << high_addr << '\n';
        for(int i=low_addr; i<=high_addr; i++) {
            uint tdat = rand();
            test_data[i] = tdat;

            rc->write_cmd(i, tdat, 0);
        }

        cout << "Initiating readback..." << endl;

        for(int i=low_addr; i<=high_addr; i++) {
            ci_payload res;
            uint8_t expected, received;

            rc->read_cmd(i, 0);
            rc->get_data(&res);         
            
            if( two_reads_mode ) {
                rc->read_cmd(i, 0);
                rc->get_data(&res);
            }

            if( (i>>1) % 2 == 0 ) {
                expected = test_data[i] & 0xff;
                received = res.data & 0xff;
            } else {
                expected = (test_data[i] & 0xff00) >> 8;
                received = (res.data & 0xff00) >> 8;
            }

            if( expected != received ) {
                cout 
                    << "  *** Ramtest failed at address = 0x" << hex << setw(8) << setfill('0') << i
                    << " (expected: 0x" << setw(2) << setfill('0') << (int)expected
                    << ", received: 0x" << setw(2) << setfill('0') << (int)received << dec << ")"
                    << endl;
                rv = false;
            }
        }

        cout << "***Ramtest complete" << endl << endl;
        return rv;
    }

    bool syn_ramtest(Syn_trans& syn, int row_a = 0, int row_b = 223, bool encr = false)
    {
        return syn_ramtest_templ<Syn_trans::DSC_START_READ, 
                Syn_trans::DSC_READ,
                Syn_trans::DSC_WRITE>(syn, row_a, row_b, encr);
    }

    bool syn_ramtest_dec(Syn_trans& syn, int row_a = 0, int row_b = 223, bool encr = false)
    {
        return syn_ramtest_templ<Syn_trans::DSC_START_RDEC, 
                Syn_trans::DSC_RDEC,
                Syn_trans::DSC_WDEC>(syn, row_a, row_b, encr);
    }

    template<Syn_trans::Dsc_cmd row_cmd, Syn_trans::Dsc_cmd read_cmd, Syn_trans::Dsc_cmd write_cmd>
    bool syn_ramtest_templ(Syn_trans& syn, const int row_a = 0, const int row_b = 223, bool encr = false)
    {
        bool result = true;
        Syn_trans::Syn_io sio[224][8];
        Syn_trans::Syn_io sio_check;
        Syn_trans::Control_reg creg;

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.encr = encr;
        creg.continuous = false;
        creg.newcmd = true;
        creg.adr = 0;
        creg.lastadr = 223;
        creg.sel = 0;
        creg.without_reset = true;
        creg.sca = true;
        creg.scc = true;

        for(int row=row_a; row<=row_b; row++) {
            creg.adr = row;

            for(int col_set=0; col_set<8/*8*/; col_set++) {
                creg.sel = col_set;

                for(int i=0; i<4; i++)
                    for(int j=0; j<8; j++)
                        sio[row][col_set][i][j] = random() & 0xf;

                syn.set_synin(sio[row][col_set]);

                creg.cmd = write_cmd;
                syn.set_control_reg(creg);
                syn.wait_while_busy();
            }
        }

        for(int row=row_a; row<=row_b; row++) {
            creg.adr = row;
            creg.cmd = row_cmd;
            syn.set_control_reg(creg);
            syn.wait_while_busy();

            for(int col_set=0; col_set<8/*8*/; col_set++) {
                creg.sel = col_set;
                creg.cmd = read_cmd;
                syn.set_control_reg(creg);
                syn.wait_while_busy();

                syn.get_synout(sio_check);

                for(int i=0; i<4; i++) {
                    for(int j=0; j<8; j++) {
                        if( sio[row][col_set][i][j] != sio_check[i][j] ) {
                            cout << "Read/write test on synapse weight memory failed at "
                                << dec << "row: " << row << " col_set: " << col_set 
                                << " slice: " << i << " column: " << j
                                << " (expected: " << hex << (int)sio[row][col_set][i][j]
                                << ", got: " << (int)sio_check[i][j] << ")"
                                << dec << endl;
                            result = false;
                        }
                    }
                }
            }

            creg.adr = row;
            creg.cmd = Syn_trans::DSC_CLOSE_ROW;
            syn.set_control_reg(creg);
            syn.wait_while_busy();
        }

        return result;
    }

	unsigned int initAllConnections(bool silent)
	{
		unsigned char state1[9] = {0,0,0,0,0,0,0,0,0};
		uint64_t state2[9] = {0,0,0,0,0,0,0,0,0};
		unsigned int break_count = 0;
		unsigned int no_init = (~(1<<(jtag->chain_length-2)))&0xff;

		//Stop all HICANN channels and start DNCs FPGA channel
		jtag->DNC_start_link(0x100);
		//Start FPGAs DNc channel
		jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x100);

		if(!silent) printf("Start FPGA-DNC initialization: \n");

		while ((((state1[8] & 0x1) == 0)||((state2[8] & 0x1) == 0)) && break_count<10) {
			usleep(900000);
			jtag->DNC_read_channel_sts(8,state1[8]);
			if(!silent) printf("DNC Status = 0x%02hx\t" ,state1[8] );

			jtag->FPGA_get_status(state2[8]);
			if(!silent) printf("FPGA Status = 0x%02hx RUN:%i\n" ,(unsigned char)state2[8],break_count);
			++break_count;
		}
		
		if (break_count == 10) {
			this->dnc_shutdown();
			this->hicann_shutdown();
			jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
			printf ("ERROR in FPGA-DNC initialization --> EXIT\n");
			return 0;
		}
		break_count = 0;

		if(!silent) printf("Start DNC-HICANN initialization: \n");
		jtag->DNC_set_lowspeed_ctrl(0xff); // ff = lowspeed
		jtag->DNC_set_speed_detect_ctrl(0x00); // 00 = no speed detect
		for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
			jtag->set_hicann_pos(i);
			jtag->HICANN_set_link_ctrl(0x061);
		}
        
		while ((no_init != 0) && (break_count < 10)) {
			for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
				if (((no_init>>i)&0x1) == 1) {
					jtag->set_hicann_pos(i);
					jtag->HICANN_stop_link();
				}
			}
			jtag->DNC_stop_link(0x000 + (no_init&0xff));

			for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
				if (((no_init>>i)&0x1) == 1) {
					jtag->set_hicann_pos(i);
					jtag->HICANN_start_link();
				}
			}
			jtag->DNC_start_link(0x100 + (no_init&0xff));

			usleep(90000);
			for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
				if (((no_init>>i)&0x1) == 1) {
					jtag->DNC_read_channel_sts(7-i,state1[i]);
					if(!silent) printf("DNC Status(%i) = 0x%02hx\t" ,i,state1[i] );
					jtag->set_hicann_pos(i);
					jtag->HICANN_read_status(state2[i]);
					if(!silent) printf("HICANN Status(%i) = 0x%02hx\n" ,i,(unsigned char)state2[i] );
					
					if ( ((state1[i]&0x73) == 0x41) && ((state2[i]&0x73) == 0x41) ) {
						no_init = no_init & ((~(1<<i))&0xff);
					}
				}
			}
			++break_count;
			if(!silent) printf("*************************************\n");
		}

		if (no_init == 0) {
			if(!silent) printf("Sucessfull init DNC-HICANN connection \n");
		} else {
			for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
				if(!silent) printf("Init channel %i with DNC state: %.02X and HICANN state %.02X\n",i,state1[i],(unsigned char)state2[i]);
				if(((no_init>>i)&0x1) == 1) {
					jtag->set_hicann_pos(i);
					this->hicann_shutdown();
				}			
			}
		}

		printf("Successfull FPGA-DNC-HICANN initialization\n");
		return 0x100 + ((~no_init)&0xff);
	}
	
	bool runFpgaBenchmark(unsigned int runtime_s) {
		uint64_t read_data_dnc = 0;
		unsigned char read_crc_dnc = 0;
		unsigned char read_crc_dnc_dly = 0;
		unsigned int total_crc_dnc = 0;
		uint64_t read_data_fpga = 0;
		unsigned char read_crc_fpga = 0;
		unsigned char read_crc_fpga_dly = 0;
		unsigned int total_crc_fpga = 0;
		printf("tm_sysbench::runFpgaBenchmark: Starting benchmark transmissions\n");

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
				
				/*
				if ((i%50) == 0) {
					printf("\rBER FPGA: %.1e Error FPGA: %i BER DNC: %.1e Error DNC: %i Time:(%i:%02i:%02i) Data (FPGA:0x%08X)(DNC:0x%08X)",
						(double(total_crc_fpga)/(double(6710886*(runtime-starttime)*128))),	total_crc_fpga,
						(double(total_crc_dnc)/(double(6710886*(runtime-starttime)*128))),total_crc_dnc,
						(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
						static_cast<unsigned int>(read_data_fpga),static_cast<unsigned int>(read_data_dnc));
					fflush(stdout);
				}
				if ((runtime_s%5000) == 0) printf("\n");
				*/
			}
		#endif
		
		// stop experiment
		printf("\ntm_sysbench::runFpgaBenchmark: Stopping benchmark transmissions\n");
		jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
		
		//results:
		printf("*** Result FPGA-DNC connection BER ***\n");
		printf("FPGA: BER : %.1e\t CRC Errors : %i\n",(double(total_crc_fpga)/(double(6710886*runtime_s*128))), total_crc_fpga);
		printf("DNC : BER : %.1e\t CRC Errors : %i\n",(double(total_crc_dnc)/(double(6710886*runtime_s*128))), total_crc_dnc);
		
		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 0;
		}
		jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC FPGAif delay values:\n");
		jtag->printDelayFPGA(vec_out);
    
		if (runtime_s) fpga_dnc_errate = ((double) total_crc_fpga + (double) total_crc_dnc) / (double) runtime_s;
    
    return ((total_crc_fpga+total_crc_dnc) == 0);
    
	}

	bool runFullBenchmark(unsigned int runtime_s) {
		uint64_t read_data_dnc = 0;
		unsigned char read_crc_dnc = 0;
		unsigned char read_crc_dnc_dly = 0;
		unsigned int total_crc_dnc = 0;
		uint64_t read_data_fpga = 0;
		unsigned char read_crc_fpga = 0;
		unsigned char read_crc_fpga_dly = 0;
		unsigned int total_crc_fpga = 0;
		uint64_t read_data_hicann = 0;
		unsigned char read_crc_hicann = 0;
		unsigned char read_crc_hicann_dly = 0;
		unsigned int total_crc_hicann = 0;

		printf("tm_sysbench::runFullBenchmark: Starting benchmark transmissions\n");

		reset_arq(hc->addr()); // ensure software ARQ and HICANN are in sync

		// loopback
		jtag->DNC_set_loopback(0x00); // DNC loopback
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
		spc->write_cfg(0x055ff);

		reset_arq(hc->addr()); // ensure that HICANN ARQ doesn't generate any traffic

		// setup DNC
		dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
		uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
		dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

		//jtag->FPGA_set_cont_pulse_ctrl(enable, channel, poisson?, delay, seed, nrn_start, char nrn_end, hicann = 0)
		jtag->FPGA_set_cont_pulse_ctrl(1, 0x01, 0, 20, 100, 1, 15, hicann_nr);

		unsigned int starttime = (unsigned int)time(NULL);
		unsigned int runtime = (unsigned int)time(NULL);
		
		printf("\rBER FPGA: %.1e BER DNC: %.1e BER HICANN: %.1e Time:(%i:%02i:%02i) Data (FPGA:0x%06X)",
					(double(total_crc_fpga)/(double(100))),
					(double(total_crc_dnc)/(double(100))),
					(double(total_crc_hicann)/(double(100))),
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_fpga));
					fflush(stdout);
		// wait...
		#ifdef NCSIM
			for(unsigned int i=0;i<runtime_s;++i) {
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

				//Read HICANN crc counter
				jtag->HICANN_read_crc_count(read_crc_hicann);
				jtag->HICANN_get_rx_data(read_data_hicann);
				if(read_crc_hicann != read_crc_hicann_dly) {
					total_crc_hicann += ((unsigned int)read_crc_hicann+0x100-(unsigned int)read_crc_hicann_dly)&0xff;
				}
				read_crc_hicann_dly = read_crc_hicann;

				printf("BER FPGA: %.1e Error FPGA: %i BER DNC: %.1e Error DNC: %i  BER HICANN: %.1e Error HICANN: %i Time:(%i:%02i:%02i) Data (FPGA:0x%06X)(DNC:0x%06X)(HICANN:0x%06X)\n",
					(double(total_crc_fpga)/(double(100))),total_crc_fpga,
					(double(total_crc_dnc)/(double(100))),total_crc_dnc,
					(double(total_crc_hicann)/(double(100))),total_crc_hicann,
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_fpga),
					static_cast<unsigned int>(read_data_dnc),
					static_cast<unsigned int>(read_data_hicann));
				fflush(stdout);
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

				//Read HICANN crc counter
				jtag->HICANN_read_crc_count(read_crc_hicann);
				jtag->HICANN_get_rx_data(read_data_hicann);
				if(read_crc_hicann != read_crc_hicann_dly) {
					total_crc_hicann += ((unsigned int)read_crc_hicann+0x100-(unsigned int)read_crc_hicann_dly)&0xff;
				}
				read_crc_hicann_dly = read_crc_hicann;
				
				/*
				if ((i%50) == 0) {
					printf("\rBER FPGA: %.1e BER DNC: %.1e BER HICANN: %.1e Time:(%i:%02i:%02i) Data (FPGA:0x%06X)",
					(double(total_crc_fpga)/(double(6710886*(runtime-starttime)*128))),
					(double(total_crc_dnc)/(double(6710886*(runtime-starttime)*128))),
					(double(total_crc_hicann)/(double(6710886*(runtime-starttime)*48))),
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_hicann));
					fflush(stdout);
				}
				if ((i%5000) == 0) printf("\n");
				*/
			}
		#endif
		
		// stop experiment
		printf("\ntm_sysbench: Stopping benchmark transmissions\n");
		jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
		
		//results:
		printf("*** Result FPGA-DNC-HICANN connection CRC ***\n");
		printf("FPGA: BER : %.1e\t CRC Errors : %i\n",(double(total_crc_fpga)/(double(6710886*runtime_s*128))), total_crc_fpga);
		printf("DNC : BER : %.1e\t CRC Errors : %i\n",(double(total_crc_dnc)/(double(6710886*runtime_s*128))), total_crc_dnc);
		printf("HICANN : BER : %.1e\t CRC Errors : %i\n",(double(total_crc_hicann)/(double(6710886*runtime_s*128))), total_crc_hicann);

		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 1;
		}
		jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC FPGAif delay values:\n");
		jtag->printDelayFPGA(vec_out);
		
		printf("DNC HICANNif delay value: %i\n", jtag->getVecDelay((7-hicann_nr)+16,vec_out));
		
		uint64_t delay_in  = 0xffffffffffffffffull;
		uint64_t delay_out = 0xffffffffffffffffull;
		jtag->HICANN_set_data_delay(delay_in,delay_out);
		printf("HICANN delay value: %i\n", (unsigned char)(delay_out&0x3f));
	
	if (runtime_s) spl1_fpga_errate = ((double) total_crc_fpga + (double) total_crc_dnc + (double) total_crc_hicann) / (double) runtime_s;
	
  	return ((total_crc_fpga+total_crc_dnc+total_crc_hicann) == 0);
  }
	
	bool runHicannBenchmark(unsigned int runtime_s) {
		uint64_t wdata64 = 0;
		unsigned int error1 = 0;
		unsigned int error2 = 0;

		uint64_t read_data_dnc = 0;
		unsigned char read_crc_dnc = 0;
		unsigned char read_crc_dnc_dly = 0;
		unsigned int total_crc_dnc = 0;
		uint64_t read_data_hicann = 0;
		unsigned char read_crc_hicann = 0;
		unsigned char read_crc_hicann_dly = 0;
		unsigned int total_crc_hicann = 0;

		printf("tm_sysbench::runHicannBenchmark: Starting benchmark transmissions\n");

		reset_arq(hc->addr()); // ensure software ARQ and HICANN are in sync

		// Testmodes
		jtag->DNC_test_mode_en(); // DNC enable testmode
		// loopback
		jtag->DNC_set_loopback(0x00); // DNC loopback
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
		spc->write_cfg(0x055ff);

		reset_arq(hc->addr()); // ensure that HICANN ARQ doesn't generate any traffic

		// setup DNC
		dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
		uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
		dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

		printf("tm_sysbench::runHicannBenchmark: Test DNC -> HICANN -> DNC\n");

		unsigned int starttime = (unsigned int)time(NULL);
		unsigned int runtime = (unsigned int)time(NULL);
		
		printf("BER DNC: %.1e Error DNC: %i BER HICANN: %.1e Error HICANN: %i Time:(%i:%02i:%02i) Data (DNC:0x%08X)(HICANN:0x%08X)",
					(double(total_crc_dnc)/(double(100))),total_crc_dnc,
					(double(total_crc_hicann)/(double(100))),total_crc_hicann,
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_dnc),
					static_cast<unsigned int>(read_data_hicann));
		int i=0;
		while ((runtime-starttime)< runtime_s) {
			++i;
			runtime = (unsigned int)time(NULL);
			wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
			jtag->DNC_start_pulse(hicann_nr,wdata64&0xdfffff);
			
				//Read HICANN crc counter
			jtag->HICANN_read_crc_count(read_crc_hicann);
			jtag->HICANN_get_rx_data(read_data_hicann);
			if(read_crc_hicann != read_crc_hicann_dly) {
				total_crc_hicann += ((unsigned int)read_crc_hicann+0x100-(unsigned int)read_crc_hicann_dly)&0xff;
			}
			unsigned char a,b;
			a=(unsigned char)((read_data_hicann>>15)&0x3f);
			b=(unsigned char)((wdata64>>15)&0x3f);
			if ( a != b ) {
				++error2;
				printf("%X %X %llX %llX\n",a,b,wdata64,read_data_hicann);
			}
			read_crc_hicann_dly = read_crc_hicann;

			//Read DNC crc counter
			jtag->DNC_set_channel(hicann_nr);
			jtag->DNC_read_crc_count(read_crc_dnc);
			jtag->DNC_get_rx_data(read_data_dnc);
			if(read_crc_dnc != read_crc_dnc_dly) {
				total_crc_dnc += ((unsigned int)read_crc_dnc+0x100-(unsigned int)read_crc_dnc_dly)&0xff;
			}
			if ( (read_data_dnc&0xffffff) != (read_data_hicann&0xffffff) ) ++error1;
			read_crc_dnc_dly = read_crc_dnc;

			/*
			printf("\rBER DNC: %.1e Error DNC: %i BER HICANN: %.1e Error HICANN: %i Time:(%i:%02i:%02i) Data (DNC:0x%08X)(HICANN:0x%08X)",
					(double(total_crc_dnc)/(double(6710886*(runtime-starttime)*48))),error1,
					(double(total_crc_hicann)/(double(6710886*(runtime-starttime)*48))),error2,
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_dnc),
					static_cast<unsigned int>(read_data_hicann));
			if ((i%5000) == 0) printf("\n");
			fflush(stdout);
			*/
		}

		//results:
		printf("*** Result DNC-HICANN connection BER, CRC Errors, Errors ***\n");
		printf("DNC : BER : %.1e, CRC Errors : %i, Errors : %i\n",(double(total_crc_dnc)/(double(6710886*runtime_s))), total_crc_dnc, error1);
		printf("HICANN : BER : %.1e, CRC Errors : %i, Errors : %i\n",(double(total_crc_hicann)/(double(6710886*runtime_s))), total_crc_hicann, error2);

		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 1;
		}
		jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC HICANNif delay value: %i\n", jtag->getVecDelay((7-hicann_nr)+16,vec_out));
		
		uint64_t delay_in  = 0xffffffffffffffffull;
		uint64_t delay_out = 0xffffffffffffffffull;
		jtag->HICANN_set_data_delay(delay_in,delay_out);
		printf("HICANN delay value: %i\n", (unsigned char)(delay_out&0x3f));

		if (runtime_s) dnc_hicann_errate = ((double) error1 + (double) error2) / (double) runtime_s;

  	return ((error1+error2) == 0);
	}
	
};


class LteeTmSysBench : public Listee<Testmode>{
public:
	LteeTmSysBench() : Listee<Testmode>(string("tm_sysbench"),string("provides example initialization order for complete (wafer-scale) system")){};
	Testmode * create(){return new TmSysBench();};
};

LteeTmSysBench ListeeTmSysBench;

