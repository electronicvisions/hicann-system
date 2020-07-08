/* -------------------------------------------------------------------------
	
	CONFIDENTIAL - FOR USE WITHIN THE FACETS PROJECT ONLY
	UNLESS STATED OTHERWISE BY AUTHOR

	Andreas Gruebl
	Vision/ASIC-LAB, KIP, University of Heidelberg
	
	June 2010
	
	s2c_jtagphys_2fpga.cpp - comments see .h file

 ----------------------------------------------------------------------- */
#define HEXOUT(number) uppercase << hex << setw(2) << right << setfill('0') << number

#include <pthread.h>

#include "common.h"
#include "s2c_jtagphys_2fpga.h"
#include "ethernet_software_if.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

using namespace facets;
using namespace std;

S2C_JtagPhys2Fpga::S2C_JtagPhys2Fpga(CommAccess const & access, myjtag_full *j, bool on_reticle, bool use_k7fpga) :
	Stage2Comm(access, j, on_reticle, use_k7fpga),
	test_transmissions_done(false),
	link_states(0),
	trans_count(100),
	disable_multiple_test_transmissions(false)
{
	hicann_ack_timeout[0] = 15;
	hicann_ack_timeout[1] = 20;
	hicann_resend_timeout[0] = 70;
	hicann_resend_timeout[1] = 100;
	init_s2comm();
	fpga_dnc_inited    = false;
	l2_link_state = 0x00;
	trans_error_count = 0;
}

S2C_JtagPhys2Fpga::~S2C_JtagPhys2Fpga(){}


S2C_JtagPhys2Fpga::Commstate S2C_JtagPhys2Fpga::Send(IData d, uint /*del*/)
{
	log(Logger::DEBUG1) << "S2C_JtagPhys2Fpga: Send packet " << hex << d.cmd() << " to HICANN " << d.addr();
	#if DNC_V == 2
		if ((l2_link_state&0x49) == 0x41) {
			log(Logger::DEBUG1) << "S2C_JtagPhys2Fpga::Send: using FPGA connection";
			jtag->FPGA_start_config_packet(dnc2jtag(d.addr()), d.cmd().to_uint64());
 	 } else {
			log(Logger::DEBUG1) << "S2C_JtagPhys2Fpga::Send: using JTAG connection";
			jtag->set_hicann_pos(d.addr());

			jtag->HICANN_set_test(0xc);
			jtag->HICANN_set_tx_data(d.cmd().to_uint64());
			jtag->HICANN_start_cfg_pkg();
			jtag->HICANN_set_test(0x0);
  	}
	#else
		jtag->FPGA_start_config_packet(dnc2jtag(d.addr()), d.cmd().to_uint64());
	#endif
	return ok;
}

S2C_JtagPhys2Fpga::Commstate S2C_JtagPhys2Fpga::Receive(IData &d)
{
	uint64_t rxdata=0;
	#if DNC_V == 2
		if ((l2_link_state&0x49) == 0x41) {
			log(Logger::DEBUG1) << "S2C_JtagPhys2Fpga::Receive: using FPGA connection";
			jtag->FPGA_get_rx_packet(dnc2jtag(d.addr()), rxdata);
		} else {
			log(Logger::DEBUG1) << "S2C_JtagPhys2Fpga::Receive: using JTAG connection";
			jtag->set_hicann_pos(d.addr()); // TODO: correct d.addr here?
			jtag->HICANN_get_rx_data(rxdata);
		}
		d.setCmd() = rxdata;
	
	#else
		jtag->FPGA_get_rx_packet(dnc2jtag(d.addr()), rxdata);
		
		d.setCmd() = rxdata; 
	#endif
	log(Logger::DEBUG1) << "S2C_JtagPhys2Fpga: Received packet " << hex << d.cmd() << ", data: 0x" << rxdata;
	return ok;
}


S2C_JtagPhys2Fpga::Commstate S2C_JtagPhys2Fpga::Init(int hicann_jtag_nr, bool silent, bool force_highspeed_init, bool return_on_error){
	// ECM: old testmodes often forget to specify the hicann_nr -- if your code breaks here... check this!
	if ((hicann_jtag_nr < 0) || (hicann_jtag_nr > 7)) {
		throw std::runtime_error("Init called with wrong hicann_nr");
	}
	std::bitset<8> hicann;
	hicann[jtag2dnc(hicann_jtag_nr)] = true;
	return S2C_JtagPhys2Fpga::Init(hicann, silent, force_highspeed_init, return_on_error);
}

