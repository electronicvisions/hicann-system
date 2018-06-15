#ifndef HICANN_CFG_H
#define HICANN_CFG_H

#include <vector>
#include <utility> // for stl pair
#include <fstream>

#include <boost/serialization/export.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/nvp.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>

/// TODO - Use if poosible the enums and defines of the hardware_base include
#define NUMBER_FG_LINES 24
#define NUMBER_FG_COLUMNS 129
#define L1SWITCH_CONTROLS_PER_HICANN 6
#define REPEATER_CONTROLS_PER_HICANN 6
#define SYNAPSE_CONTROLS_PER_HICANN 2

#define unused(x) static_cast<void>((x))

/// Floating Gate Controler Default Parameter Values.
/// Default Values taken from tmak_fg (Hicann V2) and are in correspondance with AK's results in his diploma thesis
/// also, the documentaion and ranges stem from the diploma thesis
/// cf. also issue #740
namespace floating_gate_controler_default {
enum e {
	MAXCYCLE = 255, //!< Maximum number of write pulses, applied to a line in case there is a cell that could not reach its desired value. 8-bit range (0..255)
	CURRENT_WRITE_TIME = 1, //!< Basic length of the writing pulses on the time scale. 6-bit range (0..63)
	WOLTAGE_WRITE_TIME = 15, //!< Basic length of the writing pulses on the time scale. 6-bit range (0..63)
	READ_TIME = 63, //!< Time for the voltage at the comparator to settle (in clock cycles). Has a 6-bit range.
	ACCELERATOR_STEP = 9, //!< accelarator step. The range of this parameter is 6 bits.
	PULSE_LENGTH = 9, //!< The pulse lenghte determines the time scale on which the basic operations (e.g. write pulses and read cycles) are performed by changing the controller clock. 4-bit range
	FG_BIAS_N = 5, //!< this parameter controls the voltage on the VB transistor gate (see Dipl. of A.Kononov fig. 2.4). The range is 4 bits.
	FG_BIAS = 8 //!< ???
  };
}

/// Default values for the PLL used during floating gate programming and during normal experiment operation
/// 200 MHz clock for normal operation
/// 100 MHz clock for floating gate programming
/// default values result from issue #740 for Release 1
namespace pll_setting_default {
	enum e {
		PLL_MULT_EXPERIMENT = 4, //!< PLL multiplicator during experiment run (i.e. PLL = 200 Mhz)
		PLL_DIV_EXPERIMENT = 1, //!< PLL divisor during experiment run (i.e. PLL = 200 Mhz)
		CLK_EXPERIMENT_IN_NS = 5, //!< sys clock period during experiment run (i.e. PLL = 200 Mhz)
		PLL_MULT_FGPROGRAMMING = 2, //!< PLL multiplicator during floating gate programming run (i.e. PLL = 100 Mhz)
		PLL_DIV_FGPROGRAMMING = 1, //!< PLL divisor during floating gate programming run (i.e. PLL = 100 Mhz)
		CLK_FGPROGRAMMING_IN_NS = 10 //!< sys clock period during floating gate programming run (i.e. PLL = 100 Mhz)
	};
}


/** Configuration for one row of a synapse driver.
 * holds all config data that is specific to one row (bottom or row) of a synapse driver.
 */
struct syndr_row_cfg_t {
	bool senx; //!< connect row to excitatory input of neuron
	bool seni; //!< connect row to inhibitory input of neuron
	unsigned char selgmax; //!< select V_gmax from global FGs of . Values: 0-3
	unsigned char gmax_div; //!< g_max divisor for connection to excitatory input of neurons. Values: 0-15
	unsigned char preout_even; //!< defines, to which upper 2 bit of a spl1 the EVEN neuron of a row listen to. Values: 0-3
	unsigned char preout_odd; //!< defines, to which upper 2 bit of a spl1 the ODD neuron of a row listen to. Values: 0-3

