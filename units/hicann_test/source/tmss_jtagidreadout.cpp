#include "common.h"
#include "testmode.h"

#include <boost/serialization/vector.hpp>

// testmode to perform "almost full" digital test on one HICANN each while controlling the wafer tester

#include "reticle_control.h"
#include "hicann_ctrl.h"
#include "dnc_control.h"
#include "fpga_control.h"

#include "neuron_control.h" 		//neuron control class (merger, background genarators)
#include "spl1_control.h"			//spl1 control class
#include "l1switch_control.h"  		//layer 1 switch control class
#include "fg_control.h"          	//floating gate control class
#include "synapse_control.h"       	//synapse control class
#include "repeater_control.h"      //repeater control class
#include "neuronbuilder_control.h" //neuron builder control class


using namespace std;
using namespace facets;


class TmssJtagIDreadout : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmss_jtagidreadout"; return ss.str();}
public:



	enum teststat {pass, fail, noexec};
	
	friend std::ostream & operator <<(std::ostream &os, teststat ts){
		os<<(ts==pass?"pass":(ts==fail?"fail":"not executed"));
		return os;
	}
	
	friend std::fstream & operator <<(std::fstream &os, teststat ts){
		os<<(ts==pass?"pass":(ts==fail?"fail":"not executed"));
		return os;
	}
	
	teststat test_id, test_fpga_init, test_fpga_dnc, test_dnc_hicann, test_spl1_fpga;
	
	
	//declare vectros of control modules
	HicannCtrl  *hc;
	DNCControl  *dc;
	FPGAControl *fpc;	

	vector<ReticleControl*> ret;
	//vector<FPGAControl*>    fc;
	//vector<DNCControl*>     dc;
	