S2C_JtagPhys2Fpga::Commstate S2C_JtagPhys2Fpga::Init(std::bitset<8> hicann, bool silent, bool force_highspeed_init, bool return_on_error){
	return Init(hicann, hicann, silent, force_highspeed_init, return_on_error);
}

// initializes jtag_multi. if "silent==true" => put no info on screen except errors to increase readability
// variable hicann contains DNC highspeed channel numbers
S2C_JtagPhys2Fpga::Commstate S2C_JtagPhys2Fpga::Init(std::bitset<8> hicann, std::bitset<8> highspeed_hicann, bool silent, bool force_highspeed_init, bool return_on_error){
	if ((~hicann & highspeed_hicann).any()) {
		throw std::runtime_error(
		    "S2C_JtagPhys2Fpga::Init: Required highspeed HICANNs " + highspeed_hicann.to_string() +
		    " are not a subset of available HICANNs " + hicann.to_string());
	}

	if(!silent) {
		log(Logger::INFO) << "S2C_JtagPhys2Fpga::Init: Initializing HICANNs " << hicann.to_string()
		                  << " with highspeed links: " << highspeed_hicann.to_string() << flush;
	}

	if (disable_multiple_test_transmissions && test_transmissions_done)
		return S2C_JtagPhys2Fpga::ok;

	if (highspeed_hicann.none()) {
		log(Logger::INFO) << "S2C_JtagPhys2Fpga::Init: No HICANNs provided for highspeed initialization!" << flush;
	} else {
		log(Logger::DEBUG0) << "S2C_JtagPhys2Fpga::Init: initializing high speed communication on " << highspeed_hicann.to_string() << flush;
	}

	size_t nrep, max_init_tries=10 ;
	for (nrep=0; nrep < max_init_tries; ++nrep) // try initialization 10 times
	{
    	
        if (k7fpga)
        {
			std::bitset<8> init_success = this->fpga_hicann_init(highspeed_hicann);
			// check if all requested hicanns were inited successfully
			if ((init_success.to_ulong() & highspeed_hicann.to_ulong()) != highspeed_hicann.to_ulong()) {
				log(Logger::WARNING) << "Highspeed link initialization failed."
					<< "Requested " << highspeed_hicann.to_string()
					<< " successful " << init_success.to_string();
				if (return_on_error) {
					return S2C_JtagPhys2Fpga::initfailed;
				} else {
					continue; // try again on failed initialization
				}
			}
		}
	else // non Kintex-7
		{
			// The following ensures backward compatibility:
	        // reset hicanns
	        if (nrep > 0 || force_highspeed_init ) {
	        	jtag->FPGA_set_fpga_ctrl(0x1); // reset DNC
	        	// disable DNC interface
	        	for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
	        		jtag->set_hicann_pos(i);
	        		jtag->HICANN_set_reset(0);
	        		jtag->HICANN_set_reset(1);
	        	}
	        }

	        std::bitset<9> ret = fpga_dnc_init(silent);

	        log(Logger::DEBUG0) << "S2C_JtagPhys2Fpga::Init: result of highspeed init: " << ret.to_string() << flush;

	        std::bitset<9> tmp = 1<<8 | highspeed_hicann.to_ulong(); // 1 dnc init bit, 8 hicann init bits

	        if ((ret & tmp) != tmp)
	        {
	        	log(Logger::WARNING) << "Highspeed link initialization failed.";
	        	if (return_on_error) {
	        		return S2C_JtagPhys2Fpga::initfailed;
	        	}
	        	else {
	        		continue; // try again on failed initialization
	        	}
	        }
		}
		
	#ifdef NCSIM
			break; // assume that test transmissions are successful for simulation
	#else
    
		// test transmissions
		bool trans_succ = true;
		for (size_t hicann_nr = 0; hicann_nr < hicann.size(); hicann_nr++)
		{
			// skip test transmissions for non-highspeed-link hicanns
			if (!highspeed_hicann[hicann_nr]) continue;
			
			unsigned int trans_errors = test_transmission(hicann_nr, trans_count);
			if (trans_errors)
			{
				log(Logger::WARNING) << "Test transmission for HICANN " << hicann_nr << " produced " << trans_errors << " errors";
				trans_succ = false;
				break;
			}
            else
            {
                log(Logger::DEBUG0) << "Test transmission for HICANN " << hicann_nr << " without errors";
            }
		}
		
		if (trans_succ)
		{
			log(Logger::INFO) << "Test transmissions (count: " << trans_count
			                  << ") on all requested HICANNs (" << highspeed_hicann.count()
			                  << ") successful.";
			test_transmissions_done = true;
			break;
		}
		else {
			if (return_on_error)
				return S2C_JtagPhys2Fpga::initfailed;
		}
#endif
	}

	// let it fail
	if (nrep == max_init_tries)
		return S2C_JtagPhys2Fpga::initfailed;

	// disable FPGA ARQ since JTAG - software ARQ is used: (is re-enabled by hostal-based init.)
	log(Logger::INFO) << "S2C_JtagPhys2Fpga::Init: Disabling HICANN-ARQ in FPGA";
	set_fpga_reset(jtag->get_ip(), false, false, false, false, /*ARQ:*/true);

	//initialize JTAG read access
	Clear(); // re-sets jtag position...

	// Reset software ARQ
	log(Logger::INFO) << "S2C_JtagPhys2Fpga: Resetting Software HICANN-ARQ";
	for(uint i=0; i<link_layers.size(); i++)
		for(uint j=0; j<link_layers[i].size(); j++)
			link_layers[i][j].arq.Reset();

	for (size_t hicann_nr = 0; hicann_nr < hicann.size(); hicann_nr++) {
		// this slows down sim and is not required, there -> don't use.
		// -> could be enabled for debugging spurious hicann_if output (which should not be neccessary)
		// disable forwarding of data to ARQ in FPGA for JTAG-only and/or unavailable HICANNs
		if (!link_states[hicann_nr]) {
#ifndef NCSIM
			log(Logger::WARNING) << "Disabling forwarding of highspeed data in FPGA for channel: " << hicann_nr;
			jtag->K7FPGA_set_hicannif(hicann_nr);
			jtag->K7FPGA_disable_hicannif_config_output();
#else
			log(Logger::WARNING) << "Sim-only: Skip disabling forwarding of highspeed data in FPGA for channel: " << hicann_nr;
#endif
		}

		// proceed only on available HICANNs (relevant e.g. on Cube setup!)
		if (!hicann[hicann_nr])
			continue;

		log(Logger::DEBUG0) << "S2C_JtagPhys::Init: set hicann_pos to " << hicann_nr;
		jtag->set_hicann_pos(dnc2jtag(hicann_nr));

		if (!link_states[hicann_nr]) {
			log(Logger::WARNING) << "Disabling DNC interface of HICANN on channel: " << hicann_nr;
			jtag->HICANN_set_reset(0);
		}

		// set timeouts
		log(Logger::DEBUG0) << "S2C_JtagPhys2Fpga::Init: Setting timeout values for HICANN no. " << hicann_nr << " via JTAG..." << flush;
		jtag->HICANN_arq_write_rx_timeout(hicann_ack_timeout[0], hicann_ack_timeout[1]); // sets the target (or ACK) timeout for tag0 and tag1
		jtag->HICANN_arq_write_tx_timeout(hicann_resend_timeout[0], hicann_resend_timeout[1]); // sets the master (or RESEND) timeout for tag0 and tag1

		log(Logger::DEBUG0) << "S2C_JtagPhys2Fpga::Init: Asserting ARQ reset for HICANN no. " << hicann_nr << " via JTAG..." << flush;
		// Note: jtag commands defined in "jtag_emulator.h"
		jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		jtag->HICANN_arq_write_ctrl(0x0, 0x0);
	}

	return S2C_JtagPhys2Fpga::ok;
}

	// ----------------------------------
	// DNC communication related initialization functions
	// ----------------------------------
	
