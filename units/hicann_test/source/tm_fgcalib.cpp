// Company         :   kip
// Author          :   Sebastian Millner            
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_fgcalib.cpp
// Project Name    :   p_facets
// Subproject Name :   s_system            
//                    			
// Create Date     :   6/2010
//------------------------------------------------------------------------


/*
 * This testmode measures the current measurement resistance of the floating 
 * gate controler. As only two arrays are connected to a single output, this 
 * mode has to be executed two times with the different analog outputs 
 * connected to multimeter. Before calibration, the currentsource from 
 * Dresden has to be calibrated. Measurement current is 1/25 of output current 
 * of current source.
 */

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
#include <fstream>

using namespace facets;

class TmFgcalib : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_fgcalib"; return ss.str();}
public:

	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc;
	NeuronBuilderControl *nbc;

	fg_loc loc;
	string locs;

	uint bank ;
	int cntlr;

	fstream file;
	time_t timestamp;
	tm *now;
	stringstream ss;
	string filename;
	
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


	static const int TAG             = 0;

	static const int NUM_MEAS        = 10; //number of measurements done


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::ERROR)<<" object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	log(Logger::ERROR)<<"object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		ll = hc->tags[TAG];
		ll->SetDebugLevel(0); // no ARQ deep debugging
		nbc = hc -> getNBC();
		MeasKeithley keithley;		

		keithley.SetDebugLevel(0);
	
		keithley.connect("/dev/usbtmc1");
	
		debug_level = 4;
		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		timestamp = time(0);
		now = localtime(&timestamp);
		ss <<"../results/fg_calib/"<< label <<"-"<<now->tm_year-100<<"-"<<setw(2)<<setfill('0')<<now->tm_mon+1<<"_"<<now->tm_hour<<":"<<now->tm_min<<".dat";
		filename = ss.str();
		file.open(filename.c_str(), fstream::out);
		
		log(Logger::INFO)<< "starting...." << endl;
	
		comm->Clear();
	

		log(Logger::INFO)<< "disabling dnc-interface";
		jtag->set_jtag_instr(0x5);
		int cmd = 0x0;
		jtag->set_jtag_data<int>(cmd,'1');

		log(Logger::INFO)<< "Asserting ARQ reset via JTAG..." << endl;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
                jtag->HICANN_arq_write_ctrl(0x0, 0x0);

		log(Logger::INFO)<< "Setting timeout values via JTAG..." << endl;
                jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
                jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1
     
		//activate analog output 
		nbc->setOutputEn(true,true);
		//set output to fg-array 0 and 2;
		
		nbc->setOutputMux(3,3);
	
		double measurements[NUM_MEAS];
		double mean;
		double dev;
	

		for(cntlr=0; cntlr<4;cntlr+=1)
		{
			select(cntlr);
			nbc->setOutputMux(3+((cntlr+1)%2)*6,3+((cntlr+1)%2)*6);
			log(Logger::INFO)<< "Activating floating-gate block " << locs << " for calibration." << endl;
			file<<"Controler " << locs <<":\n";
			fc->set_calib(1);
			fc->write_biasingreg();
			fc->write_operationreg();
			fc->readout_cell(1,1);


			mean = 0;

			for( int i = 0; i < NUM_MEAS; i += 1)
			{
				measurements[i]=keithley.getVoltage();
				mean+=measurements[i];
			}
			
			mean = mean / NUM_MEAS;
			dev = 0;
			
			for( int i = 0; i < NUM_MEAS; i += 1)
			{
				dev = (mean - measurements[i])*(mean - measurements[i]);
			}
			
			dev = sqrt(dev/NUM_MEAS);
			file<<mean<<"\t"<<dev<<endl;
			log(Logger::INFO)<<"Measured "<<mean<<" pm "<<dev<<".";

			fc->set_calib(0);
			fc->write_biasingreg();
			fc->write_operationreg();



		}



		log(Logger::INFO)<< "ok" << endl;
		file.close();
	
		return 1;
	};
};


class LteeTmFgcalib : public Listee<Testmode>{
public:
	LteeTmFgcalib() : Listee<Testmode>(string("tm_fgcalib"), string("Set floating gates into calibration mode and set fg_out to ouptput amplifiers to measure current programming resistor")){};
	Testmode * create(){return new TmFgcalib();};
};

LteeTmFgcalib ListeeTmFgcalib;