//	HicannCtrl  *hc2;  //
//	DNCControl  *dc2;  //
//	FPGAControl *fc2;  //
	
	NeuronControl *nc;
	SPL1Control *spc;
	//vector<SPL1Control*>  spc; 
	//vector<NeuronControl*>  nc;


	vector<ReticleControl*>::iterator it; //iterator for reticles
	
	ReticleControl::IDtype id; //varible for IP
	
	// varibles for writing in file (sysbench)
	uint num_hicanns, hicann_nr, reticle_nr;  //hicann_jtag_nr 
	double fpga_dnc_errate, dnc_hicann_errate, spl1_fpga_errate;
	
	//unsigned long loops;

	//unsigned int crc_fpga_onerun;
	//unsigned int crc_dnc_onerun;
	//unsigned int crc_hicann_onerun;
	//unsigned int err_dnc_onerun;
	//unsigned int err_hicann_onerun;

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() {

    char c;
    bool cont=true;
 
    do{
#ifdef MPIMODE
	world.barrier();
	if (MPIMASTER) {
#endif
		cout << endl;
		cout << "Select test option:" << endl;
		cout << "0: Delete reticle instances" << endl;
		cout << "1: Create reticle instances" << endl;
		cout << "2: Reset wafer and all recticles, then initialization" << endl;
		cout << "3: Initialize HICACNNs" << endl;		
		cout << "4: Reset all RAMs in reticles" << endl;
		cout << "5: Program floating gates to zero in reticles" << endl;		
		cout << "6: shift ID and read out" << endl;
		cout << "7: low level JTAG reset and ID shift" << endl;
		cout << "8: Test JTAG on all active reticles (with bypass)" << endl;
		cout << "9: Initialize highspeed on HICACNNs" << endl;		
		cout << "a: FPGA-DNC-HICANN init and bench test" << endl;


		cout << "p: Print out reticle coordinates: usage example" << endl;
		
		cout << "x: exit" << endl;

		cin >> c;
		cout << endl;
#ifdef MPIMODE
	}
	mpi::broadcast(world, c, 0);
#endif

		
		switch(c){

			case '0':{  //delete reticle instances
				cout << "Deleting all created reticles" << endl;
				for (it=ret.begin(); it < ret.end(); it++) delete (*it);
				ret.clear();
				ReticleControl::printInstantiated();

			} break;
			
			
				
			case '1':{  //create reticle instances
					ReticleControl *tmp;
					uint ret_number, ip, port, pownum;
					//~ tmp=new ReticleControl(commodel,1,4); //reticles 37-40
					//~ ret.push_back(tmp);
					//~ ret.push_back(ret[0]->createNeighbor(1,5));
					//~ ret.push_back(ret[0]->createNeighbor(0,4));
					//~ ret.push_back(ret[0]->createNeighbor(2,4));
					
					//~ tmp=new ReticleControl(commodel,2,6); //reticles 29-32
					//~ ret.push_back(tmp);
					//~ ret.push_back(ret[0]->createNeighbor(3,6));
					//~ ret.push_back(ret[1]->createNeighbor(3,7));
					//~ ret.push_back(ret[2]->createNeighbor(4,7));

					
					//~ tmp=new ReticleControl(commodel,5,6); //reticles 25-28
					//~ ret.push_back(tmp);
					//~ ret.push_back(ret[0]->createNeighbor(5,7));
					//~ ret.push_back(ret[0]->createNeighbor(4,6));
					//~ ret.push_back(ret[0]->createNeighbor(6,6));
					
					//~ tmp=new ReticleControl(commodel,3,6); //reticle 31
					//~ ret.push_back(tmp);
					
#ifdef MPIMODE
					if (MPIMASTER) {
						cout << "Numbers of reticles to initialize: ";
						ret_number = world.size();
						cout << ret_number <<endl;
					};
#else
						cout << "How many reticles do you want to initialize? ";
						cin >> ret_number;					
#endif					
					
#ifdef MPIMODE
					std::cout << "HERE1: " << world.rank() << std::endl;
					mpi::broadcast(world, ret_number, 0);
#endif

					std::vector<uint> ips;
					std::vector<uint> pownums;
					for(uint i=0; i<ret_number; i++){	
						if (MPIMASTER) {
							cout << "For reticle " << i+1 << ":" << endl;
							cout << "Enter the last IP number of the FPGA: ";
							cin >> ip;
							cout << "Enter the power number of ther reticle: ";
							cin >> pownum;
							ips.push_back(ip);
							pownums.push_back(pownum);
						};
#ifdef MPIMODE
						mpi::broadcast(world, ips, 0);
						mpi::broadcast(world, pownums, 0);
#endif
					}
#ifdef MPIMODE
					assert((ips.size() == pownums.size()) && (pownums.size() == world.size()));
					tmp=new ReticleControl(commodel, ReticleControl::ip_t(192,168,1,ips[world.rank()]), pownums[world.rank()]);
					ret.push_back(tmp);
					
					//static std::string processor_name(); um Processname auszulesen->beenden
#else
					assert(ips.size() == pownums.size());
					for (int i = 0; i < ips.size(); i++) {
						tmp=new ReticleControl(commodel, ReticleControl::ip_t(192,168,1,ips[i]), pownums[i]);
						ret.push_back(tmp);
					}
#endif
					
					//~ tmp=new ReticleControl(commodel, ReticleControl::ip_t(192,168,1,9), 29);
					//~ ret.push_back(tmp);
					//~ tmp=new ReticleControl(commodel, ReticleControl::ip_t(192,168,1,9), 30);
					//~ ret.push_back(tmp);
					//~ tmp=new ReticleControl(commodel, ReticleControl::ip_t(192,168,1,9), 31);
					//~ ret.push_back(tmp);
					//~ tmp=new ReticleControl(commodel, ReticleControl::ip_t(192,168,1,9), 32);
					//~ ret.push_back(tmp);
				} break;



			case '2':{  //reset wafer and all reticles
				uint retnumber=0, flag=0;
				cout << "reset wafer and all reticles: " << flush;
				//cin >> retnumber;
					
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {

					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							
							//dnc_shutdown();
							//hicann_shutdown();							
						
							for (uint h=0; h<8; h++){	//reset fpga	
								(*it)->jtag->set_hicann_pos(h);	
								
								cout << "Pulse FPGA reset..." << endl;
								unsigned int curr_ip;
								curr_ip = (*it)->jtag->get_ip();
								S2C_JtagPhys2Fpga *comm_ptr;
								comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>((*it)->hicann[h]->GetCommObj());
								if (comm_ptr != NULL) {
									comm_ptr->set_fpga_reset(curr_ip, true, true, true, true, true); // set reset
						
									usleep(100000); // wait: optional, to see LED flashing - can be removed
						
									comm_ptr->set_fpga_reset(curr_ip, false, false, false, false, false); // release reset
								} else {
									printf("tm_sysbench warning: bad s2comm pointer; did not perform soft FPGA reset.\n");
								}
								
							}

							cout << "Pulse DNC/HICANN reset..." << endl;
							(*it)->jtag->FPGA_set_fpga_ctrl(1);

#ifdef MPIMODE
							world.barrier();
							if(MPIMASTER){
		
								cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
								(*it)->comm->ResetWafer();
		
							}
							world.barrier();

#else
						}
					}
				
					flag=0;
					
					it=ret.begin();
					cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
					(*it)->comm->ResetWafer();										
								
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
#endif
				
							cout << "Reset JTAG..." << endl;
							(*it)->jtag->reset_jtag();
							
							for (uint i=0; i<8; i++){
								(*it)->hicannInit(i, true);
							}

							//if (!initConnectionFPGA()){
							//	cout<< "Initialization fail"<<endl;	
							//}
						}
					}				
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;

			} break;
			


			case '3':{  //init hicanns
				for (it=ret.begin(); it != ret.end(); it++)
					for (uint i=0; i<8; i++)
						(*it)->hicannInit(i, true);
			} break;	
			
			
				
			case '4':{  //reset all RAMs in reticles
				uint retnumber=0, flag=0;
				
#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif			
	
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) cout << "No such reticle instantiated!" << endl;
				else {
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							cout << "Resetting reticle " << (*it)->reticleID(ReticleControl::powernumber) << endl;
							for (uint h=0; h<8; h++){
								cout << "Resetting HICANN " << h << flush;
								(*it)->hicann[h]->getSC_top().reset_drivers(); cout << "." << flush;
								(*it)->hicann[h]->getSC_bot().reset_drivers(); cout << "." << flush;
								(*it)->hicann[h]->getLC_cl().reset(); cout << "." << flush;
								(*it)->hicann[h]->getLC_cr().reset(); cout << "." << flush;
								(*it)->hicann[h]->getLC_tl().reset(); cout << "." << flush;
								(*it)->hicann[h]->getLC_tr().reset(); cout << "." << flush;
								(*it)->hicann[h]->getLC_bl().reset(); cout << "." << flush;
								(*it)->hicann[h]->getLC_br().reset(); cout << "." << flush;
								(*it)->hicann[h]->getRC_cl().reset(); cout << "." << flush;
								(*it)->hicann[h]->getRC_tl().reset(); cout << "." << flush;
								(*it)->hicann[h]->getRC_tr().reset(); cout << "." << flush;
								(*it)->hicann[h]->getRC_bl().reset(); cout << "." << flush;
								(*it)->hicann[h]->getRC_br().reset(); cout << "." << flush;
								(*it)->hicann[h]->getRC_cr().reset(); cout << "." << flush;
								(*it)->hicann[h]->getNBC().initzeros(); cout << "." << flush;
								(*it)->hicann[h]->getNC().nc_reset(); cout << "." << flush;
								cout << " Done" << endl;
							}
							cout << endl;
						}
					}
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;
			} break;						
			


			case '5':{  //program floating gates to zero
				uint retnumber=0, flag=0;
				
#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif				
				
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							cout << "Programming floating gates to zero: reticle " << (*it)->reticleID(ReticleControl::powernumber) << endl;

							for(uint hc=0; hc<8; hc++){
								setupFg(((*it)->hicann[hc]->getFC_bl()));
								setupFg(((*it)->hicann[hc]->getFC_tl()));
								setupFg(((*it)->hicann[hc]->getFC_br()));
								setupFg(((*it)->hicann[hc]->getFC_tr()));
								(*it)->hicann[hc]->getFC_bl().initzeros(0);
								(*it)->hicann[hc]->getFC_tl().initzeros(0);
								(*it)->hicann[hc]->getFC_br().initzeros(0);
								(*it)->hicann[hc]->getFC_tr().initzeros(0);
							}
							
							cout << "Lines: " << flush;
							for(int i=0; i<24; i++){
								cout << "." << flush;
								for(uint hc=0; hc<8; hc++){
									while((*it)->hicann[hc]->getFC_bl().isBusy()); //polling
									(*it)->hicann[hc]->getFC_bl().program_cells(i,0,0);
									while((*it)->hicann[hc]->getFC_tl().isBusy()); //polling
									(*it)->hicann[hc]->getFC_tl().program_cells(i,0,0);
									while((*it)->hicann[hc]->getFC_br().isBusy()); //polling
									(*it)->hicann[hc]->getFC_br().program_cells(i,0,0);
									while((*it)->hicann[hc]->getFC_tr().isBusy()); //polling
									(*it)->hicann[hc]->getFC_tr().program_cells(i,0,0);
								}
							}
							cout << " Done" << endl;
						}
					}
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;
			} break;



			case '6':{  //shift ID and read out
							
				uint retnumber=0, flag=0;

#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif
					
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							cout << "Read out ID's in reticle: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;

							for (uint h=0; h<8; h++){	//switch through all hicanns
									
								// Set HICANN JTAG address, reset HICANN and JTAG
								(*it)->jtag->set_hicann_pos(h);
								(*it)->jtag->reset_jtag();
								(*it)->	jtag->FPGA_set_fpga_ctrl(1);
								(*it)->jtag->reset_jtag();
									
		
				
								//id read out
								uint64_t jtagid=0xf;
								(*it)->jtag->read_id(jtagid,(*it)->jtag->pos_hicann);
			
								cout << "hicann: " << (*it)->jtag->pos_hicann << endl;
								cout << "HICANN ID: 0x" << hex << jtagid << endl;
							}
							cout << endl;
						}
					}
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;
			

			} break;
		
		
		
			case '7':{  //low level JTAG reset and ID shift
			

				uint retnumber=0, flag=0;
				char data_taking='n';

#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif
					
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {
					
					unsigned long loops=0;
					
					if(MPIMASTER){	
						cout<< "Should data be written in file? (y/n): ";
						cin >> data_taking;								
						cout << "number of test loops: " << flush;
						cin >> loops;
						cout << endl;
					}
					
#ifdef MPIMODE
					mpi::broadcast(world, loops, 0);
					mpi::broadcast(world, data_taking, 0);
#endif					
					
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							
							//jtag-loop parameters
							unsigned long failure=0;
							unsigned long trun=0;
					
							string filename = "JTAG chain test/jct_pnr";
							if(!make_filepath(filename)) break;
							ofstream testfile;
					
							if(data_taking=='y'){
								//open file and write test parameters
								testfile.open(filename.c_str(), ios::trunc);
								if 	(!testfile) break;
								testfile << "                                                  "<< endl; // space for testresult
								testfile << "JTAG-chain test parameters:" << endl;
								testfile << "Powernumber: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
								
								ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
								testfile << "FPGA IP: " << temp << endl;							
															
								testfile << "DNC port: " << dec << (*it)->reticleID(ReticleControl::udpport) << endl;
	#ifdef MPIMODE
								testfile << "mpi #processes: " << world.size() <<endl;
	#else
								testfile << "mpi #processes: --"<< endl;
	#endif							
								testfile << "Loops: " << loops << endl << endl <<endl;
								testfile << "Failures: (run) working chips" <<endl;
								testfile.flush();
								testfile.close();
							};
							

							// show test parameters
							ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
							cout << "JTAG chain test on reticle:" << endl;
							cout << "Powernumber: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
							cout << "FPGA IP: " << temp << endl;							
							cout << "DNC port: " << dec << (*it)->reticleID(ReticleControl::udpport) << endl <<endl;
							
							
							
							for(trun =0; trun <loops; trun++){ 	//repeat jtagchaintest
										
								vector<uint> res_temp;
								res_temp.clear();
								
								string id_chain="";
								id_chain.clear();
			
								uint shift_width=672;
	

								(*it)->jtag->jtag_write(0,1,50);
	
								// Go to RUN-TEST/IDLE
								(*it)->jtag->jtag_write(0,0);

								// Go to SELECT-DR-SCAN
								(*it)->jtag->jtag_write(0,1);

								// Go to CAPTURE-DR
								(*it)->jtag->jtag_write(0,0);	

								// Go to SHIFT-DR
								(*it)->jtag->jtag_write(0,0);

								for(uint i=0; i < shift_width; i++)
									{
										(*it)->jtag->jtag_write( 0, i==shift_width-1, jtag_lib::TDO_READ );
								}
	
								// Go to UPDATE-DR - no effect
								(*it)->jtag->jtag_write(0,1);

								// Go to RUN-TEST/IDLE
								(*it)->jtag->jtag_write(0,0);

								(*it)->jtag->jtag_execute();

								// read bulk tdo buffer
								for(uint i=0;i<shift_width/4;i++){
									uint temp=0;
									(*it)->jtag->jtag_read(temp, 4);
									res_temp.push_back(temp);
								}
			
								//variable for counting i.O.-chips
								int nr_chips=0;
			
								//cout << "result: 0x" <<endl;
			
								for(int i1 = 1; i1<11;i1++){  //compare ID's
				
									stringstream sschip_id;
									sschip_id.str("");
									sschip_id.clear();
				
									for(int i2 = ((16*i1)-1); i2>=(16*(i1-1)); i2--){  //read out 1 ID 
										//cout << hex << res_temp[i2];
					
										sschip_id << hex << res_temp[i2];
									}
								
									string chip_id;
									chip_id = sschip_id.str();
									id_chain +=chip_id;
				
									//cout <<endl << chip_id <<endl;
				
									if (chip_id == "00000000babebeef") nr_chips++;
									if (chip_id == "000000001474346f") nr_chips++;
									if (chip_id == "0000000014849434") nr_chips++;

								}
	
								//cout << id_chain << endl;
								
								if(data_taking=='y'){
									//write in file, if defect
									if (nr_chips != 10){
										failure++;
										testfile.open(filename.c_str(), ios::app);
										if 	(!testfile) break;
										testfile << "(" << trun+1 << ") "  << nr_chips <<",   "<< id_chain << endl;
										testfile.flush();
										testfile.close();
									};
								}

								//cancel jtag-chain test
								if (trun >100 && failure > (0.95*trun)){
									trun++;
									break;
								};

							}
							
							if(data_taking=='y'){
								//write result in file
								testfile.open(filename.c_str(), ios::ate | ios::in);
								if 	(!testfile) break;
								testfile.seekp (0, ios::beg);
								testfile << "result [failure(loops)]: " << dec <<failure <<"("<< dec << trun << ")"<< endl;
								testfile.flush();
								testfile.close();
							}
							
							cout << "failure: " << dec << failure <<endl;
							cout << "loops: " << dec << trun <<endl;
							cout << "=================================" <<endl;
							cout << endl;
						}
					}
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;							
			} break;
			
			
			case '8':{  //Test JTAG on all active reticles (with bypass)
				
				uint retnumber=0, flag=0;
				char data_taking ='n';

#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif
					
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {
					
					unsigned long loops=0;
					
					if(MPIMASTER){	
						cout<< "Should data be written in file? (y/n): ";
						cin >> data_taking;	
						cout << "number of test loops: " << flush;
						cin >> loops;
						cout << endl;
					};
					
#ifdef MPIMODE
					mpi::broadcast(world, loops, 0);
					mpi::broadcast(world, data_taking, 0);
#endif					
					
					srand(1);
					
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							
							//reset jtag
							(*it)->jtag->reset_jtag();
							
							//jtag-loop parameters
							unsigned long failure=0;
							unsigned long trun=0;
							uint64_t tin, tout;
							
							string filename = "JTAG bypass/jbypt_pnr";
							if(!make_filepath(filename)) break;							
							ofstream testfile;
							
							if(data_taking=='y'){
						
								//open file and write test parameters
								testfile.open(filename.c_str(), ios::trunc);
								if 	(!testfile) break;
								testfile << "                                                  "<< endl; // space for testresult
								testfile << "JTAG-bypass test parameters:" << endl;
								testfile << "Powernumber: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
								
								ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
								testfile << "FPGA IP: " << temp << endl;							
															
								testfile << "DNC port: " << dec << (*it)->reticleID(ReticleControl::udpport) << endl;
#ifdef MPIMODE
								testfile << "mpi #processes: " << world.size() <<endl;
#else
								testfile << "mpi #processes: --"<< endl;
#endif		
								
								testfile << "Loops: " << loops << endl << endl <<endl;
								testfile << "Failures: (run) testinput,testoutput" <<endl;
								testfile.flush();
								testfile.close();
							};
							

							// show test parameters
							ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
							cout << "JTAG-bypass test on reticle:" << endl;
							cout << "Powernumber: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
							cout << "FPGA IP: " << temp << endl;							
							cout << "DNC port: " << dec << (*it)->reticleID(ReticleControl::udpport) << endl <<endl;
							
							
							
							for(trun =0; trun <loops; trun++){ 	//repeat jtagchaintest
								
								//cout << "Run " << run+1 << endl <<flush;

								tin=rand()%0xffffffff;
								tout=0;
								(*it)->jtag->bypass<uint64_t>(tin, 64, tout);
								tout = tout >> ((*it)->jtag->pos_fpga+1);
								
								
								if(data_taking=='y'){
									//write in testfile, if failure
									if (tin != tout){
										failure++;
										testfile.open(filename.c_str(), ios::app);
										if 	(!testfile) break;
										testfile << "(" << dec << trun << ") 0x" << hex << tin << ", 0x" << hex <<tout << endl;
										testfile.flush(); 
										testfile.close();
									};
								}
							
								
								//cout << "Total fails: ";
								//cout << "Reticle " << dec << (*it)->reticleID(ReticleControl::powernumber) << ": " << failure << ", ";
								//cout << "(out of " << run+1 << " runs)" << endl<< flush;
								
								
								//cancel jtag-bypass test
								if (trun > 100 && failure > (0.95*trun)){
									trun++;
									break;
								};

							}
							
							if(data_taking=='y'){
								//write result in file
								testfile.open(filename.c_str(), ios::ate | ios::in);
								if 	(!testfile) break;
								testfile.seekp (0, ios::beg);
								testfile << "result [failure(loops)]: " << dec <<failure <<"("<< dec << trun << ")"<< endl;
								testfile.flush();
								testfile.close();	
							};
							
							cout << "failure: " << dec << failure <<endl;
							cout << "loops: " << dec << trun <<endl;
							cout << "=================================" <<endl;
							cout << endl;
						}
					}
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;							
			} break;
				
			
			
			case '9':{  //initialize highspeed on hicanns
			
				uint retnumber=0, flag=0;
				char data_taking='n';

#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif
					
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {
					
					unsigned long loops=0;
					
					if(MPIMASTER){
						cout<< "Should data be written in file? (y/n): ";
						cin >> data_taking;					
						cout << "number of test loops: " << flush;
						cin >> loops;
						cout << endl;
					}
					
#ifdef MPIMODE
					mpi::broadcast(world, loops, 0);
					mpi::broadcast(world, data_taking, 0);
#endif					
					
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							
							int count_init=0;
							int trun=0;							
					
							for (trun=0; trun<loops; trun++){
								
								cout<< "Reticle: "<< dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
								cout<< "Run: "<< trun <<endl;

								//dnc_shutdown();
								//hicann_shutdown();
						
								for (uint h=0; h<8; h++){	//reset fpga	
									(*it)->jtag->set_hicann_pos(h);	
									
									cout << "Pulse FPGA reset..." << endl;
									unsigned int curr_ip;
									curr_ip = (*it)->jtag->get_ip();
									S2C_JtagPhys2Fpga *comm_ptr;
									comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>((*it)->hicann[h]->GetCommObj());
									if (comm_ptr != NULL) {
										comm_ptr->set_fpga_reset(curr_ip, true, true, true, true, true); // release reset
							
							
										usleep(100000); // wait: optional, to see LED flashing - can be removed
							
										comm_ptr->set_fpga_reset(curr_ip, false, false, false, false, false); // release reset
									} else {
										printf("tm_sysbench warning: bad s2comm pointer; did not perform soft FPGA reset.\n");
									}
									
								}
	
								cout << "Pulse DNC/HICANN reset..." << endl;
								(*it)->jtag->FPGA_set_fpga_ctrl(1);
	
#ifdef MPIMODE
								world.barrier();
#endif								
								if(MPIMASTER){
									cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
									(*it)->comm->ResetWafer();
								}
#ifdef MPIMODE								
								world.barrier();
#endif					
						
								cout << "Reset JTAG..." << endl;
								(*it)->jtag->reset_jtag();
								
								if (initConnectionFPGA()){
									count_init++;
								}
								
								if (trun >10 && count_init<2){
									break;
								}
							}
								
							if(data_taking=='y'){
#ifdef MPIMODE				
								//for file
								stringstream ss_temp;
								ss_temp.str("");
								ss_temp.clear();
						
								ss_temp << dec << (*it)->reticleID(ReticleControl::powernumber);				
								string pwnr;
								ss_temp >> pwnr;
								
								ss_temp.str("");
								ss_temp.clear();							
	
								ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
								ss_temp << temp;
								string ipadd;
								ipadd=ss_temp.str();
							
								std::cout << "I'm here: " << world.rank() << std::endl;							
								world.barrier();
								if(MPIMASTER){
									//collect data from different processes
									vector<int> all_inits;
									vector<int> all_truns;
									vector<string> all_pwnrs;
									vector<string> all_ipadds;
															
									gather(world, count_init, all_inits, 0);
									gather(world, trun, all_truns,0);
									gather(world, pwnr, all_pwnrs, 0);
									gather(world, ipadd, all_ipadds, 0);
	
									std::cout << "I'm here d): " << world.rank() << std::endl;						
								
									
									// make filepath
									
									
									ostringstream filepath;
									filepath.str("");
									filepath.clear();
									filepath << "/afs/kip.uni-heidelberg.de/user/schnier/hicann zeug/testresults/";
									filepath << "init_test.txt";
									
									string filename;
									filename=filepath.str();
	
									//open file
									ofstream testfile;
									testfile.open(filename.c_str(), ios::app);
									if 	(!testfile) break;
																
									for (int proc = 0; proc < world.size(); ++proc){  //write test parameters
										
										testfile << "FPGA IP: " << all_ipadds[proc];	
										
										testfile << ", Powernumber: " << dec << all_pwnrs[proc];
										testfile <<" , Inits(loops): "<< all_inits[proc] <<"("<< all_truns[proc] <<")"<<endl;
										
										testfile.flush();
									}
									testfile.close();
								}
								else {
								    gather(world, count_init, 0);
									gather(world, trun, 0);
									gather(world, pwnr, 0);
									gather(world, ipadd, 0);							    
								}
	
								std::cout << "I'm here: " << world.rank() << std::endl;							
								world.barrier();
																	
#else							
								// make filepath
								ostringstream filepath;
								filepath.str("");
								filepath.clear();
								filepath << "/afs/kip.uni-heidelberg.de/user/schnier/hicann zeug/testresults/";
								filepath << "init_test.txt";
								
								string filename;
								filename=filepath.str();
						
								//open file and write test parameters
								ofstream testfile;
								testfile.open(filename.c_str(), ios::app);
								if 	(!testfile) break;
								
								ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
								testfile << "FPGA IP: " << temp;	
								
								testfile << ", Powernumber: " << dec << (*it)->reticleID(ReticleControl::powernumber);
								testfile <<" , Inits(loops): "<< count_init <<"("<< trun <<")"<<endl;
								
								testfile.flush();
								testfile.close();
#endif						
							};
						}
					}
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;
								
			} break;
			
			
			
			case 'a':{ //sysbench: FPGA-DNC, DNC-Hicann, FPGA-DNC-Hicann

				uint retnumber = 0, flag=0;
				char data_taking='n';
				
#ifndef MPIMODE				
				cout << "Enter power number of the reticle (0 for all reticles): " << flush;
				cin >> retnumber;
#endif
					
				//translate retnumber to sequential number, 0 should not cause an error!
				retnumber = ReticleControl::transCoord(ReticleControl::powernumber, ReticleControl::sequentialnumber, retnumber);
				if (retnumber && !(ReticleControl::isInstantiated(retnumber))) {cout << "No such reticle instantiated!" << endl; flag=1;}
				else {
					
					unsigned long loops=0;
					unsigned int runtime_s = 15; // sec
					
					if (MPIMASTER) {
						cout<< "Should data be written in file? (y/n): ";
						cin >> data_taking;						
						cout << "number of test loops: " << flush;
						cin >> loops;
						cout << "Runtime bench tests (in sec): "<< flush;
						cin >> runtime_s;
						cout << endl;
					}
#ifdef MPIMODE
					mpi::broadcast(world, loops, 0);
					mpi::broadcast(world, runtime_s, 0);
					mpi::broadcast(world, data_taking, 0);
#endif
					for (it=ret.begin(); it != ret.end(); it++){ //find the right reticle
						if ((*it)->reticleID(ReticleControl::sequentialnumber)==retnumber || !retnumber){
							flag=1;
							
							unsigned long trun=0;

														
							test_id = noexec;
							test_fpga_init = noexec;
							test_fpga_dnc = noexec;
							test_dnc_hicann = noexec;
							test_spl1_fpga = noexec;
							//test_sw = noexec;
							//test_rep = noexec;
							//test_fg = noexec;
							//test_synblk0 = noexec;
							//test_synblk1 = noexec;
							
							string filename = "sysbench/sbt_pnr";
							if(!make_filepath(filename)) break;
							string filename_tot = "sysbench/gesamtergebnisse/sbt_tot_pnr";
							if(!make_filepath(filename_tot)) break;
							ofstream testfile;
							
							if(data_taking=='y'){				
								//open file and write test parameters
								testfile.open(filename.c_str(), ios::trunc);
								if 	(!testfile) break;
								testfile << "Bench test parameters:" << endl;
								testfile << "Powernumber: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
								
								ReticleControl::ip_t temp=(*it)->reticleID(ReticleControl::fpgaip);
								testfile << "FPGA IP: " << temp << endl;							
								testfile << "DNC port: " << dec << (*it)->reticleID(ReticleControl::udpport) << endl;
								testfile << "FPGA-DNC Bench:  6,25 MEents/s"<<endl;
								testfile << "DNC-HICANN Bench:  8,9 kEvents/s"<<endl;
								testfile << "FPGA-DNC-HICANN Bench:  6,25 MEvents/s"<<endl<<endl;
								
	
	#ifdef MPIMODE
								testfile << "mpi # of processes: " << world.size() <<endl;
	#else
								testfile << "mpi # of processes: --"<< endl;
	#endif								
								testfile << "Loops: " << loops << endl;
								testfile << "Runtime bench tests [s]: "<< runtime_s << endl <<endl;
								testfile << "Results: (run) JTAG ID, FPGA-DNC Bench, DNC-HICANN Bench, FPGA-DNC-HICANN Bench" <<endl;
								testfile << "(run) ID read, Init/crc_fpga/crc_dnc, Init/crc_dnc/crc_hc/err_dnc/err_hc, Init/crc_fpga/crc_dnc/crc_hc" <<endl<<endl;
								testfile.flush();
								testfile.close();
								
								//write in file total_data
								testfile.open(filename_tot.c_str(), ios::trunc);
								if 	(!testfile) break;
								testfile<<"pwnr_reticle,runs, runtime, hicann, id-read[#pass], init_fpga-dnc, crc_fpga, crc_dnc, data_packets, ";
								testfile<< "init_dnc-hc, crc_dnc, crc_hc, err_dnc, err_hc, data_packets, ";
								testfile<< "init_fpga-dnc-hc, crc_fpga, crc_dnc, crc_hc, data_packets"<<endl;								
								testfile.flush();
								testfile.close();	

							};
							
														
							cout << "Start bench test on reticle: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;
							

							for (uint h=0; h<8; h++){	//switch through all hicanns
								
								//variables for file: total
								unsigned long allruns_crc_dnc1=0;
								unsigned long allruns_crc_fpga1=0;
								unsigned long allruns_crc_hicann2=0;
								unsigned long allruns_crc_dnc2=0;
								unsigned long allruns_crc_hicann3=0;
								unsigned long allruns_crc_dnc3=0;
								unsigned long allruns_crc_fpga3=0;
								unsigned long allruns_err_dnc=0;
								unsigned long allruns_err_hicann=0;
								unsigned long fail_init1=0;
								unsigned long fail_init2=0;
								unsigned long fail_init3=0;
								unsigned long fail_idread=0;
								
							// Set HICANN JTAG address, reset HICANN and JTAG
								(*it)->jtag->set_hicann_pos(h);
								(*it)->jtag->reset_jtag();
								(*it)->	jtag->FPGA_set_fpga_ctrl(1);
								(*it)->jtag->reset_jtag();
								
								hicann_nr = (7-((*it)->jtag->pos_hicann));
								
								nc        = &hc->getNC();
								spc       = &hc->getSPL1Control();
								
								//nc.push_back((*it)->hicann[h]->getNC());
								//spc.push_back((*it)->hicann[h]->getSPL1Control());
								


								if(data_taking=='y'){
									//write in file
									testfile.open(filename.c_str(), ios::app);
									if 	(!testfile) break;
									testfile <<"HICANN: "<< (h+1) << endl;
									testfile.flush();
									testfile.close();
								};								
								
								for (trun=0; trun < loops; trun++){
									
									//variables for file: one run
									unsigned int crc_fpga_onerun=0;
									unsigned int crc_dnc_onerun=0;
									unsigned int crc_hicann_onerun=0;
									unsigned int err_dnc_onerun=0;
									unsigned int err_hicann_onerun=0;									

									cout << "test on reticle: " << dec << (*it)->reticleID(ReticleControl::powernumber) << endl;									
									cout << endl << "Using DNC channel " << hicann_nr << endl;
									cout << "HICANN JTAG position " << dec << (*it)->jtag->pos_hicann <<endl;									
									cout << "********** TEST RUN " << dec << (trun+1) << ": **********" << endl << endl;
															
									// ####################################################################################
									cout << "Verifying JTAG IDs..." << endl;
									uint64_t jtagid;
									test_id = pass;
									
									reset_all(h);
				
									#ifdef MPIMODE
									std::cout << "I'm here: " << world.rank() << std::endl;
									world.barrier();
									//#endif
									if (MPIMASTER) {
										cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
										(*it)->comm->ResetWafer();									
									}
									//#ifdef MPIMODE
									world.barrier();
									
									cout << "Reset JTAG..." << endl;
									(*it)->jtag->reset_jtag();
									#endif

									(*it)->jtag->read_id(jtagid,(*it)->jtag->pos_hicann);
									cout << "HICANN ID: 0x" << hex << jtagid << endl;
									if (jtagid != 0x14849434) test_id = fail;

									(*it)->jtag->read_id(jtagid,(*it)->jtag->pos_dnc);
									cout << "DNC ID: 0x" << hex << jtagid << endl;
									if (jtagid != 0x1474346f) test_id = fail;

									(*it)->jtag->read_id(jtagid,(*it)->jtag->pos_fpga);
									cout << "FPGA ID: 0x" << hex << jtagid << endl;
									if (jtagid != 0xbabebeef) test_id = fail;
									
									if (test_id != pass ) fail_idread ++;
	

									// ####################################################################################
									test_fpga_init = pass;

									cout << "Testing FPGA-DNC connection..." << endl;
									
									reset_all(h);
									
									#ifdef MPIMODE
									world.barrier();
									//#endif
									if (MPIMASTER) {
										cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
										(*it)->comm->ResetWafer();									
									}
									//#ifdef MPIMODE
									world.barrier();
									
									cout << "Reset JTAG..." << endl;
									(*it)->jtag->reset_jtag();									
									#endif
								
									if ( !initConnectionFPGA() ) {
										cout << "Stop FPGA benchmark : INIT ERROR" << endl;
										test_fpga_init = fail;
										
										fail_init1 ++;
										
										if(data_taking=='y'){
											//write in file
											testfile.open(filename.c_str(), ios::app);
											if 	(!testfile) break;
											testfile <<"(" << dec << (trun+1) <<") "<< test_id <<", "<< test_fpga_init <<"/--/--, ";
											testfile.flush();
											testfile.close();
										};										
										
										//continue;
									} else {
										test_fpga_dnc = runFpgaBenchmark(runtime_s, crc_fpga_onerun, crc_dnc_onerun) ? pass : fail;
										//Shutdown interfaces
										dnc_shutdown();
										hicann_shutdown();
										(*it)->jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
										cout <<  "Finished benchmark of FPGA-DNC connection" << endl;
										
										//count total data
										allruns_crc_fpga1 += crc_fpga_onerun;
										allruns_crc_dnc1 += crc_dnc_onerun;

										if(data_taking=='y'){
											//write in file
											testfile.open(filename.c_str(), ios::app);
											if 	(!testfile) break;
											testfile <<"(" << dec << (trun+1) <<") "<< test_id <<", "<< test_fpga_init <<"/"<< crc_fpga_onerun <<"/"<< crc_dnc_onerun <<", ";
											testfile.flush();
											testfile.close();
										};
									}

									// ####################################################################################
									test_fpga_init = pass;									
									
									cout << "Testing DNC-HICANN connection..." << endl;
									
									reset_all(h);
									
									#ifdef MPIMODE
									world.barrier();
									//#endif
									if (MPIMASTER) {
										cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
										(*it)->comm->ResetWafer();									
									}
									//#ifdef MPIMODE
									world.barrier();

									cout << "Reset JTAG..." << endl;
									(*it)->jtag->reset_jtag();
									#endif
									
									if ( !initConnectionFPGA() ) {
										cout << "Stop DNC-HICANN benchmark : INIT ERROR" << endl;
										test_fpga_init = fail;
										
										fail_init2 ++;
										
										if(data_taking=='y'){
											//write in file
											testfile.open(filename.c_str(), ios::app);
											if 	(!testfile) break;
											testfile << test_fpga_init <<"/--/--/--/--, ";
											testfile.flush();
											testfile.close();
										};										
										
										//continue;
									} else {
										test_dnc_hicann = runHicannBenchmark(runtime_s, h, crc_dnc_onerun, crc_hicann_onerun, err_dnc_onerun, err_hicann_onerun) ? pass : fail;
										//Shutdown interfaces
										dnc_shutdown();
										hicann_shutdown();
										(*it)->jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
										cout <<  "Finished benchmark of DNC-HICANN connection" << endl; 
										
										//count total data
										allruns_crc_dnc2 += crc_dnc_onerun;
										allruns_crc_hicann2 += crc_hicann_onerun;
										allruns_err_dnc += err_dnc_onerun;
										allruns_err_hicann += err_hicann_onerun;
									
										if(data_taking=='y'){
											//write in file
											testfile.open(filename.c_str(), ios::app);
											if 	(!testfile) break;
											testfile << test_fpga_init <<"/"<< crc_dnc_onerun <<"/"<< crc_hicann_onerun <<"/"<< err_dnc_onerun <<"/"<< err_hicann_onerun <<", ";
											testfile.flush();
											testfile.close();
										};										
									}

									// ####################################################################################
									test_fpga_init = pass;									
									
									cout << "Testing FPGA-DNC-HICANN L2 loopback..." << endl;
									
									reset_all(h);
									
									#ifdef MPIMODE
									world.barrier();
									//#endif
									if (MPIMASTER) {
										cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
										(*it)->comm->ResetWafer();									
									}
									//#ifdef MPIMODE
									world.barrier();

									cout << "Reset JTAG..." << endl;
									(*it)->jtag->reset_jtag();
									#endif
									
									if ( !initConnectionFPGA() ) {
										cout << "Stop full benchmark : INIT ERROR" << endl;
										test_fpga_init = fail;
										
										fail_init3 ++;
										
										if(data_taking=='y'){
											//write in file
											testfile.open(filename.c_str(), ios::app);
											if 	(!testfile) break;
											testfile << test_fpga_init <<"/--/--/--"<<endl;
											testfile.flush();
											testfile.close();
										};
										
										//continue;
									} else {
										test_spl1_fpga = runFullBenchmark(runtime_s, h, crc_fpga_onerun, crc_dnc_onerun, crc_hicann_onerun) ? pass : fail;
										//Shutdown interfaces
										dnc_shutdown();
										hicann_shutdown();
										(*it)->jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
										cout << "Finished benchmark of FPGA-DNC-HICANN connection" << endl;
										
										//count total data
										allruns_crc_fpga3 += crc_fpga_onerun;
										allruns_crc_dnc3 += crc_dnc_onerun;
										allruns_crc_hicann3 += crc_hicann_onerun;
									
										if(data_taking=='y'){
											//write in file
											testfile.open(filename.c_str(), ios::app);
											if 	(!testfile) break;
											testfile << test_fpga_init <<"/"<< crc_fpga_onerun <<"/"<< crc_dnc_onerun <<"/"<< crc_hicann_onerun <<endl;
											testfile.flush();
											testfile.close();
										};										
									}
									
									// ####################################################################################
									
									


								//	write_summary(trun, h);
									
								}
								
								if(data_taking=='y'){
									//write in file
									testfile.open(filename.c_str(), ios::app);
									if 	(!testfile) break;
									testfile <<"Total results: "<< (trun-fail_idread) <<", "<< (trun-fail_init1) <<"/"<< allruns_crc_fpga1 <<"/"<< allruns_crc_dnc1 <<", ";
									testfile << (trun-fail_init2) <<"/"<< allruns_crc_dnc2 <<"/"<< allruns_crc_hicann2 <<"/"<< allruns_err_dnc <<"/"<< allruns_err_hicann <<", ";
									testfile << (trun-fail_init3) <<"/"<< allruns_crc_fpga3 <<"/"<< allruns_crc_dnc3 <<"/"<< allruns_crc_hicann3 <<endl<<endl;
									testfile.flush();
									testfile.close();
									
									//write in file total_data
									//testfile.clear();
									testfile.open(filename_tot.c_str(), ios::app);
									if 	(!testfile) break;
									testfile<<dec << (*it)->reticleID(ReticleControl::powernumber) <<", ";
									testfile<< loops <<", "<< runtime_s <<", "<< (h+1) <<", ";
									testfile<< (trun-fail_idread) <<", "<<(trun-fail_init1) <<", "<< allruns_crc_fpga1 <<", "<< allruns_crc_dnc1 <<", "<<((trun-fail_init1)*runtime_s*6.25e+6)<<", ";
									testfile<<(trun-fail_init2) <<", "<< allruns_crc_dnc2 <<", "<< allruns_crc_hicann2 <<", "<< allruns_err_dnc <<", "<< allruns_err_hicann <<", "<< ((trun-fail_init2)*runtime_s*8.9e+3)<<", ";
									testfile<<(trun-fail_init3) <<", "<< allruns_crc_fpga3 <<", "<< allruns_crc_dnc3 <<", "<< allruns_crc_hicann3 <<", "<< ((trun-fail_init3)*runtime_s*6.25e+6)<<endl;
									testfile.flush();
									testfile.close();									
									
								};								
								
								cout << endl;
							}
						}
					}
					#ifdef MPIMODE
					world.barrier();
					#endif
				}
				if (!flag) cout << "Reticle instantiated but not found, something went wrong...." << endl;
			

			} break;


				

			case 'p':{  //Print out reticle coordinates: usage example
				uint mode;
				ReticleControl::IDtype id;
				cout << "Choose the attribute:" << endl;
				cout << "1: Print out the sequential numbers" << endl;
				cout << "2: Print out the power circuit numbers" << endl;
				cout << "3: Print out the UDP JTAG ports" << endl;
				cout << "4: Print out the analog output numbers" << endl;
				cout << "5: Print out the PIC numbers" << endl;
				cout << "6: Print out the cartesian coordinates" << endl;
				cout << "7: Print out the FPGA IPs" << endl;

				cin >> mode;
				switch (mode){
					case 1: {id=ReticleControl::sequentialnumber;} break;
					case 2: {id=ReticleControl::powernumber;} break;
					case 3: {id=ReticleControl::udpport;} break;
					case 4: {id=ReticleControl::analogoutput;} break;
					case 5: {id=ReticleControl::picnumber;} break;
					case 6: {id=ReticleControl::cartesiancoord;} break;
					case 7: {id=ReticleControl::fpgaip;} break;
					default:{id=ReticleControl::sequentialnumber;} break;
					}

				if (ret.empty()) cout << "No reticles instantiated!" << endl;
				else {
					cout << "The desired attributes of currently instantiated reticles are:" << endl;
					for (it=ret.begin(); it != ret.end(); it++){
						switch (mode){
							case 6: { //for cartesian coordinates cast as cartesian_t
								ReticleControl::cartesian_t temp=(*it)->reticleID(id);
								cout << temp << endl;
							} break;
							case 7: { //for ip cast as ip_t
								ReticleControl::ip_t temp=(*it)->reticleID(id);
								cout << temp << endl;
							} break;
							default : { //for everything else cast as uint
								uint temp=(*it)->reticleID(id);
								cout << temp << endl;
							}
						}
					}
				}
			} break;


			case 'x': cont = false; 
		}

    } while(cont);

    return true;
  }
  
	
	
  
 	void reset_all(uint hc) {

		//dnc_shutdown();
		//hicann_shutdown();		
		
		cout << "Pulse FPGA reset..." << endl;
		unsigned int curr_ip;
		curr_ip = (*it)->jtag->get_ip();
		S2C_JtagPhys2Fpga *comm_ptr;
		comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>((*it)->hicann[hc]->GetCommObj());
		if (comm_ptr != NULL) {
			comm_ptr->set_fpga_reset(curr_ip, true, true, true, true, true); // set reset


			usleep(100000); // wait: optional, to see LED flashing - can be removed

			comm_ptr->set_fpga_reset(curr_ip, false, false, false, false, false); // release reset
		} else {
			printf("tm_sysbench warning: bad s2comm pointer; did not perform soft FPGA reset.\n");
		}

		cout << "Pulse DNC/HICANN reset..." << endl;
		(*it)->jtag->FPGA_set_fpga_ctrl(1);

#ifndef MPIMODE
		cout << "Pulse Wafer (i.e. HICANN) reset..." << endl;
		(*it)->comm->ResetWafer();

		cout << "Reset JTAG..." << endl;
		(*it)->jtag->reset_jtag();
#endif
	}
	
	

	void dnc_shutdown() {
		if(!((*it)->jtag)){
			log(Logger::ERROR) << "Stage2Comm::dnc_shutdown: Object jtag not set, abort!";
			exit(EXIT_FAILURE);
		}
		(*it)->jtag->DNC_set_GHz_pll_ctrl(0xe);
		(*it)->jtag->DNC_stop_link(0x1ff);
		(*it)->jtag->DNC_set_lowspeed_ctrl(0x00);
		(*it)->jtag->DNC_set_loopback(0x00);
		(*it)->jtag->DNC_lvds_pads_en(0x1ff);
		(*it)->jtag->DNC_set_speed_detect_ctrl(0x0);
		(*it)->jtag->DNC_set_GHz_pll_ctrl(0x0);
	}



	void hicann_shutdown() {
		if(!((*it)->jtag)){
			log(Logger::ERROR) << "Stage2Comm::hicann_shutdown: Object jtag not set, abort!";
			exit(EXIT_FAILURE);
		}
		(*it)->jtag->HICANN_set_reset(1);
		(*it)->jtag->HICANN_set_GHz_pll_ctrl(0x7);
		(*it)->jtag->HICANN_set_link_ctrl(0x141);
		(*it)->jtag->HICANN_stop_link();
		(*it)->jtag->HICANN_lvds_pads_en(0x1);
		(*it)->jtag->HICANN_set_reset(0);
		(*it)->jtag->HICANN_set_GHz_pll_ctrl(0x0);
	}



	bool initConnectionFPGA()
		{
		unsigned char state1 = 0;
		uint64_t state2 = 0;
		unsigned int break_count = 0;

		//Reset Setup to get defined start point
		(*it)->jtag->FPGA_set_fpga_ctrl(0x1);
		(*it)->jtag->reset_jtag();
						
		//Stop all HICANN channels and start DNCs FPGA channel
		(*it)->jtag->DNC_start_link(0x100);
		//Start FPGAs DNc channel
		(*it)->jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x100);

		printf("Start FPGA-DNC initialization: \n");

		while ((((state1 & 0x1) == 0)||((state2 & 0x1) == 0)) && break_count<10) {
			usleep(900000);
			(*it)->jtag->DNC_read_channel_sts(8,state1);
			printf("DNC Status = 0x%02hx\t" ,state1);

			(*it)->jtag->FPGA_get_status(state2);
			printf("FPGA Status = 0x%02hx RUN:%i\n" ,(unsigned char)state2,break_count);
			++break_count;
		}
			
			if (break_count == 10) {
				dnc_shutdown();
				hicann_shutdown();
				(*it)->jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
				printf ("ERROR in FPGA-DNC initialization --> EXIT\n");
				return false;
			}
			
			
		state1 = 0;
		state2 = 0;
		break_count = 0;

		printf("Start DNC-HICANN initialization: \n");
		(*it)->jtag->DNC_set_lowspeed_ctrl(0xff); // ff = lowspeed
		(*it)->jtag->DNC_set_speed_detect_ctrl(0x00); // 00 = no speed detect
		(*it)->jtag->HICANN_set_link_ctrl(0x061);
        
		while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && break_count < 10) {
			(*it)->jtag->HICANN_stop_link();
			(*it)->jtag->DNC_stop_link(0x0ff);
			(*it)->jtag->HICANN_start_link();
			(*it)->jtag->DNC_start_link(0x100 + 0xff);

			usleep(90000);
			(*it)->jtag->DNC_read_channel_sts(hicann_nr,state1);
			printf("DNC Status = 0x%02hx\t" ,state1 );
			(*it)->jtag->HICANN_read_status(state2);
			printf("HICANN Status = 0x%02hx\n" ,(unsigned char)state2 );
			++break_count;
		}

		if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41)) {
			printf("Sucessfull init DNC-HICANN connection \n");
		} else {
			printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,(unsigned char)state2);
			dnc_shutdown();
			hicann_shutdown();
			(*it)->jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
			return 0;
		}

		printf("Successfull FPGA-DNC-HICANN initialization\n");
		return true;
	}


	void setupFg(FGControl& fgc){
		fgc.set_maxcycle(255);		
		fgc.set_currentwritetime(1);
		fgc.set_voltagewritetime(15);
		fgc.set_readtime(63);
		fgc.set_acceleratorstep(9);
		fgc.set_pulselength(9);
		fgc.set_fg_biasn(5);
		fgc.set_fg_bias(8);
		fgc.write_biasingreg();
		fgc.write_operationreg();
	}		
		