void S2C_JtagPhys2Fpga::dnc_shutdown() {
	if(!jtag){
	log(Logger::ERROR) << "S2C_JtagPhys2Fpga::dnc_shutdown: Object jtag not set, abort!";
	exit(EXIT_FAILURE);
	}
	//printf("\nDeactivating all DNC components\n");
	
	#if DNC_V == 2
		jtag->DNC_set_GHz_pll_ctrl(0xe);
		jtag->DNC_stop_link(0x1ff);
		jtag->DNC_set_lowspeed_ctrl(0x00);
		jtag->DNC_set_loopback(0x00);
		jtag->DNC_lvds_pads_en(0x1ff);
		jtag->DNC_set_init_ctrl(0x0ffff);
		jtag->DNC_set_speed_detect_ctrl(0x0);
		jtag->DNC_set_GHz_pll_ctrl(0x0);
	#else
		jtag->DNC_set_GHz_pll_ctrl(0xe);
		jtag->DNC_stop_link(0x1ff);
		jtag->DNC_set_lowspeed_ctrl(0x00);
		jtag->DNC_set_loopback(0x00);
		jtag->DNC_lvds_pads_en(0x1ff);
		jtag->DNC_set_init_ctrl(0x0aaaa);
		jtag->DNC_set_speed_detect_ctrl(0x0);
		jtag->DNC_set_GHz_pll_ctrl(0x0);
	#endif
	
}

