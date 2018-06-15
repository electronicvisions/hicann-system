#ifndef __SIMDEF__
#define __SIMDEF__


// include hardware constant definitions
#include "hardware_base.h"


//Defines for the network configurator
// activated via compiler flag -DUSE_TCPIP
#define FACETSPORT 3490 ///< The port used for FACTES configuration
#define FACETSIP "127.0.0.1" ///< The IP address used for FACTES configuration

/** the global clock definition */
#define SERDES_CLK_PER  1 ///< clock period of serial communication channels betw. HICANN and DNC [ns]
#define HC_SYS_CLK_PER 16 ///< HICANN system clock period [ns] - frequency equals 1/16 SERDES_CLK_PER

//Simulation stuff for system
#define WAFER_COUNT 1 ///< Number of Wafers that form the FACETS system

//Simulation stuff for Layer2 DNC on PCB
#define DNC_TO_ANC_COUNT 8	///< Number of connections to L1

#define L2_EVENT_WIDTH 32 ///< 12 bit label + 20 bit timestamp
#define L2_LABEL_WIDTH 12 ///< label size on point to point connection
#define FPGA_LABEL_WIDTH 16	///< extended label for fpga hypertransport connection (needs to be specified)
#define FPGA_LABEL_COUNT 32768 ///< Routing Mem block size
#define CHANNEL_MEM_SIZE_OUT 16 ///< HEAP Size in channel
#define CHANNEL_MEM_SIZE_IN 1 ///< FIFO Size in channel
#define DELAY_MEM_DEPTH 4096 ///<heap memory size, sorted memory for delaying at DNC
#define L2_LABEL_COUNT 1024 ///< Routing Mem size realistic: 4096
#define TIMESTAMP_WIDTH 15 ///< size of timestamp in a packet
#define DNC_TARGET_WIDTH 4 ///< address width for dnc internal addressing of dnc configuration and hicann connections

//#define L2_MEM_WIDTH 16
#define HYP_EVENT_WIDTH L2_EVENT_WIDTH ///< event size of the fpga hypertransport connection
#define TIME_TO_DNC_IF 200 ///< delay limit for transmission to hicann

#define TIME_SER 50 ///< Serialisation way time in ns
#define TIME_DES 50 ///< Deserialisation way time in ns

//Simulation stuff for FPGA
#define CHANNEL_HYP_SIZE_OUT 16	///<transmission buffer in hypertransport connection
#define CHANNEL_HYP_SIZE_IN 1 ///< receive buffer in hypertransport connection

#define TIME_FPGA_OUT 10    ///< Time from FPGA to FPGA tx
#define TIME_FPGA_IN 10		///< Time from FPGA to FPGA rx
#define TIME_HYP_OUT 10 	///< Time from FPGA to DNC
#define TIME_HYP_IN 10		///< Time from DNC to FPGA

#define L2_TRANSMIT_DELAY 500 ///< stimulus transmission delay in ns
#define SYSTIME_PERIOD_NS 4.0 ///< time base of system time

//Simulation stuff for Wafer
#define HICANNS_PER_RETICLE 1			///< number of hicanns per reticle
#define RETICLE_X 1						///< assumption of 2d field of reticles per wafer X size
#define RETICLE_Y 1						///< assumption of 2d field of reticles per wafer Y size
#define RETICLE_COUNT (RETICLE_X * RETICLE_Y)		///<number of reticles
#define HICANN_COUNT (HICANNS_PER_RETICLE*RETICLE_COUNT) 	///<number of hicanns per wafer/PCB system
#define HICANN_X 1						///< assumption of 2d field of hicanns per reticle X size
#define HICANN_Y 1						///< assumption of 2d field of hicanns per wafer Y size

/**	Enables use of reticles. 0 disables reticles. Is used to create wafer circle.
	Example: reticale with position[3][2] is enabled.
*/
//Simulation stuff for PCB
#define DNC_COUNT 1			///<number of DNCs for ONE wafer/PCB system
#define DNC_FPGA 4						///<how many dncs are connected to an fpga
#define FPGA_COUNT 1	///<number of fpgas in a wafer/PCB system

