// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgrowinc.cpp
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

class TmFgRowInc : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fgrowinc"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc;
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;

	uint bank ;
	int cntlr;
	
	void select(int sw)
	{
		switch (sw){
			case  0: loc=FGTL; locs="FGTL"; fc=&hc->getFC_tl(); break;
			case  1: loc=FGTR; locs="FGTR"; fc=&hc->getFC_tr(); break;
			case  2: loc=FGBL; locs="FGBL"; fc=&hc->getFC_bl(); break;
			case  3: loc=FGBR; locs="FGBR"; fc=&hc->getFC_br(); break;
			default: loc=FGTL; locs="FGTL"; fc=&hc->getFC_tl(); break;
		}
	}

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

	// access to fgrowinc
	inline void clear() {};
	inline void write(int addr, uint value) {
		fc->write_ram(bank, addr, value % 1024, value/1024);
	 }
	inline void read(int addr) { fc->read_ram(bank, addr); }

	inline bool rdata(ci_data_t& value) {

                ci_addr_t taddr;
               
                fc->get_read_data_ram(taddr, value);
                
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



	 	 log(Logger::ERROR) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;




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
		

		// prepare ramtest
	

		log(Logger::INFO)<< "starting...." << endl;

    
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(9,9);
		

		// try FG controller start
		for(cntlr=1; cntlr<4;cntlr+=5)
		{
			select(cntlr);
			//program controler
			fc->set_maxcycle(128);		
			fc->set_currentwritetime(4);
			fc->set_voltagewritetime(16);
			fc->set_readtime(43);
			fc->set_acceleratorstep(8);
			fc->set_pulselength(8);
			fc->set_fg_biasn(4);
			fc->set_fg_bias(8);
			fc->write_biasingreg();
			fc->write_operationreg();
		}		
		for(int i=0; i<24; i+=1){

			for(cntlr=0; cntlr<4;cntlr+=1)
			{
				select(cntlr);
				log(Logger::DEBUG1)<<"programing line " << i << endl;
				fc->programLine(i, (i/2)*85,(i/2)*85);
			}
		}	
		
		log(Logger::INFO)<< "ok" << endl;
	
		return 1;
	};
};


class LteeTmFgRowInc : public Listee<Testmode>{
public:
	LteeTmFgRowInc() : Listee<Testmode>(string("tm_fgrowinc"), string("Write incremented values in floating gate array")){};
	Testmode * create(){return new TmFgRowInc();};
};

LteeTmFgRowInc ListeeTmFgRowInc;

