#ifndef JD_MAPPINGMASTER_H
#define JD_MAPPINGMASTER_H

#include <vector>
#include <utility>

#include "testmode.h" //Testmode and Lister classes
#include "hicann_cfg.h"
#include "pulse_float.h"
#include "hardware_base.h"

#include "hicann_ctrl.h"
#include "repeater_control.h" //repeater control class

/** The Juelich Demonstrator mapping master class.
 *
 *  This is the testmode used for the juelich-demonstrator.
 *  It is a testmode configures one HICANN with the configuration
 *  from the mapping processs, which usually is started with a
 *  PyNN script.
 *
 *  Start this testmode via:
 *  > ./tests2 -bje 1 0 -m jd_mappingmaster
 *
 *  This will initialize this testmode class and call function test()
 *  which starts a RCF (Remote Call Framework) server.
 *
 *  The HicannControl of the mapping tool opens a client connection
 *  to this server and communicate via the Experiment class (see jd_mappingmaster.cpp)
 *  with this testmode. The Experiment class then can call functions of this testmode.
 *
 *
 */

// forward declaration:
namespace RCF {
	class RcfServer;
}
class HostALController;
class EthernetIFBase;

namespace facets {
	class S2C_JtagPhys2Fpga;
}

struct JDMappingMaster : public Testmode {

public:
	/// default constructor
	JDMappingMaster();
	/// destructor
	~JDMappingMaster();

	RCF::RcfServer* _rpc_server;

	/// enumeration of all repeater control locations
	enum repeater_control_location {
		RCTRL_MID_LEFT = 0, //<! mid left repeater control
		RCTRL_MID_RIGHT = 1, //<! mid right repeater control
		RCTRL_TOP_LEFT = 2, //<! top left repeater control
		RCTRL_BOTTOM_LEFT = 3, //<! bottom left repeater control
		RCTRL_TOP_RIGHT = 4, //<! top right repeater control
		RCTRL_BOTTOM_RIGHT = 5 //<! bottom right repeater control
		};
		
	/// enumeration of all l1 switch control locations
	enum l1switch_control_location {
		L1SCTRL_MID_LEFT = 0,  //<! mid left l1 switch control (crossbar)
		L1SCTRL_MID_RIGHT = 1,  //<! mid right l1 switch control (crossbar)
		L1SCTRL_TOP_LEFT = 2,  //<! top left l1 switch control (select-switch)
		L1SCTRL_BOTTOM_LEFT = 3,  //<! bottom left l1 switch control (select-switch)
		L1SCTRL_TOP_RIGHT = 4,  //<! top right l1 switch control (select-switch)
		L1SCTRL_BOTTOM_RIGHT = 5 //<! bottom right l1 switch control (select-switch)
		};

	/// enumeration of all floating gate control locations
	enum floting_gate_control_location {
		FGCTRL_TOP_LEFT = 0, //<! top left floating gate control
		FGCTRL_TOP_RIGHT = 1, //<! top right floating gate control
		FGCTRL_BOTTOM_LEFT = 2, //<! bottom left floating gate control
		FGCTRL_BOTTOM_RIGHT = 3  //<! bottom right floating gate control
	};

	/// enumeration of all synapse control locations
	enum synapse_control_location {
		SCTRL_TOP = 0, //<! top synapse control
		SCTRL_BOTTOM = 1 //<! bottom synapse control
	};
	
	static const int TAG = 0;

	// macro block controls
    facets::IBoardV2Ctrl * _iboard_control; //!< HAL iboard control	
    facets::FPGAControl *_fpga_control; //!< HAL fpga control 
    facets::DNCControl  *_dnc_control;  //!< HAL dnccontrol	
    facets::HicannCtrl *_hicann_control; //!< HAL hicann control object, aggregation of hicann building blocks
	
	// hicann building blocks	
	LinkLayer<facets::ci_payload,facets::ARQPacketStage2> *_link_layer;

    facets::FGControl *_floating_gate_controls[FG_pkg::fg_num];  //!< floating gate controller array
    facets::NeuronBuilderControl *_neuron_builder_control;  //!< neuron builder control
    facets::NeuronControl *_neuron_control;  //!< neuron control
	facets::SPL1Control *_spl1_control;  //!< spl1 control class
	facets::RepeaterControl *_repeater_controls[REPEATER_CONTROLS_PER_HICANN];  //!< repeater control block array
	facets::L1SwitchControl *_l1switch_controls[L1SWITCH_CONTROLS_PER_HICANN];  //!< l1 switch control array
	facets::SynapseControl *_synapse_controls[SYNAPSE_CONTROLS_PER_HICANN];  //!< synapse control block array
	HostALController * _host_al_control; //!< Host Application Layer Controller
	EthernetIFBase * _eth_stim; //!< Ethernet interface
	bool _host_al_intialized; //!< flag holding wethter host al has been initialized
    	
