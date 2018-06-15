// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   s2ctrl.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Fri Jun 20 08:12:24 2008                
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------


#include "common.h" // library includes

#include "s2comm.h"
#include "s2ctrl.h"

using namespace facets;

// *** class Stage2Ctrl ***

Stage2Ctrl::Stage2Ctrl(Stage2Comm *c, uint addr)
{
	comm   = c;
	_addr = addr;
}

Stage2Ctrl::~Stage2Ctrl(){}


// *** END class Stage2Ctrl END ***

