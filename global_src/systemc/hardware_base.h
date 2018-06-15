#ifndef _HWBASE_H_
#define _HWBASE_H_

extern "C" {
#include <stdint.h>
}

// shortcuts
typedef uint32_t uint;

// some constants from anncore geometry..
const int CB_NUMH =           8;
const int SYN_NUMH =          16;
// standard control interface (ci) packet size:
const int ci_packet_width =          64;


// location type for switch block:
enum switch_loc {CBL=0, CBR=1, SYNTL=2, SYNTR=3 ,SYNBL=4, SYNBR=5};

// location type for digital stdp control (top or bottom):
enum syn_loc {DSCT=0, DSCB=1 };

// location type for repeater sets:
enum rep_loc {REPL=0, REPR=1, REPTL=2, REPBL=3,  REPTR=4, REPBR=5};

// location type for floating gate controllers:
enum fg_loc {FGTL=0, FGTR=1, FGBL=2, FGBR=3};

// location type for mergers (from right to left according to HICANN documentation picture)
enum nc_merger {BG0=0x1, BG1=0x2, BG2=0x4, BG3=0x8, BG4=0x10, BG5=0x20, BG6=0x40, BG7=0x80, L0_0=0x100, L0_1=0x200, L0_2=0x400, L0_3=0x800, L1_0=0x1000, L1_1=0x2000, L2_0=0x4000, DNC0=0x8001, DNC1=0x8002, DNC2=0x8004, DNC3=0x8008, DNC4=0x8010, DNC5=0x8020, DNC6=0x8040, DNC7=0x8080, MERG_ALL=0x4fff};

// address map of busmux slave modules

// -> address space struct: (inputadr && mask) == adr
struct adrspc_t {
	unsigned int maxaddr;
	unsigned int startaddr;
};

// address constants
const adrspc_t
	OcpSyn_tr 			 = {0x01ff, 0xd000},
	OcpSyn_tl 			 = {0x07ff, 0xe000},
	OcpSyn_br 			 = {0x01ff, 0xd200},
	OcpSyn_bl 			 = {0x0fff, 0xa000},
	OcpCb_r 				 = {0x01ff, 0xd400},
	OcpCb_l 				 = {0x1fff, 0x0000},
	OcpRep_l  			 = {0x1fff, 0x2000},
	OcpRep_r  			 = {0x01ff, 0xd600},
	OcpRep_tl 			 = {0x07ff, 0xe800},
	OcpRep_bl 			 = {0x0fff, 0xc000},
	OcpRep_tr 			 = {0x01ff, 0xd800},
	OcpRep_br 			 = {0x01ff, 0xda00},
	OcpDncIf				 = {0x07ff, 0xf000},
	OcpSpl1 				 = {0x07ff, 0xf800},
	OcpNeuronCtrl 	 = {0x1fff, 0x4000},
	OcpNeuronBuilder = {0x1fff, 0x6000},
	OcpFgate_tl 		 = {0x1fff, 0x8000},
	OcpFgate_tr 		 = {0x01ff, 0xdc00},
	OcpFgate_bl 		 = {0x0fff, 0xb000},
	OcpFgate_br 		 = {0x01ff, 0xde00},
	OcpStdpCtrl_t 	 = {0x7fff, 0x0000},
	OcpStdpCtrl_b 	 = {0x7fff, 0x8000};

const int tagid_fast = 1;
const int tagid_slow = 0;
const int tagid_num  = 2;

// Floatinggate constants:
namespace FG_pkg {
	const uint fg_num                  = 4;
	const uint fg_datawidth            = 10;
	const uint fg_bias_dw              = 4;
	const uint fg_bias                 = 8;
	const uint fg_biasn_dw             = 4;
	const uint fg_biasn                = 5;
	const uint fg_instr_dw             = 3;
	const uint fg_ram_aw               = 7;
	const uint fg_coll_aw              = 5;
	const uint fg_maxcycle             = 255;
	const uint fg_maxcycle_dw          = 8;
	const uint fg_currentwritetime     = 1;
	const uint fg_currentwritetime_dw  = 6;
	const uint fg_voltagewritetime     = 15;
	const uint fg_voltagewritetime_dw  = 6;
	const uint fg_readtime             = 63;
	const uint fg_readtime_dw          = 6;
	const uint fg_acceleratorstep      = 9;
	const uint fg_acceleratorstep_dw   = 6;
	const uint fg_lines                = 129;
	const uint fg_pulselength          = 4;
	const uint fg_pulselength_dw       = 4;
// Instructions for controler:
	enum ControlInstruction {read=0, writeUp=1, writeDown=2, getNextFalse=3, stimulateNeurons=4, stimulateNeuronsContinuous=5};

} //of namespace FG_pkg
// neuronbuilder constants:
	const uint n_numslow  = 3; // static neuron control signals 
	const uint n_numfast  = 3; // static neuron control signals 
	const uint n_numd  = 8; // neuron sram data bus width
	const uint nb_numd  = 3; // neuronbuilder sram data bus width
	const uint spl1_numd  = 6; // neuron and neuronspl1 sram adr bus width
	const uint nspl1_numa  = 9; // neuronspl1 sram data bus width
	const uint aoutselect_dw  = 10; // analog mux 

// sramCtrl address bits for setting control timings
// !!! hard coded in SystemVerilog Code, therefore not automatically extracted to here !!!
// AG says: I nevertheless placed them here. That way, one is at least forced to think about them
//          after re-generating this file...
	const uint addrmsb_nrnbuilder =  9; // position of neuronbuilder's address field's MSB
	const uint addrmsb_repctrl    =  7; // position of repeatercontrol's address field's MSB
	const uint addrmsb_syndrvctrl = 10; // position of synapse driver SRAM control's address field's MSB

	const uint cnfgadr_nrnbuilder = 4; // position of neuronbuilder's configuration address bit
	const uint cnfgadr_repctrl    = 5; // position of repeatercontrol's configuration address bit
	const uint cnfgadr_syndrvctrl = 8; // position of synapse driver SRAM control's configuration address bit

#endif // _HWBASE_H_
