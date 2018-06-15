// Company         :   kip
// Author          :   smillner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgkeithleymean.cpp
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

using namespace std;
using namespace facets;

class TmFgKeithleyMean : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fgkeithleymean"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc;
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;
	stringstream ss;
	string filename;
	

	fstream file;

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

	static const int LINES 	= 129; //Number of measurements taken
	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to fgkeithleymean
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

		MeasKeithley keithley;		

		keithley.SetDebugLevel(0);
	
		keithley.connect("/dev/usbtmc1");
		keithley.getVoltage();


		time_t start = time (NULL);


		// prepare ramtest
	

		log(Logger::INFO)<< "starting...." << endl;
	
		comm->Clear();
		
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(3,3);
		
		// try FG controller start
		
		select(1);
		
		ss <<"../results/fgkeithleymean/"<< label; //<<"-"<<now->tm_year-100<<"-"<<setw(2)<<setfill('0')<<now->tm_mon+1<<"_"<<now->tm_hour<<":"<<now->tm_min<<".dat";
		filename = ss.str();
		file.open(filename.c_str(), fstream::out);


		log(Logger::INFO)<< "testing controller " << locs << " output operation" << endl;

		
		fc->set_maxcycle(255);		
		fc->set_currentwritetime(63);
		fc->set_voltagewritetime(63);
		fc->set_readtime(63);
		fc->set_acceleratorstep(15);
		fc->set_pulselength(4);
		fc->set_fg_biasn(4);
		fc->set_fg_bias(8);
		fc->write_biasingreg();
		fc->write_operationreg();
		
			
	 	double measurements[LINES-1];
		double mean;
		double dev;
	
		cout<<dec<<setw(2)<<setprecision(4)<<scientific;
		log(Logger::INFO)<< "Getting mean values of floating gate lines"<<Logger::flush;
		for(int i=0; i<400; i++){
			cout<<time (NULL) - start<<"\t";
			file<<time (NULL) - start<<"\t";
			for(int row=0; row<24; row+=1){
				
				
				mean = 0.0;
				for(int line=1; line < LINES; line++){ //exclude first line with ops as biases depend onf current cells
					fc->readout_cell(row ,line);
					//cout<<row<<","<<line<<endl;
					usleep(1);	
					measurements[line] = keithley.getVoltage();
					//cout<<measurements[line]<<endl; 
					mean +=measurements[line];
					
				}
				mean = mean/(LINES-1);
				dev = 0.0;
				for(int line=1; line < LINES; line++){
					dev +=(mean-measurements[line])*(mean-measurements[line]);
				}
				dev = sqrt(dev/(LINES-1));
				cout<<mean<<" "<<dev<<" ";	
				file<<mean<<" "<<dev<<" ";	
			}		
			cout<<endl;
			file<<endl;
			file.flush();
		}		
		file.close();
		dbg(0) << "ok" << endl;
	
		return 1;
	};
};


class LteeTmFgKeithleyMean : public Listee<Testmode>{
public:
	LteeTmFgKeithleyMean() : Listee<Testmode>(string("tm_fgkeithleymean"), string("test floating-gate stuff")){};
	Testmode * create(){return new TmFgKeithleyMean();};
};

LteeTmFgKeithleyMean ListeeTmFgKeithleyMean;

