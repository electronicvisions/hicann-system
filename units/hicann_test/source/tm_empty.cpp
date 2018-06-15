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


//empty test mode for doing tests on DNCIF with verification_control.

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
//#include "hicann_ctrl.h"
#include "testmode.h" //Testmode and Lister classes

//#include "ctrlmod.h"
//only if needed
//#include "synapse_control.h" //synapse control class
//#include "l1switch_control.h" //layer 1 switch control class

using namespace facets;

class TmEmpty : public Testmode
{
	public:

		bool test()
		{
			return true;
		}
	
};


class LteeTmEmpty : public Listee<Testmode>{
public:
	LteeTmEmpty() : Listee<Testmode>(std::string("tm_empty"),std::string("empty testmode for tests on DNCIF")){};
	Testmode * create(){return new TmEmpty();};
};
LteeTmEmpty ListeeTmEmpty;

