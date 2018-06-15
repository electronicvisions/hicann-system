// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   l1switch_control.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jun 24 08
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------

#include "s2comm.h"
#include "linklayer.h"

#include "s2ctrl.h"
#include "ctrlmod.h"
#include "hicann_ctrl.h"
#include "spl1_control.h" // Access to SPL1 configuration


using namespace std;

namespace facets {

SPL1Control::SPL1Control(Stage2Ctrl* c, uint ta, uint sa, uint ma):CtrlModule(c,ta,sa,ma)
{}

void SPL1Control::write_cfg(uint spl1_ctrl)
{
	log(Logger::DEBUG0)<<"SPL1Control::write_cfg " << std::hex << spl1_ctrl << " delay: " << del;
	write_cmd(0, spl1_ctrl, del); // write to local address 0
}

void SPL1Control::write_reset()
{
	log(Logger::DEBUG0)<<"SPL1Control::write_reset delay: " << del;
	write_cmd(0, 0x80000000, del); // write to local address 0
}

void SPL1Control::read_counter(uint num)
{
	log(Logger::DEBUG1)<<"SPL1Control::read_counter " << num<< " delay: " << del;
	read_cmd(num, del); // send read request to address num
}

#ifndef WITHOUT_ARQ

void SPL1Control::get_read_counter(vector<uint>& count_val,uint num)
{
	count_val.clear();
	
	ci_payload tmp;
	for (unsigned int nval=0;nval<num;++nval){
		if(get_data(&tmp) == 1){
			count_val.push_back( (unsigned int)(tmp.data) );
		} else {
			log(Logger::WARNING)<<"SPL1Control::get_read_counter no correct data available for counter: "<<nval;
		}
	}

}

#endif // WITHOUT_ARQ

} // namespace facets
