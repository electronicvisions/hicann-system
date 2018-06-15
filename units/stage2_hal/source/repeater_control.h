#pragma once
// Company         :   kip                      
// Author          :   Alexander Kononov
// E-Mail          :   akononov@kip.uni-heidelberg.de
//                    			
// Filename        :   repeater_control.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Thu Jan 20 11
// Last Change     :   Thu Jan 21 11
// by              :   akononov  
//------------------------------------------------------------------------

#include "ctrlmod.h"
#include "logger.h"

namespace HMF{
	class HICANNBackend;
}

namespace facets {

/// RepeaterControl class.
/// controls layer 1 repeaters via
/// access to repeater controller modules
class RepeaterControl :
	public CtrlModule,
	public LoggerMixin
{
public:
	/// ???
	enum rc_regs {
	    rc_config=0x80,  //!< ???
	    rc_status=0x80,  //!< ??? 
	    rc_td0=0x81,  //!< ???
	    rc_td1=0x87,  //!< ???
	    rc_scfg=0xa0  //!< ???
	    };
	    
	/// ???	    
	enum rc_srambits {
	    rc_touten=0x80,  //!< ???
	    rc_tinen=0x40,  //!< ???
	    rc_recen=0x20,  //!< ???
	    rc_dir=0x10,  //!< ???
	    rc_ren1=8,  //!< ???
	    rc_ren0=4,  //!< ???
	    rc_len1=2,  //!< ???
	    rc_len0=1  //!< ???
	    };
	    
	/// ???	    
	enum rc_config {
	    rc_fextcap1=0x80,  //!< ???
	    rc_fextcap0=0x40,  //!< ???
	    rc_drvresetb=0x20,  //!< ???
	    rc_dllresetb=0x10,  //!< ???
	    rc_start_tdi1=8,  //!< ???
	    rc_start_tdi0=4,  //!< ???
	    rc_start_tdo1=2,  //!< ???
	    rc_start_tdo0=1  //!< ???
	    };

private:

	rep_loc loc; //!< reciever location this instance corresponds to
	
	std::vector<uint> config;  //!< ???
	
	uint outputflag[2];  //!< busy flags for 2 test output channels of the repeater
	
	uint startconf;  //!< stores current configuration of the start bits
	
    virtual std::string ClassName() { return "RepeaterControl"; };
		
public:
	/// direction of event flow on the L1 repeaters.
	/// THIS documentation IS CONTRADICTIVE TODO
	enum rep_direction {
	    OUTWARDS=0x0,  //!< layer1 HICANN inbound  TODO
	    INWARDS=0x1  //!< layer1 HICANN outbound TODO
	    };
	
	/// usage mode of a repeater.
	enum rep_mode {
	    FORWARD=0x0,  //!< ???
	    TESTIN=0x1,  //!< ???
	    TESTOUT=0x2,  //!< ???
	    SPL1OUT=0x3  //!< ???
	    };

	/// to be used in the future ...
	struct repeater_cfg_t { 
		rep_mode      r_mode;  //!< ???
		rep_direction r_dir;  //!< ???
		bool          r_switch;  //!< ???
	};
	
	/// give pointer to utilized LinkLayer,	
	/// start and range of this module's address range.
	RepeaterControl(
	    rep_loc cl,
			Stage2Ctrl* c,  //!< pointer to parent Stage2Ctrl class (i.e. HicannCtrl)
			uint ta, //!< associated OCP tagid 
	    uint sa,  //!< Startaddress (OCP)
	    uint ma  //!< Maximumadress (OCP)
	    );
	
	~RepeaterControl();
	
	/// config function, must be run after instance 
	/// creation to read in current configuration	.
	void configure();
	
	/// repair address of the repeater (HICANN V1).
	/// generate repeater address by permuting address bits (for HICANN V1)
	/// also takes into account the changed HICANN V2 repeater numbering 
	/// (corresponding to lowest L1 lines)
	uint repaddr(
	    uint addr  //!< ???
	    );
	
	/// read_data returns directly the wanted data value in contrary to all other ControlModules
	uint read_data(
	    uint addr  //!< ???
	    );
	
	/// ???
	uint write_data(
	    uint addr,  //!< ???
	    uint data  //!< ???
	    );
	
	/// ???
	rep_loc get_loc();
	
	/// clear all SRAM cells in the repeater
	void reset();
	
	/// reset DLLs for the synaptic drivers
	void reset_dll();
	
	/// detect if rep_nr is a sending repeater
	bool is_spl1rep(
	    uint rep_nr  //!< ???
	    );
	
	/// set the direction of a repeater (true - into the chip, false - out of the chip)
	void set_dir(
	    uint rep_nr,  //!< ???
	    bool dir  //!< ???
	    );
	    
	/// ???	
	void set_dir(
	    std::vector<uint> rep_nr,  //!< ???
	    std::vector<bool> dir  //!< ???
	    );
	
	/// switch the repeater receiver
	void enable(
	    uint rep_nr,  //!< ???
	    bool _switch  //!< ???
	    );
	
	/// ???
	void enable(
	    std::vector<uint> rep_nr,  //!< ???
	    std::vector<bool> _switch  //!< ???
	    );
	
	/// switch test output
	void tout(
	    uint rep_nr,  //!< ???
	    bool _switch  //!< ???
	    );
	
	/// ???	
	void tout(
	    std::vector<uint> rep_nr,  //!< ???
	    std::vector<bool> _switch  //!< ???
	    );
	
	/// switch test input
	void tin(
	    uint rep_nr,  //!< ???
	    bool _switch  //!< ???
	    );
	
	/// ???	
	void tin(
	    std::vector<uint> rep_nr,  //!< ???
	    std::vector<bool> _switch  //!< ???
	    );
	
	/// configure repeater receiving input  / sending test data / forwarding L1 data
	void conf_repeater(
	    uint rep_nr,  //!< number of the repeater to configure
	    rep_mode mode,  //!< the working mode of the repeater
	    rep_direction dir,  //!< the repeaters operating direction
	    bool _switch  //!< configuration specific switch
	    );
	
	/// configure spl1 repeater for sending data from neuroncontrol
	void conf_spl1_repeater(
	    uint rep_nr,  //!< ???
	    rep_mode mode,  //!< ???
	    rep_direction dir,  //!< ???
	    bool _switch  //!< ???
	    );
	
	/// read out the test data register entries
	void tdata_read(
	    uint channel,  //!< ???
	    uint entry,  //!< ???
	    uint& neuron_nr,  //!< ???
	    uint& time  //!< ???
	    );
	
	/// write the test data register entries
	void tdata_write(
	    uint channel,  //!< ???
	    uint entry,  //!< ???
	    uint neuron_nr,  //!< ???
	    uint time  //!< ???
	    );
	
	/// start and stop the input/outout channels
	void startin(
	    uint channel  //!< ???
	    );
	
	/// this one also resets the TDI full flag
	void stopin(
	    uint channel  //!< ???
	    );
	
	/// ???	
	void startout(
	    uint channel  //!< ???
	    );
	
	/// ???	
	void stopout(
	    uint channel  //!< ???
	    );
	
	/// read the test data in full flag
	bool fullflag(
	    uint channel  //!< ???
	    );

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

	/// converts gray to binary code
	uint gtob(
	    uint time  //!< ???
	    );
	
	/// debugging function
	void print_config();
	

};
}
