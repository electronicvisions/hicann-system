// Company         :   kip
// Author          :   Marc-Olivier Schwartz       
// E-Mail          :   marcolivier.schwartz@kip.uni-heidelberg.de
//                    			
// Filename        :   tmms_neurontimeout.cpp
// Project Name    :   BrainScaleS          
//                    			
// Create Date     :   1/2011
//------------------------------------------------------------------------

// Includes

#include "s2comm.h"
#include "s2ctrl.h"
#include <bitset>
#include <fstream>
#include <ctime>

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

#include "dnc_control.h"
#include "fpga_control.h"
#include "s2c_jtagphys_2fpga.h"

#include "neuron_control.h"   //neuron / "analog" SPL1 control class
#include "spl1_control.h"     //spl1 control class

using namespace std;
using namespace facets;

class TMNeuronTimeOut : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmsm_neurontimeout"; return ss.str();}
public:

	// Declarations

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fgc[4];//array of floatinggate controlers
	NeuronBuilderControl *nbc;

	DNCControl  *dc;
	FPGAControl *fc;
  	NeuronControl *nc;	
  	SPL1Control *spc;
	
	fg_loc loc;
	string locs;
	int cntlr;
	
	// Constants for FG control

	static const int TAG             = 0;

	static const int REG_TEST_NUM    = 10;

 	static const int MEM_SIZE        =128;//(1<<(nspl1_numa)) -1;
	static const int MEM_WIDTH       = 20;
	static const int MEM_BASE       = 0;

	static const int MEM_TEST_NUM    = MEM_SIZE*20;


	// Helper functions

	uint nbdata(uint fireb, uint firet, uint connect, uint ntop, uint nbot, uint spl1){
		return ((4* fireb + 2*firet + connect)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;

	}
	
	// Overloaded function from ramtestintf

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
	
	// Test function

	bool test() 
	{
	time_t tstart,tend;
	// Init
		

		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::ERROR)<<"ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
    		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];

	 	 log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;


		nbc = &hc -> getNBC();
		spc = &hc->getSPL1Control();
		nc  = &hc->getNC();
		conf->getHicann(0);
		fgc[0]=&hc->getFC_tl();
		fgc[1]=&hc->getFC_tr();
		fgc[2]=&hc->getFC_bl();
		fgc[3]=&hc->getFC_br();
		
		// use DNC
		dc = (DNCControl*) chip[FPGA_COUNT];
		
		// use FPGA
		fc = (FPGAControl*) chip[0];
	
		//initialize nbcrams with zersos
		nbc->initzeros();

		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to neuron output line 0 and 1
		nbc->setOutputMux(1,4);
		
	// Programm floating gates
		tstart = time(0);
		log(Logger::DEBUG1)<<"tmsm_neuronout::Setting fg control parameters";
		uint fg_num;
		for(fg_num = 0; fg_num < 4; fg_num += 1)
		{
			fgc[fg_num]->set_maxcycle(255);		
			fgc[fg_num]->set_currentwritetime(1);
			fgc[fg_num]->set_voltagewritetime(4);
			fgc[fg_num]->set_readtime(63);
			fgc[fg_num]->set_acceleratorstep(15);
			fgc[fg_num]->set_pulselength(15);
			fgc[fg_num]->set_fg_biasn(15);
			fgc[fg_num]->set_fg_bias(15);
			fgc[fg_num]->write_biasingreg();
			fgc[fg_num]->write_operationreg();
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
				fgc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
				while(fgc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fgc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
			
}
			bank =( bank +1)%2; //bank is 1 or 0
		}
		log(Logger::INFO)<<"tmsm_neuronout::writing voltages ...";
		for( uint lineNumber = 0; lineNumber < 24; lineNumber += 2)
		{
			log(Logger::DEBUG2)<<"Line number: "<<lineNumber;
			for( fg_num = 0; fg_num < 4; fg_num += 1)
			{	
	
				log(Logger::DEBUG2)<<"Fg_number"<<fg_num;
				fgc[fg_num]->initArray(bank, conf->getFgLine((fg_loc)fg_num, lineNumber));//use two banks -> values can be written although fg is still working
				while(fgc[fg_num]->isBusy());//wait until fg_is not busy anymore - normaly one should do other stuff than just polling busy in this time
				fgc[fg_num]->program_cells(lineNumber,bank,1); //programm down first
			}
			bank =( bank +1)%2; //bank is 1 or 0
		}
		for( fg_num = 0; fg_num < 4; fg_num += 2)
		{
			while(fgc[fg_num]->isBusy());
			fgc[fg_num]->initSquare(0,200);
			fgc[fg_num]->set_pulselength(15);
			fgc[fg_num]->write_biasingreg();
			fgc[fg_num]->stimulateNeuronsContinuous(0);
		}
		nbc->setNeuronBigCap(1,1);

		nbc->setNeuronGl(0,0,1,1);
		nbc->setNeuronGladapt(0,0,0,0);
		nbc->setNeuronReset(1,1);
		log(Logger::DEBUG1)<<"Activating neurons 0 an 1 of upper block and activating their outputs"<<endl; 
		write(0,0x8e8e);

		
	// Configure interfaces
	
	// Neuron builder control
		
		
		uint value = nbdata(1,1,0,0x8e,0x8e,0x00);
		nbc->write_data(0,value);//neuronbuilder enable top and connect.
		nbc->write_data(1,value);//neuronbuilder enable bot 

		value = nbdata(1,1,0,0x8e,0x8e,0x00);
		nbc->write_data(2,value);//neuronbuilder enable top and connect.
		nbc->write_data(3,value);//neuronbuilder enable bot and neuronconfig.

		nbc->setSpl1Reset(0);
		nbc->setSpl1Reset(1);
		nbc->configNeuronsGbl();
		

	// Neuron control
		nc->write_data(0x0 , 0x0000); // set all mergers to be static multiplexers
		
		nc->write_data(0x1 , 0x5100); // [14:8] select identity assignment for anncore->output regs
                  // [ 7:0] select anncore outputs (not background generators)
		  
		nc->write_data(0x2 , 0x0000); // "slow" functionality not needed when sending to DNC
		
		nc->write_data(0x3 , 0x0000); // [15:8] enable all SPL1 outputs to DNC interface
                  // [ 7:0] disable all loopback tristate drivers 

	// SPL1 Control		
	spc->write_cfg(0x100ff); // TUD: configure timgetFpgaRxDataestamp, direction, enable of l1
			
	// DNC	
		// set DNC to ignore time stamps and directly forward l2 pulses
		dc->setTimeStampCtrl(0x1); // TUD: Disable DNC timestamp features

		// set transmission directions in DNC (for HICANN 0 only)
		dc->setDirection(0x00);   // TUD: direction of the l1 busses (copy)
		
	// Read data
		
		uint64_t nrn_pulses[200];
		int nrn_times[200];
		int i = 0;
		int j = 0;
		int l = 0;
		int pl_length = 0;
		uint64_t jtag_recpulse = 0;
		std::bitset<64> pulse_bin;
		std::bitset<15> timestamp;
		int add = 49;

		jtag->FPGA_enable_tracefifo();
		jtag->FPGA_disable_tracefifo();
		
		
		ofstream myfile;
		myfile.open ("spikes.txt");		

		while((!jtag->FPGA_empty_pulsefifo()) && (i < 200)) {
			i++;
			jtag->FPGA_get_trace_fifo(jtag_recpulse);
			//cout << "Pulse received 0x" << hex << jtag_recpulse << endl;
			pl_length = (int)(1+log10(jtag_recpulse));
			//cout << "Pulse length : " << dec << pl_length << endl;

			pulse_bin = std::bitset<64>(jtag_recpulse);
			for (l = 0; l < (64-add); l++)
			{
				timestamp[l] = 	pulse_bin[l];
			}
			cout << dec << timestamp.to_ulong() << "," << endl;
			myfile << timestamp.to_ulong() << endl;
		
			//cout << "Pulse received bin : " << pulse_bin << endl;
			//cout << "Event number : " << dec << i << endl;

			//if (pl_length == 19) {
			//	nrn_pulses[j] = jtag_recpulse;
			//	nrn_times[j] = timestamp.to_ulong();			
			//	j++;
				//cout << "Spike !" << endl;
			//}
			//else {cout << "o" << endl;}
		}
		printf("Trace FIFO Empty\n");

		myfile.close();

		tend = time(0);
		cout << "It took " << (tend-tstart) << " s" << endl;
		return 1;
	};
};


class LteeTMNeuronTimeOut : public Listee<Testmode>{
public:
	LteeTMNeuronTimeOut() : Listee<Testmode>(string("tmms_neurontimeout"), string("Send neuron timestamps")){};
	Testmode * create(){return new TMNeuronTimeOut();};
};

LteeTMNeuronTimeOut ListeeTMNeuronTimeOut;