	syndr_row_cfg_t():
		senx(0), seni(0), selgmax(0), gmax_div(1), preout_even(0), preout_odd(0) {}

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(senx);
		ar & BOOST_SERIALIZATION_NVP(seni);
		ar & BOOST_SERIALIZATION_NVP(selgmax);
		ar & BOOST_SERIALIZATION_NVP(gmax_div);
		ar & BOOST_SERIALIZATION_NVP(preout_even);
		ar & BOOST_SERIALIZATION_NVP(preout_odd);
	}
};

/** Configuration of one synapse driver.
 * holds the whole config data for one synapse driver.
 * This contains the config for the two rows (bottom and top),
 * as well as the config that is shared for the whole synapse driver.
 * Note:
 * "bottom" always refers to the row which is closer to the center of the chip.
 * So when looking at the bottom half of the chip, the bottom row is physically above the top row.
 */
struct syndriver_cfg_t {

	bool enable; //!< enable this synapse driver
	bool topin;  //!< select input from top neighbour switch
	bool locin;  //!< select input from L1
	bool stp_enable; //!< enable short-term plasticity (STP) for this syndriver
	bool stp_mode; //!< set STP mode for this syndriver. 0: facilitation, 1: depression
	unsigned char stp_cap; //!< select size of capacitor in STP circuit. Values??? (TODO)
	syndr_row_cfg_t bottom_row_cfg; //!< config for bottom row
	syndr_row_cfg_t top_row_cfg; //!< config for top row

	syndriver_cfg_t(): enable(0),topin(0),locin(0),stp_enable(0),stp_mode(0),stp_cap(0)
		,bottom_row_cfg(syndr_row_cfg_t()),top_row_cfg(syndr_row_cfg_t()) {}

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(enable);
		ar & BOOST_SERIALIZATION_NVP(topin);
		ar & BOOST_SERIALIZATION_NVP(locin);
		ar & BOOST_SERIALIZATION_NVP(stp_enable);
		ar & BOOST_SERIALIZATION_NVP(stp_mode);
		ar & BOOST_SERIALIZATION_NVP(stp_cap);
		ar & BOOST_SERIALIZATION_NVP(bottom_row_cfg);
		ar & BOOST_SERIALIZATION_NVP(top_row_cfg);
	}

	bool operator==(syndriver_cfg_t const& x) const { return this == &x; }
};

/// enum for the interconnections of denmems.
namespace  denmem_connections {
	enum e {
		off = 0, //!< denmems are not connected
		to_left = 1, //!< fire signal is forwarded to the left: left denmem is "driven" from right.
		to_right = 2, //!< fire signal is forwarded to the right:right denmem is "driven" from left
		both = 3      //!< denmems interconnected bidirectionally
	};
}

/// global config for a denmem block (top / bottom )
struct denmem_block_cfg_t {
	bool bigcap; //!< use big capacity for all neurons of the block (default: 1)
	bool slow_I_radapt; //!< slow bit for I_radapt (default: 0)
	bool fast_I_radapt; //!< fast bit for I_radapt (default: 0)
	bool slow_I_gladapt; //!< slow bit for I_gladapt (default: 0)
	bool fast_I_gladapt; //!< fast bit for I_gladapt (default: 0)
	bool slow_I_gl; //!< slow bit for I_gl (default: 0)
	bool fast_I_gl; //!< fast bit for I_gl (default: 0)

	denmem_block_cfg_t():
		bigcap(1), slow_I_radapt(0), fast_I_radapt(0), slow_I_gladapt(0), fast_I_gladapt(0), slow_I_gl(0), fast_I_gl(0) {}

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(bigcap);
		ar & BOOST_SERIALIZATION_NVP(slow_I_radapt);
		ar & BOOST_SERIALIZATION_NVP(fast_I_radapt);
		ar & BOOST_SERIALIZATION_NVP(slow_I_gladapt);
		ar & BOOST_SERIALIZATION_NVP(fast_I_gladapt);
		ar & BOOST_SERIALIZATION_NVP(slow_I_gl);
		ar & BOOST_SERIALIZATION_NVP(fast_I_gl);
	}
};


