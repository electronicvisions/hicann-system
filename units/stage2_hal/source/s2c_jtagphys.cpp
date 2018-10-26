/* -------------------------------------------------------------------------
	
	CONFIDENTIAL - FOR USE WITHIN THE FACETS PROJECT ONLY
	UNLESS STATED OTHERWISE BY AUTHOR

	Andreas Gr�bl
	Vision/ASIC-LAB, KIP, University of Heidelberg
	
	June 2010
	
	s2c_jtagphys_full.cpp - comments see .h file

 ----------------------------------------------------------------------- */

#include <pthread.h>

#include "common.h"
#include "s2c_jtagphys.h"

using namespace std;
using namespace facets;

S2C_JtagPhys::S2C_JtagPhys(CommAccess const & access, myjtag_full *j, bool on_reticle, bool use_k7fpga) :
	Stage2Comm(access, j, on_reticle, use_k7fpga)
{
	init_s2comm();
}

S2C_JtagPhys::~S2C_JtagPhys(){}


Stage2Comm::Commstate S2C_JtagPhys::Send(IData d, uint /*del*/)
{
	log(Logger::DEBUG1) << "S2C_JtagPhys: Send packet " << hex << d.cmd();

	jtag->set_hicann_pos(d.addr());
	
	jtag->HICANN_set_test(0xc);
	jtag->HICANN_set_tx_data(d.cmd().to_uint64());
	jtag->HICANN_start_cfg_pkg();
	jtag->HICANN_set_test(0x0);
	//cout << "S2C_JtagPhys: Sent data 0x" << hex << d.cmd().to_uint64() << endl;
	return ok;
}

Stage2Comm::Commstate S2C_JtagPhys::Receive(IData &d)
{
	jtag->set_hicann_pos(d.addr());
	
	uint64_t rxdata=0;
	jtag->HICANN_get_rx_data(rxdata);
	
	d.setCmd() = rxdata;
 	log(Logger::DEBUG1) << "S2C_JtagPhys: Received packet " << hex << d << ", rxdata: 0x" << rxdata;

	return ok;
}


S2C_JtagPhys::Commstate S2C_JtagPhys::Init(int hicann_jtag_nr, bool silent, bool force_highspeed_init, bool return_on_error){
	// ECM: old testmodes often forget to specify the hicann_nr -- if your code breaks here... check this!
	assert(hicann_jtag_nr >= 0 && "Init called with wrong hicann_nr");
	assert(hicann_jtag_nr < 8 && "Init called with wrong hicann_nr");
	std::bitset<8> hicann;
	hicann[jtag2dnc(hicann_jtag_nr)] = true;
	return S2C_JtagPhys::Init(hicann, silent, force_highspeed_init, return_on_error);
}

S2C_JtagPhys::Commstate S2C_JtagPhys::Init(
    std::bitset<8> hicann,
    std::bitset<8> /*highspeed_hicann*/,
    bool silent,
    bool force_highspeed_init,
    bool return_on_error)
{
	if (!silent) {
		log(Logger::INFO)
		    << "S2C_JtagPhys::Init: Called with highspeed_hicann parameter... ignoring." << flush;
	}
	return Init(hicann, silent, force_highspeed_init, return_on_error);
}

// initializes jtag_multi, but no high-speed. if "silent==true" => put no info on screen except errors to increase readability
// variable hicann contains DNC highspeed channel numbers
S2C_JtagPhys::Commstate S2C_JtagPhys::Init(std::bitset<8> hicann, bool silent, bool force_highspeed_init, bool /*return_on_error*/){
#ifndef HICANN_DESIGN
	S2C_JtagPhys::Commstate returnvalue = S2C_JtagPhys::ok;

	if(!silent) log(Logger::INFO) << "S2C_JtagPhys::Init: Initializing HICANNs " << hicann.to_string() << flush;
	if(hicann.none()){
		log(Logger::ERROR) << "S2C_JtagPhys::Init: MUST provide HICANNs to initialize!" << flush;
		exit(EXIT_FAILURE);
	}

	if (force_highspeed_init)
		log(Logger::WARNING) << "S2C_JtagPhys::Init: highspeed init requested but JTAG only." << flush;

	// disable FPGA ARQ:
	set_fpga_reset(jtag->get_ip(), false, false, false, false, true);

	//initialize JTAG read access
	Clear(); // re-sets jtag position...

	// Reset software ARQ
	for(uint i=0; i<link_layers.size(); i++)
		for(uint j=0; j<link_layers[i].size(); j++)
			link_layers[i][j].arq.Reset();

	for (size_t hicann_nr = 0; hicann_nr < hicann.size(); hicann_nr++) {
		if (!hicann[hicann_nr]) continue;
		// jtag stuff below this line
		log(Logger::DEBUG0) << "S2C_JtagPhys::Init: set hicann_pos to " << hicann_nr;
		jtag->set_hicann_pos(dnc2jtag(hicann_nr));

		// disable DNC interface
		jtag->HICANN_set_reset(0);

		// set timeouts
		log(Logger::DEBUG0) << "S2C_JtagPhys::Init: Setting timeout values for HICANN no. " << hicann_nr << " via JTAG..." << flush;
		jtag->HICANN_arq_write_rx_timeout(10, 10);  // tag0, tag1
		jtag->HICANN_arq_write_tx_timeout(200, 200);  // tag0, tag1

		log(Logger::DEBUG0) << "S2C_JtagPhys::Init: Asserting ARQ reset for HICANN no. " << hicann_nr << " via JTAG..." << flush;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		jtag->HICANN_arq_write_ctrl(0x0, 0x0);
	}

	return returnvalue;
#else
	throw std::runtime_error("DON'T use S2C_JtagPhys::Init in standalone HICANN testbench!");
#endif
}