	/// stepwise activate the HICANN
    void activate_hicann();

    /// stepwise program the HICANN
	void program_hicann(
		const hicann_cfg_t & config  //!< an HICANN config object
		);
    
	/// stepwise program the HICANN - final (workaround) step
	void program_hicann_final(
		const hicann_cfg_t & config  //!< an HICANN config object
		);

	/// decide whether on waferscale or single-chip system and call appropriate initialize function.
    void initialize_system();

	/// common initialization part (incl. fpga, jtag, dnc resets)
    void initialize_common();

	/// common highspeed initialization
    void initialize_highspeed();

	/// initilize the iboard V2.
	void initialize_iboardv2(
		unsigned int hicannr = 0  //<! HICANN ID according to the HAL HICANN addressing
		);

	/// initilize the waferscale-system.
	void initialize_waferscale_system(
		unsigned int hicannr = 0  //<! HICANN ID according to the HAL HICANN addressing
		);

	/// initialize Host Application Layer Controller
	void initialize_host_al_controller();


	/// sets the PLL frequency of the Repeaters.
	/// known working configuration
	/// a smaller PLL frequency will help that the repeaters are locked better, such that less
	/// wrong l1 addresses are generated
	/// (1,1):  50 MHz
	/// (2,1): 100 MHz
	/// (3,1): 150 MHz
	/// (4,1): 200 MHz
	/// (5,1): 250 MHz
	void set_pll(
			uint multiplicator, //!< clock multiplicator
			uint divisor //!< clock divisor 
			);
		
	//////////////////////////
	// PULSES
	//////////////////////////
	
	/// shared variables for pulse handling (currently set in write_playback_pulses)
    double starttime; //!< start time of experiment; set to a time right before the first pulse
    static const double clkperiod_dnc; //!< clock period of DNC in seconds (4ns normally)
    static const double clkperiod_fpga; //!< clock period of FPGA in seconds (8ns normally)
    static const size_t playback_fifo_size = 511; //!< size of playback fifo (currently 511)
    
    unsigned int hicann_nr;
    unsigned int dnc_id; //!< the dnc id, is detected from the jtag port in intialize system. 1 for vertical setup

	/** holds the config for the spl1 control (DNC IF)
	 * DNC_IF channels are disabled after every experiment run, in order to avoid mixing of pulse and arq events.
	 * For multiple calls of execute_experiment without a re-configuration, we store the config temporarily here,
	 * such that we can configure the DNC_IF before experiment run.
	 */
	unsigned int spl1_cfg;
    
    /// initialize FPGA-DNC-HICANN connection, single trial
    bool init_layer2();

	/// configure one PCS of the layer2 network.
	void program_layer2(pcs_cfg_t pcs_config);
	
	/// write pulses to playback memory.
    /// pulse list must be sorted by release time
    void write_playback_pulses(
		std::map< l2_pulse_address_t, std::vector<double> > spiketrains  //!< an input spike train in hw time
		);

	/// perform pulse experiment (enable trace and start playback - default duration 1 s).
    bool execute_experiment(float duration_in_s = 1.);
    
    /// read pulses from playback memory.
    void read_trace_pulses(
			std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
			);

    
	//////////////////////////
	// Remote Procedure Call (RPC)
	//////////////////////////
	
	/// start the RCF.
	void start_rcf();
	
	/// stop the RCF.
	void stop_rcf();
	
	/// test function of the testmode called on creation.
	bool test();
	
private:
	
	/// log ids of macro blocks.
	void log_ids();

	/// reset ARQ in HICANN.
	void reset_arq();

	/// get l2-enabled comm pointer
	facets::S2C_JtagPhys2Fpga * getL2CommPtr();


// methods for HICANN activation and programming
   
	//////////////////////////
	// FloatingGateControl
	//////////////////////////
        
    /// Prepare the floating gates for operation.
	void activate_floating_gate_control();
	
	/// write global and per-neuron floating gate parameters.
	void program_floating_gates(const hicann_cfg_t & config);

