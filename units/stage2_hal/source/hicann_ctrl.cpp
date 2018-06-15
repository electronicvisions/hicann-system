#include "linklayer.h"
#include "ctrlmod.h"

#include "synapse_control.h"
#include "neuronbuilder_control.h"
#include "neuron_control.h"
#include "fg_control.h"
#include "l1switch_control.h"
#include "repeater_control.h"
#include "spl1_control.h"
#include "dncif_control.h"

#include "hicann_ctrl.h"


using namespace std;

namespace facets {

HicannCtrl::HicannCtrl(Stage2Comm* c, uint chip) :
	Stage2Ctrl(c,chip),
	c_spl1(SPL1Control(this , tagid_slow, OcpSpl1.startaddr, OcpSpl1.maxaddr)),
	c_dncif(DncIfControl(this , tagid_slow, OcpDncIf.startaddr, OcpDncIf.maxaddr)),
	nbc(NeuronBuilderControl(this, tagid_slow, OcpNeuronBuilder.startaddr, OcpNeuronBuilder.maxaddr)),
	nc(NeuronControl(this , tagid_slow, OcpNeuronCtrl.startaddr, OcpNeuronCtrl.maxaddr))
{
	_synapse_control[0].reset(new SynapseControl(DSCT, this, tagid_fast, OcpStdpCtrl_t.startaddr, OcpStdpCtrl_t.maxaddr));
	_synapse_control[1].reset(new SynapseControl(DSCB, this, tagid_fast, OcpStdpCtrl_b.startaddr, OcpStdpCtrl_b.maxaddr));
	_fg_control[0].reset(new FGControl(FGTL, this, tagid_slow,OcpFgate_tl.startaddr, OcpFgate_tl.maxaddr));
	_fg_control[1].reset(new FGControl(FGTR, this, tagid_slow,OcpFgate_tr.startaddr, OcpFgate_tr.maxaddr));
	_fg_control[2].reset(new FGControl(FGBL, this, tagid_slow,OcpFgate_bl.startaddr, OcpFgate_bl.maxaddr));
	_fg_control[3].reset(new FGControl(FGBR, this, tagid_slow,OcpFgate_br.startaddr, OcpFgate_br.maxaddr));

	_l1_switch_control[0].reset(new L1SwitchControl(SYNTL, this, tagid_slow, OcpSyn_tl.startaddr, OcpSyn_tl.maxaddr));
	_l1_switch_control[1].reset(new L1SwitchControl(SYNTR, this, tagid_slow, OcpSyn_tr.startaddr, OcpSyn_tr.maxaddr));
	_l1_switch_control[2].reset(new L1SwitchControl(CBL,   this, tagid_slow, OcpCb_l.startaddr, OcpCb_l.maxaddr));
	_l1_switch_control[3].reset(new L1SwitchControl(CBR,   this, tagid_slow, OcpCb_r.startaddr, OcpCb_r.maxaddr));
	_l1_switch_control[4].reset(new L1SwitchControl(SYNBL, this, tagid_slow, OcpSyn_bl.startaddr, OcpSyn_bl.maxaddr));
	_l1_switch_control[5].reset(new L1SwitchControl(SYNBR, this, tagid_slow, OcpSyn_br.startaddr, OcpSyn_br.maxaddr));

	_repeater_control[0].reset(new RepeaterControl(REPTL, this, tagid_slow, OcpRep_tl.startaddr, OcpRep_tl.maxaddr));
	_repeater_control[1].reset(new RepeaterControl(REPTR, this, tagid_slow, OcpRep_tr.startaddr, OcpRep_tr.maxaddr));
	_repeater_control[2].reset(new RepeaterControl(REPL,  this, tagid_slow, OcpRep_l.startaddr,  OcpRep_l.maxaddr));
	_repeater_control[3].reset(new RepeaterControl(REPR,  this, tagid_slow, OcpRep_r.startaddr,  OcpRep_r.maxaddr));
	_repeater_control[4].reset(new RepeaterControl(REPBL, this, tagid_slow, OcpRep_bl.startaddr, OcpRep_bl.maxaddr));
	_repeater_control[5].reset(new RepeaterControl(REPBR, this, tagid_slow, OcpRep_br.startaddr, OcpRep_br.maxaddr));
}

}