// Simulation stuff for	ANNCORE
/** Speed-up factor of the hardware needed by hw_neurons. (must match "speedup" in stage2/__init__.py)*/
#define SPEED_UP 10000 ///< realistic values: 1000, 10000(DEFAULT), 100000
/** Number of neurons per anncore (obsoloete in anncore, because ecah anncore can be configured sepatrately.*/
#define NRN_ANN 64 ///< realistic values: 8, 16, 32, 64, 128, 256, 512
/** Number of synapses per synapse row */
#define SYN_PER_ROW 256 ///< realistic value: 256 (SYN_NUMCOLS_PER_ADDR * pow((double)2,(double)SYN_COLADDR_WIDTH))
/** number of syndrivers (for one side, both blocks). */
#define SYNDRIVERS 128 ///< realistic value: 128, half (1/ROWS_PER_SYNDRIVER) of synapse row count (SYN_PER_ROW)
/** Number of synapse rows each syndriver serves. */
#define ROWS_PER_SYNDRIVER 2 ///< only allowed value: 2
/** Synapse array column address width */
#define SYN_COLADDR_WIDTH 6 ///< 64 column addresses will require a width of 6
/** Synapse array number of simultaneously addressed columns per address */
#define SYN_NUMCOLS_PER_ADDR 4 ///< realistic value: 4
/** Data width for ONE column of the synapse array */
#define SYN_COLDATA_WIDTH 8 ///< realistic value: 8
/** Width of data bus of synapse array memory */
#define SYN_DATA_WIDTH (SYN_NUMCOLS_PER_ADDR * SYN_COLDATA_WIDTH)
/** Synapse array row address width. Address is also used to access synapse driver config memory */
#define SYN_ROWADDR_WIDTH 8 ///< realistic value: 8 (256 rows, 2 rows per synapse driver)

#define TSTART 300	///< Poisson distribution delay
#define PULSE_TAU ((100000/NRN_ANN)-TSTART)


/// HICANN control interface packet specific stuff (see HICANN specification in BSCW)
/// All bit positions are relative to the 64 Bit data packet transmitted by the DNC_IF via OCP connection
/** Total width of packet payload */
#define CIP_PACKET_WIDTH 64
/** TagID copied from OCP request */
#define CIP_TAGID_WIDTH  2
#define CIP_TAGID_POS   62
/** Sequence-Data part valid width and pos */
#define CIP_SEQV_WIDTH  1
#define CIP_SEQV_POS   61
/** Sequence number width and pos */
#define CIP_SEQ_WIDTH  6
#define CIP_SEQ_POS   55
/** Acknowledgement number for opposide dir width and pos */
#define CIP_ACK_WIDTH  6
#define CIP_ACK_POS   49
/** PAcket type  */
#define CIP_WRITE_WIDTH  1
#define CIP_WRITE_POS   48
/** Address within submodule (mapping defined there) */
#define CIP_ADDR_WIDTH  16
#define CIP_ADDR_POS    32
/** Data payload */
#define CIP_DATA_WIDTH  32
#define CIP_DATA_POS     0


/// Control Interface packet type definitions
/// !!! must comply to citype_t in hicann_pkg.sv !!!
#define CIP_WRITE   0	///< write packet
#define CIP_READ    1	///< read packet
// future extensions to be added here (to be contained in data field of read packet)

/// OCP command definitions (as specified by OCP spec 2.2) - basically only write (WR) will be required
#define OCP_CMD_IDLE	0 ///< Idle IDLE (none)
#define OCP_CMD_WR		1 ///< Write WR write
#define OCP_CMD_RD		2 ///< Read RD read
#define OCP_CMD_RDEX	3 ///< ReadEx RDEX read
#define OCP_CMD_RDL		4 ///< ReadLinked RDL read
#define OCP_CMD_WRNP	5 ///< WriteNonPost WRNP write
#define OCP_CMD_WRC		6 ///< WriteConditional WRC write
#define OCP_CMD_BCST	7 ///< Broadcast BCST write

