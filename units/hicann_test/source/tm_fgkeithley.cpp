// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgkeithley.cpp
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
#include "MeasKeithley.h"
#include <time.h>

using namespace std;
using namespace facets;

class TmFgKeithley : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fgkeithley"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc;
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;

	uint bank ;
	int cntlr;

	time_t timestamp;
	tm *now;
	
	fstream file;

	void select(int sw)
	{
		switch (sw){
			case  1: loc=FGTR; locs="FGTR"; fc=&hc->getFC_tr(); break;
			case  0: loc=FGTL; locs="FGTL"; fc=&hc->getFC_tl(); break;
			case  3: loc=FGBR; locs="FGBR"; fc=&hc->getFC_br(); break;
			case  2: loc=FGBL; locs="FGBL"; fc=&hc->getFC_bl(); break;
			default: loc=FGTR; locs="FGTR"; fc=&hc->getFC_tr(); break;
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

	// access to fgkeithley
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
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
	 	 log(Logger::ERROR) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;


		nbc = &hc -> getNBC();
		
		debug_level = 4;
		// ----------------------------------------------------
		// test
		// ----------------------------------------------------
	
		timestamp = time(0);
		now = localtime(&timestamp);
		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);

		MeasKeithley keithley;		

		keithley.SetDebugLevel(0);
	
		keithley.connect("/dev/usbtmc1");
		keithley.getVoltage();


		dbg(0) << hex;
		

		// prepare ramtest
	

		dbg(0) << "starting...." << endl;
	
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 1 and 3;
		nbc->setOutputMux(3,3);
		
		// try FG controller start
	 	for(int fgn=0; fgn<4; fgn++){	
			select(fgn);

			nbc->setOutputMux(3+(fgn%2)*6,3+(fgn%2)*6);
			int i;
			dbg(0) << "testing controller " << fgn << " output operation" << endl;

			//fill ram with zeros;
	//		fc->initzeros(0);
			//fc->write_ram(0,0,800,800);
			
			//program controler
			
			fc->set_maxcycle(255);		
			fc->set_currentwritetime(63);
			fc->set_voltagewritetime(63);
			fc->set_readtime(63);
			fc->set_acceleratorstep(15);
			fc->set_pulselength(15);
			//fc->set_fg_biasn(4);
			//fc->set_fg_bias(8);
			fc->set_fg_biasn(15);
			fc->set_fg_bias(15);
			fc->write_biasingreg();
			fc->write_operationreg();
			
			cout<<setw(2)<<setprecision(4);
			dbg(0) << "Reading voltages "<< endl;
			for(int line=0; line<29; line+=50){
				for(int row=0; row<12; row++){
					
					fc->readout_cell(2*row ,line);
	//				usleep(1000000);
					cout<<keithley.getVoltage()<<"\t";	
				}		
				cout<<endl;/*
				for(int row=0; row<12; row++){
					
					fc->readout_cell(2*row ,line+1);
	//				usleep(1000000);
					cout<<keithley.getVoltage()<<"\t";	
				}		
				cout<<endl;*/
			}
			dbg(0) << "Reading currents "<< endl;
			for(int line=0; line<29; line+=50){
				for(int row=0; row<12; row++){
					
					fc->readout_cell(2*row+1  ,line);
	//				usleep(10);
					cout<<keithley.getVoltage()<<"\t";	
				}		
				cout<<endl;
/*				for(int row=0; row<12; row++){
					
					fc->readout_cell(2*row + 1 ,line+1);
	//				usleep(10);
					cout<<keithley.getVoltage()<<"\t";	
				}		
				cout<<endl;*/
			}
		}
/*
		dbg(0) << "Reading currents of row 0"<< endl;
		for(int i=0; i<12; i++){
			
			fc->readout_cell(2*i+1 ,0);
			usleep(100);
			keithley.getVoltage();	
		}
		dbg(0) << "Reading voltages of row 1"<< endl;
		for(int i=0; i<12; i++){
			
			fc->readout_cell(2*i ,1);
			usleep(100);
			keithley.getVoltage();	
		}		
		dbg(0) << "Reading currents of row 1"<< endl;
		for(int i=0; i<12; i++){
			
			fc->readout_cell(2*i+1 ,1);
			usleep(100);
			keithley.getVoltage();	
		}

		dbg(0) << "Reading collumn0"<< endl;
		for(int i=0; i<129; i++){
			
			fc->readout_cell(0 ,i);
			usleep(100);
			keithley.getVoltage();	
		}*/
	dbg(0) << "ok" << endl;
	

	return 1;
	};
};


class LteeTmFgKeithley : public Listee<Testmode>{
public:
	LteeTmFgKeithley() : Listee<Testmode>(string("tm_fgkeithley"), string("test floating-gate stuff")){};
	Testmode * create(){return new TmFgKeithley();};
};

LteeTmFgKeithley ListeeTmFgKeithley;