void S2C_JtagPhys2Fpga::hicann_shutdown() {
	if(!jtag){
		log(Logger::ERROR) << "S2C_JtagPhys2Fpga::hicann_shutdown: Object jtag not set, abort!";
		exit(EXIT_FAILURE);
	}
	#if DNC_V == 2
		jtag->HICANN_set_reset(1);
		jtag->HICANN_set_GHz_pll_ctrl(0x7);
		jtag->HICANN_set_link_ctrl(0x141);
		jtag->HICANN_stop_link();
		jtag->HICANN_set_test(0); // disable memory test mode
		jtag->HICANN_lvds_pads_en(0x1);
		jtag->HICANN_set_reset(0);
		jtag->HICANN_set_bias(0x10);
		jtag->HICANN_set_GHz_pll_ctrl(0x0);
	#else
		jtag->HICANN_set_reset(1);
		jtag->HICANN_set_GHz_pll_ctrl(0x7);
		jtag->HICANN_set_link_ctrl(0x141);
		jtag->HICANN_stop_link();
		jtag->HICANN_set_test(0); // disable memory test mode
		jtag->HICANN_lvds_pads_en(0x1);
		jtag->HICANN_set_reset(0);
		jtag->HICANN_set_bias(0x10);
		jtag->HICANN_set_GHz_pll_ctrl(0x0);
	#endif
}

unsigned int S2C_JtagPhys2Fpga::initAllConnections(bool silent) {

	unsigned char state1[9] = {0,0,0,0,0,0,0,0,0};
	uint64_t state2[9] = {0,0,0,0,0,0,0,0,0};
	unsigned int break_count = 0;
	unsigned int no_init = (~(0xff<<(jtag->chain_length-2)))&0xff;
	uint hicann_jtag_nr;

	// do NOT reset the setup here, again since this MUST be done correctly in advance!
	//jtag->DNC_lvds_pads_en(~(0x100+(1<<hicann_nr)));
		
	//Stop all HICANN channels and start DNCs FPGA channel
	jtag->DNC_start_link(0x100);
	//Start FPGAs DNc channel
	jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x100);

	if(!silent) log(Logger::INFO) << "Start FPGA-DNC initialization:" << flush;

	while ((((state1[8] & 0x1) == 0)||((state2[8] & 0x1) == 0)) && break_count<10) {
		usleep(90000);
		jtag->DNC_read_channel_sts(8,state1[8]);
		if(!silent) log(Logger::INFO) << "DNC Status = 0x" << hex << ((uint)state1[8]) << " ";

		jtag->FPGA_get_status(state2[8]);
		if(!silent) log(Logger::INFO) << "FPGA Status = 0x" << hex << ((uint)state2[8]) << " RUN:" << dec << break_count << flush;
		++break_count;
	}
		
	if (break_count == 10) {
		//this->dnc_shutdown();
		//this->hicann_shutdown();
		jtag->DNC_stop_link(0x1ff);
		jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
		if(!silent) log(Logger::INFO) << "ERROR in FPGA-DNC initialization --> EXIT" << flush;
		return 0;
	}
	break_count = 0;

	if(!silent) log(Logger::INFO) << "Start DNC-HICANN initialization: " << flush;
	jtag->DNC_set_lowspeed_ctrl(0xff); // ff = lowspeed
	jtag->DNC_set_speed_detect_ctrl(0x00); // 00 = no speed detect
	for (unsigned int i = 0;i<jtag->chain_length-2; ++i) {
		hicann_jtag_nr = i;
		jtag->set_hicann_pos(hicann_jtag_nr);
		jtag->HICANN_set_link_ctrl(0x061);
	}
        
	while ((no_init != 0) && (break_count < 10)) {
		for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
			if (((no_init>>i)&0x1) == 1) {
				hicann_jtag_nr = i;
				jtag->set_hicann_pos(hicann_jtag_nr);
				jtag->HICANN_stop_link();
			}
		}
		jtag->DNC_stop_link(0x000 + (no_init&0xff));

		for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
			if (((no_init>>i)&0x1) == 1) {
				hicann_jtag_nr = i;
				jtag->set_hicann_pos(hicann_jtag_nr);
				jtag->HICANN_start_link();
			}
		}
		jtag->DNC_start_link(0x100 + (no_init&0xff));

		usleep(90000);
		for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
			if (((no_init>>i)&0x1) == 1) {
				jtag->DNC_read_channel_sts(jtag->chain_length-3-i,state1[i]);
				if(!silent) log(Logger::INFO) << "DNC Status(" << i << ") = 0x" << hex << ((uint)state1[i]) << flush;
				hicann_jtag_nr = i;
				jtag->set_hicann_pos(hicann_jtag_nr);
				jtag->HICANN_read_status(state2[i]);
				if(!silent) log(Logger::INFO) << "HICANN Status(" << i << ") = 0x" << hex << ((uint)state2[i]) << flush;
				
				if ( ((state1[i]&0x73) == 0x41) && ((state2[i]&0x73) == 0x41) ) {
					no_init = no_init & ((~(1<<i))&0xff);
				}
			}
		}
