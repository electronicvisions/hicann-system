// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   ctrlmod.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Mon Jan 12 13:09:24 2009                
// Last Change     :   Mon Jan 12 09    
// by              :   agruebl        
//------------------------------------------------------------------------

#include <sstream>

#include "common.h" // library includes

#include "ctrlmod.h"

#include "logger.h"      // global logging class

using namespace facets;
using namespace std;

// *** class CtrlModule ***

/// This is probably due to blocking of the PbMem port. The PbMem FIFO buffer
/// space is 50 commands. Estimating one command at 10 FPGA clock cycles it
/// can happen, that only one word per buffer gets transported and if this is
/// too slow, the PbMem drops packets.
static size_t const default_command_delay = 550;

CtrlModule::CtrlModule(Stage2Ctrl* c, uint ta, ci_addr_t sa, ci_addr_t ma)
	: s2c(c),
	  _tagid(ta),
	  _startaddr(sa),
	  _maxaddr(ma),
	  log(Logger::instance("hicann-system.CtrlModule", Logger::INFO,"")),
	  del(default_command_delay)
{}

CtrlModule::~CtrlModule(){}


void CtrlModule::checkAddr(ci_addr_t a, std::string prefix) {
	if(a>maxaddr()) {
		std::stringstream emsg;
		emsg << "***ERROR: " << prefix << ": address 0x" << hex << a << " out of range!";
		log(Logger::ERROR) << emsg.str() << Logger::flush;
		throw std::range_error(emsg.str());
	}
}

int CtrlModule::write_cmd(ci_addr_t a, ci_data_t d, uint del){
	checkAddr(a, __PRETTY_FUNCTION__);
	senddata.write = 1;
	senddata.addr  = a+startaddr();
	senddata.data  = d;
	return s2c->issueCommand(&senddata, _tagid, del);
}

int CtrlModule::read_cmd(ci_addr_t a, uint del){
	checkAddr(a, __PRETTY_FUNCTION__);
	senddata.write = 0;
	senddata.addr  = a+startaddr();
	senddata.data  = 0x1 << 31; // encode read command as documented in "Ansteuerung des HICANN chips via JTAG/DNC" by Stefan Philipp
	return s2c->issueCommand(&senddata, _tagid, del);
}

int CtrlModule::get_data(ci_payload *pckt){
	return s2c->recvData(pckt, _tagid);
}

int CtrlModule::get_data(ci_addr_t &addr, ci_data_t &data){
// THIS FUNCTION IS ONLY FOR COMPABILITY -> should be removed!
	ci_payload pckt;
	int result = s2c->recvData(&pckt, _tagid);
	addr=pckt.addr;
	data=pckt.data;
	return result;
}

// *** END class CtrlModule END ***
