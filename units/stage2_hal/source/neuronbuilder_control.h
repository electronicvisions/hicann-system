#ifndef NEURONBUILDER_CONTROL_H
#define NEURONBUILDER_CONTROL_H

// Company         :   kip
// Author          :   Sebastian Millner
// E-Mail          :   smillner@kip.uni-heidelberg.de
//
// Filename        :   neuronbuilder_control.h
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Mon Jan 11 10
// Last Change     :   Mon May 04 12
// by              :   akononov
//------------------------------------------------------------------------

#include <vector>
#include <map>

#include "ctrlmod.h"
#include "logger.h"

namespace facets {

/// TODO: Replace the ??? comments
/// TODO: Put the parameter defaults in header
/// TODO: Move internal functions to private

/// NeuronBuilderControl control class.
/// controls all digital parameters of the denmems.

// bit-fields for the sram configuration
union PairConfiguration  {
	struct {
		unsigned inout:3;
		unsigned interconnect_neuron_pair : 1;
		unsigned activate_firing_left : 1;
		unsigned enable_left_fire_input : 1;
		unsigned enable_right_fire_input : 1;
		unsigned activate_firing_right : 1;
	} bits;
	unsigned integer;
};

union NBData {
	struct {
		unsigned spl1 : spl1_numd;
		unsigned nmem_top : n_numd; // is used as PairConfiguration
		unsigned nmem_bot : n_numd; // is used as PairConfiguration
		unsigned vertical : 1;
		unsigned firet : 1;
		unsigned fireb : 1;
	} bits;
	unsigned integer;
};

union InOutData {
	struct{
		unsigned aout_r:1;
		unsigned aout_l:1;
		unsigned currentin_r:1;
		unsigned currentin_l:1;
	} bits;
	unsigned integer;
};

class NeuronBuilderControl : public CtrlModule
{
public:
	typedef std::map<unsigned int, unsigned int> iomap_t;	
	static iomap_t const& iomap() { return IOmap_; }

	static const uint MAXADR; //!< range of neuronbuilder sram, i. e. last memory address (calculated from address bus width)
	static const uint NREGBASE; //!< address of neuron register
	static const uint OUTREGBASE; //!< address of output register

private:
	// associative memory for the inout addresses.
	// maps the 4-bit inout-cinfiguration back to 3-bit representation
	static iomap_t IOmap_;


	virtual std::string ClassName() { return "NeuronBuilderControl"; };
	Logger& log; //!< reference to the global logger
	
	// TODO replace bool std::vectors. They are neither stl std::vectors nor containing booleans o_O
    /// holds the configuration of the output multiplexer.
	/// ten bits each for output 1 and 2
	std::vector<bool> outputcfg_;

	/// holds the of global neuron config.
	/// i.e. the switches for the big capacity,
	/// the current divisors, and the neuron and spl1 reset
	std::vector<bool> neuroncfg_;
	std::vector<NBData> sram_;
	std::vector<InOutData> inout_top_;
	std::vector<InOutData> inout_bot_;