#ifdef NCSIM
		break_count = 10;
#else
		++break_count;
#endif
		if(!silent) log(Logger::INFO) << "*************************************" << flush;
	}

	if (no_init == 0) {
		if(!silent) log(Logger::INFO) << "Sucessfull init DNC-HICANN connection " << flush;
	} else {
		for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
			if(!silent) log(Logger::INFO) << "Init channel "<< dec << i << " with DNC state: 0x" << hex << ((uint)state1[i]) << " and HICANN state 0x" << hex << ((uint)state2[i]) << flush;
			if(((no_init>>i)&0x1) == 1) {
				hicann_jtag_nr = i;
				jtag->set_hicann_pos(hicann_jtag_nr);
				this->hicann_shutdown();
			}			
		}
	}

	if(!silent) log(Logger::INFO) << "Successfull FPGA-DNC-HICANN initialization" << flush;
	return 0x100 + ((~no_init)&((1<<(jtag->chain_length-2))-1));
}


// ###################################
// deprecated
int S2C_JtagPhys2Fpga::fpga_dnc_init(unsigned char hicanns, bool force, int dnc_del, int hicann_del, bool /*silent*/) {
	UNUSED(hicanns);      // unused in the past
	UNUSED(force);      // unused in the past
	UNUSED(dnc_del);    // unused in the past
	UNUSED(hicann_del); // unused in the past
	throw std::runtime_error("deprecated function");
	return 0;
}

