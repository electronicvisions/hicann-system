// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgctrl.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   3/2009
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

class TMNeuronRam : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_neuronram"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc;
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;
	int cntlr;
	
	void select(int sw)
	{
		switch (sw){
			case  0: loc=FGTR; locs="FGTR"; fc=&hc->getFC_tr(); break;
			case  1: loc=FGTL; locs="FGTL"; fc=&hc->getFC_tl(); break;
			case  2: loc=FGBR; locs="FGBR"; fc=&hc->getFC_br(); break;
			case  3: loc=FGBL; locs="FGBL"; fc=&hc->getFC_bl(); break;
			default: loc=FGTR; locs="FGTR"; fc=&hc->getFC_tr(); break;
		}
	}

	// -----------------------------------------------------	
	// FG controller constants
	// -----------------------------------------------------	


	// for constants see "compare units/fgateCtrl/source/fgateCtrl.sv" 

	static const int TAG             = 0;
	static const int REG_TEST_NUM    = 10;

 	static const int MEM_SIZE        =128;//(1<<(nspl1_numa)) -1;
	static const int MEM_WIDTH       = 8;
	static const int MEM_BASE       = 0;

	static const int MEM_TEST_NUM    = MEM_SIZE*20;


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


		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		nbc = &hc -> getNBC();
		
		debug_level = 4;
		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		dbg(0) << hex;
		

		// prepare ramtest
	

		dbg(0) << "starting...." << endl;
	
			//initialize nbcrams with zersos
		//nbc->initzeros();

		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to neuron output line 0 and 1
		nbc->setOutputMux(4,6);
	
	/*

		// initializ floating gates  
		for(cntlr=0; cntlr<4;cntlr++)
		{
			select(cntlr);
			int i;
			dbg(0) << "Initializing controller " << cntlr << endl;
	
			//fill ram with zeros;
			fc->initmax(0);
			
			//programm controler
			
			fc->set_maxcycle(255);		
			fc->set_currentwritetime(63);
			fc->set_voltagewritetime(16);
			fc->set_readtime(63);
			fc->set_acceleratorstep(15);
			fc->set_pulselength(2);
			fc->write_biasingreg();
			fc->write_operationreg();
			
			for(i=0; i<24; i+=1){
				 dbg(0) <<"programming row " << i << endl;
				 if (fc->isBusy()) { dbg(0) << "ERROR: Client is busy before any task has been assigned ... abort" << endl; return (0); }

				 // start state machine
				 fc->programm_cells(i,0,0);
				 while(fc->isBusy());
			}	
		}*/
		dbg(0) << "incremental" << endl;
		//ramtest.Resize(0,MEM_SIZE, 1<<(2*n_numd));
		
//		ramtest.Init(MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
	//	if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::incremental)) return 0; 		

	//	dbg(0) << "testing random access" << endl;
	
	//	ramtest.Init( MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
	//	if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::random, randomseed)) return 0; 		



		ci_data_t val1;
		ci_addr_t ad1;
	       /* commented as standart values are desired for inc and random test. */
		nbc->readTRD();

		//uint val1;
		//uint ad1;
		nbc->get_read_data(ad1,val1);
		dbg(0)<<"Read trd = "<<val1<<endl;
		//dbg(0)<<"Writing new trd"<<endl;
		//nbc->writeTRD(0xff);
		//nbc->readTRD();
	
		//nbc->get_read_data(ad1,val1);
		//dbg(0)<<"Read trd = "<<val1<<endl;

		nbc->readTSUTWR();

		
	
		nbc->get_read_data(ad1,val1);
		dbg(0)<<"Read tsutwr = "<<val1<<endl;
		//dbg(0)<<"Writing new tsutwr"<<endl;
		//nbc->writeTSUTWR(0x11);
		//nbc->readTSUTWR();
	
		//nbc->get_read_data(ad1,val1);

		write(0x37,0xf608);
		dbg(0)<<"Checking address 0x37" <<endl;
		for(int j = 0; j<10; j++){
			read(0x37);

			rdata(val1);
			dbg(0)<<"Read value is: "<<val1<<" and should be 0xf608!"<<endl;
		}
		write(0x5d,0xf610);
		
		dbg(0)<<"Checking address 0x5d" <<endl;
		for(int j = 0; j<10; j++){

			read(0x5d);
			
			rdata(val1);
		
			dbg(0)<<"Read value is: "<<val1<<" and should be 0xf610!"<<endl;

		}

		
		write(0x16f,0x5f08);
		dbg(0)<<"Checking address 0x16f" <<endl;
		for(int j = 0; j<10; j++){
			read(0x16f);

			rdata(val1);
			dbg(0)<<"Read value is: "<<val1<<" and should be 0x5f08!"<<endl;
		}
		write(0x5d,0xee10);
		
		dbg(0)<<"Checking address 0x5d" <<endl;
		for(int j = 0; j<10; j++){

			read(0x5d);
			
			rdata(val1);
		
			dbg(0)<<"Read value is: "<<val1<<" and should be 0xee10!"<<endl;

		}
		dbg(0) << "ok" << endl;
		
		return 1;
	};
};


class LteeTMNeuronRam : public Listee<Testmode>{
public:
	LteeTMNeuronRam() : Listee<Testmode>(string("tm_neuronram"), string("look at a single neuron")){};
	Testmode * create(){return new TMNeuronRam();};
};

LteeTMNeuronRam ListeeTMNeuronRam;