	// helper function found in tmak_wafertest. known to create a correct sram entry.
	// used to check for correctness of the sram
	inline unsigned int nbdata(unsigned int fireb,
	                           unsigned int firet,
	                           unsigned int vertical,
	                           unsigned int ntop,
	                           unsigned int nbot,
	                           unsigned int spl1){
		//unsigned int num=0;
		//for(int i=0; i<6; i++) num |= (spl1 & (1<<i))?(1<<(5-i)):0; //reverse bit order bug
		//spl1=num;
		return ((4* fireb + 2*firet + vertical)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;
	}

public:
	/// give pointer to utilized LinkLayer, start and range of this module's address range
	NeuronBuilderControl(
			Stage2Ctrl* c,  //!< pointer to parent Stage2Ctrl class (i.e. HicannCtrl)
			uint ta, //!< associated OCP tagid 
	    uint sa,  //!< Startaddress (OCP)
	    uint ma   //!< Maximumadress (OCP)
	    );

	~NeuronBuilderControl();

	/// write data to ram or registers from bool std::vector
	int write_data(
	    uint rowaddr,  //!< Write to this address
	    std::vector<bool> cfg  //!< Data as bool std::vector
	    );

	/// write data to ram or registers with from uint
	int write_data(
	    uint rowaddr,  //!< Write to this address
	    uint cfg  //!< Data as uint
	    );

	/// writes entire content of sram
	void write_sram();

	/// sets all entries in sram struct to zero
	inline void clear_sram(){
		for (uint i=0; i<=MAXADR; ++i){
		  sram_.at(i).integer = 0;
		  inout_top_.at(i).integer=0;
		  inout_bot_.at(i).integer=0;
		}
	}

	/// check for integrity and correctness of sram
	bool check_sram();

	void print_sram(std::ostream &os);

	// set the spl1 address under which a denmem should fire
	void set_spl1address(unsigned int denmem_i, unsigned int spl1);

	// enable firing for denmem
	void enable_firing(unsigned int denmem_i);

	// set vertical connection between denmem pairs (top<->bottom)
	// denmem index should therefore range from 0 to MAXADR/2
	void connect_vertical(unsigned int denmem_i);

	// establish horizontal connections between denmems.
	// connects denmem  denmem_id with denmem_id+1
	// denmem index should therefore range from 0 to (MAXADR/2 - 1)
	// directions:
	//   1: direction to the right, i.e. "enable_left_fire_input" for right denmem
	//  -1: direction to the left, i.e. "enable_right_fire_input" for left denmem
	// Note that direction is only considered for odd denmem_ids, in other cases
	// the connection is undirected within a NeuronPair
	void connect_horizontal_top(unsigned int denmem_id, int direction);
	void connect_horizontal_bottom(unsigned int denmem_id, int direction);

	// analog input and output functions.
	// these functions write only to the internal memory, not the sram.
	// use write_sram to do this!
	void set_aout(unsigned int denmem_id);
	void set_currentin(unsigned int denmem_id);

	/// issue read command to read back data.
	/// may lead to bit flips, so use with care
	int read_data(
	    uint rowaddr  //!< initiate read from this address
	    );

    /// read delay, data valid after enable
	int writeTRD(
	    uint trd  //!< ???
	    );

    /// read delay, data valid after enable
	int readTRD();

    /// TSU(bit 7:4):setup time for address or data before enable -1,
    /// i.e. setting tsu=0 results in a setup time of one enable
    /// pulse width -1; TWR(bit 3:0): enablepulse width -1
	int writeTSUTWR(
	    uint data  //!< ???
	    );

    /// TSU(bit 7:4):setup time for address or data before enable -1,
    /// i.e. setting tsu=0 results in a setup time of one enable
    /// pulse width -1; TWR(bit 3:0): enablepulse width -1
	int readTSUTWR();

    /// sets all ram bits in neurons, neuron builder and spl1 to zero.
	int initzeros();

    /// program neuron builder control config register.
	int configNeuronsGbl();

    /// program neuron builder control output register.
	int configOutputs();

    /// choose which element to direct via the aout mux.
	/// for each output one can choose of 10 sources, which are or-ed.
	/// value 10  disables the input
	/// This functions makes sure, that always only one of them is enabled.
	/// For details about the possible values (input), see HICANN-doc
	/// (section: analog output configuration)
	int setOutputMux(
	    uint op1, //!< select input for output 1 (possible values: 0..9)
	    uint op2  //!< select input for output 2 (possible values: 0..9)
	    );

//Vctrl0<0>,prebuf<0>,amux_in<0>,fg_out<0>,n_out<1:3:2>,n_out<0:3:2>,amux_in<1>,fg_out<1>
//Vctrl0<1>,prebuf<1>,amux_in<2>,fg_out<2>,n_out<1:3:2>,n_out<0:3:2>,amux_in<3>,fg_out<3>

	/// enable analog output (aout).
	/// Care has to be taken on the wafer:
	/// The outputs of all 8 HICANNs are ORed for analog readout.
	/// so ech output should be enabled only once per reticle
	int setOutputEn(
	    bool op1,  //!< enable output 1
        bool op2  //!< enable output 2
        );

	/// fetch read data resulting from read_data.
	void get_read_data(
        ci_addr_t& rowaddr,
        ci_data_t& data
        );

	/// write the config of a single denmem.
	/// the data for that is usually generated with a function called "nbdata"
	int write_nmem(
	    uint addr,  //!< addr mapped onto neuron quadrouble number, 0..127
	    uint data  //!< two 8 bit values 15:8 bottom, 7:0 top
	    );

    /// read the config of a single denmem.
	/// CAUTION, might lead to bit flips!
	int read_nmem(
	    uint addr  //!< addr mapped onto neuron quadrouble number, 0..127
	    );

	/// fetch read data resulting from read_data
	/// CAUTION, might lead to bit flips!
	void get_read_data_nmem(
	    ci_addr_t& rowaddr, //!< address, data has been read from
	    ci_data_t& data //!< read data
	    );

    /// low active neuron reset for top and bottom block.
	/// ties membrane cap node to adapation voltage thus inhibits firing
	/// true/false - disable/enable neuron reset
	int setNeuronReset(
	    bool n0,  //!< top block neuron reset
	    bool n1  //!< bottom block neuron reset
		);

    /// low active neuron spl1 output reset.
	/// true/false - disable/enable SPL1 output buffe register reset
	int setSpl1Reset(
	    bool r //!< enable/disable SPL1 reset
	    );

	/// Set slow and fast switches for I_gl (current controlling leakage conductance).
	/// This switch config sets the divisor of this current.
	/// See Hicann-doc (section: Denmem Configuration) for details.
	/// off: 1:1,3:1
	/// slow:9:1,3:1
	/// fast:1:1:1:1
	int setNeuronGl(
	    bool slow0,  //!< slow for top denmems
	    bool slow1,  //!< slow for bottom denmems
	    bool fast0,  //!< fast for top denmems
	    bool fast1  //!< fast for bottom denmems
	    );

	/// Set slow and fast switches for I_gladapt (current controlling adaptation conductance a).
	/// This switch config sets the divisor of this current.
	/// See Hicann-doc (section: Denmem Configuration) for details.
	/// off: 1:1,3:1
	/// slow:9:1,3:1
	/// fast:1:1:1:1
	int setNeuronGladapt(
	    bool slow0,  //!< slow for top denmems
	    bool slow1,  //!< slow for bottom denmems
	    bool fast0,  //!< fast for top denmems
	    bool fast1  //!< fast for bottom denmems
	    );

    /// Set slow and fast switches for I_radapt (current controlling adaptation time constant).
	/// This switch config sets the divisor of this current.
	/// See Hicann-doc 4.3.4 for details.
	/// off: 4:1,8:1
	/// slow:4:1,5:1,4:1,8:1
	/// fast:4:1:2:1
	int setNeuronRadapt(
	    bool slow0,  //!< slow for top denmems
	    bool slow1,  //!< slow for bottom denmems
	    bool fast0,  //!< fast for top denmems
	    bool fast1  //!< fast for bottom denmems
	    );

	/// sets the big capacity to be used in the denmems.
	/// There are two capacities: big(2.6 pF) and small (400 fF)
	/// One can change for each block (top/bottom) dendrites,
	/// whether the big or the small cap is used.
	/// default is 1 (big cap)
	int setNeuronBigCap(
	    bool n0,  //!< 1: use big cap for top dendrites
	    bool n1  //!< 1: use big cap for bottom dendrites
	    );

	/// reads back and prints the config in the registers
	void print_config();

	/// sets timing parameters of full-custom SRAM controller
	void set_sram_timings(
		uint read_delay,      //! 8 Bit, chip default (V2/4): 8 | read delay (data valid after this time)
		uint setup_precharge, //! 4 Bit, chip default (V2/4): 1 | setup time for address lines and (data (write) or precharge (read))
		uint write_delay      //! 4 Bit, chip default (V2/4): 1 | actual write time
		);

	/// reads back timing parameters of full-custom SRAM controller from chip and sets values to referenced variables
	void get_sram_timings(
		uint& read_delay,      //! 8 Bit, read delay (data valid after this time)
		uint& setup_precharge, //! 4 Bit, setup time for address lines and (data (write) or precharge (read))
		uint& write_delay      //! 4 Bit, actual write time
		);

	/// resets all registers to zero
	void reset();

}; //end of NeuronBuilderControl
}  // end of namespace facets

#endif // NEURONBUILDER_CONTROL_H