// ###################################
std::bitset<9> S2C_JtagPhys2Fpga::fpga_dnc_init(bool silent) {
	if(!jtag){
	  log(Logger::ERROR) << "S2C_JtagPhys2Fpga::fpga_dnc_init: Object jtag not set, abort!" << flush;
	  exit(EXIT_FAILURE);
	}
	
	std::bitset<9> result;
	for (int i=0;i<9;++i) result[i] = false;

	uint64_t check_state_fpga = 0;
	unsigned char check_state_dnc0 = 0;
	unsigned char check_state_dnc1 = 0;
	uint64_t check_state_hicann = 0;
	uint hicann_jtag_nr;

	jtag->FPGA_get_status(check_state_fpga);
	jtag->DNC_read_channel_sts(8,check_state_dnc0);

	if(((check_state_fpga&0x01) && (check_state_dnc0&0x01)) )
	{
		fpga_dnc_inited = true;
		result[8] = true;
		log(Logger::INFO) << "S2C_JtagPhys2Fpga::fpga_dnc_init: Already initialized" << flush;
		for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
			hicann_jtag_nr = i;
			jtag->set_hicann_pos(hicann_jtag_nr);
			jtag->DNC_read_channel_sts(i,check_state_dnc1);
			jtag->HICANN_read_status(check_state_hicann);
			if ((check_state_dnc1&0x01) && (check_state_hicann&0x01) && ((check_state_dnc1&0x08) == (check_state_hicann&0x08)))
				result[i] = true;
			else
				result[i] = false;
		}
		
		return result;
	}

	uint64_t jtagid;
	jtag->read_id(jtagid, jtag->pos_fpga);
	if ( jtagid != 0xbabebeef )
    {
    	log(Logger::ERROR) << "Read wrong FPGA JTAG ID: 0x" << std::hex << jtagid << flush;
		if (jtagid == 0x1c56c007)
      		throw std::runtime_error("got Kintex7 FPGA jtag id, but using Virtex5 functions -- wrong FPGA design or wrong software option?");
		else {
			stringstream err;
			err << "got wrong FPGA jtag id: 0x"  << std::hex << jtagid << " -- power problem?";
			throw std::runtime_error(err.str());
		}
    }

	jtag->DNC_stop_link(0x1ff);  //Stop DNC-HICANN connections
	jtag->FPGA_start_dnc_packet((fpga_master << 9) + 0x000);
	for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
		hicann_jtag_nr = i;
		jtag->set_hicann_pos(hicann_jtag_nr);
		jtag->HICANN_stop_link();
	}

#ifndef NCSIM
	jtag->FPGA_get_status(check_state_fpga);
	jtag->DNC_read_channel_sts(8,check_state_dnc0);

	if(!silent) log(Logger::INFO) << "State FPGA         0x" << HEXOUT(check_state_fpga) << flush;
	else log(Logger::DEBUG0) << "State FPGA         0x" << HEXOUT(check_state_fpga) << flush;

	if(!silent) log(Logger::INFO) << "State DNC FPGAIF   0x" << HEXOUT((uint64_t) check_state_dnc0) << flush;
	else log(Logger::DEBUG0) << "State DNC FPGAIF   0x" << HEXOUT((uint64_t) check_state_dnc0) << flush;

	for (unsigned i = 0;i<jtag->chain_length-2; ++i) {
		hicann_jtag_nr = i;
		jtag->set_hicann_pos(hicann_jtag_nr);
		jtag->DNC_read_channel_sts(i,check_state_dnc1);
		jtag->HICANN_read_status(check_state_hicann);
		if(!silent) log(Logger::INFO) << "State DNC HICANN " << dec << i << "  0x" << HEXOUT((uint)check_state_dnc1) << " State HICANN " << dec << i << "    0x" << HEXOUT((uint)check_state_hicann) << flush;
        else log(Logger::DEBUG0) << "State DNC HICANN " << dec << i << "  0x" << HEXOUT((uint)check_state_dnc1) << " State HICANN " << dec << i << "    0x" << HEXOUT((uint)check_state_hicann) << flush;
	}
#endif

	unsigned int inits = initAllConnections(silent);

	for (int i=0;i<9;++i) {
		if((inits>>i)&1) result[i] = true;
	}
	
	if (result.count()>1) {
		l2_link_state = 0x49;
		fpga_dnc_inited = true;
	} else {
		l2_link_state = 0x0a;
		fpga_dnc_inited = false;
	}
	return result;
}

