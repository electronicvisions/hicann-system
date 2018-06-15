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

//binary output in stream
class bino
{
public:
	bino(uint v, uint s=32):val(v),size(s){};
	uint val,size;
	friend std::ostream & operator <<(std::ostream &os, class bino b)
	{
		for(int i=b.size-1 ; i>=0 ; i--)
			if( b.val & (1<<i) ) os<<"1"; else os<<"0";
		return os;
	}
};


class TmFg : public Testmode, public RamTestIntfUint {
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
		fc[0]=&hc->getFC_tl();
		fc[1]=&hc->getFC_tr();	
		fc[2]=&hc->getFC_bl();
		fc[3]=&hc->getFC_br();
	

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
		currentwritetime = 63;
		voltagewritetime = 63;
		readtime = 63;
		acceleratorstep = 1;
		pulselength = 8;
		fg_biasn = 4;
		fg_bias = 8;	
		// prepare ramtest
	

		log(Logger::INFO)<< "starting...." << endl;
	
		setupFg();

		char choice;
		bool cont = 1;
		do{
			displaySetup();	
			//menu:
			cout<<"r: Test memory"<<endl;
			cout<<"b: set bias"<<endl;
			cout<<"z: Program all Floatinggates down to zero"<<endl;
			cout<<"m: Program all Floating to maximum"<<endl;
			cout<<"c: Program xml-file on floating gates"<<endl;
			cout<<"R: Readout"<<endl;
			cout<<"x: End program"<<endl;
			cin>>choice;

			switch(choice){
			case 'x':{
				cont = 0;
				break;
			} 
			case 'b':{
				cout<<"fg_biasn=";
				cin>>fg_biasn;
				cout<<endl<<"fg_bias=";
				cin>>fg_bias;
				setupFg();
				break;
			}
			case 'r':{//memory check
				log(Logger::INFO)<<"tmsm_fg::memory check";

				int mem_size = FG_pkg::fg_lines/2 + FG_pkg::fg_lines % 2;
				uint error = 0;
				ci_data_t rcvdata;
				ci_addr_t rcvaddr;
				uint rmask = 0xfffff; //20 Bits
				uint tdata[mem_size];
				for(fg_num=0; fg_num<4; fg_num+=1){
					log(Logger::INFO)<<"Controler "<<fg_num;
					srand(randomseed+fg_num);
				
					for(int i=0;i<mem_size;i++)
						{
							tdata[i]=rand() & rmask;
							fc[fg_num]->write_cmd(i,tdata[i],0);
						}
					//tdata[5]=rand() & rmask;//produce error

					for(int i=0;i<mem_size;i++)
						{
							fc[fg_num]->read_cmd(i,0);
							fc[fg_num]->get_data(rcvaddr,rcvdata);
							if(rcvdata != tdata[i]){
								error++;
								log(Logger::ERROR) << hex << "row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<	bino(tdata[i],16);
							} else {
								log(Logger::INFO) << hex << "row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<	bino(tdata[i],16);
							}
						}
				}
				if(error!=0){
					log(Logger::ERROR)<<"Found "<<error<<" errors during ramtest";
					return 0;
				}
				break;
			}
			case 'm':{
				
				log(Logger::INFO)<<"tmsm_fg::programming to maximum. ";
				for(fg_num=0; fg_num<4; fg_num+=1){
					fc[fg_num]->initmax(0);
				}
				for(int i=0; i<24; i+=1){
					for(fg_num=0; fg_num<4; fg_num+=1){
						while(fc[fg_num]->isBusy());
						cout<<"."<<flush;
						log(Logger::DEBUG2)<<"programing row " << i << endl;
						 
						// start state machine
						fc[fg_num]->program_cells(i,0,1);
					}
				}
				cout<<endl;
				break;

			}
			case 'z':{
				
				log(Logger::INFO)<<"tmsm_fg::programming down to zero. ";
				for(fg_num=0; fg_num<4; fg_num+=1){
					fc[fg_num]->initzeros(0);
				}
				for(int i=0; i<24; i+=1){
					for(fg_num=0; fg_num<4; fg_num+=1){
						while(fc[fg_num]->isBusy());
						cout<<"."<<flush;
						log(Logger::DEBUG2)<<"programing row " << i << endl;
						 
						// start state machine
						fc[fg_num]->program_cells(i,0,0);
					}
				}
				cout<<endl;
				break;

			}
			case 'c':{	
				log(Logger::INFO)<<"tmsm_fg::programming xml-file on floating gate array. ";
				log(Logger::INFO)<<"tmsm_fg::writing currents ...";
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
				log(Logger::INFO)<<"tmsm_fg::writing voltages ...";
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
				break;
			}
			case 'R':{

					//activate analog output 
					nbc->setOutputEn(true,true);
					//set output to fg-array 0 and 2;
					nbc->setOutputMux(9,9);

					for(fg_num=0; fg_num<4;fg_num+=1){

						fc[fg_num]->readout_cell(1,10);
					
					}
				break;
				}
			} 

		} while (cont);
     
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		nbc->setOutputMux(9,9);
	
		
	
		for(fg_num=0; fg_num<4;fg_num+=1){

			fc[fg_num]->readout_cell(1,1);
		
		}
		nbc->setOutputEn(false,false);
		log(Logger::DEBUG1)<< "ok" << endl;
	
		return 1;
	};
};


class LteeTmFg : public Listee<Testmode>{
public:
	LteeTmFg() : Listee<Testmode>(string("tm_fg"), string("Testmode for interactive fg control")){};
	Testmode * create(){return new TmFg();};
};

LteeTmFg ListeeTmFg;

