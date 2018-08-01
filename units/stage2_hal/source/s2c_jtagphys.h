/* -------------------------------------------------------------------------
	
	CONFIDENTIAL - FOR USE WITHIN THE FACETS PROJECT ONLY
	UNLESS STATED OTHERWISE BY AUTHOR

	Andreas Grübl
	Vision/ASIC-LAB, KIP, University of Heidelberg
	
	June 2010

	
	s2c_jtagphys_full.h

	PURPOSE
		implements packet access to FPGA
		via jtag interface from software.
		uses universal Stage2Comm commands

	FUNCIONAL

 ----------------------------------------------------------------------- */

#ifndef _S2C_JtagPhys_H
#define _S2C_JtagPhys_H

#include "s2comm.h"
#include "s2ctrl.h"

namespace facets {

class S2C_JtagPhys : public Stage2Comm
{
private:
	int debug_level;
	virtual std::string ClassName() { return "S2C_JtagPhys"; };
	virtual std::ostream & dbg() { return std::cout << ClassName() << ": "; }
	virtual std::ostream & dbg(int level) { if (this->debug_level>=level) return dbg(); else return NULLOS; }

public:
	S2C_JtagPhys(CommAccess const & access, myjtag_full* j, bool on_reticle=false, bool use_k7fpga=false);
	virtual ~S2C_JtagPhys();

	// implementation of base classe's methods
	virtual Commstate Send(IData data=emptydata, uint del=0);
	virtual Commstate Receive(IData &data); 

	virtual Commstate Init(std::bitset<8> hicann, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false); //configures HICANN for jtag_multi communication
	virtual Commstate Init(int hicann_jtag_nr, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false); //configures HICANN for jtag_multi communication
	Commstate reset_hicann_arq(std::bitset<8> hicann, bool release_reset = false);
};

} // facets

#endif
