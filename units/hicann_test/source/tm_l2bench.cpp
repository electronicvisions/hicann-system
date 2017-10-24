// testmode for l2 communication measure
#include <time.h>

#include "pulse_float.h"
#include "common.h"
#include "hicann_ctrl.h"
#include "fpga_control.h"

#include "neuron_control.h" //neuron control class (merger, background generators)
#include "spl1_control.h"
#include "dnc_control.h"

#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2comm.h"
#include "testmode.h"

#define fpga_master   1
using namespace std;
using namespace facets;

class TmL2Bench : public Testmode{

public:
	HicannCtrl  *hc;
	NeuronControl *nc; //neuron control
	SPL1Control *spc;
	DNCControl  *dc;
	FPGAControl *fpga;

	S2C_JtagPhys2Fpga* s2c_jtag; // '2Fpga' needed for finding out whether on Kintex7

	double starttime;
	double clkperiod_bio_dnc;
	double clkperiod_bio_fpga;
	double playback_begin_offset;

	uint hicann_nr, hicann_jtag_nr;

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

	// test function
	bool test() 
	{
		std::cout << "Running testmode tm_l2bench" << std::endl;

		s2c_jtag = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);

		bool is_k7fpga = s2c_jtag->is_k7fpga();

		if(!s2c_jtag){
			dbg(0) << "Wrong Comm Class!!! Must use -bje (S2C_JtagPhys)! - EXIT" << endl;
			exit(EXIT_FAILURE);
		}

		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();

		if (!is_k7fpga)
			jtag->FPGA_set_fpga_ctrl(0x1);

		// disable FPGA arq
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, true); 

		// reset wafer