/// OCP Tag definitions
/** OCP tags are classified based on communication speed rather than single control modules
    for now, there only exists one for fast and one for slow communication */
#define OCP_TAG_NUM    2  ///< Number of OCP tags
#define OCP_TAG_FAST   1  ///< Reserved for synapse controller traffic
#define OCP_TAG_SLOW   0  ///< all remaining control modules

/** Definition of assignment to OCP tag and control interface sub-addresses for the single control modules 
transported "within" the OCP tags */
#define CISA_SUBADDR_WIDTH 3  ///< width of sub address field
#define CISA_SUBADDR_POS   (CIP_ADDR_WIDTH-CISA_SUBADDR_WIDTH)  ///< position of sub address field
#define CISA_SYN_TAG   0  ///< Synapse and STDP controller
#define CISA_SYN       0
#define CISA_L1CB_TAG  1  ///< Layer 1 crossbar switch controller
#define CISA_L1CB      0
#define CISA_REP_TAG   1  ///< Layer 1 repeater controller
#define CISA_REP       1

/// constants for actually implemented modules
#define OCP_TAG_NUM_IMPL  2 ///< actual number of implemented modules
#define OCP_TAG_IMPLED    2 ///< implemented TagIDs (initilizes an array -> separate by commas)

/// OCP/HICANN bus fabric FIFO parameters (each HICANN module gets a set of request/respone FIFOs)
#define HCB_SYN_RQF_DEPTH  16  ///< depth of synapse control module request FIFO
#define HCB_SYN_RSF_DEPTH  16  ///< depth of synapse control module reponse FIFO.
#define HCB_SYN_MOS         8  ///< depth of synapse controller's out of sequence buffer
#define HCB_SYN_MAS         4  ///< number of unack'ed write commands for synapse controller
#define HCB_SYN_MAST       50  ///< number of idle sysclock cycles until after a write ack is generated (for syn. ctrl)

/// Data strucure serialization related:
#define IDATA_ID_SIZE    1  ///< byte size of ID identifier within serialized IData 
#define IDATA_ADDR_SIZE  2  ///< byte size of address field within serialized IData

#define IDATA_PAYL_SIZE  1   ///< byte size of payload size field within serialized IData (char array)
#define CIP_SIZE_REG     ((CIP_PACKET_WIDTH+7)/8 + 1)  ///< byte size of char array carrying regular ci_packet including size identifier
#define CIP_SIZE_VERB    (CIP_SIZE_REG + 1)            ///< same as above, verbose
#define L2P_SIZE_REG     ((L2_EVENT_WIDTH+7)/8 + 1)    ///< byte size of char array carrying regular l2_pulse including size identifier
#define L2P_SIZE_VERB    (L2P_SIZE_REG + 1)            ///< same as above, verbose


/// Synapse controller specific stuff:
/** Address subranges within synapse memory controller */
#define SYN_SUB_WIDTH 2  ///< width of synapse control sub address range
#define SYN_SUB_POS   (CIP_ADDR_WIDTH - SYN_SUB_WIDTH)  ///< position within control interface address field
#define SYN_COLADDR_POS SYN_ROWADDR_WIDTH  ///< position of column address within control interface address field
#define SYN_ROWADDR_POS 0                  ///< position of row address within control interface address field
/** sub address range meanings */
#define SYN_SUB_WEIGHT    0  ///< synapse weights
#define SYN_SUB_ADDR_DEC  1  ///< synapse local addres decoder configuration
#define SYN_SUB_SYNDR     2  ///< syndriver mirrors
#define SYN_SUB_NRNBLD    3  ///< neuron builder

