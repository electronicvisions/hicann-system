// cOmpany         :   kip
// Author          :   Sebastian Millner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tmsm_aout.cpp
// Project Name    :   p_facets
// Subproject Name :   s_system            
//                    			
// Create Date     :   2/2012
//------------------------------------------------------------------------

#include "s2comm.h"
#include "s2ctrl.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
#include "fg_control.h"
#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class
#include "repeater_control.h" //repeater control class
#include "neuronbuilder_control.h"
#include "linklayer.h"
#include "neuron_control.h" //neuron control class (merger, background genarators)
#include "testmode.h" 
#include "ramtest.h"


#include "dnc_control.h"
#include "fpga_control.h"
#include "s2c_jtagphys_2fpga.h" //jtag-fpga library
#include "spl1_control.h" //spl1 control class
#include "iboardv2_ctrl.h" //iBoard controll class
#include <time.h>

#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>

//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;
using namespace boost::numeric::ublas;

class TMSmaout : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmsm_aout"; return ss.str();}
public:
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater location
	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc[4];//array of floatinggate controlers
	NeuronBuilderControl *nbc;

	NeuronControl *nc; //neuron control
	RepeaterControl *rca[6];

	SynapseControl *sc_top, *sc_bot;
	L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;

//DNC
	SPL1Control *spc;
	DNCControl  *dc;
	FPGAControl *fpc;

	fg_loc loc;
	string locs;
	int cntlr;
	uint fg_num;
	uint bank;

	
	uint maxcycle;		
	uint currentwritetime;
	uint voltagewritetime;
	uint readtime;
	uint acceleratorstep;
	uint pulselength;
	uint fg_biasn;
	uint fg_bias;	

	static const int TAG             = 0;

	std::stringstream ss; //used for filename generatiot for instance

	string filename;
	

	fstream file;

	time_t rawtime;
	tm * now;

	// helper functions

	uint nbdata(uint fireb, uint firet, uint connect, uint ntop, uint nbot, uint spl1){
		return ((4* fireb + 2*firet + connect)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;

	}
	void getVoltage(double &av, double &std, SBData::ADtype DAC_channel, IBoardV2Ctrl &board){
		uint num = 5;
		std::vector<double> meas(100);
		av = 0.0;
		board.getVoltage(DAC_channel);
		for(int i=0; i<num; i++){ 
			meas[i] = board.getVoltage(DAC_channel);
			av += meas[i];
		}
		av = av/num;
		std = 0.0;
		for(int i=0; i<num; i++){ 
			std += (meas[i]-av)*(meas[i]-av);
		}
		std = sqrt(std/num);
	}


		


	void setupFg(){

		for(fg_num=0; fg_num<4;fg_num+=1)
		{//init all floating gate controlers
			
		
			log(Logger::DEBUG1)<< "Writing controller " << fg_num << "." << endl;
	
			fc[fg_num]->set_maxcycle(maxcycle);		
			fc[fg_num]->set_currentwritetime(currentwritetime);
			fc[fg_num]->set_voltagewritetime(voltagewritetime);
			fc[fg_num]->set_readtime(readtime);
			fc[fg_num]->set_acceleratorstep(acceleratorstep);
			fc[fg_num]->set_pulselength(pulselength);
			fc[fg_num]->set_fg_biasn(fg_biasn);
			fc[fg_num]->set_fg_bias(fg_bias);
			fc[fg_num]->write_biasingreg();
			fc[fg_num]->write_operationreg();
		}	
	}

	void displaySetup(){
		cout<<"Current Setup is:"<<endl;
		cout<<"Maxcycle = "<< maxcycle<<endl;
		cout<<"Currentwritetime = "<< currentwritetime << endl;
		cout<<"Voltagewritetime = "<< voltagewritetime << endl;
		cout<<"Readtime = "<< readtime << endl;
		cout<<"Acceleratorstep = "<< acceleratorstep << endl;
		cout<<"Pulselength = "<< pulselength << endl;
		cout<<"fg_biasn = " << fg_biasn << endl;
		cout<<"fg_bias = " << fg_bias << endl;
	}
	uint fgzero(){
		log(Logger::INFO)<<"tmsm_aout::reseting all to zero ...";

		for( fg_num = 0; fg_num < 4; fg_num += 1)
		{
			log(Logger::DEBUG2)<<"Fg_number"<<fg_num;

			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(63);
			fc[fg_num]->set_voltagewritetime(63);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(1);
			fc[fg_num]->set_pulselength(15);
			fc[fg_num]->set_fg_biasn(15);
			fc[fg_num]->set_fg_bias(15);
			fc[fg_num]->write_biasingreg();
			fc[fg_num]->write_operationreg();
			fc[fg_num]->initzeros(0);
		}
		for( uint lineNumber = 0; lineNumber < 24; lineNumber += 1) //starting at line 1 as this is the first current!!!!!
		{	
				
			for( fg_num = 0; fg_num < 4; fg_num += 1){
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,0,0); //programm down first
			}
		}

		while(fc[3]->isBusy());//wait until last fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
		setupFg();	

		log(Logger::INFO)<<"done";
		return(1);
	}

	

	uint program_fg(uint line, uint global, uint neuronval){ 
		for( fg_num = 0; fg_num < 4; fg_num += 1) //using left block only ignore right block
		{	
			while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
			log(Logger::DEBUG1)<<"Programming line "<<line<<".";
			log(Logger::DEBUG1)<<"Fg_number"<<fg_num;
			fc[fg_num]->initval1(0, global, neuronval);
			fc[fg_num]->program_cells(line,0,1); //programm  up only
			log(Logger::DEBUG1)<<"done";
		}
		
		while(fc[3]->isBusy());//wait until last fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
		return 1;
	}
	//sets the repeater frequency (use 2,1 to get 100 MHz)
	void set_pll(uint multiplicator, uint divisor){
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
	}

	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to fgctrl
	inline void clear() {
		nbc->initzeros();
	};
	inline void write(int addr, uint value) {
		nbc->write_nmem(addr, value);
	 }
	inline void read(int addr) { 
		nbc->read_nmem(addr);
	}

	inline bool rdata(ci_data_t& value) {

                ci_addr_t taddr;
               
                nbc->get_read_data_nmem(taddr, value);
                
                return true;

	 }
	/*inline bool rdatabl(uint& value) { cmd_pck_t p; int r=ll->get_data(&p, true); value=p.data; return (bool)r; }

*/

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
	{
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}
		uint hicannr = 0;
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		jtag->reset_jtag();
		dbg(0) << "Try Init() ..." ;


		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;
	
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		nbc        = &hc->getNBC();
		nc         = &hc->getNC();
		fc[0]      = &hc->getFC_tl();
		fc[1]      = &hc->getFC_tr();
		fc[2]      = &hc->getFC_bl();
		fc[3]      = &hc->getFC_br();
		sc_top     = &hc->getSC_top();  // get pointer to synapse control class of HICANN 0
		sc_bot     = &hc->getSC_bot();  // get pointer to synapse control class of HICANN 0
		lcl        = &hc->getLC_cl();  // get pointer to left crossbar layer 1 switch control class of HICANN 0
		lcr        = &hc->getLC_cr();
		lctl       = &hc->getLC_tl();
		lcbl       = &hc->getLC_bl();
		lctr       = &hc->getLC_tr();
		lcbr       = &hc->getLC_br();
		rca[rc_l]  = &hc->getRC_cl(); // get pointer to left repeater of HICANN 0
		rca[rc_r]  = &hc->getRC_cr();
		rca[rc_tl] = &hc->getRC_tl();
		rca[rc_bl] = &hc->getRC_bl();
		rca[rc_tr] = &hc->getRC_tr();
		rca[rc_br] = &hc->getRC_br();
		spc        = &hc->getSPL1Control(); // get pointer to SPL1 Control
		dc = (DNCControl*) chip[FPGA_COUNT]; // use DNC
		fpc = (FPGAControl*) chip[0]; // use FPGA
		
		IBoardV2Ctrl board(conf, jtag, 0); //create an iboard control instance
		
		nc->configure(); //configure all the instances (read memory registers to get the current internal configuration)
		lcl->configure(); //otherwise the instance assumes to have zeros in all registers
		lcr->configure();
		lctl->configure();
		lctr->configure();
		lcbl->configure();
		lcbr->configure();
		rca[rc_l]->configure();
		rca[rc_r]->configure();
