

#ifndef _DNC_CTRL_CPP_
#define _DNC_CTRL_CPP_


#include "dnc_control.h"
#include "s2_types.h"

using namespace std;

namespace facets
{


#ifndef SYSTIME_PERIOD_NS
	#define SYSTIME_PERIOD_NS 4.0
#endif


// initialize Stage2Comm with 0 linklayer instances since there are none in the DNC ;-)
DNCControl::DNCControl(Stage2Comm* comm_class, unsigned int set_dnc_id) :
	Stage2Ctrl(comm_class,set_dnc_id)
	,log(Logger::instance("hicann-system.DNCControl", Logger::INFO,""))
	,dnc_id(set_dnc_id)
	, directions()
	, curr_hicann(0)
	, curr_packet(l2_packet_t::DNC_ROUTING,set_dnc_id,0)
{}

void DNCControl::setTimeStampCtrl(unsigned char tsc)
{
	if (GetCommObj()->jtag->is_kintex)
		return;
	log(Logger::DEBUG0) << "DNCControl::setTimeStampCtrl enable:" << tsc;
	GetCommObj()->jtag->DNC_set_timestamp_ctrl(tsc);
}

void DNCControl::setDirection(uint64 dirs)
{
	if (GetCommObj()->jtag->is_kintex)
		return;
	log(Logger::DEBUG0) << "DNCControl::setDirection: " << std::hex << dirs;
	GetCommObj()->jtag->DNC_set_l1direction(dirs);
}
		
	
}  // end of namespace facets

#endif //_DNC_CTRL_CPP_