unsigned int S2C_JtagPhys2Fpga::test_transmission(unsigned int hicann, unsigned int packet_count)
{
	log(Logger::INFO) << "test_transmission: Testing " << packet_count << " transmissions to HICANN " << hicann;

	// enable test mode in HICANNs&FPGA -> no config packets transmitted from/to ARQ communication partners

	if (!k7fpga)
    {
		jtag->FPGA_testmode(0x3);
		jtag->DNC_set_timestamp_ctrl(0x00);
	}

	unsigned int hicann_count = (k7fpga) ? (jtag->chain_length-1) : (jtag->chain_length-2);
	for (unsigned int nhicann=0;nhicann<hicann_count;++nhicann)
	{
		jtag->set_hicann_pos(nhicann);
		jtag->HICANN_set_test(1);
	}

	// set jtag chain position to specified hicann
	jtag->set_hicann_pos(dnc2jtag(hicann));
	if (k7fpga)
    {
        jtag->K7FPGA_set_hicannif(hicann);
        jtag->K7FPGA_disable_hicannif_config_output();
    }

	unsigned int trans_errors = 0;
	for (unsigned int ntrans=0;ntrans<packet_count;++ntrans)
	{
		// start config packet from FPGA
		uint64_t sent_data = (uint64_t)(0x1234)<<(ntrans%8);

		if (k7fpga)
        {
            jtag->K7FPGA_set_hicannif_tx_data(sent_data);
            jtag->K7FPGA_set_hicannif_control(0x02);
		}
        else
		{
			jtag->FPGA_start_pulse_event(hicann, sent_data);
		}
    
		usleep(1000);
		// receive packet in HICANN
		uint64_t rec_data = sent_data + 1;
		jtag->HICANN_get_rx_data(rec_data);
		
		if ((rec_data&0xffffff) != (sent_data&0xffffff) )
		{
			log(Logger::INFO) << "Error transmitting config. packet " << ntrans+1 << " FPGA->HICANN: sent 0x" << HEXOUT(sent_data) << ", but received 0x" << HEXOUT(rec_data);
			++trans_errors;
		}
		
		// start config packet in HICANN
		sent_data = (uint64_t)(0xfedcba98)<<(ntrans%32);
		jtag->HICANN_set_tx_data(sent_data);
		jtag->HICANN_start_cfg_pkg();

		// receive config packet in FPGA
		rec_data = sent_data + 1;
        
        if (k7fpga)
        {
            jtag->K7FPGA_get_hicannif_rx_cfg(rec_data);
        }
        else
        {
			jtag->FPGA_get_rx_data(rec_data);
		}

		if (rec_data != sent_data)
		{
			log(Logger::INFO) << "Error transmitting config. packet " << ntrans+1 << " HICANN->FPGA: sent 0x" << HEXOUT(sent_data) << ", but received 0x" << HEXOUT(rec_data);
			++trans_errors;
		}
	}

	// disable test mode
    if (!k7fpga)
		jtag->FPGA_testmode(0x0);

	for (unsigned int nhicann=0;nhicann<hicann_count;++nhicann)
	{
		jtag->set_hicann_pos(nhicann);
		jtag->HICANN_set_test(0);
	}

	// set jtag chain position to specified hicann
	jtag->set_hicann_pos(dnc2jtag(hicann));
	if (k7fpga)
    {
        jtag->K7FPGA_set_hicannif(hicann);
        jtag->K7FPGA_enable_hicannif_config_output();
	}
    
	return trans_errors;
}


unsigned int S2C_JtagPhys2Fpga::get_transmission_errors() {
	return this->trans_error_count;
}


namespace {
//helper function for packet formating for trigger_systime and trigger_experiment
static void config_fpga_pbmem_experiment_related_things(unsigned int const fpga_ip, bool const enable, bool const listen_global, bool const expstart_mode, uint const sysstart_port) {
	#pragma omp critical(EthIF)
	{
	EthernetSoftwareIF eth_if;
	eth_if.init(sysstart_port);

	uint32_t pck = 0x55000000;
	if (enable) pck |= 0x1;
    if (listen_global) pck |= 0x2;
    if (expstart_mode) pck |= 0x4;

	pck = htonl(pck);
	eth_if.sendUDP(fpga_ip, sysstart_port, &pck, 4);

	eth_if.end();
	}
}
} // namespace

// synchronously start systime counter in FPGA and HICANN via Ethernet packet to port 1800
void S2C_JtagPhys2Fpga::trigger_systime(unsigned int const fpga_ip, bool const enable, bool const listen_global, uint const sysstart_port) {
    log(Logger::WARNING) << "Calling trigger_systime from S2C_JtagPhys2Fpga is deprecated. Please call it from S2C_JtagPhys2FpgaArq.";
	config_fpga_pbmem_experiment_related_things(fpga_ip, enable, listen_global, false, sysstart_port);
}

// synchronously start experiment on FPGA
void S2C_JtagPhys2Fpga::trigger_experiment(unsigned int const fpga_ip, bool const enable, bool const listen_global, uint const sysstart_port) {
    log(Logger::WARNING) << "Calling trigger_experiment from S2C_JtagPhys2Fpga is deprecated. Please call it from S2C_JtagPhys2FpgaArq.";
	config_fpga_pbmem_experiment_related_things(fpga_ip, enable, listen_global, true, sysstart_port);
}

