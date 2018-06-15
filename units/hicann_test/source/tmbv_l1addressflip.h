#ifndef TMBV_L1ADDRESSFLIP_H
#define TMBV_L1ADDRESSFLIP_H

#include <vector>
#include <utility>

#include "testmode.h" //Testmode and Lister classes
#include "hicann_cfg.h"
#include "pulse_float.h"
#include "hardware_base.h"
#include "repeater_control.h" //repeater control class

/** Testmode to test the flipping of L1 Addresses.
 *
 *  How this testmode works:
 *  The output from BEG Nr 4, which sends address 0, is merged with input from Layer 2 onto horizontal bus number 25
 *  The horizontal bus can be connected to one of the 8 available vertical buses.
 *  At the corresponding bottom repeater for each of the 8 vertical buses the received events are read out.
 *
 */

struct TmBVL1AddressFlip : public Testmode {

public:

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

	unsigned int _v_bus; //!< vertical bus, to which horizontal bus nur 25 connects, (default 140);
	unsigned int _h_bus; //!< horizontal bus, connected to currently sending repeater
	unsigned int _channel;
	repeater_control_location _block_of_readout_repeater; //!< block of repeater, that will read out the events
	unsigned int _id_of_sending_repeater; //!< id of repeater within block, that will send the event (0, 4, 8, .. 28)
	unsigned int _id_of_readout_repeater; //!< id of repeater within block, that will read out the events
	const unsigned int _pll_div; // currently 1
	unsigned int _pll_mult;

	/// constructor
	TmBVL1AddressFlip();
    	
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
	        
	/// initilize the iboard V2.
	void initialize_iboardv2(
		unsigned int hicannr = 0  //<! HICANN ID according to the HAL HICANN addressing
		);

	/// initilize the waferscale-system.
	void initialize_waferscale_system(
		unsigned int hicannr = 0  //<! HICANN ID according to the HAL HICANN addressing
		);

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
	bool set_v_bus(
		unsigned int v_bus //!< vertical bus, to which horizontal bus nur 25 connects, (default 140);
			);

	/// set input channe, i.e. the output register, which receives input from L2 and fro the background event generator
	bool set_channel(
		unsigned int channel
			);
		
	//////////////////////////
	// PULSES
	//////////////////////////
	
	/// shared variables for pulse handling (currently set in write_playback_pulses)
    double starttime; //!< start time of experiment; set to a time right before the first pulse
    static const double clkperiod_dnc; //!< clock period of DNC in seconds (4ns normally)
    static const double clkperiod_fpga; //!< clock period of FPGA in seconds (8ns normally)
    static const size_t playback_fifo_size = 511; //!< size of playback fifo (currently 511)
	static const std::vector<unsigned int> channel_to_repeater; //!< connectivity from channel (output register) to mid left repeater 
	static const std::vector<unsigned int> channel_to_hbus; //!< connectivity from channel (output register) to horizontal bus
    
    unsigned int hicann_nr;
    
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
    bool execute_experiment(
			float duration_in_s,
			std::vector< std::vector<uint> >& times,
			std::vector< std::vector<uint> >& nnums
			);
    
    /// read pulses from playback memory.
    void read_trace_pulses(
			std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
			);

	/// 
	void readout_repeater(repeater_control_location loca, facets::RepeaterControl::rep_direction dir, uint repnr);

	void readout_repeater(repeater_control_location loca, facets::RepeaterControl::rep_direction dir, uint repnr, std::vector<uint>& times, std::vector<uint>& nnums, bool silent);

	void set_readout_repeater(unsigned int v_bus);
    
	/// test function of the testmode called on creation.
	bool test();

	/// 
	bool run_address_flip_experiment(
			unsigned int address, //!< L1 Address (0..63)
			unsigned int period, //!< period between spikes ( in clock cycles )
			unsigned int repetitions, //!< number of repetitions
			std::vector< std::vector<uint> >& times,
			std::vector< std::vector<uint> >& nnums
			);
	/// returns mean error rate and appends result for one hbus-vbus combination to file
	double write_results_to_file(
			std::string filename,
			unsigned int period,
			std::vector< std::vector<uint> >& result
			);
	std::vector<uint> get_vbuses_for_hbus(unsigned int hbus);




	
private:
	
	/// log ids of macro blocks.
	void log_ids();

	/// reset ARQ in HICANN.
	void reset_arq();

// methods for HICANN activation and programming
   
	//////////////////////////
	// FloatingGateControl
	//////////////////////////
        
    /// Prepare the floating gates for operation.
	void activate_floating_gate_control();
	
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
	

	//////////////////////////
	// SPL1Control
	//////////////////////////

	/// Prepare the spl1 control for operation.
	void activate_spl1_control();

	/// configure the HICANN-DNC connection on HICANN side
	void program_spl1_control(const hicann_cfg_t & config);

};

#endif // TMBV_L1ADDRESSFLIP_H