// neuron builder control
// same as in "hardware_base.h"
namespace nc_merger_names {enum e {BG0=0x1, BG1=0x2, BG2=0x4, BG3=0x8, BG4=0x10, BG5=0x20, BG6=0x40, BG7=0x80, L0_0=0x100, L0_1=0x200, L0_2=0x400, L0_3=0x800, L1_0=0x1000, L1_1=0x2000, L2_0=0x4000, DNC0=0x8001, DNC1=0x8002, DNC2=0x8004, DNC3=0x8008, DNC4=0x8010, DNC5=0x8020, DNC6=0x8040, DNC7=0x8080, MERG_ALL=0x4fff};
}

struct merger_config_t {
	bool enable; // enable merging
	bool select; // select input 0 or 1, only considered when enable=0
	bool slow; // has to be enabled for DNC Merger connecting to SPL1 Repeater
	merger_config_t():enable(0),select(0),slow(0){}
	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(enable);
		ar & BOOST_SERIALIZATION_NVP(select);
		ar & BOOST_SERIALIZATION_NVP(slow);
	}
};

struct background_generator_t {
	unsigned int period; //!< period (16-bit)
	unsigned int address; //!< l1 address (6-bit)
	bool random; //!< random(1) or regular(0)
	bool reset_n; //!< enable this background generator;
	background_generator_t (): period(1),address(0),random(0),reset_n(0){}
	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(period);
		ar & BOOST_SERIALIZATION_NVP(address);
		ar & BOOST_SERIALIZATION_NVP(random);
		ar & BOOST_SERIALIZATION_NVP(reset_n);
	}
};

/** enum for storing the configuration of a repeater. The last 3 choices(spl1_X) are only valid for the spl1-repeaters in the left repeater block.*/
namespace repeater_config{
	enum e{
		off = 0, //!< repeater is off (used as a normal repeater)
		outwards = 1, //!< repeater reproduces signal from this hicann for the neighbour neighbour (used as a normal repeater)
		inwards = 2, //!< repeater reproduces signal from neighbour hicann for this hicann (used as a normal repeater)
		spl1_int = 3, //!< input from spl1-merger is applied to the bus on this hicann.
		spl1_ext = 4, //!< input from spl1-merger is applied to the bus on the left neighbour hicann.
		spl1_int_and_ext = 5 //!< input from spl1-merger is applied to both direction(bus on this and the bus on the left neighbour hicann)
	};
}


// configuration struct that fully describes a (single) mapped hicann
struct hicann_cfg_t {
	unsigned int xcoord, ycoord;


	////////////////////////////////////////////
	// NeuronBuilderControl
	////////////////////////////////////////////
	//
	// Note: The counting of denmems in the mapping goes as following:
	// There are 256 upper and 256 lower dendrites.
	// Counting goes from left to right, upper block first.

	/** std::vector filled with the config for the switches between top denmems.
	 * this is a dense std::vector with size 255(!!!)
	 * switch[0] is the switch between denmem 0 and 1. (counting as described above)
	 * switch[1] is the switch between denmem 1 and 2. (counting as described above)
	 * config of these switches can be either "off", "to_left" or "to_right".
	 * Hence, here we don't consider that the switches within a denmem pair are undirected,
	 * and the others are directed. Here all enabled switches are directed,
	 * which can be easily transformed into an unambiguous hardware configuration.
	 */
	std::vector< denmem_connections::e > denmem_switches_top;

	/** std::vector to be filled with 'vertical' switches between denmems of top and bottom block.
	 * this is a dense std::vector with size 256
	 * switch[X] = true means, that connection between denmems X and 256+x is enabled.
	 * Switch X connects denmem X of upper block and denemm X of lower block
	 */
	std::vector< bool > denmem_switches_vertical;

	/** std::vector filled with the config for the switches between bottom denmems.
	 * this is a dense std::vector with size 255(!!!)
	 * switch[0] is the switch between denmem 256 and 257. (counting as described above)
	 * switch[1] is the switch between denmem 257 and 258. (counting as described above)
	 * config of these switches can be either "off", "to_left" or "to_right".
	 * Hence, here we don't consider that the switches within a denmem pair are undirected,
	 * and the others are directed. Here all enabled switches are directed,
	 * which can be easily transformed into an unambiguous hardware configuration.
	 */
	std::vector< denmem_connections::e > denmem_switches_bottom;

