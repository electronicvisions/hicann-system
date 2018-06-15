// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_outamp.cpp
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

using namespace facets;

class TmOutamp : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_outamp"; return ss.str();}
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
			case  0: loc=FGTR; locs="FGTR"; fc=hc->getFC_tr(); break;
			case  1: loc=FGTL; locs="FGTL"; fc=hc->getFC_tl(); break;
			case  2: loc=FGBR; locs="FGBR"; fc=hc->getFC_br(); break;
			case  3: loc=FGBL; locs="FGBL"; fc=hc->getFC_bl(); break;
			default: loc=FGTR; locs="FGTR"; fc=hc->getFC_tr(); break;
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

	// access to fgctrl
	inline void clear() {};
	inline void write(int addr, uint value) {
		fc->write_ram(bank, addr, value % 1024, value/1024);
	 }
	inline void read(int addr) { fc->read_ram(bank, addr); }

	inline bool rdata(uint& value) {

                uint taddr;
               
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



	 	 log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;


	ll = hc->tags[TAG];
		ll->SetDebugLevel(0); // no ARQ deep debugging
		nbc = hc -> getNBC();
		
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
	

				//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(1,1);
		

		// register tests for all 4 controllers
		/*
		for (cntlr=0; cntlr<4; cntlr++)
		{
			fg->set_loc((fg_loc)cntlr);
			dbg(0) << "testing controller " << cntlr << " register 'op'" << endl;
			ramtest.Resize(REG_OP, 1, -1);  // 32 bit size
			ramtest.Clear();
			write(REG_OP, 0); // clear
			if (!ramtest.Test(REG_TEST_NUM)) return 0; 		

			dbg(0) << "testing controller " << cntlr << " register 'bias'" << endl;
			ramtest.Resize(REG_BIAS, 1, 1<<REG_BIAS_WIDTH);
			ramtest.Clear();
			write(REG_BIAS, 0); // clear
			if (!ramtest.Test(REG_TEST_NUM)) return 0; 		
		}

		*//*
		// first init rams doing incremental tests
		for (cntlr=0; cntlr<4; cntlr++)
		{
			select(cntlr);
			dbg(0) << "testing controller " << locs << " sram bank 0 (incremental)" << endl;
			bank = 0;
			ramtest.Resize(MEM_BASE, MEM_SIZE, 1<<MEM_WIDTH);
			if (!ramtest.Test(MEM_SIZE, RamTestUint::incremental)) return 0; 		

			dbg(0) << "testing controller " << locs << " sram bank 1 (incremental)" << endl;
			bank = 1;
			ramtest.Resize(MEM_BASE, MEM_SIZE, 1<<MEM_WIDTH);
			if (!ramtest.Test(MEM_SIZE, RamTestUint::incremental)) return 0; 		
		}
		
		// random access ...
		for (cntlr=0; cntlr<4; cntlr++)
		{
			select(cntlr);

			dbg(0) << "testing controller " << locs << " sram bank 0 (random access)" << endl;
			bank = 0;
			ramtest.Resize(MEM_BASE, MEM_SIZE, 1<<MEM_WIDTH);
			ramtest.Init(MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
			if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::random)) return 0; 		

			dbg(0) << "testing controller " << locs << " sram bank 1 (random access)" << endl;
			bank = 1;
			ramtest.Resize(MEM_BASE, MEM_SIZE, 1<<MEM_WIDTH);
			ramtest.Init(MEM_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
			if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::random)) return 0; 		
		}
		
		if(fc->isBusy()){
			dbg(3)<<"Slave is busy"<<endl;
		} else {
			dbg(3)<<"Slave is idle"<<endl;	
		}
		*/

		// try FG controller start
		for(cntlr=0; cntlr<4;cntlr+=1)
		{
			select(cntlr);
			int i;
			dbg(0) << "Writing controller " << cntlr << "." << endl;
	
			//fill ram with zeros;
			fc->initval(0,500);
			/*for(int i=64;i<65;i++){
				fc->write_ram(0,i,1023,1023);
			} 
			fc->write_ram(0,0,1023,0);*/
			//program controler
			
			fc->set_maxcycle(255);		
			fc->set_currentwritetime(16);
			fc->set_voltagewritetime(16);
			fc->set_readtime(63);
			fc->set_acceleratorstep(15);
			fc->set_pulselength(15);
			fc->set_fg_biasn(0);
			fc->set_fg_bias(15);
			fc->write_biasingreg();
			fc->write_operationreg();
			
			for(i=0; i<24; i+=1){
				 dbg(0) <<"programing row " << i << endl;
				 if (fc->isBusy()) { dbg(0) << "ERROR: Client is busy before any task has been assigned ... abort" << endl; return (0); }
				//fc->initval(0,(i/2)*85);
				 // start state machine
				 fc->program_cells(i,0,1);
				 while(fc->isBusy());
			}	
			// waiting for busy going high
			/*for (i=50;i>0; i--) {
				if(fc->isBusy()) break; 
			}
			if (!i) { dbg(0) << "ERROR: busy not going high" << endl; return (0); }
			
			// waiting for busy going low
			
			for (i=50;i>0; i--) {
				if(!fc->isBusy()) break;
			}
			if (!i) { dbg(0) << "ERROR: busy not going low" << endl; return (0); }*/
			fc->readout_cell(1,0);
			//cin>>pause;
		}
		// try FG controller writeup operation
	/*	
		for(cntlr=0; cntlr<4;cntlr++)
		{	
			dbg(0) << "testing controller " << cntlr << " writeFGup operation" << endl;
	
			read(REG_SLAVE);
			if (!rdatabl(value)) return 0;
			if (value) { dbg(0) << "ERROR: controller not idle, abort" << endl; return (0); }
			
			//configurate controller
			write(REG_BIAS, 0);//same clock as ocp to work faster in testbench
			write(REG_OP, 0x1041040e);
			// start state machine
			write(REG_ADDRINS, 0x4040); 
	
			// waiting for busy going high
			
			int i;
			value=0;
			for (i=50;i>0; i--) {
				read(REG_SLAVE);
				if (!rdatabl(value)) return 0;
				dbg(0) << "read value 0x" << hex << value << endl;
				if ((value & REG_SLAVE_BUSY) !=0) break;
				wait(1, SC_US); 
			}
			if (!i) { dbg(0) << "ERROR: busy not going high" << endl; return (0); }
			
			// waiting for busy going low
			value=0;
			for (i=500;i>0; i--) {
				read(REG_SLAVE);
				if (!rdatabl(value)) return 0;
				dbg(0) << "read value 0x" << hex << value << endl;
				if ((value & REG_SLAVE_BUSY) ==0) break;
				wait(1, SC_US); 
			}
			if (!i) { dbg(0) << "ERROR: busy not going low" << endl; return (0); }
		}	
*/
		dbg(0) << "ok" << endl;
	
		return 1;
	};
};


class LteeTmOutamp : public Listee<Testmode>{
public:
	LteeTmOutamp() : Listee<Testmode>(string("tm_outamp"), string("Programm floating gates up and set fg_out outamps to measure output resistances")){};
	Testmode * create(){return new TmOutamp();};
};

LteeTmOutamp ListeeTmOutamp;

