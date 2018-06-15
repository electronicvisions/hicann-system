// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_example.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jun 24 08
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------


//example file for a custom testmode implementation

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" //Testmode and Lister classes

#include "ctrlmod.h"
//only if needed
#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class

using namespace facets;

class TmExample : public Testmode{

public:

	bool test() {
		bool result = true;
		
		HicannCtrl* hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];  // use HICANN number 0 (following FPGA and DNC
		                                                            // in address space) for this testmode
		
//		SynapseControl*  sc_top = hc->getSC_top();		// get pointer to synapse control class of HICANN 0
		L1SwitchControl* lc_cl = hc->getLC_cl();		// get pointer to left crossbar layer 1 switch control class of HICANN 0
	
//		sc->write_weight(0, 0, 5);
//		sc->write_weight(0, 2, 3);
//		sc->write_weight(0, 7, 9);
		
		uint rcfg, del;
		vector<bool> d;
		
		uint numruns = 30;
		uint mindelay = 10, maxdelay = 25;
//deprecated since loc is known to L1SwitchControl class!		switch_loc loc = CBL;   // !!! at the moment the only one supported by the software !!!
		
		srand(1);
		for(uint i=0; i<numruns; i++){
			rcfg    = rand() % (CB_NUMH/2); // these two parameters correspond to loc = CBL.
			del     = mindelay + (rand() % (maxdelay-mindelay));

			d.clear();
			for(uint j=0;j<(CB_NUMH/2);j++) {d.push_back(rcfg & 0x1); rcfg >>= 1;}

			dbg(0) << "TmExample: Write L1 Config for: ADDR " << i << ", CFG ";
			for(uint j=0;j<d.size();j++) dbg(0) << d[d.size()-1-j];
			dbg(0) << endl;
	
//			lc_cl->write_cfg(loc, i, d, del);
			lc_cl->write_cfg(i, d, del);
		}

		// flush buffer to chip in case the employed communication model (Stage2Comm child) should require this.
		hc->Flush();

		// read back with max. rate
//		for(uint i=0; i<numruns; i++) lc_cl->read_cfg(loc, i, 100);
		for(uint i=0; i<numruns; i++) lc_cl->read_cfg(i, 100);
		
		// flush buffer to chip in case the employed communication model (Stage2Comm child) should require this.
		hc->Flush();

		wait (5, SC_US);

		srand(1);
//deprecated since loc is known to L1SwitchControl class!		switch_loc tloc;
		uint taddr,tcfg;
		for(uint i=0;i<numruns;i++){
			// reproduce random numbers generated for test data...
			rcfg    = rand() % (CB_NUMH/2); // these two parameters correspond to loc = CBL.
			del     = mindelay + (rand() % (maxdelay-mindelay));

//			lc_cl->get_read_cfg(tloc, taddr, d);
			lc_cl->get_read_cfg(taddr, d);
			tcfg = 0;
			for(uint j=0;j<d.size();j++) tcfg += (d[j] << j);
			
			dbg(0) << "TmExample: Read back L1 Config for: ADDR " << taddr << "(" << i <<
																								 "), CFG " << tcfg << "(" << rcfg << ") ... ";
			if(rcfg == tcfg) dbg(0) << "OK :-)" << endl;   // taddr == 0 corresponds to AdrStatus = ci_ok in SV-code. (Status info important?)
			else             {dbg(0) << "FAIL :-(" << endl; result = false;}
		}


		dbg(0) << "TmExample: Finished, finally waiting additional 20 us..." << endl;
		wait (20, SC_US);

		return result;
	};
};


class LteeTmExample : public Listee<Testmode>{
public:
	LteeTmExample() : Listee<Testmode>(string("tm_example"),string("minimal example testmode accessing synapse ram")){};
	Testmode * create(){return new TmExample();};
};
LteeTmExample ListeeTmExample;