	// map holding a pair of denmem indices, which shall fire (notifiy a spike) and the corresponding l1 addresses.
	// Note: we use a MAP here instead of a std::vector in order to ensure that there is only one l1 address entry per 
	// firing denmem.
	// denmem ids range from 0..511, counting is from left to right, upper denmem block first.
	// valid addresses are 0..63
	std::map< unsigned int, unsigned int > firing_denmems_and_addresses;


	// global config of top and bottom denmem block
	// for details see doc of: denmem_block_cfg_t
	denmem_block_cfg_t denmem_block_config_top;
	denmem_block_cfg_t denmem_block_config_bottom;


	// current input to denmem mapping.
	// for each floating gate bock, it holds the target denmem, that shall receive 
	// current input.
	// pair.first  : enable current input
	// pair.second : target denmem (counting from left to right, upper block first)
	// Note that EVEN denmems can receive input from the LEFT floating gate block,
	// and ODD denmems from the RIGHT FG-block.
	// These four containers ensure that not more than one denmem is connected 
	// to a current source, avoiding a short-circuit.
	std::pair< bool, unsigned int > current_input_enable_and_target_denmem_top_left; // possible tgts: EVEN in range 0..255
	std::pair< bool, unsigned int > current_input_enable_and_target_denmem_top_right; // possible tgts: ODD in range 0..255
	std::pair< bool, unsigned int > current_input_enable_and_target_denmem_bottom_left; // possible tgts: EVEN in range 256..511
	std::pair< bool, unsigned int > current_input_enable_and_target_denmem_bottom_right; // possible tgts: ODD in range 256..511

	// enable the analog output OPs 0 and 1 and select the source denmem.
	// pair.first  : enable output 50 Ohm output buffer.
	// pair.second : the source denmem id.
	// denmem_ids range from 0..511 for BOTH output buffers.
	// CAUTION: On a reticle the 8 output buffers of the 8 HICANNs are ORed. So make sure, that each buffer is enabled on maximum one HICANN per reticle.
	std::pair< bool, unsigned int > analog_output_enable_and_source_denmem_0; // output buffer 0 (also called top)
	std::pair< bool, unsigned int > analog_output_enable_and_source_denmem_1; // output buffer 0 (also called bottom)


	/////////////////////////////////////////////
	// L1SwitchControl config
	/////////////////////////////////////////////
	typedef unsigned int HorizontalLineID;
	typedef unsigned int HorizontalBusID;
	typedef unsigned int VerticalBusID;

	// configuration of the a crossbar switches
	// HorizontalBusID: see Hicann Spec 0-63
	// VerticalBusID: see Hicann Spec also rechts: 128-255 und links 0-127
	 std::vector< std::pair<HorizontalBusID, VerticalBusID> > crossbar_left;
	 std::vector< std::pair<HorizontalBusID, VerticalBusID> > crossbar_right;

	// config of the syndriver switches
	// HorizontalLineID: 0..111
	// VerticalBusID: see Hicann Spec also rechts: 128-255 und links 0-127
	 std::vector< std::pair<HorizontalLineID, VerticalBusID> > syndriver_switch_top_left;
	 std::vector< std::pair<HorizontalLineID, VerticalBusID> > syndriver_switch_top_right;
	 std::vector< std::pair<HorizontalLineID, VerticalBusID> > syndriver_switch_bottom_left;
	 std::vector< std::pair<HorizontalLineID, VerticalBusID> > syndriver_switch_bottom_right;

	/////////////////////////////////////////////
	// SynapseControl config
	/////////////////////////////////////////////

	// configuration of the syndrivers for each quadrant.
	// possible indices are 0..55
	// counting starts in the center of the chip
	std::vector< syndriver_cfg_t > syndriver_config_top_left;
	std::vector< syndriver_cfg_t > syndriver_config_top_right;
	std::vector< syndriver_cfg_t > syndriver_config_bottom_left;
	std::vector< syndriver_cfg_t > syndriver_config_bottom_right;