/*	void write_summary(uint run, uint hc){
		stringstream ss;
		fstream file;
		ss.str("");
		ss << "/afs/kip.uni-heidelberg.de/user/schnier/hicann zeug/testresults/sysbench/results_reticle_";
		ss << dec << (*it)->reticleID(ReticleControl::powernumber)<< ".txt";
		string filename = ss.str();
		file.open(filename.c_str(), fstream::app | fstream::out);
		
		cout << endl << "########## SUMMARY run " << run << ": ##########" << endl;
		cout << "JTAG ID:                " << test_id << endl;
		cout << "FPGA Init:              " << test_fpga_init << endl;
		cout << "FPGA-DNC Bench:         " << test_fpga_dnc << endl;
		cout << "DNC-HICANN Bench:       " << test_dnc_hicann << endl;
		cout << "SPL1 loopback via FPGA: " << test_spl1_fpga << endl;

		file << endl << "############ HICANN " << hc << " #############" << endl;
		file << "########## SUMMARY run " << run << ": ##########" << endl;
		file << "JTAG ID:                " << test_id << endl;
		file << "FPGA Init:              " << test_fpga_init << endl;
		file << "FPGA-DNC Bench:         " << test_fpga_dnc << "     data packets: " << dp_onerun1 << endl;
		file << "DNC-HICANN Bench:       " << test_dnc_hicann << "     data packets: " << dp_onerun2 << endl;
		file << "SPL1 loopback via FPGA: " << test_spl1_fpga << "     data packets:" << dp_onerun3 << endl;
		if (test_fpga_dnc==fail)   file << "FPGA-DNC Error Rate:      " << fpga_dnc_errate << " (per second)" << endl;
		if (test_dnc_hicann==fail) file << "DNC-HICANN Error Rate:    " << dnc_hicann_errate << " (per second)" << endl;
		if (test_spl1_fpga==fail)  file << "SPL1 loopback Error Rate: " << spl1_fpga_errate << " (per second)" << endl;
		file.flush(); file.close();
		//~ cout << "Switch config memory:   " << test_sw << endl;
		//~ cout << "Repeater config memory: " << test_rep << endl;
		//~ cout << "FG controller memory:   " << test_fg << endl;
		//~ cout << "Synapse Block 0 memory: " << test_synblk0 << endl;
		//~ cout << "Synapse Block 1 memory: " << test_synblk1 << endl;
	}*/								

	
	bool make_filepath (string& filename){

		//date and time
		time_t curtime;
		tm *now;
		curtime = time(0);
		now = localtime(&curtime);
		cout << now->tm_mday << '.' << now->tm_mon+1 << '.'<< now->tm_year+1900 << " - " << setfill('0') << setw(2) << dec << now->tm_hour<< ':' << setw(2)<< dec << now->tm_min <<endl;
		
		// make filepath
		ostringstream filepath;
		filepath.str("");
		filepath.clear();
		filepath << "/afs/kip.uni-heidelberg.de/user/schnier/hicann zeug/testresults/";
		filepath << filename;
		filepath << dec << (*it)->reticleID(ReticleControl::powernumber) << "_";
		filepath << now->tm_mday << "_" << now->tm_mon+1 << "_" << now->tm_year+1900 << "_" << dec << ((now->tm_hour)*100 + now->tm_min);
		filepath << ".txt";
		
		filename=filepath.str();
		
		return true;
	}
	
	
	
	// ********** test methods **********	

	bool runFpgaBenchmark(unsigned int runtime_s, 
							unsigned int& crc_fpga_onerun, 
							unsigned int& crc_dnc_onerun) {

		//variables for file:
		crc_fpga_onerun=0;
		crc_dnc_onerun=0;
	
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
		(*it)->jtag->DNC_set_loopback(0xff); // DNC loopback
		// setup DNC
		(*it)->dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
		uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
		(*it)->dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

		//jtag->FPGA_set_cont_pulse_ctrl(enable, channel, poisson?, delay, seed, nrn_start, char nrn_end)
		(*it)->jtag->FPGA_set_cont_pulse_ctrl(1, 0xff, 0, 20, 100, 1, 15, hicann_nr);

		unsigned int starttime = (unsigned int)time(NULL);
		unsigned int runtime = (unsigned int)time(NULL);
		
		printf("BER FPGA: %e\tError FPGA: %i\t BER DNC: %e\tError DNC: %i\t Time:(%i:%02i:%02i)",(double(total_crc_fpga)/(double(100))),
					total_crc_fpga,(double(total_crc_dnc)/(double(100))),total_crc_dnc,(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60);

		//int i=0;
		while((runtime-starttime)< runtime_s) {
			//++i;
			usleep(1000);
			runtime = (unsigned int)time(NULL);


			//Read DNC crc counter
			(*it)->jtag->DNC_set_channel(8+hicann_nr);
			(*it)->jtag->DNC_read_crc_count(read_crc_dnc);
			(*it)->jtag->DNC_get_rx_data(read_data_dnc);
			if(read_crc_dnc != read_crc_dnc_dly) {
				total_crc_dnc += ((unsigned int)read_crc_dnc+0x100-(unsigned int)read_crc_dnc_dly)&0xff;
			
			}
			read_crc_dnc_dly = read_crc_dnc;

			//Read FPGA crc counter
			(*it)->jtag->FPGA_get_crc(read_crc_fpga);
			(*it)->jtag->FPGA_get_rx_data(read_data_fpga);
			if(read_crc_fpga != read_crc_fpga_dly) {
				total_crc_fpga += ((unsigned int)read_crc_fpga+0x100-(unsigned int)read_crc_fpga_dly)&0xff;

			}
			read_crc_fpga_dly = read_crc_fpga;
		}  
		
		// stop experiment
		printf("\ntm_sysbench::runFpgaBenchmark: Stopping benchmark transmissions\n");
		(*it)->jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
		
		//results:
		printf("*** Result FPGA-DNC connection CRC ***\n");
		printf("FPGA: CRC : %i\n", total_crc_fpga);
		printf("DNC : CRC : %i\n", total_crc_dnc);
		
		//results for file:
		crc_fpga_onerun=total_crc_fpga;
		crc_dnc_onerun=total_crc_dnc;
		
		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 0;
		}
		(*it)->jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC FPGAif delay values:\n");
		(*it)->jtag->printDelayFPGA(vec_out);
    
		if (runtime_s) fpga_dnc_errate = ((double) total_crc_fpga + (double) total_crc_dnc) / (double) runtime_s;
    
    return ((total_crc_fpga+total_crc_dnc) == 0);
    
	}

	bool runFullBenchmark(unsigned int runtime_s, 
							uint hc, 
							unsigned int& crc_fpga_onerun, 
							unsigned int& crc_dnc_onerun, 
							unsigned int& crc_hicann_onerun) {
		
		//variables for file:
		crc_fpga_onerun=0;
		crc_dnc_onerun=0;
		crc_hicann_onerun=0;
		
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

		cout << "tm_sysbench::runFullBenchmark: reset ARQ before initial loopback test" << endl;
		(*it)->jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		(*it)->jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		//for(uint t=0;t<2;t++) (*it)->hicann[hc]->tags[t]->arq.Reset();

		printf("tm_sysbench::runFullBenchmark: Starting benchmark transmissions\n");
		// loopback
		(*it)->jtag->DNC_set_loopback(0x00); // DNC loopback
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
		spc->write_cfg(0x055ff);

		cout << "tm_sysbench::runFullBenchmark: reset ARQ after initial loopback setup" << endl;
		(*it)->jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		(*it)->jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		//for(uint t=0;t<2;t++) (*it)->hicann[hc]->tags[t]->arq.Reset();

		// setup DNC
		(*it)->dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
		uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
		(*it)->dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

		//jtag->FPGA_set_cont_pulse_ctrl(enable, channel, poisson?, delay, seed, nrn_start, char nrn_end, hicann = 0)
		(*it)->jtag->FPGA_set_cont_pulse_ctrl(1, 0x01, 0, 20, 100, 1, 15, hicann_nr);

		unsigned int starttime = (unsigned int)time(NULL);
		unsigned int runtime = (unsigned int)time(NULL);
		
		printf("\rBER FPGA: %.1e BER DNC: %.1e BER HICANN: %.1e Time:(%i:%02i:%02i) Data (FPGA:0x%06X)",
					(double(total_crc_fpga)/(double(100))),
					(double(total_crc_dnc)/(double(100))),
					(double(total_crc_hicann)/(double(100))),
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_fpga));
					fflush(stdout);

		//int i=0;
		while((runtime-starttime)< runtime_s) {
			//++i;
			usleep(1000);
			runtime = (unsigned int)time(NULL);


			//Read DNC crc counter
			(*it)->jtag->DNC_set_channel(8+hicann_nr);
			(*it)->jtag->DNC_read_crc_count(read_crc_dnc);
			(*it)->jtag->DNC_get_rx_data(read_data_dnc);
			if(read_crc_dnc != read_crc_dnc_dly) {
				total_crc_dnc += ((unsigned int)read_crc_dnc+0x100-(unsigned int)read_crc_dnc_dly)&0xff;

			}
			read_crc_dnc_dly = read_crc_dnc;

			//Read FPGA crc counter
			(*it)->jtag->FPGA_get_crc(read_crc_fpga);
			(*it)->jtag->FPGA_get_rx_data(read_data_fpga);
			if(read_crc_fpga != read_crc_fpga_dly) {
				total_crc_fpga += ((unsigned int)read_crc_fpga+0x100-(unsigned int)read_crc_fpga_dly)&0xff;

			}
			read_crc_fpga_dly = read_crc_fpga;

			//Read HICANN crc counter
			(*it)->jtag->HICANN_read_crc_count(read_crc_hicann);
			(*it)->jtag->HICANN_get_rx_data(read_data_hicann);
			if(read_crc_hicann != read_crc_hicann_dly) {
				total_crc_hicann += ((unsigned int)read_crc_hicann+0x100-(unsigned int)read_crc_hicann_dly)&0xff;
			
			}
			read_crc_hicann_dly = read_crc_hicann;
		} 
		
		// stop experiment
		printf("\ntm_sysbench: Stopping benchmark transmissions\n");
		(*it)->jtag->FPGA_set_cont_pulse_ctrl(0, 0, 0, 0, 0, 0, 0, 0);
		
		//results:
		printf("*** Result FPGA-DNC-HICANN connection CRC ***\n");
		printf("FPGA   : CRC : %i\n",total_crc_fpga);
		printf("DNC    : CRC : %i\n",total_crc_dnc);
		printf("HICANN : CRC : %i\n",total_crc_hicann);
		
		//results for file:
		crc_fpga_onerun=total_crc_fpga;
		crc_dnc_onerun=total_crc_dnc;
		crc_hicann_onerun=total_crc_hicann;

		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 1;
		}
		(*it)->jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC FPGAif delay values:\n");
		(*it)->jtag->printDelayFPGA(vec_out);
		
		printf("DNC HICANNif delay value: %i\n", (*it)->jtag->getVecDelay((7-hicann_nr)+16,vec_out));
		
		uint64_t delay_in  = 0xffffffffffffffffull;
		uint64_t delay_out = 0xffffffffffffffffull;
		(*it)->jtag->HICANN_set_data_delay(delay_in,delay_out);
		printf("HICANN delay value: %i\n", (unsigned char)(delay_out&0x3f));
	
		if (runtime_s) spl1_fpga_errate = ((double) total_crc_fpga + (double) total_crc_dnc + (double) total_crc_hicann) / (double) runtime_s;
	
		return ((total_crc_fpga+total_crc_dnc+total_crc_hicann) == 0);
	}  
	
	
	
	bool runHicannBenchmark(unsigned int runtime_s, 
							uint hc, 
							unsigned int& crc_dnc_onerun, 
							unsigned int& crc_hicann_onerun, 
							unsigned int& err_dnc_onerun, 
							unsigned int& err_hicann_onerun) {
		
		//variables for file:
		crc_dnc_onerun=0;
		crc_hicann_onerun=0;
		err_dnc_onerun=0;
		err_hicann_onerun=0;

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

		cout << "tm_sysbench::runHicannBenchmark: reset ARQ before initial loopback test" << endl;
		(*it)->jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		(*it)->jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		//for(uint t=0;t<2;t++) (*it)->hicann[hc]->tags[t]->arq.Reset();
		throw std::runtime_error("this breaks... hicann arq reset w/o fpga/sw arq reset... call Init() somewhere here");
		
		
		printf("tm_sysbench::runHicannBenchmark: Starting benchmark transmissions\n");

		// Testmodes
		(*it)->jtag->DNC_test_mode_en(); // DNC enable testmode
		// loopback
		(*it)->jtag->DNC_set_loopback(0x00); // DNC loopback
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
		spc->write_cfg(0x055ff);

		cout << "tm_sysbench::runHicannBenchmark: reset ARQ after initial loopback setup" << endl;
		(*it)->jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		(*it)->jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		//for(uint t=0;t<2;t++) (*it)->hicann[hc]->tags[t]->arq.Reset();
		throw std::runtime_error("this breaks... hicann arq reset w/o fpga/sw arq reset... call Init() somewhere here");
			
		// setup DNC
		(*it)->dc->setTimeStampCtrl(0x00); // Disable DNC timestamp features
		uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
		(*it)->dc->setDirection(dirs); // set transmission directions in DNC for hicann_nr

		printf("tm_sysbench::runHicannBenchmark: Test DNC -> HICANN -> DNC\n");

		unsigned int starttime = (unsigned int)time(NULL);
		unsigned int runtime = (unsigned int)time(NULL);
		
		printf("BER DNC: %.1e Error DNC: %i BER HICANN: %.1e Error HICANN: %i Time:(%i:%02i:%02i) Data (DNC:0x%08X)(HICANN:0x%08X)\n",
					(double(total_crc_dnc)/(double(100))),total_crc_dnc,
					(double(total_crc_hicann)/(double(100))),total_crc_hicann,
					(runtime-starttime)/3600,((runtime-starttime)/60)%60,(runtime-starttime)%60,
					static_cast<unsigned int>(read_data_dnc),
					static_cast<unsigned int>(read_data_hicann));
		
		//int i=0;
		while ((runtime-starttime)< runtime_s) {
			//++i;
			
			runtime = (unsigned int)time(NULL);
			wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
			(*it)->jtag->DNC_start_pulse(hicann_nr,wdata64&0xdfffff);
			
				//Read HICANN crc counter
			(*it)->jtag->HICANN_read_crc_count(read_crc_hicann);
			(*it)->jtag->HICANN_get_rx_data(read_data_hicann);
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
			(*it)->jtag->DNC_set_channel(hicann_nr);
			(*it)->jtag->DNC_read_crc_count(read_crc_dnc);
			(*it)->jtag->DNC_get_rx_data(read_data_dnc);
			if(read_crc_dnc != read_crc_dnc_dly) {
				total_crc_dnc += ((unsigned int)read_crc_dnc+0x100-(unsigned int)read_crc_dnc_dly)&0xff;
			
			}
			if ( read_data_dnc != read_data_hicann ){
				++error1;
				printf("%X %X\n", read_data_dnc, read_data_hicann);
			}
			read_crc_dnc_dly = read_crc_dnc;
		}

		//results:
		printf("*** Result DNC-HICANN connection CRC, Errors ***\n");
		printf("DNC    : CRC : %i\t Error : %i\n", total_crc_dnc, error1);
		printf("HICANN : CRC : %i\t Error : %i\n", total_crc_hicann, error2);
		
		//results for file:
		crc_dnc_onerun=total_crc_dnc;
		crc_hicann_onerun=total_crc_hicann;
		err_dnc_onerun=error1;
		err_hicann_onerun=error2;

		std::vector<bool> vec_in(144,false);
		std::vector<bool> vec_out(144,false);

		for(int m=0   ; m<144;++m) {
			vec_in[m]  = 1;
			vec_out[m] = 1;
		}
		(*it)->jtag->DNC_set_data_delay(vec_in,vec_out);

		printf("DNC HICANNif delay value: %i\n", (*it)->jtag->getVecDelay((7-hicann_nr)+16,vec_out));
		
		uint64_t delay_in  = 0xffffffffffffffffull;
		uint64_t delay_out = 0xffffffffffffffffull;
		(*it)->jtag->HICANN_set_data_delay(delay_in,delay_out);
		printf("HICANN delay value: %i\n", (unsigned char)(delay_out&0x3f));

		if (runtime_s) dnc_hicann_errate = ((double) error1 + (double) error2) / (double) runtime_s;

		return ((error1+error2) == 0);
	}

};
		
		

class LteeTmssJtagIDreadout : public Listee<Testmode>{
public:
	LteeTmssJtagIDreadout() : Listee<Testmode>(string("tmss_jtagidreadout"), string("jtag id readout")){};
	Testmode * create(){return new TmssJtagIDreadout();};
};

LteeTmssJtagIDreadout ListeeTmssJtagIDreadout;