//		comm->ResetWafer();

		this->starttime = 0.0; // set by sendPlaybackPulses
		this->clkperiod_bio_dnc = 4.0e-9*1.0e4*1.0e3; // in ms
		this->clkperiod_bio_fpga = 8.0e-9*1.0e4*1.0e3; // in ms
		this->playback_begin_offset = 5.0; // in ms

		this->hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
		nc  = &hc->getNC();
		spc = &hc->getSPL1Control();

		if (!is_k7fpga)
			dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC

		hicann_jtag_nr = hc->addr();
		jtag->set_hicann_pos(hicann_jtag_nr);
		cout << "HICANN JTAG position " << (jtag->pos_hicann) << endl;
		cout << "FPGA JTAG position " << (jtag->pos_fpga) << endl;

		uint64_t jtagid;
		jtag->read_id(jtagid,jtag->pos_fpga);
		cout << "Read FPGA JTAG ID: 0x" << hex << jtagid << endl;
		if (((!is_k7fpga) && (jtagid != 0xbabebeef)) || ((is_k7fpga) && (jtagid != 0x1c56c007))) {
			cout << "Got invalid FPGA JTAG ID -> reset JTAG" << endl;
			jtag->reset_jtag();
		}

		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;
		if (jtagid != 0x14849434) return false;

		if (!is_k7fpga) {
			jtag->read_id(jtagid, jtag->pos_dnc);
			cout << "DNC ID: 0x" << hex << jtagid << endl;
			if (jtagid != 0x1474346f)
				return false;
		}

		hicann_nr = comm->jtag2dnc(hicann_jtag_nr);

		if (!is_k7fpga)
			cout << "Using DNC channel " << hicann_nr << endl;
		else
			cout << "Using HICANN channel " << hicann_nr << endl;

		char c;
		bool cont=true;
		do {

			cout << "Select test option:" << endl;
			cout << "1: Test FPGA connection " << endl;
			cout << "2: Test HICANN connection (automatic init)" << endl;
			cout << "3: Measure HICANN connections (manual)" << endl;
			cout << "4: Test all connections " << endl;
			cout << "5: Init all connections " << endl;
			cout << "6: Init all connections (manual) " << endl;
			cout << "7: Test HICANN connection for one delay setting " << endl;
			cout << "x: exit" << endl;

#ifdef NCSIM
			c = '3'; // eye measurement
			cout << "Selected option " << c << " for simulation." << endl;
#else
			if (is_k7fpga) {
				c = '3'; // eye measurement
				cout << "Only option " << c << " supported for Kintex7. Starting it." << endl;
				cont = false;
			}
			else
				cin >> c;
#endif

			switch(c) {

				case '1':{
					if ( !this->initConnectionFPGA() ) {
						printf("Stop FPGA benchmark : INIT ERROR\n");
						break;
					}
					unsigned int runtime_s = 10; // sec
					this->runFpgaBenchmark(runtime_s);
					
					//Shutdown interfaces
					this->dnc_shutdown();
					this->hicann_shutdown();
					jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
					printf ("Finished benchmark of FPGA-DNC connection\n");
					
				} break;

				case '2':{
					if ( !this->initConnectionFPGA() ) {
						printf("Stop DNC-HICANN benchmark : INIT ERROR\n");
						break;
					}
					unsigned int runtime_s = 10; // sec
					this->runHicannBenchmark(runtime_s);
					
					//Shutdown interfaces
					this->dnc_shutdown();
					this->hicann_shutdown();
					jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
					printf ("Finished benchmark of DNC-HICANN connection\n");
					
				} break;

				case '3':{
					bool do_reset = true;
					printf("Measure complete data eye of HICANN %i\n",hicann_nr);
#ifdef NCSIM
					printf("  -> with reset after each run\n");
					do_reset = true;
#else
					if (is_k7fpga) {
						printf("  -> with reset after each run\n");
						do_reset = true;
					}
					else {
						cout << "With reset after each run (1/0)? ";
						cin >> do_reset;
						cout << endl;
					}
#endif

					this->measureHicannConnection(do_reset, is_k7fpga);
					//Shutdown interfaces
					if (!is_k7fpga)
						this->dnc_shutdown();

					this->hicann_shutdown();
					jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000); // to do: ??
					printf ("Finished measurement of DNC-HICANN connection\n");
					
				} break;

				case '4':{
					if ( !this->initConnectionFPGA() ) {
						printf("Stop full benchmark : INIT ERROR\n");
						break;
					}
					unsigned int runtime_s = 10; // sec
					this->runFullBenchmark(runtime_s);
					
					//Shutdown interfaces
					this->dnc_shutdown();
					this->hicann_shutdown();
					jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
					printf ("Finished benchmark of FPGA-DNC-HICANN connection\n");
					
				} break;

				case '5':{
					unsigned int inits = this->initAllConnections();
					if (inits == 0) {
						printf("Stop all init : INIT ERROR\n");
						break;
					}
					printf ("Finished initialization of all connection (0x%03X)\n",inits);
					
					//Shutdown interfaces
					this->dnc_shutdown();
					this->hicann_shutdown();
					jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
				} break;
				case '6':{
					//std::vector<unsigned char> delays((jtag->chain_length-2)*2,0);
					//for (int k=0;k<(jtag->chain_length-2);++k) {
					//	delays[2*k+0] = 52;
					//	delays[2*k+1] = 52;
					//}
					
					std::vector<unsigned char> delays(16,0);
					delays[ 0] = 48;
					delays[ 1] = 54;
					delays[ 2] = 51;
					delays[ 3] = 54;
					delays[ 4] = 51;
					delays[ 5] = 36;
					delays[ 6] = 54;
					delays[ 7] = 54;
					delays[ 8] = 51;
					delays[ 9] = 54;
					delays[10] = 54;
					delays[11] = 34;
					delays[12] = 52;
					delays[13] = 38;
					delays[14] = 54;
					delays[15] = 35;

					unsigned int inits = this->initAllConnections(0,delays);
					if (inits == 0) {
						printf("Stop manual all init : INIT ERROR\n");
						break;
					}
					printf ("Finished initialization of all connection (0x%03X)\n",inits);
					
					uint64_t rdata64;
					uint64_t wdata64;
					int error1 = 0;
					int error2 = 0;
					
					int tr_count = 10;

					for (int k=0;k<(jtag->chain_length-2);++k) {
						error1 = 0;
						error2 = 0;
						
						jtag->set_hicann_pos(k);

						jtag->DNC_test_mode_en();
						jtag->HICANN_set_test(1);
						jtag->DNC_set_timestamp_ctrl(0x00);
					
						for (int num_data=0;num_data<tr_count;++num_data) {
							wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
							//jtag->DNC_start_config(jtag2dnc(k),wdata64);
							jtag->DNC_start_pulse(jtag2dnc(k),wdata64&0x7fff);
							//usleep(10);
							jtag->HICANN_get_rx_data(rdata64);

							if ( (rdata64&0x7fff) != (wdata64&0x7fff) ) {
								++error1;
								//printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
							}
						}

						//printf("Test DNC2 -> DNC1  \n");

						for (int num_data=0;num_data<tr_count;++num_data) {
							wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
							jtag->HICANN_set_tx_data(wdata64);
							jtag->HICANN_start_cfg_pkg();
							//usleep(10);
							jtag->DNC_set_channel(jtag2dnc(k));
							jtag->DNC_get_rx_data(rdata64);

							if (rdata64 != wdata64) {
								++error2;
								//printf("Wrote: 0x%016llX Read: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
							}
						}
						printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);


						jtag->DNC_test_mode_dis();
						jtag->HICANN_set_test(0);
					}
				} break;

				case '7':{
					unsigned int delay_dnc = 0;
					unsigned int delay_hicann = 0;
					bool do_reset = true;
					cout << "Delay DNC: ";
					cin >> delay_dnc;
					cout << "Delay HICANN: ";
					cin >> delay_hicann;
					cout << "With reset after each run (1/0)? ";
					cin >> do_reset;
					cout << endl;
				
					measureSingleDelay(delay_dnc, delay_hicann, do_reset);
					//Shutdown interfaces
					//this->dnc_shutdown();
					//this->hicann_shutdown();
					//jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
				} break;
				case 'x': cont = false; 
			}
			#ifdef NCSIM
				sc_stop();
				wait();
			#endif
		} while(cont);

		return true;
	}

	bool initConnectionFPGA()
	{
		//Reset Setup to get defined start point
		jtag->FPGA_set_fpga_ctrl(0x1);
		jtag->reset_jtag();
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

	unsigned int initAllConnections()
	{
		unsigned char state1[9] = {0,0,0,0,0,0,0,0,0};
		uint64_t state2[9] = {0,0,0,0,0,0,0,0,0};
		unsigned int break_count = 0;
		unsigned int no_init = (~(1<<(jtag->chain_length-2)))&0xff;

		//Reset Setup to get defined start point
		jtag->FPGA_set_fpga_ctrl(0x1);
		jtag->reset_jtag();
		//jtag->DNC_lvds_pads_en(~(0x100+(1<<hicann_nr)));
		
		//Stop all HICANN channels and start DNCs FPGA channel
		jtag->DNC_start_link(0x100);
		//Start FPGAs DNc channel
		jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x100);

		printf("Start FPGA-DNC initialization: \n");

		while ((((state1[8] & 0x1) == 0)||((state2[8] & 0x1) == 0)) && break_count<10) {
			usleep(900000);
			jtag->DNC_read_channel_sts(8,state1[8]);
			printf("DNC Status = 0x%02hx\t" ,state1[8] );

			jtag->FPGA_get_status(state2[8]);
			printf("FPGA Status = 0x%02hx RUN:%i\n" ,(unsigned char)state2[8],break_count);
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

		printf("Start DNC-HICANN initialization: \n");
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
					printf("DNC Status(%i) = 0x%02hx\t" ,i,state1[i] );
					jtag->set_hicann_pos(i);
					jtag->HICANN_read_status(state2[i]);
					printf("HICANN Status(%i) = 0x%02hx\n" ,i,(unsigned char)state2[i] );
					
					if ( ((state1[i]&0x73) == 0x41) && ((state2[i]&0x73) == 0x41) ) {
						no_init = no_init & ((~(1<<i))&0xff);
					}
				}
			}
			++break_count;
			printf("*************************************\n");
		}

		if (no_init == 0) {
			printf("Sucessfull init DNC-HICANN connection \n");
		} else {
			for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
				printf("Init channel %i with DNC state: %.02X and HICANN state %.02X\n",i,state1[i],(unsigned char)state2[i]);
				if(((no_init>>i)&0x1) == 1) {
					jtag->set_hicann_pos(i);
					this->hicann_shutdown();
				}			
			}
		}

		printf("Successfull FPGA-DNC-HICANN initialization\n");
		return 0x100 + ((~no_init)&0xff);
	}

	unsigned int dnc2jtag(int const dnc_channel_nr) {
		if(jtag->chain_length == 10) {
			assert (((7-dnc_channel_nr) >= 0) && ((dnc_channel_nr-7) < 8));
			return 7-dnc_channel_nr;
		} else {
			assert ((dnc_channel_nr >= 0) && (dnc_channel_nr < 8));
			return dnc_channel_nr;
		}
	}

	// symmetrical :)
	unsigned int jtag2dnc(int const jtag_chain_nr) {
		return dnc2jtag(jtag_chain_nr);
	}

	unsigned int initAllConnections(bool silent, std::vector<unsigned char> delays) {

		if ( ( delays.size() != 16 && jtag->chain_length == 10 ) || ( delays.size() != 2 && jtag->chain_length == 3 ) ) {
			log(Logger::ERROR) << "tm_l2bench::initAllConnections: Number of init values ( " << delays.size() << " ) for comm wrong, abort!" << flush;
			exit(EXIT_FAILURE);
		}		

		unsigned char state1[9] = {0,0,0,0,0,0,0,0,0};
		uint64_t state2[9] = {0,0,0,0,0,0,0,0,0};
		unsigned int break_count = 0;
		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);
		int delay_out = 0;
		unsigned short no_init = 0xff;
		if(jtag->chain_length == 10) {
			no_init = 0xff;
		} else {
			no_init = 0x01;
		}

		//Reset Setup to get defined start point --> only for testcase
		jtag->FPGA_set_fpga_ctrl(0x1);
		jtag->reset_jtag();
		// do NOT reset the setup here, again since this MUST be done correctly in advance!
		//jtag->DNC_lvds_pads_en(~(0x100+(1<<hicann_nr)));
		
		//Stop all HICANN channels and start DNCs FPGA channel
		jtag->DNC_start_link(0x100);
		//Start FPGAs DNc channel
		jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x100);

		if(!silent) log(Logger::INFO) << "Start FPGA-DNC initialization:" << flush;

		while ((((state1[8] & 0x1) == 0)||((state2[8] & 0x1) == 0)) && break_count<10) {
			usleep(90000);
			jtag->DNC_read_channel_sts(8,state1[8]);
			if(!silent) log(Logger::INFO) << "DNC Status = 0x" << hex << ((uint)state1[8]) << " ";

			jtag->FPGA_get_status(state2[8]);
			if(!silent) log(Logger::INFO) << "FPGA Status = 0x" << hex << ((uint)state2[8]) << " RUN:" << dec << break_count << flush;
			++break_count;
		}
		
		if (break_count == 10) {
			jtag->DNC_stop_link(0x1ff);
			jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
			if(!silent) log(Logger::INFO) << "ERROR in FPGA-DNC initialization --> EXIT" << flush;
			return 0;
		}
		break_count = 0;

		if(!silent) log(Logger::INFO) << "Start manual DNC-HICANN initialization: " << flush;
		jtag->DNC_set_lowspeed_ctrl(0xff); // ff = lowspeed
		jtag->DNC_set_speed_detect_ctrl(0x00); // 00 = no speed detect
		jtag->DNC_set_init_ctrl(0x0aaaa);

		for (unsigned int i = 0;i<(jtag->chain_length-2); ++i) {
			// lowspeed all HICANNs
			jtag->set_hicann_pos(i);
			jtag->HICANN_set_link_ctrl(0x060);
		}

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 1;
		}
		for(unsigned int i=0; i<(jtag->chain_length-2);  ++i) {
			jtag->set_hicann_pos(i);
			jtag->HICANN_set_data_delay((uint64_t)(delays[(2*i)] & 0x3f),(uint64_t&)delay_out);
			for(int m=6; m>0;  --m)
				vec_in[(16*6+(8-jtag2dnc(i))*6)-m] = ((delays[2*i+1] & 0x3f) >> (6-m)) & 0x1;
		}

		/*printf("\nDelay Vector DNC : 0x");
		jtag->printBoolVect(vec_in);
		printf("\n");*/
	
		jtag->DNC_set_data_delay(vec_in,vec_out);

		for (unsigned int i=0;i<(jtag->chain_length-2); ++i) {
			jtag->set_hicann_pos(i);
			jtag->DNC_stop_link(1<<jtag2dnc(i));
			jtag->HICANN_stop_link();

			break_count = 0;

			while ((((state1[i] & 0x73) != 0x41) || ((state2[i] & 0x73) != 0x41)) && break_count < 1) {
				jtag->HICANN_stop_link();
				jtag->DNC_stop_link(1<<jtag2dnc(i));
				jtag->HICANN_start_link();
				jtag->DNC_start_link(0x000 + (0xff & (1<<jtag2dnc(i))));

				usleep(5000);
				jtag->DNC_read_channel_sts(jtag2dnc(i),state1[i]);
				if(!silent) log(Logger::INFO) << "  DNCIF  " << jtag2dnc(i) << " Status = 0x" << hex << (unsigned int)state1[i];
				jtag->HICANN_read_status(state2[i]);
				if(!silent) log(Logger::INFO) << "  HICANN " << i << " Status = 0x" << hex << (unsigned int)state2[i] << flush;
				++break_count;
			}
		
			if (((state1[i] & 0x73) == 0x41) || ((state2[i] & 0x73) == 0x41))
				no_init &= (~(1<<i));
		}

		if(!silent) log(Logger::INFO) << "*************************************" << flush;

		if (no_init == 0) {
			if(!silent) log(Logger::INFO) << "Sucessfull init DNC-HICANN connection " << flush;
		} else {
			for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
				if(!silent) log(Logger::INFO) << "Init channel "<< dec << i << " with DNC state: 0x" << hex << ((uint)state1[i]) << " and HICANN state 0x" << hex << ((uint)state2[i]) << flush;
				/*if(((no_init>>i)&0x1) == 1) {
					jtag->set_hicann_pos(i);
					this->hicann_shutdown();
				}*/
			}
		}

		if(!silent) log(Logger::INFO) << "Successfull FPGA-DNC-HICANN initialization" << flush;
		return 0x100 + ((~no_init)&((1<<(jtag->chain_length-2))-1));
	}


	void runFpgaBenchmark(unsigned int runtime_s) {
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
	}

	void runFullBenchmark(unsigned int runtime_s) {
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
		printf("tm_l2bench: Starting benchmark transmissions\n");

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
				
			}
		#endif
		
		// stop experiment
		printf("\ntm_l2bench: Stopping benchmark transmissions\n");
		jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
		
		//results:
		printf("*** Result FPGA-DNC connection BER ***\n");
		printf("FPGA: BER : %.1e\t Error : %i\n",(double(total_crc_fpga)/(double(6710886*runtime_s*128))), total_crc_fpga);
		printf("DNC : BER : %.1e\t Error : %i\n",(double(total_crc_dnc)/(double(6710886*runtime_s*128))), total_crc_dnc);
		printf("HICANN : BER : %.1e\t Error : %i\n",(double(total_crc_hicann)/(double(6710886*runtime_s*128))), total_crc_hicann);

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
	}
	
	void runHicannBenchmark(unsigned int runtime_s) {
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
		printf("tm_l2bench: Starting benchmark transmissions\n");

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

		printf("tm_l2bench: Test DNC -> HICANN -> DNC\n");

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
				//printf("%X %X %llX %llX\n",a,b,wdata64,read_data_hicann);
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

			printf("\rBER DNC: %.1e Error DNC: %i BER HICANN: %.1e Error HICANN: %i Time:(%i:%02i:%02i) Data (DNC:0x%08X)(HICANN:0x%08X)",
					(double(total_crc_dnc)/(double(6710886*(runtime-starttime)*48))),error1,
					(double(total_crc_hicann)/(double(6710886*(runtime-starttime)*48))),error2,
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_dnc),
					static_cast<unsigned int>(read_data_hicann));
			if ((i%5000) == 0) printf("\n");
			fflush(stdout);
		}

		//results:
		printf("*** Result FPGA-DNC connection BER ***\n");
		printf("DNC : BER : %.1e Error : %i\n",(double(total_crc_dnc)/(double(6710886*runtime_s))), error1);
		printf("HICANN : BER : %.1e Error : %i\n",(double(total_crc_hicann)/(double(6710886*runtime_s))), error2);

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
	}

	void measureHicannConnection(bool do_reset, bool is_k7fpga)
	{
		static const int NUM_DELAY = 32; // number of delay settings to be tested (maximum for HICANN: 32)
		static const int NUM_DATA  = 1000; // number of test transmissions performed for each delay setting
		uint64_t jtagid;
		uint64_t rdata64;
		uint64_t wdata64;
		unsigned char state1;
		uint64_t state2;
		uint64_t fpga_state;
		unsigned int error1;
		unsigned int error2;
		unsigned int init_error;
		unsigned int data_error;
		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);
		uint64_t delay_in;
		uint64_t delay_out;
		int eye_stat[NUM_DELAY][NUM_DELAY][2];
		for(int i=0;i<NUM_DELAY;++i)
			for(int j=0;j<NUM_DELAY;++j)
				for(int k=0;k<2;++k)
					eye_stat[i][j][k] = -1;

		init_error = 0;
		data_error = 0;

		unsigned int dnc_delay_count = NUM_DELAY;
		unsigned int dnc_delay_start = 0;
		if (is_k7fpga)
			dnc_delay_count = 1;

		for (int i = dnc_delay_start; i < dnc_delay_start+dnc_delay_count; ++i) // DNC
			for (int j=0; j<NUM_DELAY ; ++j) {  //HICANN
				//Reset Setup to get defined start point
				if (do_reset)
				{
					jtag->FPGA_set_fpga_ctrl(0x1);
					this->hicann_shutdown();
					fpga->reset();
					jtag->reset_jtag();
				}

				if (is_k7fpga) {
					jtag->K7FPGA_set_hicannif(hicann_nr);
					jtag->K7FPGA_set_hicannif_config(
						0x008); // stop link, manual init, master enabled
//						0x00c); // stop link, auto init, master enabled
				} else {
					jtag->DNC_stop_link(0x1ff);
					// jtag->DNC_lvds_pads_en(~(0x100+(1<<hicann_nr)));
					jtag->DNC_set_lowspeed_ctrl(0xff);
					jtag->DNC_set_speed_detect_ctrl(0x0);
					jtag->DNC_set_init_ctrl(0x0aaaa);
					jtag->DNC_set_timestamp_ctrl(0x00);
				}

				jtag->HICANN_stop_link();
				jtag->HICANN_lvds_pads_en(0);
				//jtag->HICANN_set_link_ctrl(0x020); // manual init
				jtag->HICANN_set_link_ctrl(0x021); // auto init

				if (is_k7fpga)
					jtag->K7FPGA_disable_hicannif_config_output();
				else
					jtag->DNC_test_mode_en();

				jtag->HICANN_set_test(1);

				for(int m=0   ; m<144;++m) {
					vec_in[m]  = 1;
					vec_out[m] = 1;
				}
				for(int m=6; m>0;  --m)
					vec_in[(16*6+(8-hicann_nr)*6)-m] = ((~(i & 0x3f)) >> (6-m)) & 0x1;

				/*printf("\nDelay Vector DNC1 : 0x");
				jtag1.printBoolVect(vec_in);
				printf("\n");
				*/

				if (is_k7fpga)
					jtag->K7FPGA_set_hicannif_data_delay(i);
				else
					jtag->DNC_set_data_delay(vec_in, vec_out);

				delay_in = ~(j & 0x3f);
				jtag->HICANN_set_data_delay(delay_in,delay_out);

				std::cout << "HICANN init with data delay " << std::hex << delay_out << "  "
						  << (0x3f & delay_in) << std::endl;

				//Check Status
				state1 = 0;
				state2 = 0;
				jtagid = 0;

				if (is_k7fpga)
					std::cout << "Delay Setting FPGA: " << i << "\t HICANN: " << (63 - j)
							  << std::endl;
				else
					std::cout << "Delay Setting DNC: " << (63 - i) << "\t HICANN: " << (63 - j)
							  << std::endl;

				while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 1) {
					if (is_k7fpga) {
						jtag->HICANN_stop_link();
						jtag->K7FPGA_stop_fpga_link();
						jtag->HICANN_start_link();
						jtag->K7FPGA_start_fpga_link();
					} else {
						jtag->HICANN_stop_link();
						jtag->DNC_stop_link(0x0ff);
						jtag->HICANN_start_link();
						jtag->DNC_start_link(0x000 + (0xff /*& 1<<hicann_nr*/));
					}
#ifdef NCSIM
					wait(100.0, SC_US);
#else
					for (unsigned int nrep=0; nrep<10;++nrep) {
						if (is_k7fpga) {
							jtag->K7FPGA_get_hicannif_status(fpga_state);
							unsigned int init_fsm_state = (fpga_state>>30) & 0x3f;
							std::cout << "state of init FSM on K7 FPGA: " << std::dec << init_fsm_state << std::endl;
							unsigned int init_rxdata = (fpga_state>>36) & 0xff;
							std::cout << "rx data on K7 FPGA: 0x" << std::hex << init_rxdata << std::dec << std::endl;
							unsigned int init_txdata = (fpga_state>>44) & 0xff;
							std::cout << "tx data on K7 FPGA: 0x" << std::hex << init_txdata << std::dec << std::endl;
							std::cout << "complete : " << std::hex << fpga_state << std::dec << std::endl;
						}
					}
#endif

					if (is_k7fpga) {
						jtag->K7FPGA_get_hicannif_status(fpga_state);
						state1 = (unsigned char) (fpga_state & 0xff);
					} else {
						jtag->DNC_read_channel_sts(hicann_nr, state1);
					}

					jtag->HICANN_read_status(state2);

					std::cout << "Status loop " << std::dec << jtagid << ": FPGA: " << std::hex
							  << ((unsigned int) (state1))
							  << ", HICANN: " << ((unsigned int) (state2)) << std::endl;

					// additional status information for Kintex7
					if (is_k7fpga) {
						unsigned int init_fsm_state = (fpga_state>>30) & 0x3f;
						std::cout << "state of init FSM on K7 FPGA: " << std::dec << init_fsm_state << std::endl;
						std::cout << "complete FPGA state: " << std::hex << fpga_state << std::dec << std::endl;
					
						uint64_t read_delay_val = 0;
						jtag->K7FPGA_get_hicannif_data_delay(read_delay_val);
						if (read_delay_val != i)
							std::cout << "WARNING: read-back of delay value was " << read_delay_val
									  << ", but should be " << i << "." << std::endl;
					}

					++jtagid;
				}

				if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))
				{
					error1 = 0;
					error2 = 0;
					//printf("Sucessfull init  \n");
					//printf("Test DNC1 -> DNC2  \n");

					for (int num_data=0;num_data<NUM_DATA;++num_data) {
						wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
						//jtag.DNC_start_config(hicann_nr,wdata64);
						if (is_k7fpga) {
							jtag->K7FPGA_set_hicannif_tx_data(wdata64 & 0x7fff);
							jtag->K7FPGA_set_hicannif_control(0x2);
						} else {
							jtag->DNC_start_pulse(hicann_nr, wdata64 & 0x7fff);
						}

#ifdef NCSIM
						wait(1, SC_US);
#endif

						jtag->HICANN_get_rx_data(rdata64);

						if ( (rdata64&0x7fff) != (wdata64&0x7fff) ) {
							++error1;
							//printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
						}
					}

					//printf("Test DNC2 -> DNC1  \n");

					for (int num_data=0;num_data<NUM_DATA;++num_data) {
						wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
						jtag->HICANN_set_tx_data(wdata64);
						jtag->HICANN_start_cfg_pkg();

#ifdef NCSIM
						wait(1, SC_US);
#endif

						if (is_k7fpga) {
							jtag->K7FPGA_get_hicannif_rx_cfg(rdata64);
						} else {
							jtag->DNC_set_channel(hicann_nr);
							jtag->DNC_get_rx_data(rdata64);
						}

						if (rdata64 != wdata64) {
							++error2;
							//printf("Wrote: 0x%016llX Read: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
						}
					}
					printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);
					if (error1 || error2) 
						++data_error;
				} else {
					error1 = -1;
					error2 = -1;
					++init_error;
					printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
				}

				eye_stat[i][j][0]=error1;
				eye_stat[i][j][1]=error2;


				if (is_k7fpga)
					jtag->K7FPGA_enable_hicannif_config_output();
				else
					jtag->DNC_test_mode_dis();

				jtag->HICANN_set_test(0);
			}
			//printf("Successfull inits : %i  Successfull Data Transmissions : %i\n",NUM_DELAY-init_error,NUM_DELAY-init_error-data_error);
			printf("\n\n** Results *** \nData eye:\n");
			printf("	  HICANN Delay\n");
			printf("	   ");
			for(int i=0;i<NUM_DELAY;++i) printf("%1i ",((63-i)/10));printf("\n");
			printf("	   ");
			for(int i=0;i<NUM_DELAY;++i) printf("%1i ",((63-i)%10));printf("\n");
			for (int i = dnc_delay_start; i < dnc_delay_start+dnc_delay_count; ++i) {
				if(NUM_DELAY/2-1 == i) printf("D ");
				else if(NUM_DELAY/2-0 == i) printf("N ");
				else if(NUM_DELAY/2+1 == i) printf("C ");
				else printf("  ");
				printf("[%02i]",63-i);
				for(int j=0;j<NUM_DELAY;++j) {
					if(eye_stat[i][j][0] < 0)
						printf(" X");
					else if ( (eye_stat[i][j][0] == 0) && (eye_stat[i][j][1] == 0) )
						printf(" O");
					else if ( (eye_stat[i][j][0] > 0)  && (eye_stat[i][j][1] == 0) )
						printf(" +");
					else if ( (eye_stat[i][j][0] == 0)  && (eye_stat[i][j][1] > 0) )
						printf(" -");
					else if ( (eye_stat[i][j][0] > 0)  && (eye_stat[i][j][1] > 0) )
						printf(" /");
				}
				printf("\n");
			}
			if (is_k7fpga)
				printf("\nLegend: 'O': Okay, 'X': init failed, '+': trans. errors FPGA->HICANN, '-': trans. errors HICANN->FPGA, '/': trans. errors both directions\n");
			else
				printf("\nLegend: 'O': Okay, 'X': init failed, '+': trans. errors DNC->HICANN, "
					   "'-': trans. errors HICANN->DNC, '/': trans. errors both directions\n");

			// do automatic init and extract FPGA delay setting
			if (hc->GetCommObj()->Init(hc->addr(), false, true) != Stage2Comm::ok)
		 		printf("Automatic init failed.\n");
			else
				printf("Automatic init successful.\n");

			jtag->K7FPGA_get_hicannif_status(fpga_state);
			std::cout << "FPGA state: 0x" << std::hex << fpga_state << std::dec << std::endl;
			std::cout << "FPGA init status: " << std::hex << (unsigned int)(fpga_state&0xff) << std::dec << std::endl;
		 	unsigned int init_fsm_state = (fpga_state>>30) & 0x3f;
			std::cout << "state of init FSM on K7 FPGA: " << std::dec << init_fsm_state << std::endl;
			unsigned int init_rxdata = (fpga_state>>36) & 0xff;
			std::cout << "rx data on K7 FPGA: 0x" << std::hex << init_rxdata << std::dec << std::endl;
			unsigned int init_txdata = (fpga_state>>44) & 0xff;
			std::cout << "tx data on K7 FPGA: 0x" << std::hex << init_txdata << std::dec << std::endl;
			uint64_t read_delay_val = 0;
			jtag->K7FPGA_get_hicannif_data_delay(read_delay_val);
			std::cout << "FPGA delay value after automatic init: " << std::dec << read_delay_val << std::endl;

	}


	void measureSingleDelay(unsigned int delay_dnc, unsigned int delay_hicann, bool do_reset) {
		static const int NUM_DELAY = 32;
		static const int NUM_DATA  = 1000;
		uint64_t jtagid;
		uint64_t rdata64;
		uint64_t wdata64;
		unsigned char state1;
		uint64_t state2;
		unsigned int error1;
		unsigned int error2;
		unsigned int init_error;
		unsigned int data_error;
		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);
		uint64_t delay_in;
		uint64_t delay_out;

		init_error = 0;
		data_error = 0;

		if (do_reset)
		{
				//Reset Setup to get defined start point
				jtag->FPGA_set_fpga_ctrl(0x1);
				jtag->reset_jtag();
				jtag->DNC_stop_link(0x1ff);
				//jtag->DNC_lvds_pads_en(~(0x100+(1<<hicann_nr)));
				jtag->DNC_set_lowspeed_ctrl(0xff);
				jtag->DNC_set_speed_detect_ctrl(0x0);
				jtag->DNC_set_init_ctrl(0x0aaaa);
				jtag->DNC_set_timestamp_ctrl(0x00);

				for (unsigned int i = 0;(i<(jtag->chain_length-2))&&(i<8); ++i) {
					jtag->set_hicann_pos(i);
					jtag->HICANN_stop_link();
					jtag->HICANN_lvds_pads_en(0);
					jtag->HICANN_set_link_ctrl(0x020);
				}
		}
		else
		{
				jtag->DNC_stop_link(0x1ff);
				jtag->DNC_set_lowspeed_ctrl(0xff);
				jtag->DNC_set_speed_detect_ctrl(0x0);
				jtag->DNC_set_init_ctrl(0x0aaaa);
				jtag->DNC_set_timestamp_ctrl(0x00);
				for (unsigned int i = 0;(i<(jtag->chain_length-2))&&(i<8); ++i) {
					jtag->set_hicann_pos(i);
					jtag->HICANN_stop_link();
					jtag->HICANN_set_link_ctrl(0x020);
				}
		
		}
				jtag->set_hicann_pos(hicann_jtag_nr);

				jtag->DNC_test_mode_en();
				jtag->HICANN_set_test(1);

				for(int m=0   ; m<144;++m) {
					vec_in[m]  = 1;
					vec_out[m] = 1;
				}
				for(int m=6; m>0;  --m)
					vec_in[(16*6+(8-hicann_nr)*6)-m] = ((~(delay_dnc & 0x3f)) >> (6-m)) & 0x1;

				/*printf("\nDelay Vector DNC1 : 0x");
				jtag1.printBoolVect(vec_in);
				printf("\n");
				*/
				jtag->DNC_set_data_delay(vec_in,vec_out);

				delay_in = ~(delay_hicann & 0x3f);
				jtag->HICANN_set_data_delay(delay_in,delay_out);

				//printf("HICANN init with data delay %.02X  %.02X\n",delay_out,0x3f &delay_in);

				//Check Status
				state1 = 0;
				state2 = 0;
				jtagid = 0;
				
				for (unsigned int i = 0;(i<(jtag->chain_length-2))&&(i<8); ++i) {
					jtag->set_hicann_pos(i);
					jtag->HICANN_start_link();
				}
				jtag->set_hicann_pos(hicann_jtag_nr);
				
				
				while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 1) {
					jtag->HICANN_stop_link();
					jtag->DNC_stop_link(0x0ff);
					jtag->HICANN_start_link();
					jtag->DNC_start_link(0x000 + (0xff /*& 1<<hicann_nr*/ ) );

					usleep(5000);
					jtag->DNC_read_channel_sts(hicann_nr,state1);
					//snprintf(buf, sizeof(buf), "DNC Status = 0x%02hx\n" ,state1 );
					//jtag.logging(buf);
					jtag->HICANN_read_status(state2);
					//snprintf(buf, sizeof(buf), "HICANN Status = 0x%02hx\n" ,state2 );
					//jtag.logging(buf);
					++jtagid;
				}

				if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))
				{
					error1 = 0;
					error2 = 0;
					//printf("Sucessfull init  \n");
					//printf("Test DNC1 -> DNC2  \n");

					for (int num_data=0;num_data<NUM_DATA;++num_data) {
						wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
						//jtag.DNC_start_config(hicann_nr,wdata64);
						jtag->DNC_start_pulse(hicann_nr,wdata64&0x7fff);
						//usleep(10);
						jtag->HICANN_get_rx_data(rdata64);

						if ( (rdata64&0x7fff) != (wdata64&0x7fff) ) {
							++error1;
							//printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
						}
					}

					//printf("Test DNC2 -> DNC1  \n");

					for (int num_data=0;num_data<NUM_DATA;++num_data) {
						wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
						jtag->HICANN_set_tx_data(wdata64);
						jtag->HICANN_start_cfg_pkg();
						//usleep(10);
						jtag->DNC_set_channel(hicann_nr);
						jtag->DNC_get_rx_data(rdata64);

						if (rdata64 != wdata64) {
							++error2;
							printf("Trial %d error: Wrote: 0x%016llX Read: 0x%016llX\n",num_data,static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
						}
					}
					printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);
					if (error1 || error2) 
						++data_error;
				} else {
					error1 = -1;
					error2 = -1;
					++init_error;
					printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
				}

				jtag->DNC_test_mode_dis();
				jtag->HICANN_set_test(0);
			
	}


};


class LteeTmL2Bench : public Listee<Testmode>{
public:
	LteeTmL2Bench() : Listee<Testmode>(string("tm_l2bench"),string("Benchmark for L2 communications")){};
	Testmode * create(){return new TmL2Bench();};
};

LteeTmL2Bench ListeeTmL2Bench;