// Kintex7-specific functions
std::bitset<8> S2C_JtagPhys2Fpga::fpga_hicann_init(std::bitset<8> hicann)
{
	log(Logger::INFO) << "S2C_JtagPhys2Fpga::fpga_hicann_init(" << hicann.to_string() << ")" << flush;

	uint64_t jtagid;
	jtag->read_id(jtagid, jtag->pos_fpga);
	if ( jtagid != 0x1c56c007 )
    {
    	log(Logger::ERROR) << "Read wrong FPGA JTAG ID: 0x" << std::hex << jtagid << flush;
		log(Logger::INFO) << "Temp" << flush;
		if (jtagid == 0xbabebeef)
      		throw std::runtime_error("got Virtex5 FPGA jtag id, but using Kintex7 functions -- wrong FPGA design or wrong software option?");
		else {
			stringstream err;
			err << "got wrong FPGA jtag id: 0x"  << std::hex << jtagid << " -- power problem?";
			throw std::runtime_error(err.str());
		}
    }

	log(Logger::INFO) << "Read FPGA JTAG ID 0x" << std::hex << jtagid << " from JTAG position " << std::dec << jtag->pos_fpga << flush;

    for (int hicannnr=0;hicannnr<8;++hicannnr)
    if (hicann[hicannnr])
    {
        log(Logger::INFO) << "Initializing highspeed link Kintex7-HICANN " << hicannnr << " -> JTAG position " << dnc2jtag(hicannnr);
        jtag->K7FPGA_set_hicannif(hicannnr); // highspeed interface number
        jtag->set_hicann_pos(dnc2jtag(hicannnr)); // JTAG number
        
        // sanity test: read HICANN JTAG ID        
        jtagid = 0;
		jtag->read_id(jtagid, dnc2jtag(hicannnr));
        if ( jtagid != 0x14849434 )
        {
            log(Logger::WARNING) << " Invalid HICANN ID: 0x" << std::hex << jtagid << std::dec << ". Stop highspeed init.";
            return false;
        }
		log(Logger::INFO) << "Read correct HICANN ID: 0x" << std::hex << jtagid << std::dec;

        jtag->K7FPGA_stop_fpga_link();
        jtag->HICANN_stop_link();
        // jtag->HICANN_lvds_pads_en(1);

		// P/N signals of RX Data line from HighSpeed-HICANN-Ch 3 to FPGA are swapped on mainPCB
		// enable inversion of deserialized data for this channel, when running on wafer.
		if (running_on_reticle && hicannnr == 3) {
			log(Logger::DEBUG0) << "Inverting deserialized data of HICANN with HighSpeed channel 3 "
				                   "to cancel P/N swap on mainPCB.";
			jtag->K7FPGA_invert_rx_data(true);
		}

		uint64_t status = 0;
        jtag->K7FPGA_get_hicannif_status(status);
        unsigned int link_state = (status >> 0) & 0xff;
        unsigned int crc = (status >> 8) & 0xff;
        unsigned int systime = (status >> 16) & 0x3fff;
        log(Logger::INFO) << "HICANN interface status: systime: 0x" << std::hex << systime << " CRC: " << std::dec << crc << " Link: 0x" << std::hex << link_state << std::dec;

        //Start init
        usleep(20);
        jtag->HICANN_set_link_ctrl(0x061);
        jtag->K7FPGA_set_hicannif_config(0x00c);
        jtag->HICANN_lvds_pads_en(0);
        jtag->HICANN_start_link();
        jtag->K7FPGA_start_fpga_link();

        unsigned int k=0;
        uint64_t state2 = 0;
        while (k<10 && (((state2&0x49) != 0x49) || ((link_state&0x49) != 0x49))) {
          usleep(100000);
          jtag->HICANN_read_status(state2);
          log(Logger::INFO) << "  HICANN Status = 0x" << std::hex << state2 << std::dec;
          jtag->K7FPGA_get_hicannif_status(status);
          link_state = (status >> 0) & 0xff;
          log(Logger::INFO) << "FPGA Link Status = 0x" << std::hex << link_state << std::dec;
          ++k;
        }

		if (k==10) {
			log(Logger::WARNING) << "Highspeed init for HICANN " << hicannnr << " (highspeed-ID) failed.";
			link_states[hicannnr] = false;
		} else {
			log(Logger::INFO) << "Highspeed init for HICANN " << hicannnr << " successful";
			link_states[hicannnr] = true;
		}
    }

	return link_states;
}