//		rca[rc_tl]->configure();
//		rca[rc_bl]->configure();
//		rca[rc_tr]->configure();
//		rca[rc_br]->configure();

		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		

	

		log(Logger::INFO)<<"tmsm_aout: starting...." << endl;
	
		
		//sets the repeater frequency (use 2,1 to get 100 MHz)
		// set_pll(2, 1);
		//initialize nbcrams with zersos
		nbc->initzeros();

		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to neuron output line 0 and 1
		nbc->setOutputMux(6,4);
	
	
		nbc->setNeuronBigCap(1,1);

		nbc->setNeuronGl(0,0,1,1);
		nbc->setNeuronGladapt(0,0,0,0);
 
		nbc->setNeuronReset(1,1);
		
		nbc->configNeuronsGbl();
		// initializ floating gates  
		
 
		bank=0; // bank number, programming data is written to

		//start values

		maxcycle = 255;
		currentwritetime = 1;
		voltagewritetime = 15;
		readtime = 63;
		acceleratorstep = 9;
		pulselength = 9;
		fg_biasn = 5;
		fg_bias = 8;
		setupFg();
		int mode = 0;
		uint denmem = 0;
		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "a: compare aouts using neuron 0" << endl;
			cout << "b: compare neuron aouts" << endl;
			cout << "c: compare neuron aouts using fixed voltage" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "z: set floating gates to zero and readout" <<endl;
			cout << "m: set floating gates to middle and readout"<<endl;
			cout << "R: Reset neuron configuration" << endl;
			cout << "r: config neuron reset" << endl;
			cout << "O: Configure output" <<endl;
			cout << "Z: ARQ reset/init" << endl;
			cout << "l: reload XML" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){
			case 'a':{
				log(Logger::INFO)<<"performing aout comparision";
	
				bank = 0;

				double av; double std;

				SBData::ADtype DAC_channel;  

				nbc->setOutputMux(4,4); //short input of outputs to neuron line 1. (even neurons top)

			 	nbc->clear_sram(); //reset internal sram
				
				nbc->set_aout(0); // readout neuron 0

				nbc->check_sram();

				nbc->write_sram();

				fgzero();//initialize floating gate

				program_fg(1, 1023, 0); //program neuron output bias to maximum
				program_fg(3, 1023, 0); //program neuron output bias to maximum
				program_fg(11, 0, 1023); //program set leakage conductance to maximum

			/*	
				uint add=0;
			       	uint fireb=0;
				uint firet=1;
			       	uint connect=0;
			       	uint ntop=0x8e; 
				uint nbot=0x8e;
				
			       	uint spl1=21;

				uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);
*/
				time(&rawtime);//get time

				now = localtime(&rawtime);
				 
				ss <<"../results/tmsm_aout/"<< "opcomp-"<<now->tm_year-100<<"-"<<setw(2)<<setfill('0')<<now->tm_mon+1<<"_"<<now->tm_hour<<":"<<now->tm_min<<label<<".dat";
				string filename = ss.str();
				file.open(filename.c_str(), fstream::out);
				
				for(uint El = 0; El < 1024; El++){
					//write leakage line
					program_fg(6, 0, El);
					//now readout values and store to file
					getVoltage(av,std,SBData::aout0, board);
					file<<av<<" "<<std<<" ";
					getVoltage(av,std,SBData::aout1, board);
					file<<av<<" "<<std<<endl;
					//now readout values and store to file
				}
				
				file.close();	

				//start values
				maxcycle = 255;
				currentwritetime = 1;
				voltagewritetime = 15;
				readtime = 63;
				acceleratorstep = 9;
				pulselength = 9;
				fg_biasn = 5;
				fg_bias = 8;
				setupFg();
			}break;

			case 'b':{
				log(Logger::INFO)<<"Find aout offset";
	

				matrix<double> av (64, 1024);
				matrix<double> mstd (64, 1024);
				matrix<double> std (64, 1024);
				double tmp;
				boost::numeric::ublas::vector<double> Eav (1024);
				boost::numeric::ublas::vector<double> Estd (1024);

				boost::numeric::ublas::vector<double> dav (64);
				boost::numeric::ublas::vector<double> dstd (64);

				nbc->setOutputMux(4,4); //short input of outputs to neuron line 1. (even neurons top)

				for (uint denmem = 0; denmem < 64; denmem ++){
					nbc->write_nmem(denmem, 0x28);
				}	

				fgzero();//initialize floating gate

				program_fg(3, 1023, 0); //program neuron output bias to maximum
				program_fg(11, 0, 1023); //program set leakage conductance to maximum

				time(&rawtime);//get time

				now = localtime(&rawtime);
				 
				ss <<"../results/tmsm_aout/"<<"noffset-"<<now->tm_year-100<<"-"<<setw(2)<<setfill('0')<<now->tm_mon+1<<"-"<<setw(2)<<setfill('0')<<now->tm_mday+1<<"_"<<now->tm_hour<<":"<<now->tm_min<<label<<".dat";
				string filename = ss.str();
				file.open(filename.c_str(), fstream::out);
				
				for(uint El = 0; El < 1024; El+=1){
					//write leakage line
					program_fg(6, 0, El);
					Eav(El) = 0;
					log(Logger::INFO)<<"Measuring... El="<<El;
					for (uint denmem = 0; denmem < 32; denmem+=1){ //use only one fourth of the chip as first test as floating gates for other side have to be programmed different.
						nbc->write_nmem(denmem, 0x2b);
						
						nbc->setOutputMux(4,6); //short input of outputs to neuron line 1. (even neurons top)
						getVoltage(av(2*denmem,El),mstd(2*denmem,El),SBData::aout0, board);
						//file<<av<<","<<std<<",";
						nbc->setOutputMux(6,4); //short input of outputs to neuron line 1. (even neurons top)
						getVoltage(av(2*denmem+1,El),mstd(2*denmem+1,El),SBData::aout0, board);
						//file<<tmp<<","<<std<<",";
						nbc->write_nmem(denmem, 0x28);
						Eav(El) += av(2*denmem,El) + av(2*denmem + 1,El);

					}
					Eav(El)=Eav(El)/64;
					Estd(El)=0;
					for (uint denmem = 0; denmem < 32; denmem+=1){
						std(2*denmem,El) = av(2*denmem,El) - Eav(El);
						dav(2*denmem) += std(2*denmem,El);
						Estd(El) += std(2*denmem,El)*std(2*denmem,El);
						std(2*denmem+1,El) = av(2*denmem+1,El) - Eav(El);
						dav(2*denmem+1) += std(2*denmem+1,El);
						Estd(El) += std(2*denmem+1,El)*std(2*denmem+1,El);
					}
					Estd(El)=sqrt(Estd(El)/64);

				}
				dav = dav /1024;
				uint dstdtmp;
				for(uint El = 0; El < 1024; El+=1){
					for(uint denmem = 0; denmem < 64; denmem++){
						dstd(denmem)+= (std(denmem, El)-dav(denmem))*(std(denmem, El)-dav(denmem));
					}
				}
				dstd = dstd / 1024;
				file<<"measurement:"<<av<<endl<<"measstd:"<<mstd<<endl<<"Average:"<<Eav<<endl<<"std:"<<std<<endl<<"Estd"<<Estd<<endl<<"diffav:"<<dav<<endl<<"diffstd:"<<dstd;	
				file.close();	
			}break;

			case 'c':{
				log(Logger::INFO)<<"Find aout offset single";
	

				matrix<double> av (64, 10);
				matrix<double> mstd (64, 10);
				matrix<double> std (64, 10);
				double tmp;
				boost::numeric::ublas::vector<double> Eav (10);
				boost::numeric::ublas::vector<double> Estd (10);

				boost::numeric::ublas::vector<double> dav (64);
				boost::numeric::ublas::vector<double> dstd (64);

				nbc->setOutputMux(4,4); //short input of outputs to neuron line 1. (even neurons top)

				for (uint denmem = 0; denmem < 64; denmem ++){
					nbc->write_nmem(denmem, 0x28);
				}	

				fgzero();//initialize floating gate


				program_fg(3, 1023, 0); //program neuron output bias to maximum
				program_fg(11, 0, 1023); //program set leakage conductance to maximum

				time(&rawtime);//get time

				now = localtime(&rawtime);
				 
				ss <<"../results/tmsm_aout/"<<"singlenoffset-"<<now->tm_year-100<<"-"<<setw(2)<<setfill('0')<<now->tm_mon+1<<"-"<<setw(2)<<setfill('0')<<now->tm_mday+1<<"_"<<now->tm_hour<<":"<<now->tm_min<<label<<".dat";
				string filename = ss.str();
				file.open(filename.c_str(), fstream::out);
				
				program_fg(6, 0, 500);
				for(uint El = 0; El < 10; El+=1){
					//write leakage line
					Eav(El) = 0;
					log(Logger::INFO)<<"Measuring... Nr="<<El;
					for (uint denmem = 0; denmem < 32; denmem+=1){ //use only one fourth of the chip as first test as floating gates for other side have to be programmed different.
						nbc->write_nmem(denmem, 0x2b);
						
						nbc->setOutputMux(4,6); //short input of outputs to neuron line 1. (even neurons top)
						getVoltage(av(2*denmem,El),mstd(2*denmem,El),SBData::aout0, board);
						//file<<av<<","<<std<<",";
						nbc->setOutputMux(6,4); //short input of outputs to neuron line 1. (even neurons top)
						getVoltage(av(2*denmem+1,El),mstd(2*denmem+1,El),SBData::aout0, board);
						//file<<tmp<<","<<std<<",";
						nbc->write_nmem(denmem, 0x28);
						Eav(El) += av(2*denmem,El) + av(2*denmem + 1,El);

					}
					Eav(El)=Eav(El)/64;
					Estd(El)=0;
					for (uint denmem = 0; denmem < 32; denmem+=1){
						std(2*denmem,El) = av(2*denmem,El) - Eav(El);
						dav(2*denmem) += std(2*denmem,El);
						Estd(El) += std(2*denmem,El)*std(2*denmem,El);
						std(2*denmem+1,El) = av(2*denmem+1,El) - Eav(El);
						dav(2*denmem+1) += std(2*denmem+1,El);
						Estd(El) += std(2*denmem+1,El)*std(2*denmem+1,El);
					}
					Estd(El)=sqrt(Estd(El)/64);

				}
				dav = dav /10;
				uint dstdtmp;
				for(uint El = 0; El < 10; El+=1){
					for(uint denmem = 0; denmem < 64; denmem++){
						dstd(denmem)+= (std(denmem, El)-dav(denmem))*(std(denmem, El)-dav(denmem));
					}
				}
				dstd = dstd / 10;
				file<<"measurement:"<<av<<endl<<"measstd:"<<mstd<<endl<<"Average:"<<Eav<<endl<<"std:"<<std<<endl<<"Estd"<<Estd<<endl<<"diffav:"<<dav<<endl<<"diffstd:"<<dstd;	
				file.close();	
			}break;


			case 'z':{
				log(Logger::INFO)<<"Set floating gates to zero and readout cell 1 of line 0.";
				
				fgzero();//initialize floating gate

				//activate analog output 
				nbc->setOutputEn(true,true);
				//set output to fg-array 0 and 2;
				nbc->setOutputMux(9,9);

				for(fg_num=0; fg_num<4;fg_num+=1){

					fc[fg_num]->readout_cell(0,10);
				
				}
			}break;
			case 'm':{
				log(Logger::INFO)<<"Set floating gate line 0 to midle value and readout cell 1.";
				
				
				fgzero();//initialize floating gate

				program_fg(0, 0, 500); //program neuron output bias to maximum
				//activate analog output 
				nbc->setOutputEn(true,true);
				//set output to fg-array 0 and 2;
				nbc->setOutputMux(9,9);

				for(fg_num=0; fg_num<4;fg_num+=1){

					fc[fg_num]->readout_cell(0,10);
				
				}
			}break;
			case 'M':{
				log(Logger::INFO)<<"Set floating gate line 0 to midle value and readout cell 1.";
				
				

				program_fg(0, 0, 1023); //program neuron output bias to maximum
				program_fg(0, 0, 1023); //program neuron output bias to maximum
				program_fg(0, 0, 1023); //program neuron output bias to maximum
				//activate analog output 
				nbc->setOutputEn(true,true);
				//set output to fg-array 0 and 2;
				nbc->setOutputMux(9,9);

				for(fg_num=0; fg_num<4;fg_num+=1){

					fc[fg_num]->readout_cell(0,10);
				
				}
			}break;
			case 'p':{
			//write currents first as ops need biases before they work.
			log(Logger::INFO)<<"tmsm_aout::writing currents ...";
			for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2) //starting at line 1 as this is the first current!!!!!
			{
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
				for( fg_num = 0; fg_num < 4; fg_num += 1)
				{	
					log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
					if(mode == 0) fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
					else fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber), denmem);//use two banks -> values can be written although fg is still working
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
				}
			/*	
				for( fg_num = 0; fg_num < 4; fg_num += 1){
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
				}*/
				bank =( bank +1)%2; //bank is 1 or 0
			}
			log(Logger::INFO)<<"tmsm_aout::writing voltages ...";
			for( uint lineNumber = 0; lineNumber < 24; lineNumber += 2)
			{
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
				for( fg_num = 0; fg_num < 4; fg_num += 1)
				{	
		
					log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
					if(mode == 0) fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
					else fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber), denmem);//use two banks -> values can be written although fg is still working
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
				}
			/*	
				for( fg_num = 0; fg_num < 4; fg_num += 1){
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
				}*/
				bank =( bank +1)%2; //bank is 1 or 0
			}
			
		 	cont = true;		
			}break;

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
			case 'd':{
				nbc->initzeros();
				uint add=0;
			       	uint fireb=0;
				uint firet=1;
			       	uint connect=0;
			       	uint ntop=0x8e; 
				uint nbot=0x8e;
				
			       	uint spl1=21;

				cout<<"Denmem:";
				cin>>add;
				uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);
				cont = true;		
			}break;
			case 'R':{
				nbc->initzeros();
				cont = true;
			}break;

			case 'r':{

				cout <<"Enter values for neuron resetn for upper and lower half:";
				uint r1, r2=0;
				cin >>r1>>r2; //check of order needed
				nbc->setNeuronReset(r1,r2);
				nbc->configNeuronsGbl();
				cont = true;
			}break;

			case 'L':{
//use bank 1 to keep neuron stimulus data
			bank=1;
			float syn_tc;
			uint syn_tc_int;
			cout<<"Enter leakage parameter:";
			cin>>syn_tc;
			syn_tc_int=(syn_tc/1.8)*1023;
			cout<<"DAC value:"<<syn_tc_int<<endl;
			uint lineNumber=6;
			log(Logger::INFO)<<"tmsm_aout:: parameter...";
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
			for( fg_num = 0; fg_num < 4; fg_num += 1) //using left block only ignore right block
			{	
				log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
				fc[fg_num]->initval(bank, syn_tc_int);//ignore wrong value for line 0 so far (quicktest)
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
			}
			
			for( fg_num = 0; fg_num < 4; fg_num += 1){
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
			}
	
//activate neuron stimulus again, using bank 0 
			for( fg_num = 0; fg_num < 4; fg_num += 1){
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->stimulateNeuronsContinuous(0);
			}
		 	cont = true;		
			}break;
		      case 'O':{
				uint val1, val2;
				cout<<"Enter mux configuration for outputs: ";
				cin>>val1;
				cin>>val2;			
				nbc->setOutputMux(val1,val2);
			} break;
		      case 'Z':{
			dbg(0) << "Try Init() ..." ;

			if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
				dbg(0) << "ERROR: Init failed, abort" << endl;
				return 0;
						}
			
			dbg(0) << "Init() ok" << endl;
						
			dbg(0) << "Reset software part of ARQ.";
			ll->arq.Reset();
						
			} break;

			case'l':{
				
				conf->reLoad();
				conf->getHicann(0);
			} break;
			case'x':cont=false;
			}		
		}while(cont);


		log(Logger::DEBUG0)<< "tmsm_aout::ok" << endl;
		//disable analog output 
		nbc->setOutputEn(false,false);
		return 1;
	};
};


class LteeTMSmaout : public Listee<Testmode>{
public:
	LteeTMSmaout() : Listee<Testmode>(string("tmsm_aout"), string("characterice analog output capapilities of hicann and neuron")){};
	Testmode * create(){return new TMSmaout();};
};

LteeTMSmaout ListeeTMSmaout;