	/// synapse addresses and decoders for the two blocks
	/// for both, a dense matrix is returned, filled with values between 0 and 15
	/// counting is rows-first, always starting at the center of the chip, and from left to right.
	/// rows go from 0..223, per row there are 256 synapses
	/// One could also only return rows that contain enabled synapses,
	/// using a map of std::vectors, such that less rows need to be configured.
	/// This information can also be derived from the synapse driver config (member enable)
	std::vector< std::vector< unsigned char > > synaptic_weights_top;
	std::vector< std::vector< unsigned char > > synaptic_decoders_top;
	std::vector< std::vector< unsigned char > > synaptic_weights_bottom;
	std::vector< std::vector< unsigned char > > synaptic_decoders_bottom;


	/////////////////////////////////////////////
	// RepeaterControl config
	/////////////////////////////////////////////

	// holds for each repeater the configuration
	// vertical repeaters have size 64, horizontal size 32
	std::vector<repeater_config::e> repeater_top_left;
	std::vector<repeater_config::e> repeater_top_right;
	std::vector<repeater_config::e> repeater_mid_left;
	std::vector<repeater_config::e> repeater_mid_right;
	std::vector<repeater_config::e> repeater_bottom_left;
	std::vector<repeater_config::e> repeater_bottom_right;


	/////////////////////////////////////////////
	// NeuronControl config
	/////////////////////////////////////////////

	/** configuration of the SPL1 Output Merger (also called neuroncontrol).
	 * This includes the config of the Mergers, the Background Event Generators and the connection to and from the DNC Interface channels.
	 * configuration is returned according to HICANN Specification.
	 * with one std::vector<bool> corresponding to one config address.
	 * the data_width = 16 -> inner std::vector has size 16.
	 */
	std::vector< std::vector<bool> > spl1_output_merger;


	/** configuration of the mergers in neuron control.
	 *  contains for each merger the associated config
	 *  with boolean enable, select and slow.
	 */
	std::map< nc_merger_names::e, merger_config_t > neuron_control_mergers;

	/** configuration of all background generators.
	 * size of std::vector is 8.
	 * Counting of BG-Event-Generators is as in Hicann-Doc (from right to left)
	 */
	std::vector< background_generator_t > background_generators;


	/// directions of the DNC interfaces on a HICANN.
	/// returns the configured direction settings of the DNC interfaces on the specified HICANN
	/// the return std::vector has 8 entries; a 'true' entry means downward direction (DNC->HICANN).
	/// dnc_if_directions[0] corresponds to DNC-HICANN channel 0
	std::vector<bool> dnc_if_channels_direction;

	/// enable of the 8 channels between the DNC and this HICANN.
	/// the return std::vector has 8 entries; a 'true' entry means enabled.
	std::vector<bool> dnc_if_channels_enable;


	/////////////////////////////////////////////
	// FGControl config (Floating Gate)
	/////////////////////////////////////////////

	/// flag, whether floating gates shall be programmed (true) or not (false)
	/// default: True
	bool program_floating_gates;

	// array of neuron floating gate parameters for each quadrant.
	// enumeration is up-left=0, up-right=1, down-left=2, down-right=3
	//
	// std::vector with DAC-Values for the neurons.
	// size of outer std::vector = 512
	// Counting: from left to right, upper block first
	// Size of inner std::vector is 24.
	// For the access of single entries in the inner std::vector
	// use enum defined in mappingtool/include/ParameterMappingAlgorithms/stage2/floating_gate_def.h
	// TODO(BV): include this header file!
	// @note within each block, the neurons are addressed like in the FG-Control Module:
	//       i.e. for the right block, neuron 0 is the rightmost neuron and 127 the leftmost.
	//       and in the left block, neuron 0 is the leftmost and 127 the rightmost within the block.
	std::vector< std::vector<int> > neuron_floating_gates[4];

	// array of global floating gate parameters for each quadrant.
	// enumeration is up-left=0, up-right=1, down-left=2, down-right=3
	//
	// std::vector with DAC-Values for the 4 global floating gates blocks
	// For the access of single entries in the inner std::vector
	// use enum defined in mappingtool/include/ParameterMappingAlgorithms/stage2/floating_gate_def.h
	// TODO(BV): include this header file!
	std::vector<int> global_floating_gates[4];


