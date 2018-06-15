// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fg.cpp
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

class TmSmFgFunctional : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fg"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc[4]; //array of the four floating gate controlers
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;

	uint bank ;
	int fg_num;

	//start values
	uint maxcycle;		
	uint currentwritetime;
	uint voltagewritetime;
	uint readtime;
	uint acceleratorstep;
	uint pulselength;
	uint fg_biasn;
	uint fg_bias;	

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

	// access to fg
	inline void clear() {};
	inline void write(int addr, uint value) {
		fc[fg_num]->write_ram(bank, addr, value % 1024, value/1024);
	 }
	inline void read(int addr) { fc[fg_num]->read_ram(bank, addr); }

	inline bool rdata(ci_data_t& value) {

                ci_addr_t taddr;
               
                fc[fg_num]->get_read_data_ram(taddr, value);
                
                return true;

	 }
	/*inline bool rdatabl(uint& value) { cmd_pck_t p; int r=ll->get_data(&p, true); value=p.data; return (bool)r; }

*/
        //helper functions

	void setupFg(){

		for(int fg_num=0; fg_num<4;fg_num+=1)
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
	
	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		uint maxval=8;

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

	 	log(Logger::INFO) << "Init() ok" << endl;



        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		nbc = &hc -> getNBC();
		fc[0]=&hc->getFC_tr();
		fc[1]=&hc->getFC_tl();
		fc[2]=&hc->getFC_br();
		fc[3]=&hc->getFC_bl();
	

		debug_level = 4;

		// --------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		

		//start values
		maxcycle = 255;		
		currentwritetime = 3;
		voltagewritetime = 3;
		readtime = 2;
		acceleratorstep = 1;
		pulselength = 1;
		fg_biasn = 4;
		fg_bias = 8;	
		// prepare ramtest
	

		log(Logger::INFO)<< "starting...." << endl;

		//~ for(fg_num=0; fg_num<4; fg_num+=1){
		//~ log(Logger::INFO)<<"tmsm_fgfunctional::seting HICANNversion to 1!!!";
				//~ fc[fg_num]->set_version(1);
		//~ }
	
		setupFg();
		uint regSlave=0xffff;
///Writing incremental values into floating gates...
		log(Logger::INFO)<<"tmsm_fg::programming to incremental. ";
		for(fg_num=0; fg_num<4; fg_num+=1){
			for(int i=0; i<65; i++)
				fc[fg_num]->write_ram(0,i,(2*i)%maxval,(2*i+1)%maxval); //limit values to maxval to save simulation time
		}
		for(fg_num=0; fg_num<4; fg_num+=1){
			log(Logger::DEBUG2)<<"programing row " << endl;
			// start state machine
			fc[fg_num]->program_cells(6,0,1);
		}
//waiting for floating gates to finish
		for(fg_num=0; fg_num<4; fg_num+=1){
			cout<<"Waiting for fg "<<fg_num;
			regSlave = fc[fg_num]->get_regslave();	
			while(regSlave & 0x100){
				regSlave = fc[fg_num]->get_regslave();	
				cout<<"."<<flush;
			}
			cout <<endl;
			if(regSlave & 0x200){
				cout<<"ERROR: Value of cell "<<(regSlave & 0xff) <<" not reached and should... aborting.";
				return 0;
			}
		}
		cout<<endl;


//Programming randomvalues

		srand(randomseed);
		log(Logger::INFO)<<"tmsm_fg::programming to random. ";
		for(fg_num=0; fg_num<4; fg_num+=1){
			for(int i=0; i<65; i++)
				fc[fg_num]->write_ram(0,i,(rand())%maxval,(rand())%maxval); //limit values to maxval to save simulation time
		}

		//programm floating gates down first.
		for(fg_num=0; fg_num<4; fg_num+=1){
			while(fc[fg_num]->isBusy());
			cout<<"."<<flush;
			log(Logger::DEBUG2)<<"programing row 4 down" << endl;
			 
			// start state machine
			fc[fg_num]->program_cells(4,0,0);
		}
		
        //waiting for floating gates to finish
		for(fg_num=0; fg_num<4; fg_num+=1){
			cout<<"Waiting for fg "<<fg_num;
			regSlave = fc[fg_num]->get_regslave();	
			while(regSlave & 0x100){
				regSlave = fc[fg_num]->get_regslave();	
				cout<<"."<<flush;
			}
			cout <<endl;
			if(regSlave & 0x200){
				cout<<"ERROR: Value of cell "<<(regSlave & 0xff) <<" not reached and should... aborting.";
				return 0;
			}
		}
		cout<<endl;
		for(fg_num=0; fg_num<4; fg_num+=1){
			while(fc[fg_num]->isBusy());
			cout<<"."<<flush;
			log(Logger::DEBUG2)<<"programing row 5 down" << endl;
			 
			// start state machine
			fc[fg_num]->program_cells(5,0,1);
		}
		
        //waiting for floating gates to finish
		for(fg_num=0; fg_num<4; fg_num+=1){
			cout<<"Waiting for fg "<<fg_num;
			regSlave = fc[fg_num]->get_regslave();	
			while(regSlave & 0x100){
				regSlave = fc[fg_num]->get_regslave();	
				cout<<"."<<flush;
			}
			cout <<endl;
			if(regSlave & 0x200){
				cout<<"ERROR: Value of cell "<<(regSlave & 0xff) <<" not reached and should... aborting.";
				return 0;
			}
		}
		cout<<endl;
		
//Programming Zeros

		log(Logger::INFO)<<"tmsm_fg::programming to incremental. ";
		for(fg_num=0; fg_num<4; fg_num+=1){
			for(int i=0; i<65; i++)
				fc[fg_num]->write_ram(0,i,0,0); //limit values to 16 to save simulation time
		}
		for(fg_num=0; fg_num<4; fg_num+=1){
			log(Logger::DEBUG2)<<"programing row " << endl;
			// start state machine
			fc[fg_num]->program_cells(6,0,0);
		}
//waiting for floating gates to finish
		for(fg_num=0; fg_num<4; fg_num+=1){
			cout<<"Waiting for fg "<<fg_num;
			regSlave = fc[fg_num]->get_regslave();	
			while(regSlave & 0x100){
				regSlave = fc[fg_num]->get_regslave();	
				cout<<"."<<flush;
			}
			cout <<endl;
			if(regSlave & 0x200){
				cout<<"ERROR: Value of cell "<<(regSlave & 0xff) <<" not reached and should... aborting.";
				return 0;
			}
		}
		cout<<endl;
    

//Programming Random with unreachable values 
		maxcycle=4;
		setupFg();

		srand(randomseed+1);
		log(Logger::INFO)<<"tmsm_fg::programming to random. ";
		for(fg_num=0; fg_num<4; fg_num+=1){
			for(int i=0; i<65; i++)
				fc[fg_num]->write_ram(0,i,(rand())%maxval,(rand())%maxval); //limit values to 16 to save simulation time
		}
		//programm up
		for(fg_num=0; fg_num<4; fg_num+=1){
			while(fc[fg_num]->isBusy());
			cout<<"."<<flush;
			log(Logger::DEBUG2)<<"programing row 5 down" << endl;
			 
			// start state machine
			fc[fg_num]->program_cells(5,0,1);
		}
		
        //waiting for floating gates to finish
		for(fg_num=0; fg_num<4; fg_num+=1){
			cout<<"Waiting for fg "<<fg_num;
			regSlave = fc[fg_num]->get_regslave();	
			while(regSlave & 0x100){
				regSlave = fc[fg_num]->get_regslave();	
				cout<<"."<<flush;
			}
			cout <<endl;
			if(!(regSlave & 0x200)){
				cout<<"ERROR: Unreached values expected, but not found... aborting.";
				return 0;
			}
			
			cout<<endl;
			cout<<"Cells with unreached values:"<<endl;
			while((regSlave & 0xff)<129 && (regSlave & 0x200)){
				cout<<(regSlave & 0xff)<<endl;
				fc[fg_num]->get_next_false();
				regSlave = fc[fg_num]->get_regslave();	
			}
	
		}
			
			
		
		//programm up
		for(fg_num=0; fg_num<4; fg_num+=1){
			while(fc[fg_num]->isBusy());
			cout<<"."<<flush;
			log(Logger::DEBUG2)<<"programing row 5 down(give it another try)" << endl;
			 
			// start state machine
			fc[fg_num]->program_cells(5,0,1);
		}
		
        //waiting for floating gates to finish
		for(fg_num=0; fg_num<4; fg_num+=1){
			cout<<"Waiting for fg "<<fg_num;
			regSlave = fc[fg_num]->get_regslave();	
			while(regSlave & 0x100){
				regSlave = fc[fg_num]->get_regslave();	
				cout<<"."<<flush;
			}
			cout <<endl;
			if(regSlave & 0x200){
				cout<<"ERROR: Value of cell "<<(regSlave & 0xff) <<" not reached and should now... aborting.";
				return 0;
			}
		}
		cout<<endl;
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(9,9);
		// try FG controller start
	
		
	
		for(fg_num=0; fg_num<4;fg_num+=1){

			fc[fg_num]->readout_cell(1,0);
		
		}
		log(Logger::DEBUG1)<< "ok" << endl;
	
		return 1;
	};
};


class LteeTmSmFgFunctional : public Listee<Testmode>{
public:
        LteeTmSmFgFunctional() : Listee<Testmode>(string("tmsm_fgfunctional"), string("Testing basic functionality of floating gate controller")){};
        Testmode * create(){return new TmSmFgFunctional();};
};

LteeTmSmFgFunctional ListeeTmSmFgFunctional;

