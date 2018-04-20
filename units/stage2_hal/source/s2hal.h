#ifndef _S2HAL_H_
#define _S2HAL_H_
/*
 * Header file for Python wrapping.
 * This file includes all Classes & Functions to be wrapped
 *
 */

// HACK for pyplusplus :)
#ifdef HICANN_V2
#define HICANN_V 2
#endif

#include "common.h"

#include "ctrlmod.h"
#include "dnc_control.h"
#include "dncif_control.h"
#include "fg_control.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "l1switch_control.h"
#include "linklayer.h"
#include "neuronbuilder_control.h"
#include "neuron_control.h"
//#include "ramtest.h"       // junk
//#include "s2c_dncif.h"     // verification_control.h missing
//#include "s2c_jtag.h"      // SystemC
#include "s2c_jtagphys_2fpga.h"
//#include "s2c_jtagphys_fpga.h"
//#include "s2c_jtagphys_full.h"
#include "s2c_jtagphys.h"
//#include "s2c_ocpbus.h"    // SystemC
#include "s2comm.h"
//#include "s2c_systemsim.h" // SystemC
//#include "s2c_tcpip.h"     // TCP sucks
#include "s2ctrl.h"
#include "s2hal_def.h"
#include "s2_types.h"
#include "spl1_control.h"
#include "synapse_control.h"

// some other stuff
#include "hardware_base.h"   // global_src
#include "stage2_conf.h"     // units/stage2_conf
#include "arq.h"             // units/arq
#include "jtag_cmdbase.h"
#include "jtag_cmdbase_fpgadnchicann_full.h"

namespace py_details {
	inline void instantiate() {
		// make templated stuff visible for code generator

		// ask AG why this one is needed (see tests2.cpp), he said:
		// <ci_payload, ARQPacketStage2> should be enough
		unused(sizeof( ::PacketLayer<facets::ARQPacketStage2,facets::ARQPacketStage2> ));
		// the one and only (ag):
		unused(sizeof( ::PacketLayer<facets::ci_payload,facets::ARQPacketStage2> ));
		unused(sizeof( ::LinkLayer<facets::ci_payload,facets::ARQPacketStage2> ));
		unused(sizeof( ::ARQ<facets::ci_payload,facets::ARQPacketStage2> ));
		unused(sizeof( ::jtag_cmdbase_fpgadnchicann_full<jtag_lib::jtag_hub> ));
		if (false) {
			jtag_cmdbase_fpgadnchicann_full<jtag_lib::jtag_hub> j;
			uint64_t x;
			j.DNC_get_rx_data(x);
			j.HICANN_read_status(x);
			j.HICANN_read_crc_count(x);
			j.HICANN_set_data_delay(x,x);
		}
	}
}

#endif // _S2HAL_H_