	// std::vector holding the sequence of current amplitudes, that are used as current stimulus.
	// size of std::vector = 129. max value = 1023
	std::vector<unsigned int> current_stimulus_top_left;
	std::vector<unsigned int> current_stimulus_top_right;
	std::vector<unsigned int> current_stimulus_bottom_left;
	std::vector<unsigned int> current_stimulus_bottom_right;

	// the pulselength for the current stimulus.
	// max value = 15
	// defines the number of slow clock cycles(16ns), for which
	// each value of the stimulation std::vector is applied.
	// duration of one value = (pulselength+1)*16ns
	unsigned int current_stimulus_top_left_pulselength;
	unsigned int current_stimulus_top_right_pulselength;
	unsigned int current_stimulus_bottom_left_pulselength;
	unsigned int current_stimulus_bottom_right_pulselength;


	// add new stuff to serialize() below too!!!

	bool finished;

	hicann_cfg_t(unsigned int x, unsigned int y) :
		xcoord(x), ycoord(y), program_floating_gates(true), finished(false)
	{}

	hicann_cfg_t(): // for serialization
		xcoord(0), ycoord(0), program_floating_gates(true), finished(false)
	{}

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(xcoord);
		ar & BOOST_SERIALIZATION_NVP(ycoord);
		ar & BOOST_SERIALIZATION_NVP(denmem_switches_top);
		ar & BOOST_SERIALIZATION_NVP(denmem_switches_vertical);
		ar & BOOST_SERIALIZATION_NVP(denmem_switches_bottom);
		ar & BOOST_SERIALIZATION_NVP(denmem_block_config_top);
		ar & BOOST_SERIALIZATION_NVP(denmem_block_config_bottom);
		ar & BOOST_SERIALIZATION_NVP(current_input_enable_and_target_denmem_top_left);
		ar & BOOST_SERIALIZATION_NVP(current_input_enable_and_target_denmem_top_right);
		ar & BOOST_SERIALIZATION_NVP(current_input_enable_and_target_denmem_bottom_left);
		ar & BOOST_SERIALIZATION_NVP(current_input_enable_and_target_denmem_bottom_right);
		ar & BOOST_SERIALIZATION_NVP(analog_output_enable_and_source_denmem_0);
		ar & BOOST_SERIALIZATION_NVP(analog_output_enable_and_source_denmem_1);
		ar & BOOST_SERIALIZATION_NVP(synaptic_weights_top);
		ar & BOOST_SERIALIZATION_NVP(synaptic_decoders_top);
		ar & BOOST_SERIALIZATION_NVP(synaptic_weights_bottom);
		ar & BOOST_SERIALIZATION_NVP(synaptic_decoders_bottom);
		ar & BOOST_SERIALIZATION_NVP(crossbar_left);
		ar & BOOST_SERIALIZATION_NVP(crossbar_right);
		ar & BOOST_SERIALIZATION_NVP(syndriver_switch_top_left);
		ar & BOOST_SERIALIZATION_NVP(syndriver_switch_top_right);
		ar & BOOST_SERIALIZATION_NVP(syndriver_switch_bottom_left);
		ar & BOOST_SERIALIZATION_NVP(syndriver_switch_bottom_right);
		ar & BOOST_SERIALIZATION_NVP(syndriver_config_top_left);
		ar & BOOST_SERIALIZATION_NVP(syndriver_config_top_right);
		ar & BOOST_SERIALIZATION_NVP(syndriver_config_bottom_left);
		ar & BOOST_SERIALIZATION_NVP(syndriver_config_bottom_right);
		ar & BOOST_SERIALIZATION_NVP(repeater_top_left);
		ar & BOOST_SERIALIZATION_NVP(repeater_top_right);
		ar & BOOST_SERIALIZATION_NVP(repeater_mid_left);
		ar & BOOST_SERIALIZATION_NVP(repeater_mid_right);
		ar & BOOST_SERIALIZATION_NVP(repeater_bottom_left);
		ar & BOOST_SERIALIZATION_NVP(repeater_bottom_right);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_top_left);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_top_right);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_bottom_left);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_bottom_right);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_top_left_pulselength);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_top_right_pulselength);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_bottom_left_pulselength);
		ar & BOOST_SERIALIZATION_NVP(current_stimulus_bottom_right_pulselength);
		ar & BOOST_SERIALIZATION_NVP(spl1_output_merger);
		ar & BOOST_SERIALIZATION_NVP(neuron_control_mergers);
		ar & BOOST_SERIALIZATION_NVP(background_generators);
		ar & BOOST_SERIALIZATION_NVP(dnc_if_channels_direction);
		ar & BOOST_SERIALIZATION_NVP(dnc_if_channels_enable);
		ar & BOOST_SERIALIZATION_NVP(program_floating_gates);
		ar & BOOST_SERIALIZATION_NVP(neuron_floating_gates);
		ar & BOOST_SERIALIZATION_NVP(global_floating_gates);
		ar & BOOST_SERIALIZATION_NVP(firing_denmems_and_addresses);
		ar & BOOST_SERIALIZATION_NVP(finished);
	}

	template<typename Archiver>
	void serialize(Archiver& ar) { // SF version
		serialize(ar, 0); // call other one
	}
};

