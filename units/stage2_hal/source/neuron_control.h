#pragma once
// Company         :   kip                      
// Author          :   Sebastian Millner		      
// E-Mail          :   smillner@kip.uni-heidelberg.de
//                    			
// Filename        :   neuron_control.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Fr Feb 26 10
// Last Change     :   Fr Jan 26 11
// by              :   akononov
//------------------------------------------------------------------------

#include "common.h"
#include "ctrlmod.h"
#include "logger.h"

namespace facets {

/// NeuronControl neuron control class.
///
/// Control neuron outputs to spl1, 
/// spl1 mergers and background event generators
///
/// Note, that at least one BEG sending on one L1 Bus 
/// has to have address 0, as this ensures that the 
/// timing of the L1 repeaters works correctly.

class NeuronControl : public CtrlModule
{
private:
	std::vector<ci_data_t> config;  //!< stores the current configuration
	
	Logger& log; //!< reference to the global logger
	
	virtual std::string ClassName() { return "NeuronControl"; };
	
public:
	static std::map<nc_merger, std::string> merger_names;

	/// enumerations for the neuron control registers.
	enum nc_regs {
        nc_enable=0,     //!< merger enable register address
	    nc_select=1,     //!< merger select register address
	    nc_slow=2,       //!< merger slow register address
	    nc_dncloopb=3,   //!< dnc enable/loopback control register address
	    nc_randomreset=4,//!< BEG poisson enable, generator enable register address
	    nc_phase=5,      //!< SPL1 output sampling control register address (rising/falling edge)
	    nc_seed=6,       //!< seed for the BEGs register address
	    nc_period=7,     //!< period between events in cycles/comparison threshold in poisson mode
	    nc_dncmerger=15, //!< slow/enable register address for the DNC mergers
	    nc_nnumber=16    //!< begin of address space for BEG neuron numbers
	};

	/// give pointer to utilized LinkLayer, start and range of this module's address range.
	NeuronControl(
			Stage2Ctrl* c,  //!< pointer to parent Stage2Ctrl class (i.e. HicannCtrl)
			uint ta, //!< associated OCP tagid 
	    uint sa,  //!< Startaddress (OCP)
	    uint ma   //!< Maximumadress (OCP)
	    );
	    
	~NeuronControl();

	/// config function, must be run after instance creation to read in current configuration.
	void configure();

	/// write data to ram or registers from bool std::vector.
	int write_data(
	    uint rowaddr,  //!< relative address of a register to be written
	    std::vector<bool> cfg  //!< the bit configuration to be written into register
	    );
	    
	/// write data to ram or registers from uint.
	int write_data(
	    uint rowaddr,  //!< relative address of a register to be written
	    uint cfg  //!< configuration to be written
	    );

	/// issue read command to read back data.
	int read_data(
	    uint rowaddr  //!< configuration to be written
	    );
	
	/// fetch read data resulting from read_data.
	void get_read_data(
	    ci_addr_t& rowaddr,  //!< relative address of a register is returned
	    ci_data_t& data  //!< data is returned
	    );
	
	/// set common seed for background event generators.
	void set_seed(
	    uint seed  //!< common seed for the backround event generators (lower 16-bit)
	    );
	
	/// switch background event generators on.
	/// enable background event generator (0..7)
	/// if gen_r = 8 -> activate all
	void beg_on(
	    uint gen_nr  //!< nr of background event generator (0..7)
	    );
	    
	/// switch background event generators off.
	/// disable background event generator (0..7)
	/// if gen_r = 8 -> deactivate all	
	void beg_off(
	    uint gen_nr  //!< nr of background event generator (0..7)
	    );
	
	/// configure a background event generator.    
	/// @param gen_nr numer of the background event generator (0..7), 8 for all BEGs.
    /// @param poisson type of pulse sequence to generate, true -- random pulse sequence, false -- regular pulse sequence.
    /// @param period the period of a background event generato  (16-bit long) 
    /// the meaning of this register is different for random and regular spikes. See the Hicann-doc for details.
	void beg_configure(
	    uint gen_nr,  //!< nr of background event generator (0..7)
	    bool poisson,  //!< pulse mode
	    uint period  //!< period between spikes (lower 16-bit)
	    );

	/// set a neuron number for the background generator.
	/// @param nnumber the sending spl1 address of a beg. possible values: 0-63. DEFAULT: 0.
	void beg_set_number(
	    uint gen_nr,  //!< number of BEG (0-7)
	    uint nnumber  //!< neuron number
	    );

	/// single merger enable/select/slow signal configuration.
	void merger_set(
	    nc_merger merg,  //!< merger type (see hardware_base.h)
	    bool enable,  //!< enable bit
	    bool select,  //!< select bit
	    bool slow  //!< slow bit
	    );

	/// all mergers enable/select/slow signal configuration.
	/// MERG_ALL - all mergers have same configuration
	/// struct nc_merger in hardware_base.h
	void merger_set(
	    std::vector<nc_merger> merg,  //!<  merger type std::vector (see hardware_base.h)
	    std::vector<bool> enable,  //!< eneble bit std::vector
	    std::vector<bool> select,  //!< select bit std::vector
	    std::vector<bool> slow  //!< slow bit std::vector
	    );

	/// get merger enable/select/slow configuration.
	void merger_get(
	    nc_merger merg,  //!< merger type (see hardware_base.h)
	    bool& enable,  //!< enable bit
	    bool& select,  //!< select bit
	    bool& slow  //!< slow bit
	    );
	
	/// spl1 output register phase configuration.
	/// select data sampling on: true -- rising edge, false -- falling edge
	void outputphase_set(
	    bool reg0,  //!< channel 0 data sampling
	    bool reg1,  //!< channel 1 data sampling
	    bool reg2,  //!< channel 2 data sampling
	    bool reg3,  //!< channel 3 data sampling
	    bool reg4,  //!< channel 4 data sampling
	    bool reg5,  //!< channel 5 data sampling
	    bool reg6,  //!< channel 6 data sampling
	    bool reg7  //!< channel 7 data sampling
	    );
	
	/// DNC input enable configuration.
	/// true - HICANN inbound
	/// false - HICANN outbound
	void dnc_enable_set(
	    bool dnc0,  //!< DNC input channel 0 enable bit
	    bool dnc1,  //!< DNC input channel 1 enable bit
	    bool dnc2,  //!< DNC input channel 2 enable bit
	    bool dnc3,  //!< DNC input channel 3 enable bit
	    bool dnc4,  //!< DNC input channel 4 enable bit
	    bool dnc5,  //!< DNC input channel 5 enable bit
	    bool dnc6,  //!< DNC input channel 6 enable bit
	    bool dnc7  //!< DNC input channel 7 enable bit
	    );
	
	/// spl1 loopback enable/configuration.
	void loopback_on(
	    uint source,  //!< source channel for loopback
	    uint end  //!< target channel for loopback
	    );
	    
	/// spl1 loopback disable
	/// disable a loopback by providing a channel 
	/// participating in an active loopback
	void loopback_off(
	    uint number  //!< a number of a channel participating in an active loopback
	    );
	
	/// reset everything to zero.
	void nc_reset();
	
	/// function for debugging purposes (displays current configuration).
	void print_config();
	
}; //end of NeuronControl 
}  // end of namespace facets
