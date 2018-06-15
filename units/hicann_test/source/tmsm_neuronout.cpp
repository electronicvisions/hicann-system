// cOmpany         :   kip
// Author          :   Sebastian Millner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tmsm_neuronout.cpp
// Project Name    :   p_facets
// Subproject Name :   s_system            
//                    			
// Create Date     :   7/2010
//------------------------------------------------------------------------

#include "s2comm.h"
#include "s2ctrl.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
#include "fg_control.h"
#include "neuronbuilder_control.h"
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"

using namespace std;
using namespace facets;

class TMNeuronOut : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmsm_neuronout"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc[4];//array of floatinggate controlers
	NeuronBuilderControl *nbc;

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
		 	log(Logger::ERROR)<<"ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
//		ll = hc->tags[TAG]; 
//		ll->SetDebugLevel(3); // no ARQ deep debugging



	 	 log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;


		nbc = &hc -> getNBC();
		conf->getHicann(0);
		fc[0]=&hc->getFC_tl();
		fc[1]=&hc->getFC_tr();
		fc[2]=&hc->getFC_bl();
		fc[3]=&hc->getFC_br();
	

		debug_level = 4;
		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		

		// prepare ramtest
	

		log(Logger::INFO)<<"tmsm_singleNeuron: starting...." << endl;
	
		
		//initialize nbcrams with zersos
		nbc->initzeros();

		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to neuron output line 0 and 1
		nbc->setOutputMux(4,4);
	
	

		// initializ floating gates  
		
		log(Logger::DEBUG1)<<"tmsm_neuronout::Setting fg control parameters";
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
		
		//write currents first as ops need biases before they work.
		log(Logger::INFO)<<"tmsm_neuronout::writing currents ...";
		for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2) //starting at line 1 as this is the first current!!!!!
		{
			log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
			for( fg_num = 0; fg_num < 4; fg_num += 1)
			{	
				log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
				fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
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
		log(Logger::INFO)<<"tmsm_neuronout::writing voltages ...";
		for( uint lineNumber = 0; lineNumber < 24; lineNumber += 2)
		{
			log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
			for( fg_num = 0; fg_num < 4; fg_num += 1)
			{	
	
				log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
				fc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
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
		for( fg_num = 0; fg_num < 4; fg_num += 2)
		{
			while(fc[fg_num]->isBusy());
			fc[fg_num]->initSquare(0,420);
		
	//		fc[fg_num]->initSquare(0,400);
			fc[fg_num]->set_pulselength(15);
			fc[fg_num]->write_biasingreg();

			fc[fg_num]->stimulateNeuronsContinuous(0);
		}
/*	
		dbg(0) << "incremental" << endl;
		ramtest.Resize(0,MEM_SIZE, 1<<(2*n_numd));
		
		ramtest.Init(MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
		if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::incremental)) return 0; 		

		dbg(0) << "testing random access" << endl;
	
		//ramtest.Init( MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
		if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::random)) return 0; 		
	        */
		nbc->setNeuronBigCap(1,1);

		nbc->setNeuronGl(0,0,1,1);
		nbc->setNeuronGladapt(0,0,0,0);
		//
		nbc->setNeuronReset(1,1);
		log(Logger::DEBUG1)<<"Activating neurons 0 an 1 of upper block and activating their outputs"<<endl; 
		write(0,0x8e8e);
//010 for neuronbuilder - upper neuron is set to fire(both).
//0x8e8e for neurons
//111111 for spl1
//0xa3a3bf
		uint value = nbdata(1,1,0,0x8e,0x8e,0x00);
		log(Logger::ERROR)<<hex<<value;	
		nbc->write_data(0,value);//neuronbuilder enable top and connect.
		nbc->write_data(1,value);//neuronbuilder enable bot 

		value = nbdata(1,1,0,0x8e,0x8e,0x00);
		nbc->write_data(2,value);//neuronbuilder enable top and connect.
		nbc->write_data(3,value);//neuronbuilder enable bot and neuronconfig.
		//read(0);
		//uint testd;
		//rdata(testd);
		//dbg(0)<<testd<<endl;

		nbc->setSpl1Reset(0);
		nbc->setSpl1Reset(1);
		nbc->configNeuronsGbl();
		log(Logger::DEBUG0)<< "tmsm_neuronout::ok" << endl;
		for(uint i=0;i<10;i++){nbc->read_data(i);ci_addr_t addr; ci_data_t data;nbc->get_read_data(addr, data);cout<<hex<<addr<<" "<<data<<endl;}
		
		return 1;
	};
};


class LteeTMNeuronOut : public Listee<Testmode>{
public:
	LteeTMNeuronOut() : Listee<Testmode>(string("tmsm_neuronout"), string("test neuron l1output")){};
	Testmode * create(){return new TMNeuronOut();};
};

LteeTMNeuronOut ListeeTMNeuronOut;

