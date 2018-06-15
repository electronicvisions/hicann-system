// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgzero.cpp
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

class TmFgZero : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fgzero"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc[4]; //array of the four floating gate controlers
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;

	uint bank ;
	int cntlr;

	// -----------------------------------------------------	
	// FG controller constants
	// -----------------------------------------------------	


	// for constants see "compare units/fgateCtrl/source/fgateCtrl.sv" 

	static const int TAG             = 0;

	static const int REG_BASE        = 256;  // bit 8 set
	static const int REG_OP          = REG_BASE+2;
	static const int REG_ADDRINS     = REG_BASE+1;
	static const int REG_SLAVE       = REG_BASE+4;
	static const int REG_BIAS        = REG_BASE+8;

	static const int REG_OP_WIDTH    = 32;   // pos 00010
	static const int REG_BIAS_WIDTH  = 14;   // pos 01000
	static const int REG_SLAVE_BUSY  = 256;  // bit 8

	static const int REG_TEST_NUM    = 10;

 	static const int MEM_SIZE        = 65;
	static const int MEM_WIDTH       = 20;
	static const int MEM_BASE       = 0;

	static const int MEM_TEST_NUM    = MEM_SIZE*4;


	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to fgzero
	inline void clear() {};
	inline void write(int addr, uint value) {
		fc[cntlr]->write_ram(bank, addr, value % 1024, value/1024);
	 }
	inline void read(int addr) { fc[cntlr]->read_ram(bank, addr); }

	inline bool rdata(ci_data_t& value) {

                ci_addr_t taddr;
               
                fc[cntlr]->get_read_data_ram(taddr, value);
                
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
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;
	
		log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
		nbc = &hc->getNBC();

		fc[0]=&hc->getFC_tl();
		fc[1]=&hc->getFC_tr();
		fc[2]=&hc->getFC_bl();
		fc[3]=&hc->getFC_br();

		// --------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		
		

		// prepare ramtest
	

		log(Logger::INFO)<< "starting...." << endl;
	
		comm->Clear();

//		log(Logger::DEBUG1)<< "Setting Pll multiplier";

	//	jtag->HICANN_set_pll_far_ctrl(1, 1, 0, 0, 0);
	//	jtag->HICANN_set_pll_far_ctrl(1, 1, 1, 0, 0);


     
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(9,9);
		// try FG controller start
		for(cntlr=0; cntlr<4;cntlr+=1)
		{//init all floating gate controlers
			
		
			log(Logger::DEBUG1)<< "Writing controller " << cntlr << "." << endl;
	
			//fill ram with zeros;
			fc[cntlr]->initzeros(0);
			//program controler
			
			fc[cntlr]->set_maxcycle(255);		
			fc[cntlr]->set_currentwritetime(63);
			fc[cntlr]->set_voltagewritetime(63);
			fc[cntlr]->set_readtime(2);
			fc[cntlr]->set_acceleratorstep(1);
			fc[cntlr]->set_pulselength(15);
			fc[cntlr]->set_fg_biasn(15);
			fc[cntlr]->set_fg_bias(15);
			fc[cntlr]->write_biasingreg();
			fc[cntlr]->write_operationreg();
		}	
	 	log(Logger::INFO)<<"Init done, programming now";
		for(int i=0; i<24; i+=1){
			for(cntlr=0; cntlr<4; cntlr+=1){
				while(fc[cntlr]->isBusy());

				log(Logger::DEBUG2)<<"programing row " << i << endl;
				 
				// start state machine
				fc[cntlr]->program_cells(i,0,0);
			}
		}	
		for(cntlr=0; cntlr<4;cntlr+=1){

			fc[cntlr]->readout_cell(1,0);
		
		}
		log(Logger::DEBUG1)<< "ok" << endl;
	
		return 1;
	};
};


class LteeTmFgZero : public Listee<Testmode>{
public:
	LteeTmFgZero() : Listee<Testmode>(string("tm_fgzero"), string("Initialize floating gates with zeros")){};
	Testmode * create(){return new TmFgZero();};
};

LteeTmFgZero ListeeTmFgZero;

