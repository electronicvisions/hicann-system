// cOmpany         :   kip
// Author          :   Sebastian Millner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_neuron.cpp
// Project Name    :   p_facets
// Subproject Name :   s_system            
//                    			
// Create Date     :   2/2010
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
//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class TMNeuron : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_neuron"; return ss.str();}
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
	
	

	// -----------------------------------------------------	
	// FG controller constants
	// -----------------------------------------------------	


	// for constants see "compare units/fgateCtrl/source/fgateCtrl.sv" 

	static const int TAG             = 0;
	static const int REG_TEST_NUM    = 10;

 	static const int MEM_SIZE        =128;//(1<<(nspl1_numa)) -1;
	static const int MEM_WIDTH       = 20;
	static const int MEM_BASE       = 0;

	static const int MEM_TEST_NUM    = MEM_SIZE*20;

	// helper functions

	uint nbdata(uint fireb, uint firet, uint connect, uint ntop, uint nbot, uint spl1){
		return ((4* fireb + 2*firet + connect)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;

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
		nbc = &hc->getNBC();
		nc  = &hc->getNC();
		fc[0]=&hc->getFC_tl();
		fc[1]=&hc->getFC_tr();
		fc[2]=&hc->getFC_bl();
		fc[3]=&hc->getFC_br();
		sc_top = &hc->getSC_top();  // get pointer to synapse control class of HICANN 0
		sc_bot = &hc->getSC_bot();  // get pointer to synapse control class of HICANN 0
		lcl = &hc->getLC_cl();  // get pointer to left crossbar layer 1 switch control class of HICANN 0
		lcr = &hc->getLC_cr();
		lctl= &hc->getLC_tl();
		lcbl= &hc->getLC_bl();
		lctr= &hc->getLC_tr();
		lcbr= &hc->getLC_br();
		rca[rc_l]=&hc->getRC_cl(); // get pointer to left repeater of HICANN 0
		rca[rc_r]=&hc->getRC_cr();
		rca[rc_tl]=&hc->getRC_tl();
		rca[rc_bl]=&hc->getRC_bl();
		rca[rc_tr]=&hc->getRC_tr();
		rca[rc_br]=&hc->getRC_br();
		spc = &hc->getSPL1Control(); // get pointer to SPL1 Control
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
		rca[rc_tl]->configure();
		rca[rc_bl]->configure();
		rca[rc_tr]->configure();
		rca[rc_br]->configure();

		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		

		// prepare ramtest
	

		log(Logger::INFO)<<"tm_Neuron: starting...." << endl;
	
		
		//initialize nbcrams with zersos
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
		// initializ floating gates  
		
		log(Logger::DEBUG1)<<"tm_neuron::Setting fg control parameters";
		uint fg_num;
		for(fg_num = 0; fg_num < 4; fg_num += 1)
		{
			fc[fg_num]->set_maxcycle(255);		
			fc[fg_num]->set_currentwritetime(1);
			fc[fg_num]->set_voltagewritetime(4);
			fc[fg_num]->set_readtime(63);
			fc[fg_num]->set_acceleratorstep(15);
			fc[fg_num]->set_pulselength(15);
			fc[fg_num]->set_fg_biasn(15);
			fc[fg_num]->set_fg_bias(15);
			fc[fg_num]->write_biasingreg();
			fc[fg_num]->write_operationreg();
			//fc[fg_num]->initzeros(0);
			//fc[fg_num]->initzeros(1);
	

		}
 
		int bank=0; // bank number, programming data is written to

		int mode = 0;
		uint denmem = 0;
		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "M: Select mode" <<endl;
			cout << "z: Programm all floating gates to zero:" << endl;
			cout << "p: Program floating gates" << endl;
			cout << "P: Program floating gates for single denmem pair only" << endl;
			cout << "c: config neurons" << endl;
			cout << "C: config global neuron parameter" << endl;
			cout << "d: Activate denmem pair" << endl;
			cout << "G: Build giant neuron" << endl;
			cout << "n: Activate single denmem" << endl;
			cout << "o: Switch analog output" << endl;
			cout << "y: Disable analog outputs" << endl;
			cout << "R: Reset neuron configuration" << endl;
			cout << "r: config neuron reset" << endl;
			cout << "s: Stimulate neurons" << endl;
			cout << "q: Stimulate neurons with square pulse" <<endl;
			cout << "S: Set synapse parameters" <<endl;
			cout << "O: Configure output" <<endl;
			cout << "a: activate spl1 output" <<endl;
			cout << "m: perform ram write/read check" <<endl;
			cout << "w: set synapse weights" <<endl;
			cout << "X: configure spl1 stuff" <<endl;
			cout << "Z: ARQ reset/init" << endl;
			cout << "l: reload XML" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){
			case 'M':{//single denmem mode
				cout << "Select mode [1:single denmem/0:all]";
				cin>>mode;
				if(mode == 0) break; // nothing to be done, all denmems selected
				cout << "Select single denmem: ";
				cin >> denmem;
			

				//connect dedicated denmem to output and activate current input	
				nbc->initzeros();
				uint add = 0;
			       	uint fireb=0;
				uint firet=1;
			       	uint connect=0;
			       	uint ntop=0x96; 
				uint nbot=0x96;
				
			       	uint spl1=0;

				cout<<"spl1:";
				cin>>spl1;
				if(denmem%2==1){
					//right neuron has been chosen
					ntop=0x95;
					nbot=0x95;
				}
				add=denmem/2;	
				uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);	

				if(denmem%2==0){//left neuron has been selected, another write needed to set fire enalble bit
					nbc->write_data(add*4+1,value);	
				}					
			
				//activate spl1	
				nbc->setSpl1Reset(0);
				nbc->setSpl1Reset(1);
				nbc->configNeuronsGbl();
				cont = true;		
			} break;
			case 'z':{
					 
					 
			//write currents first as ops need biases before they work.
			log(Logger::INFO)<<"tm_neuron::reseting all to zero ...";

			for( fg_num = 0; fg_num < 4; fg_num += 1)
			{
				log(Logger::DEBUG2)<<"Fg_number"<<fg_num;

				fc[fg_num]->set_maxcycle(255);		
				fc[fg_num]->set_currentwritetime(63);
				fc[fg_num]->set_voltagewritetime(63);
				fc[fg_num]->set_readtime(2);
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
					fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
				}
			}

			for(fg_num = 0; fg_num < 4; fg_num += 1)
			{
				fc[fg_num]->set_maxcycle(255);		
				fc[fg_num]->set_currentwritetime(1);
				fc[fg_num]->set_voltagewritetime(4);
				fc[fg_num]->set_readtime(63);
				fc[fg_num]->set_acceleratorstep(15);
				fc[fg_num]->set_pulselength(15);
				fc[fg_num]->set_fg_biasn(15);
				fc[fg_num]->set_fg_bias(15);
				fc[fg_num]->write_biasingreg();
				fc[fg_num]->write_operationreg();
				//fc[fg_num]->initzeros(0);
				//fc[fg_num]->initzeros(1);
		

			}
			log(Logger::INFO)<<"tm_neuron::done";
			}break;
			case 'p':{
			//write currents first as ops need biases before they work.
			log(Logger::INFO)<<"tm_neuron::writing currents ...";
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
			log(Logger::INFO)<<"tm_neuron::writing voltages ...";
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

			case 'P':{
			uint add;
			cout<<"Denmem_number:";
			cin>>add;
			//write currents first as ops need biases before they work.
			log(Logger::INFO)<<"tm_neuron::writing currents ...";
			for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2) //starting at line 1 as this is the first current!!!!!
			{
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
				for( fg_num = 0; fg_num < 4; fg_num += 1)
				{	
					log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
					fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber), add*2);//use two banks -> values can be written although fg is still working
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
				}
				
				for( fg_num = 0; fg_num < 4; fg_num += 1){
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
				}
				bank =( bank +1)%2; //bank is 1 or 0
			}
			log(Logger::INFO)<<"tm_neuron::writing voltages ...";
			for( uint lineNumber = 0; lineNumber < 24; lineNumber += 2)
			{
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
				for( fg_num = 0; fg_num < 4; fg_num += 1)
				{	
		
					log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
					fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber), add*2);//use two banks -> values can be written although fg is still working
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
				}
				
				for( fg_num = 0; fg_num < 4; fg_num += 1){
					while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
					fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
				}
				bank =( bank +1)%2; //bank is 1 or 0
			}
			
		 	cont = true;		
			}break;
			case 'c':{
				uint add=0;
			       	uint fireb=0;
				uint firet=0;
			       	uint connect=0;
			       	uint ntop=0; 
				uint nbot=0;
			       	uint spl1=9;

				cout<<"Address:";
				cin>>add;
				cout<<"firetop, firebot, and connect:";
				cin>>fireb>>firet>>connect;
				cout<<"Neuron configuration top and bottom";
				cin>>hex>>ntop>>nbot;
				cout<<"Spl1 address:";
				cin>>spl1;
				uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				//write to all four addresses to be masking independent
				nbc->write_data(add*4,value);
				nbc->write_data(add*4+1,value);
				nbc->write_data(add*4+2,value);
				nbc->write_data(add*4+3,value);
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
			case 'G':{
				nbc->initzeros();
				uint size=2;
			       	uint fireb=1;
				uint firet=1;
			       	uint connect=1;
			       	uint ntop=0x28; 
				uint nbot=0x08;
				uint value;	
			       	uint spl1=4;

				cout<<"Size:";
				cin>>size;
				for(int i=0; i<size; i++){
					value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
					nbc->write_data(i*4,value);
					nbc->write_data(i*4+1,value);
					nbc->write_data(i*4+2,value);
					nbc->write_data(i*4+3,value);
				}

				value = nbdata(fireb,firet,connect,0x16,nbot,spl1);
				nbc->write_data(3,value);
				value = nbdata(fireb,1,connect,0x21,nbot,spl1);
				nbc->write_data((size-1)*4+3,value);
				cont = true;		
			}break;
			case 'n':{
				nbc->initzeros();
				uint add=0;
			       	uint fireb=0;
				uint firet=0;
			       	uint connect=0;
			       	uint ntop=0x96; 
				uint nbot=0x96;
				
			       	uint spl1=0;

				cout<<"Denmem:";
				cin>>add;
				if(add%2==1){
					//right neuron has been chosen
					ntop=0x95;
					nbot=0x95;
				}
				add=add/2;	
				uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);
				cont = true;		
			}break;
			case 'o':{
				
				uint op1=0;
			    uint op2=0;

				cout<<"Op1:";
				cin>>op1;
				cout<<"Op2:";
				cin>>op2;

				nbc->setOutputMux(op1,op2);

				cont = true;		
			}break;
			case 'y':{
				nbc->setOutputEn(false,false);
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

			case 's': {


			cout <<"Enter peak current of square stimulus:";
			uint sval=0;
			cin >>dec>>sval;
			for( fg_num = 0; fg_num < 4; fg_num += 1)
			{
				while(fc[fg_num]->isBusy());
	//			fc[fg_num]->initSquare(0,420);
				fc[fg_num]->initval(0,sval);
	//			fc[fg_num]->initSquare(0,0);	
		//		fc[fg_num]->initSquare(0,sval);
				fc[fg_num]->set_pulselength(15);
				fc[fg_num]->write_biasingreg();

				fc[fg_num]->stimulateNeuronsContinuous(0);
			}
			}break;
			case 'q': {


			cout <<"Enter peak current of square stimulus:";
			uint sval=0;
			cin >>dec>>sval;
			for( fg_num = 0; fg_num < 4; fg_num += 1)
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
			log(Logger::INFO)<<"tm_neuron:: parameter...";
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
			case 'S':{
//use bank 1 to keep neuron stimulus data
			bank=1;
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

				if(mode == 0) fc[fg_num]->initArray(bank, val);//use two banks -> values can be written although fg is still working
				else fc[fg_num]->initArray(bank, val, denmem);//use two banks -> values can be written although fg is still working
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
/*
			cout<<"Enter iconv parameter:";
			cin>>syn_tc;
			syn_tc_int=(syn_tc/1.8)*1023;
			cout<<"DAC value:"<<syn_tc_int<<endl;
			lineNumber=17;
			log(Logger::INFO)<<"tm_neuron:: parameter...";
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
			for( fg_num = 0; fg_num < 4; fg_num += 1){
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->stimulateNeuronsContinuous(0);
			}*/
			}break;
			case 'E':{
			uint lineNumber;
			cout<<"Enter Esyn parameter:";
			uint syn_tc;
			std::vector<int> val;
			cin>>syn_tc;
			uint syn_tc_int=(syn_tc/1.8)*1023;
			cout<<"DAC value:"<<syn_tc_int<<endl;
			lineNumber=16;
			log(Logger::INFO)<<"tm_neuron:: parameter...";
				log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
			for( fg_num = 0; fg_num < 4; fg_num += 1) //using left block only ignore right block
			{	
				lineNumber = 16 - (fg_num%2) * 16;
				//generate data vector from input and xmlfile
				val = conf->getFgLine((fg_loc)fg_num, lineNumber);
				for(int i=1; i<val.size(); i++){
					val[i] = syn_tc_int;
				}
				log(Logger::DEBUG2)<<"Fg_number"<<fg_num;

				if(mode == 0) fc[fg_num]->initArray(bank, val);//use two banks -> values can be written although fg is still working
				else fc[fg_num]->initArray(bank, val, denmem);//use two banks -> values can be written although fg is still working
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,bank,0); //programm down first
			}
			
			for( fg_num = 0; fg_num < 4; fg_num += 1){
				lineNumber = 16 - (fg_num%2) * 16;
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
			}
	
			for( fg_num = 0; fg_num < 4; fg_num += 1){
				lineNumber = 16 - (fg_num%2) * 16;
				while(fc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fc[fg_num]->stimulateNeuronsContinuous(0);
			}/*
			cout<<"Enter Iintbbx parameter:";
			cin>>syn_tc;
			syn_tc_int=(syn_tc/1.8)*1023;
			cout<<"DAC value:"<<syn_tc_int<<endl;
			lineNumber=19;
			log(Logger::INFO)<<"tm_neuron:: parameter...";
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
			}*/
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
			case'a':{

				//nbc->initzeros();
				uint add=127;
			       	uint fireb=0;
				uint firet=0;
			       	uint connect=1;
			       	uint ntop=0x8f; 
				uint nbot=0x8f;
				
			       	uint spl1=0;

				cout<<"spl1:";
				cin>>spl1;
				uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4,value);
				value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+1,value);
				value = nbdata(fireb,firet,connect,ntop,nbot,0);
				nbc->write_data(add*4+2,value);
				value = nbdata(fireb,1,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);
				nbc->setSpl1Reset(0);
				nbc->setSpl1Reset(1);
				nbc->configNeuronsGbl();
				//receiver neuron
				add = 120; // distance to minimize effects by crosstalk
				fireb = 0;
				firet = 0;	
				connect = 0;
				ntop = 0x81; //output of right neuron, right neuron activated
				nbot = 0x81;
	

				value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4,value);
				value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+1,value);
				value = nbdata(fireb,firet,connect,ntop,nbot,0);
				nbc->write_data(add*4+2,value);
				value = nbdata(fireb,0,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);

				//2nd receiver to look for delay
				add = 0; // left side of chip - as far left as possible
				fireb = 0;
				firet = 0;	
				connect = 0;
				ntop = 0x12;//left neuron activated and read out
				nbot = 0x12;
	

				value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4,value);
				value = nbdata(fireb,firet,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+1,value);
				value = nbdata(fireb,firet,connect,ntop,nbot,0);
				nbc->write_data(add*4+2,value);
				value = nbdata(fireb,0,connect,ntop,nbot,spl1);
				nbc->write_data(add*4+3,value);
			} break;
			case 'm':{
				//perform ram write read check
				ci_addr_t taddr;
				char pause;
				nbc->initzeros();
	
				nbc->setOutputMux(4,5);
				//uint value = nbdata(fireb,firet,connect,ntop,nbot,spl1);

				uint value = nbdata(0,0,1,0x08, 0x16,0);
				ci_data_t val1;
				nbc->write_data(0x16f,value);
				nbc->write_data(0x16c,value);
				dbg(0)<<"Checking address 0x16f" <<endl;
				for(int j = 0; j<10; j++){
					nbc->read_data(0x16f);

					nbc->get_read_data(taddr,val1);
					dbg(0)<<hex<<"Read value is: "<<((val1 & 0x3fffc0)>>6)<<" and should be 0x1608!"<<endl;
				}
				
				nbc->setNeuronBigCap(1, 0);	
				nbc->configNeuronsGbl();
				cout  <<"Writing configuration of lower neurons only"<<endl;
				value = nbdata(0,0,1,0x00, 0x16,0);
				nbc->write_data(0x16f,value);
				nbc->write_data(0x16c,value);
				cin>>pause;
				cout <<"Writing configuration interconnecting both upper neurons - should double memcap" <<endl;
				value = nbdata(0,0,1,0x08, 0x16,0);
				nbc->write_data(0x16f,value);
				nbc->write_data(0x16c,value);
				cin>>pause;
				cout <<"Reading memory - memcap should be smaller again if flip is caused" <<endl;
		
				nbc->read_data(0x16f);

				nbc->get_read_data(taddr,val1);
				cout<<hex<<"Read value is: "<<((val1 & 0x3fffc0)>>6)<<" and should be 0x1608!"<<endl;
				break;
			}

			case 'w': {
				uint w, row, col;
				vector<uint> weights(32,0);
				cout << "Enter weight (0-15): ";
				cin >> dec >> w;
				cout << "Enter row (0-223): ";
				cin >> dec >> row;
				cout << "Enter column (0-255): ";
				cin >> dec >> col;
				sc_top->write_weight(row, col, w);
			} break;

			case 'W': {
				uint d, row, col;
				cout << "Enter decoder (0-15): ";
				cin >> dec >> d;
				cout << "Enter row (0-223): ";
				cin >> dec >> row;
				cout << "Enter column (0-255): ";
				cin >> dec >> col;
				sc_top->write_decoder(row, col, d);
			} break;
			case 'b':{//set background on synaptic input
				nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
						DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
				bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
				//bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //all channels forward SPL1 data
				  bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //all channels forward BEG data
				bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
				//initialize a vector from an array
				vector<nc_merger> merg(mer, mer+23);
				vector<bool> slow(sl, sl+23);
				vector<bool> select(se, se+23);
				vector<bool> enable(en, en+23);

				nc->nc_reset(); //reset neuron control
				nc->set_seed(200); //set seed for BEGs
				nc->beg_configure(8, false, 60); //configure BEGs to spike every 60 cycles (no Poisson)

				nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
				nc->outputphase_set(1,1,1,1,1,1,1,1); //not sure if it makes any difference :)

				nc->beg_on(8); //turn all BEGs on
				nc->print_config();
	
			  	//set neuron numbers for background gen
				nc->beg_set_number(0,0);
				nc->beg_set_number(1,0);
				nc->beg_set_number(2,0);
				nc->beg_set_number(3,0);
				nc->beg_set_number(4,0);
				nc->beg_set_number(5,0);
				nc->beg_set_number(6,0); //repeater 0
				nc->beg_set_number(7,0);
			} break;

			case '0':{
				vector<uint> weights(32,0x00000000),address(32,0x00000000);
				sc_top->write_weight(0, weights);
				sc_top->write_weight(1, weights);
				sc_top->write_decoder(0, address, address);
			} break;
			case 'X':{
				nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
						DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
				bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
				bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //all channels forward SPL1 data
				//bool se[23] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; //all channels forward BEG data
				bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
				//initialize a vector from an array
				vector<nc_merger> merg(mer, mer+23);
				vector<bool> slow(sl, sl+23);
				vector<bool> select(se, se+23);
				vector<bool> enable(en, en+23);

				nc->nc_reset(); //reset neuron control
				nc->set_seed(200); //set seed for BEGs
				nc->beg_configure(8, false, 60); //configure BEGs to spike every 60 cycles (no Poisson)
				//~ nc->beg_set_number(0,10);
				//~ nc->beg_set_number(1,11);
				//~ nc->beg_set_number(2,12);
				//~ nc->beg_set_number(3,13);
				//~ nc->beg_set_number(4,14);
				//~ nc->beg_set_number(5,15);
				//~ nc->beg_set_number(6,0); //channel 0 (repeater)
				//~ nc->beg_set_number(7,17);
				nc->merger_set(merg, enable, select, slow); //set mergers to translate BEG operations to SPL1 register
				nc->outputphase_set(1,1,1,1,1,1,1,1); //not sure if it makes any difference :)
				nc->beg_off(8); //turn all BEGs off 
				//nc->print_config();

				rca[rc_l]->reset();
				rca[rc_l]->conf_spl1_repeater(0, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(4, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(8, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(12, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(16, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(20, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(24, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				rca[rc_l]->conf_spl1_repeater(28, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
				//rca[rc_l]->print_config();

				nc->beg_set_number(0,0);
				nc->beg_set_number(1,0);
				nc->beg_set_number(2,0);
				nc->beg_set_number(3,0);
				nc->beg_set_number(4,0);
				nc->beg_set_number(5,0);
				nc->beg_set_number(6,0); //repeater 0
				nc->beg_set_number(7,0);
				
				lcl->reset();
				lcr->reset();
				lcl->switch_connect(57,28,1);
				lctl->reset();
				lctl->switch_connect(0,28,1);
				
				sc_top->reset_drivers(); //reset top synapse block drivers
				sc_top->configure();
				rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
				sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
				sc_top->preout_set(0, 0, 0, 0, 0);
				sc_top->drv_set_gmax(0, 0, 0, 15, 15);
				//sc_top->print_config();
				
				vector<uint> weights(32,0xffffffff),address(32,0x00000000);
				sc_top->write_weight(0, weights);
				sc_top->write_weight(1, weights);
				sc_top->write_decoder(0, address, address);
				/// receive data at left repeater block test input (trivial)
				//~ for (int i=0; i<32; i+=4) readout_repeater(rc_l, i);
				
				/// receive data at right repeater block test input
				//~ lcl->reset();
				//~ lcl->switch_connect(1,0,1);
				//~ lcl->switch_connect(0,0,1);
				//~ lcl->switch_connect(9,4,1);
				//~ lcl->switch_connect(8,4,1);
				//~ lcl->switch_connect(17,8,1);
				//~ lcl->switch_connect(16,8,1);
				//~ lcl->switch_connect(25,12,1);
				//~ lcl->switch_connect(24,12,1);
				//~ lcl->switch_connect(33,16,1);
				//~ lcl->switch_connect(32,16,1);
				//~ lcl->switch_connect(41,20,1);
				//~ lcl->switch_connect(40,20,1);
				//~ lcl->switch_connect(49,24,1);
				//~ lcl->switch_connect(48,24,1);
				//~ lcl->switch_connect(57,28,1);
				//~ lcl->switch_connect(56,28,1);
				//~ lcl->print_config();
				//~ rca[rc_r]->reset();
				//~ for (int i=0; i<32; i+=4) readout_repeater(rc_r, i);
				
				
				//~ /// receive data at top and bottom left repeater block test input
				//~ lcl->reset();
				//~ lcl->switch_connect(1, 0,1);
				//~ lcl->switch_connect(9, 4,1);
				//~ lcl->switch_connect(17,8,1);
				//~ lcl->switch_connect(25,12,1);
				//~ lcl->switch_connect(33,16,1);
				//~ lcl->switch_connect(41,20,1);
				//~ lcl->switch_connect(49,24,1);
				//~ lcl->switch_connect(57,28,1);
				//~ lctl->reset();
				//~ lctl->switch_connect(110,0,1);
				//~ lctl->switch_connect(110,1,1);
				//~ lctl->switch_connect(108,4,1);
				//~ lctl->switch_connect(108,5,1);
				//~ lctl->switch_connect(106,8,1);
				//~ lctl->switch_connect(106,9,1);
				//~ lctl->switch_connect(104,12,1);
				//~ lctl->switch_connect(104,13,1);
				//~ lctl->switch_connect(102,16,1);
				//~ lctl->switch_connect(102,17,1);
				//~ lctl->switch_connect(100,20,1);
				//~ lctl->switch_connect(100,21,1);
				//~ lctl->switch_connect(98, 24,1);
				//~ lctl->switch_connect(98, 25,1);
				//~ lctl->switch_connect(96, 28,1);
				//~ lctl->switch_connect(96, 29,1);
				//~ lctl->print_config();
				//~ rca[rc_tl]->reset();
				//~ for (int i=1; i<16; i+=2) readout_repeater(rc_tl, i);
				//~ cout << endl;
				//~ rca[rc_bl]->reset();
				//~ for (int i=0; i<64; i++) readout_repeater(rc_bl, i);
			
				
				/// receive data at top and bottom right repeater block test input
				//~ lcr->reset();
				//~ lcr->switch_connect(1,128,1);
				//~ lcr->switch_connect(9,132,1);
				//~ lcr->switch_connect(17,136,1);
				//~ lcr->switch_connect(25,140,1);
				//~ lcr->switch_connect(33,144,1);
				//~ lcr->switch_connect(41,148,1);
				//~ lcr->switch_connect(49,152,1);
				//~ lcr->switch_connect(57,156,1);
				//~ lctr->reset();
				//~ lctr->switch_connect(110,128,1);
				//~ lctr->switch_connect(110,129,1);
				//~ lctr->switch_connect(108,132,1);
				//~ lctr->switch_connect(108,133,1);
				//~ lctr->switch_connect(106,136,1);
				//~ lctr->switch_connect(106,137,1);
				//~ lctr->switch_connect(104,140,1);
				//~ lctr->switch_connect(104,141,1);
				//~ lctr->switch_connect(102,144,1);
				//~ lctr->switch_connect(102,145,1);
				//~ lctr->switch_connect(100,148,1);
				//~ lctr->switch_connect(100,149,1);
				//~ lctr->switch_connect(98,152,1);
				//~ lctr->switch_connect(98,153,1);
				//~ lctr->switch_connect(96,156,1);
				//~ lctr->switch_connect(96,157,1);
				//~ lctr->print_config();
				//~ rca[rc_tr]->reset();
				//~ for (int i=1; i<16; i+=2) readout_repeater(rc_tr, i);
				//~ cout << endl;
				//~ rca[rc_br]->reset();
				//~ for (int i=0; i<64; i++) readout_repeater(rc_br, i);


				///attempt to use the test output
				//~ vector<uint> times(3,0);
				//~ vector<uint> nnums(3,0);
				//~ uint repnr=0;
				//~ cin >> repnr;
				//~ uint odd=0;
				//~ if (repnr%2) odd=1;
				//~ 
				//~ rca[rc_tl]->conf_repeater(0, TESTOUT, INWARDS, true);
				//~ rca[rc_tl]->tdata_write(0,0,0,50);
				//~ rca[rc_tl]->tdata_write(0,1,0,60);
				//~ rca[rc_tl]->tdata_write(0,2,0,70);
				//~ rca[rc_tl]->startout(odd);
				//~ 
				//~ rca[rc_tl]->conf_repeater(repnr, TESTIN, OUTWARDS, true); //configure repeater in desired block to receive BEG data
				//~ usleep(500); //time for the dll to lock
				//~ rca[rc_tl]->stopin(odd); //reset the full flag
				//~ rca[rc_tl]->startin(odd); //start recording received data to channel 0
				//~ usleep(1000);  //recording lasts for ca. 4 µs - 1ms
				//~ 
				//~ for (int i=0; i<3; i++){
					//~ rca[rc_tl]->tdata_read(odd, i, nnums[i], times[i]);
					//~ cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
				//~ }
				//~ rca[rc_tl]->stopin(odd); //reset the full flag, one time is not enough somehow...
				//~ rca[rc_tl]->stopin(odd);
				//~ rca[rc_tl]->tout(repnr, false); //set tout back to 0 to prevent conflicts
			
			
				///see digital spike output on the synaptic drivers (top and bottom)
				//~ nbc->setOutputEn(true,true);
				//~ nbc->setOutputMux(1,1);
				//~ lcl->reset();
				//~ lcl->switch_connect(57,28,1);
				//~ lctl->reset();
				//~ lctl->switch_connect(0,28,1);
				//~ lcbl->reset();
				//~ lcbl->switch_connect(0,28,1);
//~ 
				//~ sc_top->reset_drivers(); //reset top synapse block drivers
				//~ sc_top->configure();
				//~ rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
				//~ sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
				//~ sc_top->preout_set(0, 0, 0, 0, 0);
				//~ sc_top->drv_set_gmax(0, 0, 0, 15, 15);
				//~ sc_top->print_config();
				//~ 
				//~ sc_bot->reset_drivers();
				//~ sc_bot->configure();
				//~ rca[rc_tl]->reset_dll();
				//~ sc_bot->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
				//~ sc_bot->preout_set(0, 0, 0, 0, 0);
				//~ sc_bot->sc_top->drv_set_gmax(0, 0, 0, 15, 15);
				
				
				///creating synaptic input
//				nbc->setOutputEn(true,true);
//				nbc->setOutputMux(1,4);
//				lcl->reset();
//				lcl->switch_connect(57,28,1);
//				lctl->reset();
//				lctl->switch_connect(0,28,1);
//				
//				sc_top->reset_drivers(); //reset top synapse block drivers
//				sc_top->configure();
//				rca[rc_tl]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
//				sc_top->drv_config(0, 1, 0, 1, 0, 1, 1, 0);
//				sc_top->preout_set(0, 0, 0, 0, 0);
//				sc_top->sc_top->drv_set_gmax(0, 0, 0, 15, 15);
//				sc_top->print_config();
//				
//				vector<uint> weights(32,0xffffffff),address(32,0x00000000);
//				sc_top->write_row(0, weights, 0);
//				sc_top->write_row(1, weights, 0);
//				sc_top->write_row(0, address, 1);
//				sc_top->write_row(1, address, 1);
				
			
				///attempt to write and read from the synapses
				//~ vector<uint> weights(32,0x99999999),address(32,0x33333333);
				//~ sc_top->write_row(0, weights, 0);
				//~ sc_top->write_row(0, weights, 1);
				//~ sc_top->read_row(0, address, 0);
				//~ cout << hex << address[0] << address[31] << endl;
				//~ sc_top->read_row(0, address, 1);
				//~ cout << hex << address[0] << address[31] << endl;
					
			/*	uint64_t systime = 0xffffffff;


				jtag->HICANN_set_test(0);
				jtag->HICANN_set_layer1_mode(0x55);
				nc->dnc_enable_set(0,0,0,0,0,0,0,0); //activate dnc stuff
				
				spc->write_cfg(0x55ff);
				
				dc->setTimeStampCtrl(0x0); // Disable DNC timestamp features
				dc->setDirection(0x55); // set transmission directions in DNC (for HICANN 0 only)
				
				jtag->HICANN_read_systime(systime);*/
			} break;
			case'l':{
				
				conf->reLoad();
				conf->getHicann(0);
			} break;
			case'x':cont=false;
			}		
//			if(kbhit()>0)cont=false;
		}while(cont);


/*	
		dbg(0) << "incremental" << endl;
		ramtest.Resize(0,MEM_SIZE, 1<<(2*n_numd));
		
		ramtest.Init(MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
		if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::incremental)) return 0; 		

		dbg(0) << "testing random access" << endl;
	
		//ramtest.Init( MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
		if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::random)) return 0; 		
	        */
		//
		//log(Logger::DEBUG1)<<"Activating neurons 0 an 1 of upper block and activating their outputs"<<endl; 
		//write(0,0x8e8e);
		//read(0);
		//uint testd;
		//rdata(testd);
		//dbg(0)<<testd<<endl;
		log(Logger::DEBUG0)<< "tm_neuron::ok" << endl;
		//disable analog output 
		//nbc->setOutputEn(false,false);
		return 1;
	};
};


class LteeTMNeuron : public Listee<Testmode>{
public:
	LteeTMNeuron() : Listee<Testmode>(string("tm_neuron"), string("look at a  neuron")){};
	Testmode * create(){return new TMNeuron();};
};

LteeTMNeuron ListeeTMNeuron;

