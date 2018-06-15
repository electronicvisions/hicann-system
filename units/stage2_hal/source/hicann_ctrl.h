#pragma once

#include "s2comm.h"
#include "s2ctrl.h"
#include "common.h"

#include "synapse_control.h"
#include "neuron_control.h"
#include "neuronbuilder_control.h"
#include "fg_control.h"
#include "l1switch_control.h"
#include "repeater_control.h"
#include "spl1_control.h"
#include "dncif_control.h"

namespace facets {

/** This class integrates all functional units of the chip to be controlled (i.e. HICANN in this case). */
class HicannCtrl : public Stage2Ctrl
{
private:
	virtual std::string ClassName() { return "HicannCtrl"; };

public:
	HicannCtrl(Stage2Comm* c, uint chip);

	/// Floating Gate Control addresses
	enum FG {
		num_fg_control = 4,
		FG_TOP_LEFT     = 0,
		FG_TOP_RIGHT    = 1,
		FG_BOTTOM_LEFT  = 2,
		FG_BOTTOM_RIGHT = 3
	};

	enum L1Switch {
		num_l1_switch_control = 6,
		L1SWITCH_TOP_LEFT     = 0,
		L1SWITCH_TOP_RIGHT    = 1,
		L1SWITCH_CENTER_LEFT  = 2,
		L1SWITCH_CENTER_RIGHT = 3,
		L1SWITCH_BOTTOM_LEFT  = 4,
		L1SWITCH_BOTTOM_RIGHT = 5
	};

	enum Repeater {
		num_repeater_control  = 6,
		REPEATER_TOP_LEFT     = 0,
		REPEATER_TOP_RIGHT    = 1,
		REPEATER_CENTER_LEFT  = 2,
		REPEATER_CENTER_RIGHT = 3,
		REPEATER_BOTTOM_LEFT  = 4,
		REPEATER_BOTTOM_RIGHT = 5
	};

	enum Synapse {
		num_synapse_control = 2,
		SYNAPSE_TOP         = 0,
		SYNAPSE_BOTTOM      = 1
	};

private:
	/// the following are control modules specific to the Stage2Ctrl child (in this case HICANN control modules)
	boost::scoped_ptr<SynapseControl> _synapse_control[num_synapse_control];
	boost::scoped_ptr<FGControl> _fg_control[num_fg_control];
	boost::scoped_ptr<L1SwitchControl> _l1_switch_control[num_l1_switch_control];
	boost::scoped_ptr<RepeaterControl> _repeater_control[num_repeater_control];

	SPL1Control c_spl1;
	DncIfControl c_dncif;
	NeuronBuilderControl nbc;
	NeuronControl nc;

public:
	// access functions to control module classes (are used by testmodes or control software):
	SynapseControl& getSC_top() __attribute__((deprecated)) { return *_synapse_control[SYNAPSE_TOP]; }
	SynapseControl& getSC_bot() __attribute__((deprecated)) { return *_synapse_control[SYNAPSE_BOTTOM]; }
	SynapseControl& getSC(size_t sc)
	{
		assert(sc<num_synapse_control);
		return *_synapse_control[sc];
	}
	SynapseControl const& getSC(size_t sc) const
	{
		assert(sc<num_synapse_control);
		return *_synapse_control[sc];
	}

	FGControl& getFC_tr() __attribute__((deprecated)) { return *_fg_control[FG_TOP_RIGHT];    }
	FGControl& getFC_tl() __attribute__((deprecated)) { return *_fg_control[FG_TOP_LEFT];     }
	FGControl& getFC_br() __attribute__((deprecated)) { return *_fg_control[FG_BOTTOM_RIGHT]; }
	FGControl& getFC_bl() __attribute__((deprecated)) { return *_fg_control[FG_BOTTOM_LEFT];  }
	FGControl& getFC(size_t fc)
	{
		assert(fc<num_fg_control);
		return *_fg_control[fc];
	}
	FGControl const& getFC(size_t fc) const
	{
		assert(fc<num_fg_control);
		return *_fg_control[fc];
	}

	L1SwitchControl& getLC_cl() __attribute__((deprecated)) { return *_l1_switch_control[L1SWITCH_CENTER_LEFT];  }
	L1SwitchControl& getLC_cr() __attribute__((deprecated)) { return *_l1_switch_control[L1SWITCH_CENTER_RIGHT]; }
	L1SwitchControl& getLC_tl() __attribute__((deprecated)) { return *_l1_switch_control[L1SWITCH_TOP_LEFT];     }
	L1SwitchControl& getLC_tr() __attribute__((deprecated)) { return *_l1_switch_control[L1SWITCH_TOP_RIGHT];    }
	L1SwitchControl& getLC_bl() __attribute__((deprecated)) { return *_l1_switch_control[L1SWITCH_BOTTOM_LEFT];  }
	L1SwitchControl& getLC_br() __attribute__((deprecated)) { return *_l1_switch_control[L1SWITCH_BOTTOM_RIGHT]; }
	L1SwitchControl& getLC(size_t l1_switch_control)
	{
		assert(l1_switch_control<num_l1_switch_control);
		return *_l1_switch_control[l1_switch_control];
	}
	L1SwitchControl const& getLC(size_t l1_switch_control) const
	{
		assert(l1_switch_control<num_l1_switch_control);
		return *_l1_switch_control[l1_switch_control];
	}

	RepeaterControl& getRC_cl() __attribute__((deprecated)) { return *_repeater_control[REPEATER_CENTER_LEFT];  }
	RepeaterControl& getRC_cr() __attribute__((deprecated)) { return *_repeater_control[REPEATER_CENTER_RIGHT]; }
	RepeaterControl& getRC_tl() __attribute__((deprecated)) { return *_repeater_control[REPEATER_TOP_LEFT];     }
	RepeaterControl& getRC_bl() __attribute__((deprecated)) { return *_repeater_control[REPEATER_BOTTOM_LEFT];  }
	RepeaterControl& getRC_tr() __attribute__((deprecated)) { return *_repeater_control[REPEATER_TOP_RIGHT];    }
	RepeaterControl& getRC_br() __attribute__((deprecated)) { return *_repeater_control[REPEATER_BOTTOM_RIGHT]; }
	RepeaterControl& getRC(size_t repeater_control)
	{
		assert(repeater_control<num_repeater_control);
		return *_repeater_control[repeater_control];
	}
	RepeaterControl const& getRC(size_t repeater_control) const
	{
		assert(repeater_control<num_repeater_control);
		return *_repeater_control[repeater_control];
	}

	SPL1Control&          getSPL1Control()  { return c_spl1;  }
	DncIfControl&         getDncIfControl() { return c_dncif; }
	NeuronBuilderControl& getNBC()          { return nbc;     }
	NeuronControl&        getNC()           { return nc;      }
};

}  // end of namespace facets