#define SYN_RAM_LATENCY   4  ///< latency of synapse memory accesses in units of HC_SYS_CLK_PER
#define SYN_ADD_LATENCY   4  ///< latency of synapse local addres decoder accesses in units of HC_SYS_CLK_PER
#define SYN_SYD_LATENCY   1  ///< latency of syndriver mirrors accesses in units of HC_SYS_CLK_PER
#define SYN_NBD_LATENCY   1  ///< latency of neuron builder accesses in units of HC_SYS_CLK_PER

#define SYN_DEC_WIDTH     4  ///< width of address information stored within synapse address decoder
#define SYN_DEC_TOP_POS  16  ///< the information for two decoder rows is stored in one packet: 

/// Analog Floating Gate specific stuff (specific parameter meanings are to be defined!)
#define AFG_PARAM_WIDTH    10  ///< resolution of analog parameters
#define AFG_PARAM_POS       0  ///< position of parameter data within ci packet data field

#define AFG_NUM_PARAMSETS 516  ///< total number of VI parameter sets per chip.
#define AFG_PSETADDR_WIDTH 10  ///< width of V/I parameter set address for one neuron
#define AFG_PSETADDR_POS    0  ///< position of AFG_PSETADDR within ci packet address field

#define AFG_NUM_V_PARAMS   12  ///< number of voltage parameters per neuron
#define AFG_NUM_I_PARAMS   12  ///< number of current parameters
#define AFG_VIADDR_WIDTH    5  ///< width of one specific V/I address within one parameter set
#define AFG_VIADDR_POS      AFG_PSETADDR_WIDTH  ///< position of AFG_VIADDR within ci packet address field


/// This is verilog stuff, needed for the SystemC wrapper
/// I think this section is obsolete. It was used by Tillmann for his verilog model (J Fieres)
#define CFG_WIDTH_SYNAPSE			3
#define CFG_WIDTH_UPP_LOW_SYN		1
#define CFG_WIDTH_WEIGHT_DECODE		1
#define CFG_WIDTH_SYNAPSE_GROUP		5
#define CFG_WIDTH_SYNAPSEGROUP_BLK	7
#define CFG_WIDTH_ANNCORE_MODULES 	3

#define CFG_BITS_SYNAPSE 3
#define CFG_BITS_UPP_LOW_SYN (CFG_BITS_SYNAPSE     + CFG_WIDTH_UPP_LOW_SYN) ///< 4
#define CFG_BITS_WEIGHT_DECODE (CFG_BITS_UPP_LOW_SYN + CFG_WIDTH_WEIGHT_DECODE) ///< 5
#define CFG_BITS_SYNAPSE_GROUP (CFG_BITS_WEIGHT_DECODE	+ CFG_WIDTH_SYNAPSE_GROUP) ///< 10
#define CFG_BITS_SYNAPSEGROUP_BLK (CFG_BITS_SYNAPSE_GROUP	+ CFG_WIDTH_SYNAPSEGROUP_BLK) ///< 17
#define CFG_BITS_ANNCORE (CFG_BITS_SYNAPSEGROUP_BLK + CFG_WIDTH_ANNCORE_MODULES)	///< 20
#define CFG_DATA_ANNCORE 8


/// l2 to l1 packet specification
#define L1_BUS_ADDR_WIDTH 6		///<layer1 target horizontal bus for neural event from dnc
#define L1_ADDR_WIDTH 6			///<neuron bits per layer 1 channel
#define L1_BUS_PERIOD_NS 1		///<speed of asynchronous layer1 communication
#define L1_ADDR_RANGE (1<<(L1_ADDR_WIDTH)) ///< range of l1 addresses
#define L1_TARGET_BUS_ID_WIDTH 7  ///< simulation only: Width of target L1 bus id, e.g. at input of L1 crossbar switch
#define NEURON_ON_L1_ADDR_WIDTH 6    ///< simulation only: Width of neuron id on layer1 bus
#define L1_FROM_DNCIF_ADDR_WIDTH 3   ///< simulation only: Width of layer1 bus id from DNCIF


//#define DELAY_WIDTH 10

