/* -------------------------------------------------------------------------
	
	CONFIDENTIAL - FOR USE WITHIN THE FACETS PROJECT ONLY
	UNLESS STATED OTHERWISE BY AUTHOR

	Andreas Grübl
	Vision/ASIC-LAB, KIP, University of Heidelberg
	
	September 2010

	
	s2c_jtagphys_2fpga.h

	PURPOSE
		implements packet access to FPGA
		via jtag interface from software.
		uses universal Stage2Comm commands

	FUNCIONAL

 ----------------------------------------------------------------------- */

#ifndef _S2C_JtagPhys2Fpga_H
#define _S2C_JtagPhys2Fpga_H

#include "s2comm.h"
#include "s2ctrl.h"

namespace facets {

class S2C_JtagPhys2Fpga : public Stage2Comm
{
private:
	int debug_level;
	virtual std::string ClassName() { return "S2C_JtagPhys2Fpga"; };
	virtual std::ostream & dbg() { return std::cout << ClassName() << ": "; }
	virtual std::ostream & dbg(int level) { if (this->debug_level>=level) return dbg(); else return NULLOS; }

	bool fpga_dnc_inited;
	uint trans_error_count;
	unsigned char l2_link_state;
	bool test_transmissions_done;

public:
	size_t trans_count;

	S2C_JtagPhys2Fpga(CommAccess const & access, myjtag_full* j, bool on_reticle=false, bool use_k7fpga=false);
	virtual ~S2C_JtagPhys2Fpga();

	// implementation of base classe's methods
	virtual Commstate Send(IData data=emptydata, uint del=0);
	virtual Commstate Receive(IData &data); 

	// initialize links between fpga, dnc, and hicann(s).
	// BV+JP: introduced new parameter 'return_on_error' required for use from HALBE and the synchronization of the systime counters
	//      If true:
	//         In the case of any error occuring during the initialization of the FPGA-DNC-HICANN link or the test transmission, directly return Commstate::initfailed.
	//      if false:
	//         Old behavior: in the case of an error, the initialization process is repeated up to 10 times. However, this includes a hicann reset, which destroys
	//         the sync of the systime counters on the HICANN and the FPGA.
	virtual Commstate Init(std::bitset<8> hicann, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false); //configures HICANN for jtag_multi communication
	virtual Commstate Init(int hicann_jtag_nr, bool silent=false, bool force_highspeed_init=false, bool return_on_error=false); //configures HICANN for jtag_multi communication
	
	// ----------------------------------
	// DNC communication related initialization functions
	// ----------------------------------
	
#define fpga_master   1
#define DATA_TX_C     1000
	
	void dnc_shutdown();

	void hicann_shutdown();

	// deprecated: will throw runtime error
	int fpga_dnc_init(unsigned char hicanns=0, bool force=false, int dnc_del=-1, int hicann_del=-1, bool silent=false);
	// THIS IS THE VERSION TO BE USED:
	std::bitset<9> fpga_dnc_init(bool silent);
	
	unsigned int test_transmission(unsigned int hicann, unsigned int packet_count);
	
	unsigned int get_transmission_errors();

	void printHighspeedSettings();

	/// trigger start of systime in FPGA.
	/// if enable is false, systime counter gets primed
	/// if listen_global is true, the FPGA reacts to the external trigger pin only,
	void trigger_systime(unsigned int const fpga_ip, bool const enable=true, bool const listen_global=false, uint const sysstart_port=1800);

	/// trigger start of experiment in FPGA.
	/// if enable is false, experiment start gets primed
	/// if listen_global is true, the FPGA reacts to the external trigger pin only,
	void trigger_experiment(unsigned int const fpga_ip, bool const enable=true, bool const listen_global=false, uint const sysstart_port=1800);
	
	unsigned int initAllConnections(bool silent);
	unsigned int initAllConnections(bool silent, std::vector<unsigned char> delays);

	bool fpga_hicann_init(std::bitset<8> hicann);

	uint16_t hicann_ack_timeout[2], hicann_resend_timeout[2]; // tag 0, tag 1
	bool disable_multiple_test_transmissions;
};

} // facets

#endif