/// defines the address of a l2 pulse in the system.
/// address works for both sources and targets.
struct l2_pulse_address_t {
	union {
		struct
		{
			unsigned l1_address : 6;  //!< address of the pulse on the layer 1 bus 
			unsigned dnc_if_channel: 3;  //!< dnc if channel id 
			unsigned hicann_id : 3;//!< identifier of the HICANN connected to the DNC
			unsigned dnc_id : 2;   //!< identifier of the DNC on the FPGA board
			unsigned fpga_id : 4;  //!< identifier of the FPGA board on the wafer
			unsigned wafer_id : 4; //!< identifier of the wafer in the system
		} fields;
		unsigned int integer; //!< the unsigned integer representation of the address
	} data;

	/// convenience constructor: set address values from single integer
	l2_pulse_address_t(unsigned int set_addr=0){data.integer = set_addr;}
	bool operator==(const l2_pulse_address_t &t) const
	{
		return data.integer == t.data.integer;
	}
	bool operator<(const l2_pulse_address_t &t) const
	{
		return data.integer < t.data.integer;
	}
	// getter and settes for boost python wrapping
	unsigned get_l1_address() const { return data.fields.l1_address;}
	void set_l1_address(unsigned val) { data.fields.l1_address = val;}
	unsigned get_dnc_if_channel() const { return data.fields.dnc_if_channel;}
	void set_dnc_if_channel(unsigned val) { data.fields.dnc_if_channel = val;}
	unsigned get_hicann_id() const { return data.fields.hicann_id;}
	void set_hicann_id(unsigned val) { data.fields.hicann_id = val;}
	unsigned get_dnc_id() const { return data.fields.dnc_id;}
	void set_dnc_id(unsigned val) { data.fields.dnc_id = val;}
	unsigned get_fpga_id() const { return data.fields.fpga_id;}
	void set_fpga_id(unsigned val) { data.fields.fpga_id = val;}
	unsigned get_wafer_id() const { return data.fields.wafer_id;}
	void set_wafer_id(unsigned val) { data.fields.wafer_id = val;}
	unsigned get_integer() const { return data.integer;}
	void set_integer(unsigned val) { data.integer = val;}
};

// mark union l2_pulse_address_t as serializable
BOOST_CLASS_IMPLEMENTATION(l2_pulse_address_t, boost::serialization::object_serializable)

template<typename Archiver>
void serialize(Archiver& ar, l2_pulse_address_t & pa, const unsigned int& version) {
	unused(version);
	ar & boost::serialization::make_nvp("integer",pa.data.integer);
}


/** defines configuration information for one pulse communication subgroup (PCS), consisting of one FPGA and 4 DNCs.
*/