	/// Program a single floating gate line.
	void write_floating_gate_line(
	    uint fg_num,  //<! number of the floating gate controller
	    uint fg_line,  //<! the floating gate line to write 
	    std::vector<int> values);  //<! values to write
		

	/// write the current stimulus values.
	/// @param bank:    floating gate control ram bank
	/// @param current: std::vector of current values that will be written to the floating gate control
	/// @param pulselength: affects the speed of the current playback clock:
	///     The period duration is proportional to (pulselength + 1)
	/// @param fg_num: number of the affected floating gate control
	/// @param stimulate_continuous: whether stimulation is applied once or
	///   periodically.
	void write_current_stimulus(
		unsigned int bank, //!< FG bank to write into
		std::vector<unsigned int> const & current, //!< current samples
		unsigned int pulselength=15, //!< sample count
		unsigned int fg_num=0, //!< FG block to write into
		bool stimulate_continuous=true //!< type of stiumulation
	);

	//////////////////////////
	// NeuronBuilderControl
	//////////////////////////

	/// Prepare the neuron builder control for operation.
	void activate_neuron_builder_control();	

	/// write neuron, global neuron builder and spl1 setting.
	void program_neuron_builder(const hicann_cfg_t & config);
	
	/*
	/// data for the neuron builder.
    /// fireb and connect - for even addresses, 
	/// firet - for odd addresses, 
	/// ntop, nbot for every 3+4*n'th address
	uint build_neuron_builder_data(
	    uint fireb, 
	    uint firet, 
	    uint connect, 
	    uint ntop, 
	    uint nbot, 
	    uint spl1
	    );
    	
	/// activate a single denmem.
	/// taken from tm_neuron : 'n' . add a denmem
	void activate_single_denmem(uint add);
	*/
	
	//////////////////////////
	// NeuronControl
	//////////////////////////
	
	/// Prepare the neuron control for operation.
	void activate_neuron_control();
	
    /// write configuration for background event generators,
    /// the spl1-merger tree, and the output to spl1 and dnc_if.
	void program_neuron_control(const hicann_cfg_t & config);

	//////////////////////////
	// RepeaterControl
	//////////////////////////

	/// Prepare the repeater control for operation.
	void activate_repeater_control();

	/// configure all repeaters
	void program_repeaters(const hicann_cfg_t & config);

	/// Configure a whole repeater block (horizontal or vertical)
	void configure_repeater_block(
		repeater_control_location rep_loc, //!< the repeater block to be configured.
		const std::vector<repeater_config::e>& repeater_config //!< std::vector holding the configuration for all repeaters of a block.
	);
	
	//////////////////////////
	// L1SwitchControl
	//////////////////////////
	
	/// Prepare the l1switch control for operation.
	void activate_l1switch_control();	
	
    /// configure the cross bars & select switches.
	void program_l1switches(const hicann_cfg_t & config);	
	
	/// Configure l1 switches (cross-bars and select-switches).
    void configure_l1_switch(
			l1switch_control_location switch_loc,  //<! the select switch to write to
	 		const std::vector< std::pair<unsigned int, unsigned int> >& connections  //<! configuration to write
			);
			
	//////////////////////////
	// SynapseControl
	//////////////////////////

	/// Prepare the synapse control for operation.
	void activate_synapse_control();
	
	/// configure synapse drivers & synpse matrix.
	void program_synapse_control(const hicann_cfg_t & config);

	/// brings a std::vector with 256 4-bit values into a std::vector of 32 unsigned ints, which is the perfect form to write to hardware.
	void compress_synapse_row_values(
		const std::vector< unsigned char > & values,
		std::vector<unsigned int> & compressed_values
	);

	/// configure a single synapse driver.
	void configure_synapse_driver(
		synapse_control_location sctrl_loc, //!< synapse block (SCTRL_TOP or SCTRL_BOTTOM )
		unsigned int drv_nr, //!< driver number, corresponds to bottom synapse line of this syndriver
		const syndriver_cfg_t & syndr_config //!< struct holding the syndriver config
	);


	//////////////////////////
	// SPL1Control
	//////////////////////////

	/// Prepare the spl1 control for operation.
	void activate_spl1_control();

	/// configure the HICANN-DNC connection on HICANN side
	void program_spl1_control(const hicann_cfg_t & config);

};

#endif // JD_MAPPINGMASTER_H
