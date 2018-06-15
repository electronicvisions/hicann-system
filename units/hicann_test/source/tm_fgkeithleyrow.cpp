// Company         :   kip
// Author          :   smillner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgkeithleyrow.cpp
// Project Name    :   p_facets
// Subproject Name :   s_system            
//                    			
// Create Date     :   3/2010
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

using namespace facets;

class TmFgKeithleyRow : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fgkeithleyrow"; return ss.str();}
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
			case  0: loc=FGTL; locs="FGTL"; fc=hc->getFC_tl(); break;
			case  1: loc=FGTR; locs="FGTR"; fc=hc->getFC_tr(); break;
			case  2: loc=FGBL; locs="FGBL"; fc=hc->getFC_bl(); break;
			case  3: loc=FGBR; locs="FGBR"; fc=hc->getFC_br(); break;
			default: loc=FGTL; locs="FGTL"; fc=hc->getFC_tl(); break;
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

	static const int NUMMESS 	= 10; //Number of measurements taken
	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to fgkeithleyrow
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
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
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

		MeasKeithley keithley;		

		keithley.SetDebugLevel(0);
	
		keithley.connect("/dev/usbtmc1");
		keithley.getVoltage();


		time_t start = time (NULL);

		dbg(0) << hex;
		

		// prepare ramtest
	

		dbg(0) << "starting...." << endl;
	
		comm->Clear();
		
		dbg(0)  << "disabling dnc-interface";
		jtag->set_jtag_instr(0x5);
		int cmd = 0x0;
		jtag->set_jtag_data<int>(cmd,'1');

		dbg(0) << "Asserting ARQ reset via JTAG..." << endl;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
                jtag->HICANN_arq_write_ctrl(0x0, 0x0);

		dbg(0) << "Setting timeout values via JTAG..." << endl;
                jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
                jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1
     
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(9,9);
		
		// try FG controller start
		
		select(1);
		
		dbg(0) << "testing controller " << cntlr << " output operation" << endl;

		//fill ram with zeros;
//		fc->initzeros(0);
		//fc->write_ram(0,0,800,800);
		
		//programm controler
		
		fc->set_maxcycle(255);		
		fc->set_currentwritetime(63);
		fc->set_voltagewritetime(63);
		fc->set_readtime(63);
		fc->set_acceleratorstep(15);
		fc->set_pulselength(15);
		fc->set_fg_biasn(0);
		fc->set_fg_bias(15);
		fc->write_biasingreg();
		fc->write_operationreg();
		
			
	 	double measurements[NUMMESS];
		double mean;
		double dev;
	
		cout<<dec<<setw(2)<<setprecision(4)<<scientific;
		dbg(0) << "Reading currents of row 2"<< endl;
		for(int line=0; line<20; line++){
			cout<<time (NULL) - start<<"\t";
			for(int row=0; row<12; row++){
				
				fc->readout_cell(2*row+1 ,2);
				usleep(100);
				mean = 0.0;
				for(int meas=0; meas < NUMMESS; meas++){
					measurements[meas] = keithley.getVoltage();
					mean +=measurements[meas];
				}
				mean = mean/NUMMESS;
				dev = 0.0;
				for(int meas=0; meas < NUMMESS; meas++){
					dev +=(mean-measurements[meas])*(mean-measurements[meas]);
				}
				dev = sqrt(dev/NUMMESS);
				cout<<mean<<" "<<dev<<" ";	
			}		
			cout<<endl;
			
			usleep(100);

		}/*
		dbg(0) << "Reading currents "<< endl;
		for(int line=0; line<50; line++){
			for(int row=0; row<12; row++){
				
				fc->readout_cell(2*row + 1 ,line);
//				usleep(100);
				cout<<keithleyrow.getVoltage()<<"\t";	
			}		
			cout<<endl;
		}


		dbg(0) << "Reading currents of row 0"<< endl;
		for(int i=0; i<12; i++){
			
			fc->readout_cell(2*i+1 ,0);
			usleep(100);
			keithleyrow.getVoltage();	
		}
		dbg(0) << "Reading voltages of row 1"<< endl;
		for(int i=0; i<12; i++){
			
			fc->readout_cell(2*i ,1);
			usleep(100);
			keithleyrow.getVoltage();	
		}		
		dbg(0) << "Reading currents of row 1"<< endl;
		for(int i=0; i<12; i++){
			
			fc->readout_cell(2*i+1 ,1);
			usleep(100);
			keithleyrow.getVoltage();	
		}

		dbg(0) << "Reading collumn0"<< endl;
		for(int i=0; i<129; i++){
			
			fc->readout_cell(0 ,i);
			usleep(100);
			keithleyrow.getVoltage();	
		}*/
		dbg(0) << "ok" << endl;
	
		return 1;
	};
};


class LteeTmFgKeithleyRow : public Listee<Testmode>{
public:
	LteeTmFgKeithleyRow() : Listee<Testmode>(string("tm_fgkeithleyrow"), string("test floating-gate stuff")){};
	Testmode * create(){return new TmFgKeithleyRow();};
};

LteeTmFgKeithleyRow ListeeTmFgKeithleyRow;