struct pcs_cfg_t
{
	bool enable_dnc_loopback; //!< enables loopback of pulses in DNC (pulses are looped back on the same channel)
	bool enable_hicann_loopback; //!< enables loopback of pulses in HICANN (even channels are connected to the subsequent odd ones, i.e. 0 down -> 1 up,...)
	bool enable_playback_loop; //!< enables loop mode of playback memory: having played back all pulses, it starts over again
	bool enable_fpga_feedback; //!< enables feedback of upstream pulses in the FPGA (only available for HICANN 0): pulses can be sent back to two target channels
	unsigned int feedback_in; //!< defines channel from which pulses are fed back in the FPGA
	unsigned int feedback_out0; //!< first feedback output channel
	unsigned int feedback_out1; //!< second feedback output channel
	double feedback_delay0; //!< delay of first feedback in FPGA (no transmission delays included)
	double feedback_delay1; //!< delay of second feedback in FPGA (no transmission delays included)

	pcs_cfg_t() : enable_dnc_loopback(false), enable_hicann_loopback(false), enable_playback_loop(false), enable_fpga_feedback(false)
    {}

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, const unsigned int& version) {
		unused(version);
		ar & BOOST_SERIALIZATION_NVP(enable_dnc_loopback);
		ar & BOOST_SERIALIZATION_NVP(enable_hicann_loopback);
		ar & BOOST_SERIALIZATION_NVP(enable_playback_loop);
		ar & BOOST_SERIALIZATION_NVP(enable_fpga_feedback);
		ar & BOOST_SERIALIZATION_NVP(feedback_in);
		ar & BOOST_SERIALIZATION_NVP(feedback_out0);
		ar & BOOST_SERIALIZATION_NVP(feedback_out1);
		ar & BOOST_SERIALIZATION_NVP(feedback_delay0);
		ar & BOOST_SERIALIZATION_NVP(feedback_delay1);
	}
};

/// saves any serializable struct to a text file.
/// requires that the mySerializableClass contains
///		friend class boost::serialization::access;
/// 	template<typename Archiver>
/// 	void serialize(Archiver& ar, const unsigned int& version);
template < class mySerializableClass >
void save_config_to_text_file(mySerializableClass & instance, std::string filename ) {
	std::ofstream ofs(filename.c_str());
	boost::archive::text_oarchive oa(ofs);
	oa <<  instance;
}

/// saves any serializable to a xml file
template < class mySerializableClass >
void save_config_to_xml_file(mySerializableClass & instance, std::string filename ) {
	std::ofstream ofs(filename.c_str());
	boost::archive::xml_oarchive oa(ofs);
	oa << BOOST_SERIALIZATION_NVP(instance);
}

/// loads any serializable struct from a xml file.
/// requires that the mySerializableClass contains
///		friend class boost::serialization::access;
/// 	template<typename Archiver>
/// 	void serialize(Archiver& ar, const unsigned int& version);
template < class mySerializableClass >
void load_config_from_xml_file(mySerializableClass & instance, std::string filename ) {
	std::ifstream ifs(filename.c_str());
	boost::archive::xml_iarchive ia(ifs);
	ia >> BOOST_SERIALIZATION_NVP(instance);
}


/// loads any serializable struct from a text file.
/// requires that the mySerializableClass contains
///		friend class boost::serialization::access;
/// 	template<typename Archiver>
/// 	void serialize(Archiver& ar, const unsigned int& version);
template < class mySerializableClass >
void load_config_from_text_file(mySerializableClass & instance, std::string filename ) {
	std::ifstream ifs(filename.c_str());
	boost::archive::text_iarchive ia(ifs);
	ia >> instance;
}

/// saves any serializable struct to a binary file.
/// requires that the mySerializableClass contains
///		friend class boost::serialization::access;
/// 	template<typename Archiver>
/// 	void serialize(Archiver& ar, const unsigned int& version);
template < class mySerializableClass >
void save_config_to_binary_file(mySerializableClass & instance, std::string filename ) {
	std::ofstream ofs(filename.c_str());
	boost::archive::binary_oarchive oa(ofs);
	oa <<  instance;
}

/// loads any serializable struct from a binary file.
/// requires that the mySerializableClass contains
///		friend class boost::serialization::access;
/// 	template<typename Archiver>
/// 	void serialize(Archiver& ar, const unsigned int& version);
template < class mySerializableClass >
void load_config_from_binary_file(mySerializableClass & instance, const std::string filename ) {
	std::ifstream ifs(filename.c_str());
	boost::archive::binary_iarchive ia(ifs);
	ia >> instance;
}
#endif