#define LD_NRN_MAX 9     ///< ld(HICANN_COUNT*NRN_ANN)= logarithm of total neurons
#define PULSEDURATION 10 ///< Pulseduration in ns

// network structure equal to the l1/l2 channels
// #define HICANNS_PER_DNC 4
// #define DNCS_PER_DNC 9

// for dnc_if
#define DNC_IF_2_L2_BUFFERSIZE 10		///<number of elements that can be buffered in dnc_if until the release time to layer1 network is reached
#define DNC_IF_PER_HICANN 1				///<number of dnc_if per HICANN
#define MEM_ADDR_WIDTH 10	///<configuration of the HICANN: address width
#define MEM_DATA_WIDTH 48	///<configuration of the HICANN: data width
#define L2_CFGPKT_WIDTH 64	///<configuration of the HICANN: data width

// for layer1net
//#define WS_CHANNEL 4
//#define TO_WS_FIFO_SIZE  40
//#define L1_EVENT_WIDTH 32	// Timestamp + label

// layer1net verilog parameters for l1 bus counts and widths
#define ANNL1BUSCOUNT	        8		///<number of connections from anncore to layer1 horizontal bus
#define DNCL1BUSINCOUNT         8		///<number of busses connected from dnc_if to HICANNs horizontal bus
#define DNCL1BUSOUTCOUNT        8		///<number of layer1 busses which can be additionaly transfered to layer2
#define HORIZONTAL_L1_COUNT 	64		///<number of horizontal layer1 busses
#define VERTICAL_L1_COUNT		128		///<number of vertical layer1 busses (one side)
#define L1BUSWIDTH				2		///<number of wires per layer1 bus
#define SELL1BUSOUTCOUNT		SYNDRIVERS		///<number of busses to anncore from select switch (one side)
#define RTHCFGADDRWIDTH 		4		///<route through switch: configuration address width
#define RTHCFGDATAWIDTH 		32		///<route through switch: configuration data width
#define CROSSCFGADDRWIDTH		6		///<crossbar switch: configuration address width
#define CROSSCFGDATAWIDTH		7		///<crossbar switch: configuration data width
#define SELCFGADDRWIDTH 		7		///<select switch: configuration address width
#define SELCFGDATAWIDTH 		8		///<select switch: configuration data width
#define DNCIF_CFGADDRWIDTH 		1		///<dnc_if to layer1 connection: configuration address width
#define DNCIF_CFGDATAWIDTH 		8		///<dnc_if to layer1 connection: configuration data width
#define ANN_CFGADDRWIDTH 		3		///<anncore to layer1 connection: configuration address width
#define ANN_CFGDATAWIDTH		6		///<anncore to layer1 connection: configuration data width
#define SELL1BUSINCOUNT 		(2*VERTICAL_L1_COUNT) ///<number of input busses to select switch
#define RTHL1BUSINCOUNT 		(2*VERTICAL_L1_COUNT) ///<number of input busses to route through switch


// defines for MappingTool/Configurator
#define L1BANDWIDTH 64

#define VCHANNELS_PER_SIDE 128
#define VCHANNELS (2*VCHANNELS_PER_SIDE)
#define HCHANNELS 64

#define ANNCORE_COLUMNS 256
#define ANNCORE_ROWS_PER_SIDE 128
#define ANNCORE_ROWS (2*ANNCORE_ROWS_PER_SIDE)
#define ANNCORE_WTA 8

#define HICANN_CROSSBAR_STRIDE 4
#define HICANN_CROSSBAR_SPARSENESS 32
#define HICANN_SELECTSWITCH_STRIDE 1
#define HICANN_SELECTSWITCH_SPARSENESS 8

#define CHANNELS_PER_DNC 8
#define CHANNELS_PER_DNCIF 8

#define PARAM_COUNT_DENDRITE 24
#define MAX_NEURONS_PER_HICANN 512
#define MAX_INPUTS_PER_HICANN 16384

#define HICANNS_PER_DNC HICANNS_PER_RETICLE




#endif//__SIMDEF__
