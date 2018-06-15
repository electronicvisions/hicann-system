#include "common.h"

#include <map>
#include <cassert>
#include <cmath>

#ifdef NCSIM
#include "systemc.h"
#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" //Testmode and Lister classes

#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class
//#include "repeater_control.h" //repeater control class
#include "neuron_control.h" //neuron control class (merger, background genarators)
#include "neuronbuilder_control.h" //neuron builder control class
#include "fg_control.h" //floating gate control

#include "dnc_control.h"
#include "fpga_control.h"
#include "s2c_jtagphys_2fpga.h" //jtag-fpga library
#include "spl1_control.h" //spl1 control class 
#include "iboardv2_ctrl.h" //iBoard controll class

#include <boost/assign/list_of.hpp>

#include "tmbv_l1addressflip.h"

#include <cassert>
#include <algorithm>

struct Experiment; // forward declaration

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class bino
{
public:
	bino(uint v, uint s=32):val(v),size(s){};
	uint val,size;
	friend std::ostream & operator <<(std::ostream &os, class bino b)
	{
		for(int i=b.size-1 ; i>=0 ; i--)
			if( b.val & (1<<i) ) os<<"1"; else os<<"0";
		return os;
	}
};

//const int TmBVL1AddressFlip::TAG = 0;
const double TmBVL1AddressFlip::clkperiod_dnc = 4.0e-9; // in seconds
const double TmBVL1AddressFlip::clkperiod_fpga = 8.0e-9; // in seconds
const size_t TmBVL1AddressFlip::playback_fifo_size;
const vector<unsigned int> TmBVL1AddressFlip::channel_to_repeater = boost::assign::list_of(28)(24)(20)(16)(12)(8)(4)(0); //!< connectivity from channel (output register) to mid left repeater 
const vector<unsigned int> TmBVL1AddressFlip::channel_to_hbus = boost::assign::list_of(57)(49)(41)(33)(25)(17)(9)(1);

TmBVL1AddressFlip::TmBVL1AddressFlip():
	Testmode()
	,_pll_div(1)
	,_pll_mult(4)
{
	set_channel(4);
	set_readout_repeater(140);
}

void TmBVL1AddressFlip::set_readout_repeater(unsigned int v_bus)
{
	_v_bus = v_bus;
	if (_v_bus < 128 ){
		if (_v_bus%2)
			_block_of_readout_repeater = RCTRL_TOP_LEFT;
		else
			_block_of_readout_repeater=RCTRL_BOTTOM_LEFT;
	}
	else if (_v_bus < 256 ){
		if (_v_bus%2)
			_block_of_readout_repeater=RCTRL_TOP_RIGHT;
		else
			_block_of_readout_repeater=RCTRL_BOTTOM_RIGHT;
	}
	_id_of_readout_repeater = (_v_bus%128)/2;
}


void TmBVL1AddressFlip::initialize_system()
{
	// ugly: needs to set already here for infering if we are on wafer or not
	_hicann_control = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];

	// numbering valid for both setups
	unsigned int hicann_jtag_nr = _hicann_control->addr();
	this->hicann_nr      = jtag->pos_dnc-1-hicann_jtag_nr; // store this for pulse processing
	jtag->set_hicann_pos(hicann_jtag_nr);

	// indexing in DNC (i.e. pulse addresses) and JTAG indexing are different (only applies if several HICANNs are present in the system)
	log(Logger::INFO) << "HICANN IDs: DNC links: " << hicann_nr << ", JTAG: " << hicann_jtag_nr;

	S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(_hicann_control->GetCommObj());
	if ( (comm_ptr != NULL) && (comm_ptr->running_on_reticle) )
    {
    	// on the wafer
		log(Logger::INFO) << "Waferscale system active";
		initialize_waferscale_system(hicann_nr);
    }
    else
    {
    	// single HICANN setup with IBoard
        initialize_iboardv2(hicann_nr);
    }

	// this sets the PLL to 100 HZ instead of 250 Hz
	// TODO should this move to initialize_iboardv2(..) ?
	//set_pll(2,1); // 100 Mhz
	//set_pll(4,1); //200 MHz
	 set_pll(3,1); //150 MHz
	 //set_pll(5,1); //250 MHz
}


/// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
/// init sequence taken from tmak_iboardv2
void TmBVL1AddressFlip::initialize_iboardv2(unsigned int hicannr)
{

    // initializing stuff which seems to be necessary
	log(Logger::INFO) << "setting up iboard";
	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

#ifndef WITHOUT_ARQ
	log(Logger::INFO) << "\t Compiled for use WITHOUT_ARQ";
#else
	log(Logger::INFO) << "\t Compiled for use WITH_ARQ";
#endif // WITHOUT_ARQ

#if HICANN_V >= 2
	log(Logger::INFO) << "\t Compiled for use HICANN_V >= 2";
#elif HICANN_V == 1
	log(Logger::INFO) << "\t Compiled for use HICANN_V == 1";
#else
	#error Missing code for this HICANN revision.
#endif

#if DNC_V == 2
	log(Logger::INFO) << "\t Compiled for use DNC_V == 2";
#elif DNC_V == 1
	log(Logger::INFO) << "\t Compiled for use DNC_V == 1";
#else
	#error Missing code for this DNC revision.
#endif



	/////////////////////////////////////////////////////////////////
	// check if system setup is correct
	/////////////////////////////////////////////////////////////////
	if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		log(Logger::ERROR) << "object 'chip' in testmode not set, abort";
		return;
	}

	/////////////////////////////////////////////////////////////////
	// get the pointers to the macro control blocks
	/////////////////////////////////////////////////////////////////
	_dnc_control = (DNCControl*) chip[FPGA_COUNT]; // use DNC
	_fpga_control = (FPGAControl*) chip[0]; // use FPGA
	_iboard_control = new IBoardV2Ctrl(conf, jtag, 0); //create an iboard control instance

	/////////////////////////////////////////////////////////////////
	// get the HICANN controllog(Logger::INFO) << "HICANN communication Init()";
	/////////////////////////////////////////////////////////////////

	_hicann_control = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0]; // do not insert hicannr here, because we only have one HICANN

	/////////////////////////////////////////////////////////////////
	// init the FPGA, DNC, HICANN communication
	// includes JTAG reset
	/////////////////////////////////////////////////////////////////

	log(Logger::INFO) << "\tHICANN communication Init()";

	if (_hicann_control->GetCommObj()->Init(_hicann_control->addr()) != Stage2Comm::ok) {
		log(Logger::INFO) << "HICANN communication Init() failed, abort";
		return;
	}

	_dnc_control->setTimeStampCtrl(0x00); // disable the DNC timestamp handling

	log_ids();


	/////////////////////////////////////////////////////////////////
	// init the iboard v2
	/////////////////////////////////////////////////////////////////

	// case '7' - set all voltages to "meaningful" vaues
	if (!_iboard_control->setAllVolt()) {
		log(Logger::ERROR) << "could not set voltages";
		throw std::string("could not set voltages, abort");
	}

	// case 'c' - set both output muxes to default values
	log(Logger::INFO) << "\tset both MUX to default values";
	_iboard_control->setBothMux();

	/////////////////////////////////////////////////////////////////
	// 	get pointers to HICANN block controls
	/////////////////////////////////////////////////////////////////
	log(Logger::INFO) << "\tgetting control instances from HicannCtrl";

	_floating_gate_controls[FGCTRL_TOP_LEFT]=&_hicann_control->getFC_tl();
	_floating_gate_controls[FGCTRL_TOP_RIGHT]=&_hicann_control->getFC_tr();
	_floating_gate_controls[FGCTRL_BOTTOM_LEFT]=&_hicann_control->getFC_bl();
	_floating_gate_controls[FGCTRL_BOTTOM_RIGHT]=&_hicann_control->getFC_br();

	_neuron_builder_control = &_hicann_control->getNBC();

	_neuron_control = &_hicann_control->getNC();

	_spl1_control = &_hicann_control->getSPL1Control();

	_repeater_controls[RCTRL_MID_LEFT]=&_hicann_control->getRC_cl();
	_repeater_controls[RCTRL_MID_RIGHT]=&_hicann_control->getRC_cr();
	_repeater_controls[RCTRL_TOP_LEFT]=&_hicann_control->getRC_tl();
	_repeater_controls[RCTRL_BOTTOM_LEFT]=&_hicann_control->getRC_bl();
	_repeater_controls[RCTRL_TOP_RIGHT]=&_hicann_control->getRC_tr();
	_repeater_controls[RCTRL_BOTTOM_RIGHT]=&_hicann_control->getRC_br();

	_l1switch_controls[L1SCTRL_MID_LEFT] = &_hicann_control->getLC_cl();
	_l1switch_controls[L1SCTRL_MID_RIGHT] = &_hicann_control->getLC_cr();
	_l1switch_controls[L1SCTRL_TOP_LEFT] = &_hicann_control->getLC_tl();
	_l1switch_controls[L1SCTRL_BOTTOM_LEFT] = &_hicann_control->getLC_bl();
	_l1switch_controls[L1SCTRL_TOP_RIGHT] = &_hicann_control->getLC_tr();
	_l1switch_controls[L1SCTRL_BOTTOM_RIGHT] = &_hicann_control->getLC_br();

	_synapse_controls[SCTRL_TOP] = &_hicann_control->getSC_top();
	_synapse_controls[SCTRL_BOTTOM] = &_hicann_control->getSC_bot();

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

}

void TmBVL1AddressFlip::initialize_waferscale_system(unsigned int hicannr) {
    // initializing stuff which seems to be necessary
	log(Logger::INFO) << "setting up waferscale system";
	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

	/////////////////////////////////////////////////////////////////
	// check if system setup is correct
	/////////////////////////////////////////////////////////////////
	if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		log(Logger::ERROR) << "object 'chip' in testmode not set, abort";
		return;
	}

	/////////////////////////////////////////////////////////////////
	// get the pointers to the macro control blocks
	/////////////////////////////////////////////////////////////////
	_dnc_control = (DNCControl*) chip[FPGA_COUNT]; // use DNC
	_fpga_control = (FPGAControl*) chip[0]; // use FPGA

	/////////////////////////////////////////////////////////////////
	// get the HICANN controllog(Logger::INFO) << "HICANN communication Init()";
	/////////////////////////////////////////////////////////////////

	_hicann_control = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0]; // do not enter HICANN id here, because we only use one HICANN

	/////////////////////////////////////////////////////////////////
	// init the FPGA, DNC, HICANN communication
	/////////////////////////////////////////////////////////////////

	log(Logger::INFO) << "\tHICANN communication Init()";

	if (_hicann_control->GetCommObj()->Init(_hicann_control->addr()) != Stage2Comm::ok) {
		log(Logger::INFO) << "HICANN communication Init() failed, abort";
		return;
	}

	_dnc_control->setTimeStampCtrl(0x00); // disable the DNC timestamp handling

	log_ids();

	/////////////////////////////////////////////////////////////////
	// 	get pointers to HICANN block controls
	/////////////////////////////////////////////////////////////////
	log(Logger::INFO) << "\tgetting control instances from HicannCtrl";

	_floating_gate_controls[FGCTRL_TOP_LEFT]=&_hicann_control->getFC_tl();
	_floating_gate_controls[FGCTRL_TOP_RIGHT]=&_hicann_control->getFC_tr();
	_floating_gate_controls[FGCTRL_BOTTOM_LEFT]=&_hicann_control->getFC_bl();
	_floating_gate_controls[FGCTRL_BOTTOM_RIGHT]=&_hicann_control->getFC_br();

	_neuron_builder_control = &_hicann_control->getNBC();

	_neuron_control = &_hicann_control->getNC();

	_spl1_control = &_hicann_control->getSPL1Control();

	_repeater_controls[RCTRL_MID_LEFT]=&_hicann_control->getRC_cl();
	_repeater_controls[RCTRL_MID_RIGHT]=&_hicann_control->getRC_cr();
	_repeater_controls[RCTRL_TOP_LEFT]=&_hicann_control->getRC_tl();
	_repeater_controls[RCTRL_BOTTOM_LEFT]=&_hicann_control->getRC_bl();
	_repeater_controls[RCTRL_TOP_RIGHT]=&_hicann_control->getRC_tr();
	_repeater_controls[RCTRL_BOTTOM_RIGHT]=&_hicann_control->getRC_br();

	_l1switch_controls[L1SCTRL_MID_LEFT] = &_hicann_control->getLC_cl();
	_l1switch_controls[L1SCTRL_MID_RIGHT] = &_hicann_control->getLC_cr();
	_l1switch_controls[L1SCTRL_TOP_LEFT] = &_hicann_control->getLC_tl();
	_l1switch_controls[L1SCTRL_BOTTOM_LEFT] = &_hicann_control->getLC_bl();
	_l1switch_controls[L1SCTRL_TOP_RIGHT] = &_hicann_control->getLC_tr();
	_l1switch_controls[L1SCTRL_BOTTOM_RIGHT] = &_hicann_control->getLC_br();

	_synapse_controls[SCTRL_TOP] = &_hicann_control->getSC_top();
	_synapse_controls[SCTRL_BOTTOM] = &_hicann_control->getSC_bot();

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
}

void TmBVL1AddressFlip::set_pll(uint multiplicator, uint divisor){
	uint pdn = 0x1;
	uint frg = 0x1;
	uint tst = 0x0;
	jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
}

void TmBVL1AddressFlip::log_ids(){
	uint64_t id=0xf;
	jtag->read_id(id, jtag->pos_fpga);
	log(Logger::INFO) << "FPGA ID: 0x" << hex << id;
	jtag->read_id(id, jtag->pos_dnc);
	log(Logger::INFO) << "DNC ID: 0x" << hex << id;
	jtag->read_id(id, jtag->pos_hicann);
	log(Logger::INFO) << "HICANN ID: 0x" << hex << id;

}

void TmBVL1AddressFlip::reset_arq(){
	throw std::runtime_error("there's no ARQ reset anymore... use Init()");
	// Init() resets FPGA/HICANN ARQ stuff
}

void TmBVL1AddressFlip::activate_hicann(){

	log(Logger::INFO) << "stepwise activate() HICANN ...";
	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

	activate_floating_gate_control();
	activate_neuron_builder_control();
	activate_neuron_control();
	activate_repeater_control();
	activate_l1switch_control();
	activate_synapse_control();
	activate_spl1_control();

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

}

void TmBVL1AddressFlip::program_hicann(
	const hicann_cfg_t & config
	)
{
	log(Logger::INFO) << "stepwise program() HICANN ...";
	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

	program_neuron_builder(config);
	program_neuron_control(config);
	program_repeaters(config);
	program_l1switches(config);
	program_spl1_control(config);

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
}

/////////////////////////////////////////////////////////////////
// FloatingGateControl
/////////////////////////////////////////////////////////////////

void TmBVL1AddressFlip::activate_floating_gate_control(){

	log(Logger::INFO) << "\tactivate() Floating Gate Control";

	for(uint fg_num = 0; fg_num < FG_pkg::fg_num; fg_num += 1){
		std:: string blockname;
		switch (fg_num) {
			case FGCTRL_TOP_LEFT:
				blockname = "FGCTRL_TOP_LEFT";
				break;
			case FGCTRL_TOP_RIGHT:
				blockname = "FGCTRL_TOP_RIGHT";
				break;
			case FGCTRL_BOTTOM_LEFT:
				blockname = "FGCTRL_BOTTOM_LEFT";
				break;
			case FGCTRL_BOTTOM_RIGHT:
				blockname = "FGCTRL_BOTTOM_RIGHT";
				break;
			default:
				blockname = "invalid";
				break;
		}

		log(Logger::DEBUG0) << "\t\t block " << blockname;
		_floating_gate_controls[fg_num]->set_maxcycle(floating_gate_controler_default::MAXCYCLE);
		_floating_gate_controls[fg_num]->set_currentwritetime(floating_gate_controler_default::CURRENT_WRITE_TIME);
		_floating_gate_controls[fg_num]->set_voltagewritetime(floating_gate_controler_default::WOLTAGE_WRITE_TIME);
		_floating_gate_controls[fg_num]->set_readtime(floating_gate_controler_default::READ_TIME);
		_floating_gate_controls[fg_num]->set_acceleratorstep(floating_gate_controler_default::ACCELERATOR_STEP);
		_floating_gate_controls[fg_num]->set_pulselength(floating_gate_controler_default::PULSE_LENGTH);
		_floating_gate_controls[fg_num]->set_fg_biasn(floating_gate_controler_default::FG_BIAS_N);
		_floating_gate_controls[fg_num]->set_fg_bias(floating_gate_controler_default::FG_BIAS);
		_floating_gate_controls[fg_num]->write_biasingreg();
		_floating_gate_controls[fg_num]->write_operationreg();
	}
}

/////////////////////////////////////////////////////////////////
// Neuron Builder
/////////////////////////////////////////////////////////////////

void TmBVL1AddressFlip::activate_neuron_builder_control(){

	log(Logger::INFO) << "\tactivate() Neuron Builder Control";

    _neuron_builder_control->initzeros(); // init all rams of neurons, neuron builder and spl1 to zero
    _neuron_builder_control->setNeuronReset(true,true); // disable denmem reset
    // enables spiking for denmems on this hicann.
    // might be a good idea to always set this true in "normal" experiments
    _neuron_builder_control->setSpl1Reset(false); // enable SPL1 reset
    _neuron_builder_control->setSpl1Reset(true); // disable SPL1 reset
	_neuron_builder_control->setOutputEn(false, false); // disable output for top and bottom
	_neuron_builder_control->setOutputMux(10,10); // disable analog output for the mux. 10	log(Logger::INFO) << "\t program() Neuron Builder Control"; is not connected
	_neuron_builder_control->clear_sram();
}

void TmBVL1AddressFlip::program_neuron_builder(const hicann_cfg_t & config){

	log(Logger::INFO) << "\tprogram() Neuron Builder Control";

    //////////////////////////////
    // set global options for
    // neuromorphic circuitry
    // via the the neuron register
    //////////////////////////////

	_neuron_builder_control->setNeuronBigCap(config.denmem_block_config_top.bigcap,
	                                         config.denmem_block_config_bottom.bigcap);

	_neuron_builder_control->setNeuronGl(config.denmem_block_config_top.slow_I_gl,
	                                     config.denmem_block_config_bottom.slow_I_gl,
	                                     config.denmem_block_config_top.fast_I_gl,
	                                     config.denmem_block_config_bottom.fast_I_gl);

	_neuron_builder_control->setNeuronGladapt(config.denmem_block_config_top.slow_I_gladapt,
	                                          config.denmem_block_config_bottom.slow_I_gladapt,
	                                          config.denmem_block_config_top.fast_I_gladapt,
	                                          config.denmem_block_config_bottom.fast_I_gladapt);

	_neuron_builder_control->setNeuronRadapt(config.denmem_block_config_top.slow_I_radapt,
	                                         config.denmem_block_config_bottom.slow_I_radapt,
	                                         config.denmem_block_config_top.fast_I_radapt,
	                                         config.denmem_block_config_bottom.fast_I_radapt);

	// send config to HW
	_neuron_builder_control->configNeuronsGbl();

    //////////////////////////////////////
    // configure analog input and output
    //////////////////////////////////////

	log(Logger::INFO) << "\t\tconfiguring analog IO";

	// step 1: write corresponding bits in inout-data

	if (config.analog_output_enable_and_source_denmem_0.first)
		_neuron_builder_control->set_aout(config.analog_output_enable_and_source_denmem_0.second);
	if (config.analog_output_enable_and_source_denmem_1.first)
		_neuron_builder_control->set_aout(config.analog_output_enable_and_source_denmem_1.second);
	if(config.current_input_enable_and_target_denmem_top_left.first)
		_neuron_builder_control->set_currentin(config.current_input_enable_and_target_denmem_top_left.second);
	if(config.current_input_enable_and_target_denmem_top_right.first)
		_neuron_builder_control->set_currentin(config.current_input_enable_and_target_denmem_top_right.second);
	if(config.current_input_enable_and_target_denmem_bottom_left.first)
		_neuron_builder_control->set_currentin(config.current_input_enable_and_target_denmem_bottom_left.second);
	if(config.current_input_enable_and_target_denmem_bottom_right.first)
		_neuron_builder_control->set_currentin(config.current_input_enable_and_target_denmem_bottom_right.second);

	// step 2: set mux for analog output

	// for values of muxconf see table in sec. 4.3.5 of hicann-doc
	unsigned int muxconf_top=10, muxconf_bot=10; // 10 is default, i.e. not connected

	bool is_top = true, is_even = true;
	// first aout mux thingy
	if (config.analog_output_enable_and_source_denmem_0.first){
		muxconf_top = 4; // bad naming...
		if(config.analog_output_enable_and_source_denmem_0.second >= ((1 << nspl1_numa)/2)) is_top  = false;
		if(config.analog_output_enable_and_source_denmem_0.second%2)                        is_even = false;
		if (!is_top)  muxconf_top += 1;
		if (!is_even) muxconf_top += 2;
		// top, even => 4, top, odd => 6, bottom, even => 5, bottom, odd => 7
	}
	// second aout mux thingy
	is_top = true;
	is_even = true;
	if (config.analog_output_enable_and_source_denmem_1.first){
		muxconf_bot = 4; // still bad naming
		if(config.analog_output_enable_and_source_denmem_1.second >= ((1 << nspl1_numa)/2)) is_top  = false;
		if(config.analog_output_enable_and_source_denmem_1.second%2)                        is_even = false;
		if (!is_top)  muxconf_bot += 1;
		if (!is_even) muxconf_bot += 2;
	}
	log(Logger::INFO) << "\t\t _neuron_builder_control->setOutputMux(" << muxconf_top << ", " << muxconf_bot << ")";
	_neuron_builder_control->setOutputMux(muxconf_top, muxconf_bot);

	// step 3 SetOutputEn

	_neuron_builder_control->setOutputEn(config.analog_output_enable_and_source_denmem_0.first, // top amplifier can receive from bottom also
	                                     config.analog_output_enable_and_source_denmem_1.first);// vice versa

	// hack:
	// override config
	// on 1st output:  enable preout of inner left syndrivers
	// on 2nd output: even denmems top
	_neuron_builder_control->setOutputMux(1, 4);
	_neuron_builder_control->setOutputEn(1, 1);

	//////////////////////////////////////
	// configure denmems
	//////////////////////////////////////

	if (config.firing_denmems_and_addresses.size())
		log(Logger::INFO) << "\t\tsetting " << config.firing_denmems_and_addresses.size() << " firing denmems";
	for (std::map< unsigned int, unsigned int >::const_iterator it = config.firing_denmems_and_addresses.begin();
	     it != config.firing_denmems_and_addresses.end();
	     ++it){
		unsigned int denmem_id = it->first;
		unsigned int spl1      = it->second;
		_neuron_builder_control->enable_firing(denmem_id);
		_neuron_builder_control->set_spl1address(denmem_id, spl1);
	}

	log(Logger::INFO) << "\t\testablish vertical denmem connections";
	for (unsigned int i=0; i< config.denmem_switches_vertical.size(); ++ i){
		if (config.denmem_switches_vertical.at(i))
			_neuron_builder_control->connect_vertical(i);
	}

	assert(config.denmem_switches_top.size()==config.denmem_switches_bottom.size());
	log(Logger::INFO) << "\t\testablish horizontal denmem connections";
	std::map<denmem_connections::e,int> direction2int;
	direction2int[denmem_connections::to_left]  = -1;
	direction2int[denmem_connections::to_right] =  1;
	for (unsigned int i=0; i<config.denmem_switches_bottom.size(); ++i){

		if(config.denmem_switches_top.at(i) != denmem_connections::off){
			log(Logger::DEBUG0) << "horizontal connection top , index is " << i;
			_neuron_builder_control->connect_horizontal_top(i,direction2int[config.denmem_switches_top.at(i)]);
		}

		if(config.denmem_switches_bottom.at(i) != denmem_connections::off){
			log(Logger::DEBUG0) << "horizontal connection bottom , index is " << i;
			_neuron_builder_control->connect_horizontal_bottom(i,direction2int[config.denmem_switches_bottom.at(i)]);
		}
	}

	_neuron_builder_control->print_sram(std::cout);

	// check sram
	log(Logger::INFO) << "\t\tchecking for neuronbuilder sram integrity";
	//assert(_neuron_builder_control->check_sram());
	_neuron_builder_control->check_sram();

	// finally write all sram data
	log(Logger::INFO) << "\t\twriting sram...";
	_neuron_builder_control->write_sram();
}

/*
uint TmBVL1AddressFlip::build_neuron_builder_data(
    uint fireb,
    uint firet,
    uint connect,
    uint ntop,
    uint nbot,
    uint spl1
    )
{
	uint num=0;
	for(int i=0; i<6; i++) num |= (spl1 & (1<<i))?(1<<(5-i)):0; //reverse bit order bug
	spl1=num;
	return ((4* fireb + 2*firet + connect)<<(2*n_numd+spl1_numd))+(nbot<<(n_numd+spl1_numd))+(ntop<<spl1_numd)+spl1;
}

void TmBVL1AddressFlip::activate_single_denmem(uint add) {

	uint fireb=0;
	uint firet=0;
	uint connect=0;
	uint ntop=0x96;
	uint nbot=0x96;
	uint spl1=0;

	//cout<<"Denmem:";
	//cin>>add;
	if(add%2==1){
		//right neuron has been chosen
		ntop=0x95;
		nbot=0x95;
	}
	add=add/2;
	uint value = build_neuron_builder_data(fireb,firet,connect,ntop,nbot,spl1);
	_neuron_builder_control->write_data(add*4+3,value);
}
*/

/////////////////////////////////////////////////////////////////
// NeuronControl
/////////////////////////////////////////////////////////////////

void TmBVL1AddressFlip::activate_neuron_control(){

	log(Logger::INFO) << "\tactivate() Neuron Control";

	// mandatory otherwise the instance assumes to have zeros
	// in all registers, i.e. reads the registers to
	// get the current internal configuration
    _neuron_control->configure();

	 // optionally reset all to zero
    _neuron_control->nc_reset();

    // switch of BEGs
	// selective (0-7) or 8 for all
    _neuron_control->beg_off(8);

}

void TmBVL1AddressFlip::program_neuron_control(const hicann_cfg_t & config){

	log(Logger::INFO) << "\tprogram() Neuron Control";

    //////////////////////////////
    // background event generators
	//////////////////////////////

//    _neuron_control->set_seed((uint)seed);
//    // configure generator
//    _neuron_control->beg_configure((unit)gen_nr, (bool)poisson, (unit)period);
//    // set a neuron number for a BEG
//   _neuron_control->beg_set_number((uint)gen_nr, (unit)nnumber);
//    // enable BEGs
//    _neuron_control->beg_on((uint)gen_nr); // selective (0-7) or 8 for all

//    // Note, that at least one BEG sending
//    // on one L1 Bus has to have address 0,
//    // as this ensures that the timing of the L1 repeaters works correctly.
//    _neuron_control->beg_on(0);
//    _neuron_control->beg_set_number(0,0);

	// TODO: add seed to hicann_cfg_t
	// AND add a possibility to run all background generators with a different seed (there is a HACK from JS)
	_neuron_control->set_seed(42);

	const std::vector< background_generator_t >& bg_gens = config.background_generators;
	for ( size_t n_bg = 0; n_bg < bg_gens.size(); ++n_bg ) {
		_neuron_control->beg_configure( n_bg, bg_gens[n_bg].random, bg_gens[n_bg].period );
		_neuron_control->beg_set_number(n_bg, bg_gens[n_bg].address);

		if ( bg_gens[n_bg].reset_n ) {
			_neuron_control->beg_on(n_bg);
			log(Logger::INFO) << "Activated background event generator " << n_bg;
			if ( bg_gens[n_bg].random )
				log << " in random firing mode ";
			else
				log << " in regular firing mode ";

			log << " with period " << bg_gens[n_bg].period << " and address " << bg_gens[n_bg].address;
		}
		else {
			_neuron_control->beg_off(n_bg);
		}
	}

    //////////////////////////////
    // l1 merger config
    //////////////////////////////


	/// merger enable/select/slow signal configuration (complete configuration)
	// enum for the names of all mergers (nc_merger) as defined in hardware_base.h
	// enum nc_merger {
	//	BG0...BG7 -- 8 background merger
	//	L0_0,L0_1,L0_2,L0_3,L1_0,L1_1, L2_0, -- 7 level merger
	//	DNC0...DNC7 -- 8 dnc mergers
	//	MERG_ALL -- all mergers have same configuration
	// };
	// (bool)enable -- enable merging
	// (bool)select -- if merging is not enabled, select input 0 or 1 (see Hicann-spec for the connectivity)
	// (bool)slow -- has to be enabled for dnc-mergers that are connected to a spl1 repeater.

//    _neuron_control->merger_set(merg, enable, select, slow);

	log(Logger::INFO) << "\t\t configuring merger tree";
	const std::map< nc_merger_names::e, merger_config_t >& merger_configs =  config.neuron_control_mergers;
	std::map< nc_merger_names::e, merger_config_t >::const_iterator it_mg = merger_configs.begin();
	std::map< nc_merger_names::e, merger_config_t >::const_iterator it_mg_end = merger_configs.end();
	// Note: This sets also the dnc if channels directions for neuron_control!
	for(; it_mg != it_mg_end; ++it_mg) {
		nc_merger merg = static_cast<nc_merger>(it_mg->first);
		_neuron_control->merger_set( merg, it_mg->second.enable, it_mg->second.select, it_mg->second.slow);
	}

    //////////////////////////////
    // output to spl1 and dnc_if
    //////////////////////////////

    // choose the clock edge for sampling neurons spl1 output
	// for each DNC-HICANN channel indepenently.
//	_neuron_control->outputphase_set(
//		(bool)reg0, (bool)reg1, (bool)reg2, (bool)reg3,
//		(bool)reg4, (bool)reg5, (bool)reg6, (bool)reg7
//	);
	// Setting all phases to 0 (rising edge), which is the default!
	_neuron_control->outputphase_set(0,0,0,0,0,0,0,0);

	// enables loop back between two given 	SUCCESSIVE layer 2 channels (0..7)
	// with an inbound EVEN source to an outbound ODD end
//	_neuron_control->loopback_on((uint)source,(uint)end);

	// disable loopback for channel number
	// see hicann-doc for details
	// set loopback flag to 0 in channel number
	// if number = 8, in all channel loopback is disabled
//	_neuron_control->loopback_off((uint)number);

	// ugly version
	//// the following should work, but in the future this should be changed use commands like below.
	//assert( config.spl1_output_merger.size() == 20 );
	//for (size_t n_addr = 0; n_addr < 20; ++n_addr) {
		//_neuron_control->write_data( n_addr, config.spl1_output_merger[n_addr]);
	//}


	// configure HICANN-DNC interface as seen from HICANN side,
	// use:
	const std::vector<bool>& dnc_if_channels_direction = config.dnc_if_channels_direction; // 'true' means from DNC->HICANN

	// 0 - is inbound HICANN
	//	_neuron_control->dnc_enable_set(0,1,0,1,0,1,1,0); // L1 injection settings (?)
	_neuron_control->dnc_enable_set(
			dnc_if_channels_direction[0],
			dnc_if_channels_direction[1],
			dnc_if_channels_direction[2],
			dnc_if_channels_direction[3],
			dnc_if_channels_direction[4],
			dnc_if_channels_direction[5],
			dnc_if_channels_direction[6],
			dnc_if_channels_direction[7]);

	std::stringstream invenablestream;
	invenablestream << "neuron control enable [0:7]: " << dnc_if_channels_direction[0] << dnc_if_channels_direction[1] <<
			 dnc_if_channels_direction[2] << dnc_if_channels_direction[3] <<
			 dnc_if_channels_direction[4] << dnc_if_channels_direction[5] <<
			 dnc_if_channels_direction[6] << dnc_if_channels_direction[7] << std::endl ;
	log(Logger::DEBUG0) << invenablestream.str();

}

/////////////////////////////////////////////////////////////////
// RepeaterControl
/////////////////////////////////////////////////////////////////

void TmBVL1AddressFlip::activate_repeater_control(){
	log(Logger::INFO) << "\tactivate() Repeater Control";

	log(Logger::DEBUG0) << "\t\t repeater:" << "RCTRL_MID_LEFT";
	_repeater_controls[RCTRL_MID_LEFT]->configure();
	_repeater_controls[RCTRL_MID_LEFT]->reset();

	log(Logger::DEBUG0) << "\t\t repeater:" << "RCTRL_MID_RIGHT";
	_repeater_controls[RCTRL_MID_RIGHT]->configure();
	_repeater_controls[RCTRL_MID_RIGHT]->reset();

	log(Logger::DEBUG0) << "\t\t repeater:" << "RCTRL_TOP_LEFT";
	_repeater_controls[RCTRL_TOP_LEFT]->configure();
	_repeater_controls[RCTRL_TOP_LEFT]->reset();

	log(Logger::DEBUG0) << "\t\t repeater:" << "RCTRL_BOTTOM_LEFT";
	_repeater_controls[RCTRL_BOTTOM_LEFT]->configure();
	_repeater_controls[RCTRL_BOTTOM_LEFT]->reset();

	log(Logger::DEBUG0) << "\t\t repeater:" << "RCTRL_TOP_RIGHT";
	_repeater_controls[RCTRL_TOP_RIGHT]->configure();
	_repeater_controls[RCTRL_TOP_RIGHT]->reset();

	log(Logger::DEBUG0) << "\t\t repeater:" << "RCTRL_BOTTOM_RIGHT";
	_repeater_controls[RCTRL_BOTTOM_RIGHT]->configure();
	_repeater_controls[RCTRL_BOTTOM_RIGHT]->reset();

}

void TmBVL1AddressFlip::program_repeaters(const hicann_cfg_t & config)
{
	log(Logger::INFO) << "\tprogram() Repeater Control";

	// horizontal repeaters
	configure_repeater_block( RCTRL_MID_LEFT, config.repeater_mid_left);
	configure_repeater_block( RCTRL_MID_RIGHT, config.repeater_mid_right);

	// vertical repeaters
	configure_repeater_block( RCTRL_TOP_LEFT, config.repeater_top_left);
	configure_repeater_block( RCTRL_TOP_RIGHT, config.repeater_top_right);
	configure_repeater_block( RCTRL_BOTTOM_LEFT, config.repeater_bottom_left);
	configure_repeater_block( RCTRL_BOTTOM_RIGHT, config.repeater_bottom_right);

	return;
}

void TmBVL1AddressFlip::configure_repeater_block(
	repeater_control_location rep_loc,
	const std::vector<repeater_config::e>& repeater_config
	)
{

	log(Logger::INFO) << "\t\tconfigure_repeater_block(), Block = " << rep_loc;

	// check size of vector: 64 for vertical repeater blocks and 32 for horizontal
	if ( rep_loc ==  RCTRL_TOP_LEFT || rep_loc ==  RCTRL_TOP_RIGHT
		|| rep_loc ==  RCTRL_BOTTOM_LEFT || rep_loc ==  RCTRL_BOTTOM_RIGHT ) {
		assert( repeater_config.size() == 64 );
	}
	else if ( rep_loc ==  RCTRL_MID_RIGHT || rep_loc ==  RCTRL_MID_LEFT ) {
		assert( repeater_config.size() == 32 );
	}
	else {
		log(Logger::INFO) << "\t\tconfigure_repeater_block(), Block = " << rep_loc << " is INVALID, doing nothing";
		return;
	}

	/// Special treatment for left repeater block, which includes spl1-repeaters.
	if( rep_loc == RCTRL_MID_LEFT ) {

		// the following should work. However, in the future this could be replace a more abstract config (e.g. direction and on/off)
		for(size_t n_rep = 0; n_rep < repeater_config.size(); ++n_rep) {
			if (repeater_config[n_rep] != repeater_config::off){
				// normal repeater
				if( !_repeater_controls[rep_loc]->is_spl1rep(n_rep)) {
					RepeaterControl::rep_mode mode = RepeaterControl::FORWARD; // activate this repeater
					if (repeater_config[n_rep] == repeater_config::outwards) {
						_repeater_controls[rep_loc]->conf_repeater(n_rep,mode, RepeaterControl::OUTWARDS, true);
					} else if (repeater_config[n_rep] == repeater_config::inwards) {
						_repeater_controls[rep_loc]->conf_repeater(n_rep,mode, RepeaterControl::INWARDS, true);
					}
					else {
						log(Logger::WARNING) << "\t\tinvalid repeater config for rep " << n_rep << " in Block = " << rep_loc;
					}
				}
				// spl1 repeater
				else {
					// used as normal repeater outwards
					if (repeater_config[n_rep] == repeater_config::outwards) {
						_repeater_controls[rep_loc]->conf_spl1_repeater(n_rep, RepeaterControl::FORWARD, RepeaterControl::OUTWARDS, true);
					}
					// used as normal repeater inwards
					else if (repeater_config[n_rep] == repeater_config::inwards) {
						_repeater_controls[rep_loc]->conf_spl1_repeater(n_rep, RepeaterControl::FORWARD, RepeaterControl::INWARDS, true);
					}
					// spl1 is inserted only to hbus on this Hicann
					else if (repeater_config[n_rep] == repeater_config::spl1_int) {
						_repeater_controls[rep_loc]->conf_spl1_repeater(n_rep, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);
					}
					// spl1 is inserted only to hbus on left neighbour Hicann
					else if (repeater_config[n_rep] == repeater_config::spl1_ext) {
						_repeater_controls[rep_loc]->conf_spl1_repeater(n_rep, RepeaterControl::SPL1OUT, RepeaterControl::OUTWARDS, true);
					}
					// spl1 is inserted into both: hbus of this and hbus of left neighbour Hicann
					else if (repeater_config[n_rep] == repeater_config::spl1_int_and_ext) {
						log(Logger::ERROR) << "unsupported SPL1 repeater config for rep " << n_rep;
						log(Logger::ERROR) << "RepeaterControl does not offer an interface to configure spl1 repeater to send into both direction at the same time (to this HICANN and left neighbour HICANN)";
						log(Logger::ERROR) << "HARDIES NERVEN!" << Logger::flush;
						assert(false);
					}
					else {
						log(Logger::WARNING) << "\t\tinvalid repeater config for rep " << n_rep << " in Block = " << rep_loc;
					}
				}
			}
		}

	} else { // rep_loc != RCTRL_MID_LEFT

		for(size_t n_rep = 0; n_rep < repeater_config.size(); ++n_rep) {
			if (repeater_config[n_rep] != repeater_config::off ) {
				RepeaterControl::rep_mode mode = RepeaterControl::FORWARD; // activate this repeater
				if (repeater_config[n_rep] == repeater_config::outwards) {
					_repeater_controls[rep_loc]->conf_repeater(n_rep,mode, RepeaterControl::OUTWARDS, true);
				} else if (repeater_config[n_rep] == repeater_config::inwards) {
					_repeater_controls[rep_loc]->conf_repeater(n_rep,mode, RepeaterControl::INWARDS, true);
				}
				else {
					log(Logger::WARNING) << "\t\tinvalid repeater config for rep " << n_rep << " in Block = " << rep_loc;
				}
			}
		}

	} // repeater == RCTRL_MID_LEFT

	return;
}

/////////////////////////////////////////////////////////////////
// L1SwitchControl
/////////////////////////////////////////////////////////////////

// L1SwitchControl::configure()
// otherwise the instance assumes to have zeros in all registers
// and which reads the registers to
// get the current internal configuration)
void TmBVL1AddressFlip::activate_l1switch_control(){

	log(Logger::INFO) << "\tactivate() L1 Switch  Control";

	log(Logger::DEBUG0) << "\t\t l1-switch:" << "L1SCTRL_MID_LEFT";
	_l1switch_controls[L1SCTRL_MID_LEFT]->configure();

	log(Logger::DEBUG0) << "\t\t l1-switch:" << "L1SCTRL_MID_RIGHT";
	_l1switch_controls[L1SCTRL_MID_RIGHT]->configure();

	log(Logger::DEBUG0) << "\t\t l1-switch:" << "L1SCTRL_TOP_LEFT";
	_l1switch_controls[L1SCTRL_TOP_LEFT]->configure();

	log(Logger::DEBUG0) << "\t\t l1-switch:" << "L1SCTRL_BOTTOM_LEFT";
	_l1switch_controls[L1SCTRL_BOTTOM_LEFT]->configure();

	log(Logger::DEBUG0) << "\t\t l1-switch:" << "L1SCTRL_TOP_RIGHT";
	_l1switch_controls[L1SCTRL_TOP_RIGHT]->configure();

	log(Logger::DEBUG0) << "\t\t l1-switch:" << "L1SCTRL_BOTTOM_RIGHT";
	_l1switch_controls[L1SCTRL_BOTTOM_RIGHT]->configure();

}

void TmBVL1AddressFlip::program_l1switches(const hicann_cfg_t & config){

	log(Logger::INFO) << "\tprogram() L1 Switch  Control";

	//////////////////////////////
    // cross bars
    //////////////////////////////
	configure_l1_switch( TmBVL1AddressFlip::L1SCTRL_MID_LEFT, config.crossbar_left);
	configure_l1_switch( TmBVL1AddressFlip::L1SCTRL_MID_RIGHT, config.crossbar_right);

	//////////////////////////////
    // select switches
    //////////////////////////////
	configure_l1_switch( TmBVL1AddressFlip::L1SCTRL_TOP_LEFT, config.syndriver_switch_top_left);
	configure_l1_switch( TmBVL1AddressFlip::L1SCTRL_BOTTOM_LEFT, config.syndriver_switch_bottom_left);
	configure_l1_switch( TmBVL1AddressFlip::L1SCTRL_TOP_RIGHT, config.syndriver_switch_top_right);
	configure_l1_switch( TmBVL1AddressFlip::L1SCTRL_BOTTOM_RIGHT, config.syndriver_switch_bottom_right);
}

void TmBVL1AddressFlip::configure_l1_switch(
	l1switch_control_location switch_loc,
	const std::vector< std::pair<unsigned int, unsigned int> >& connections
	)
{
	std::vector< std::pair<unsigned int, unsigned int> >::const_iterator it_con = connections.begin();
	std::vector< std::pair<unsigned int, unsigned int> >::const_iterator it_con_end = connections.end();

	log(Logger::INFO) << "\t\tblock = " << switch_loc << " with " << connections.size() << " enabled switches";
	_l1switch_controls[switch_loc]->reset();
	for(;it_con != it_con_end; ++it_con) {
		// connects/disconnects vertical and horizontal lines,
		// last argument specifies to connect(true) or not connect
		_l1switch_controls[switch_loc]->switch_connect( it_con->first, it_con->second, true);
	}

//	_l1switch_controls[switch_loc]->print_config();
}

/////////////////////////////////////////////////////////////////
// SynapseControl
/////////////////////////////////////////////////////////////////

void TmBVL1AddressFlip::activate_synapse_control(){

	log(Logger::INFO) << "\tactivate() Synapse Control";

	// configure the synapse array, i.e.
	// read in current status of the (mandatory)
	_synapse_controls[SCTRL_TOP]->configure();
	_synapse_controls[SCTRL_BOTTOM]->configure();

	// reset drivers (mandatory)
	_synapse_controls[SCTRL_TOP]->reset_drivers();
	_synapse_controls[SCTRL_BOTTOM]->reset_drivers();

	// reset all, incl weights and decoders to 0 (optional)
	//_synapse_controls[SCTRL_TOP]->reset_all();
	//_synapse_controls[SCTRL_BOTTOM]->reset_all();

}

/////////////////////////////////////////////////////////////////
// SPL1 Control
/////////////////////////////////////////////////////////////////

void TmBVL1AddressFlip::activate_spl1_control(){
	log(Logger::INFO) << "\tactivate() SPL1 Control";
	_spl1_control->write_reset();
}

void TmBVL1AddressFlip:: program_spl1_control(const hicann_cfg_t & config){
	log(Logger::DEBUG0) << "\tprogram() SPL1 Control";

	// use:
	const std::vector<bool>& dnc_if_channels_direction = config.dnc_if_channels_direction; // 'true' means from DNC->HICANN
	const std::vector<bool>& dnc_if_channels_enable = config.dnc_if_channels_enable; // true = enable

	// TODO -- sanity check for interface direction and interface enable vector lengths
	// dnc_if_channels_direction.size() == DNCL1BUSINCOUNT & dnc_if_channels_enable.size() == DNCL1BUSINCOUNT

	//////////////////////////////
	// configure HICANN-DNC interface
	// as seen from DNC side,
	//////////////////////////////

	// 0 - is outbound DNC
	//	_dnc_control->setDirection(0xA9); // set transmission directions in DNC (for HICANN 0 only)
	//	_spl1_control->write_cfg(0x0A9ff); // transmission direction on HICANN side, no timestamp handling (MSB=0)

	// set transmission directions in DNC (for HICANN 0 only)
	// reverse bit order of config struct direction vector as last element is for channel 7
	// which is MSB in dnc_control->setDirection(..)
	unsigned int dnc_if_dirs = 0;
	vector<bool>::const_iterator it = dnc_if_channels_direction.end()-1;
	std::stringstream dirstream;
	dirstream << "dnc if channel dirs [0:7]:";
	for(;it >= dnc_if_channels_direction.begin();--it) {
		dirstream << *it;
		dnc_if_dirs <<= 1;
			if(*it){
				dnc_if_dirs++;
			}
	}
	dirstream << " dnc_if_dirs [0:7]:" << hex << dnc_if_dirs;
	log(Logger::DEBUG0) << dirstream.str();
	_dnc_control->setDirection(dnc_if_dirs);

	// transmission direction on HICANN as seen from DNC side,
	// thus same config bits as for the dnc_control
	// also reverse bit order of config struct enable vector
	// as last element is for channel 7 which is MSB in
	// spl1_control->write_cfg(..)
	// Note: no timestamp handling fast hack
	//[16] timestamp handling
	//[15:8] channel direction
	//[7:0] channel enable
	unsigned int dnc_if_enbl = dnc_if_dirs;
	it = dnc_if_channels_enable.end()-1;
	std::stringstream enablestream;
	enablestream << "dnc if channel enables [0:7]:";
	for(;it >= dnc_if_channels_enable.begin();--it) {
		enablestream << *it;
		dnc_if_enbl <<= 1;
		if(*it){
			dnc_if_enbl++;
		}

	}
	enablestream << " dnc_if_enbl [0:7]:" << hex << dnc_if_enbl;
	log(Logger::DEBUG0) << enablestream.str();
	_spl1_control->write_cfg(dnc_if_enbl);

}

/////////////////////////////////////////////////////////////////
// PULSES
/////////////////////////////////////////////////////////////////

bool TmBVL1AddressFlip::init_layer2()
{

	// test if configuration pointers already set
	if ( !_hicann_control )
    {
    	log(Logger::WARNING) << "Started init_layer2 without initial setup. Calling initialize_system.";
        this->initialize_system();
    }

	// for DNC version 2, several initialization trials are not needed anymore -> Init() call in initialize_iboardv2 is enough
#if DNC_V == 2
	log(Logger::INFO) << "Using DNC version 2";
	S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(_hicann_control->GetCommObj());
#else
	log(Logger::INFO) << "Initializing DNC version 1";
    
	uint64_t jtagid;
	jtag->read_id(jtagid,jtag->pos_fpga); // avoid re-resetting of FPGA (would be unsynchronized to HICANN)
	if (jtagid != 0xbabebeef)
		jtag->reset_jtag();

	log_ids();

	//////////////////////////////////////////
	// FPGA<->DNC LVDS
	//////////////////////////////////////////

	log(Logger::INFO) << "TmBVL1AddressFlip::init_layer2 initialize FPGA<->DNC LVDS links" << Logger::flush;

	// initialize FPGA<->DNC LVDS links (this may take a while!

	S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(_hicann_control->GetCommObj());
	if (comm_ptr == NULL)
    {
    	log(Logger::INFO) << "Warning: wrong communication model used for highspeed L2 transmission. Please use option -bje2f";
        return false;
    }

	if (comm_ptr->fpga_dnc_init(0,true) != 1)
    	return false;

	if (comm_ptr->get_transmission_errors()>20)
		return false;

#endif // DNC_V

	//////////////////////////////////////////
	// System Timer
	//////////////////////////////////////////

	log(Logger::INFO) << "TmBVL1AddressFlip::init_layer2 enabling systime counter" << Logger::flush;

	unsigned int curr_ip = jtag->get_ip();

	if ( (curr_ip != 0) && (comm_ptr != NULL)) // if Ethernet-JTAG available, trigger via Ethernet (without timestamp offset), else use JTAG method (with offset...)
	{
		comm_ptr->trigger_systime(curr_ip,false); // stop first -> necessary when re-initializing
		jtag->HICANN_stop_time_counter();
		jtag->HICANN_start_time_counter();
		comm_ptr->trigger_systime(curr_ip,true);
		log(Logger::INFO) << "Systime counters started synchronously.";
	}
	else { // use JTAG method (with offset...)
		jtag->FPGA_set_systime_ctrl(1);
	}

    return true;
}


/// pulses are written to the FPGA via JTAG_Ethernet bridge to the FPGA Playback FIFO
/// this includes transformation of pulse times to timestamp values; currently transformation assumes biological times in ms
void TmBVL1AddressFlip::write_playback_pulses(
		std::map< l2_pulse_address_t, std::vector<double> > spiketrains  //!< an input spike train in hw time
	)
{

	// setup pulse channels
	jtag->FPGA_reset_tracefifo();

	double playback_begin_offset = 500e-9; // in seconds, offset time of between start of experiment and first pulse

	double fpgahicann_delay = 400e-9; // in seconds, denotes the transmission delay FPGA-HICANN

	// convert spiketrains to single list, sorted by time
	std::vector<pulse_float_t> pulselist;

	std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st= spiketrains.begin();
	std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st_end = spiketrains.end();

    for (;it_st != it_st_end; ++it_st )
    {
		l2_pulse_address_t curr_addr = it_st->first;
		std::vector<double> & times = it_st->second;
        // ignore all IDs from HICANN upwards, because used only for single HICANN experiments - ID is set in testmode
		//unsigned int curr_id = ((this->hicann_nr&0x7)<<9) | (curr_addr.address&0x1ff);
		unsigned int curr_id = curr_addr.data.integer & 0xfff ;

		for (unsigned int npulse=0;npulse<times.size();++npulse)
			pulselist.push_back(pulse_float_t(curr_id,times[npulse]));
    }

	std::sort(pulselist.begin(),pulselist.end());

	log(Logger::DEBUG0) << "TmBVL1AddressFlip::write_playback_pulses: Send " << pulselist.size() << " pulses:";
	for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
		log(Logger::DEBUG0) << "  id=" << pulselist[npulse].id << ", time=" <<pulselist[npulse].time;


	if (pulselist.size())
	{
		this->starttime = pulselist[0].time - fpgahicann_delay - playback_begin_offset; // always start first pulse at clk cycle 0 + offset
		int prev_reltime = 1; // account for delay of 1 clk during playback fifo release
		unsigned int discarded_count = 0;

		// only write playback_fifo_size pulses.
		// otherwise only the last "modulo playback_fifo_size" pulses would be considered.
		for (unsigned int npulse=0; (npulse<pulselist.size() && npulse < playback_fifo_size);++npulse)
		{
			double curr_time = pulselist[npulse].time - starttime;
			int curr_cycle = (int)((curr_time - fpgahicann_delay) / this->clkperiod_fpga + 0.5);
			int curr_tstamp = (int)(fpgahicann_delay / this->clkperiod_dnc + 0.5); // write only FPGA-HICANN delay, because FPGA release time is added in playback FIFO

			if ((prev_reltime+2 > curr_cycle) && (npulse>0)) // ensure minimum of two clk cycles between release times
			{
				curr_cycle = prev_reltime + 2;
				if (curr_cycle > curr_tstamp) // release time too late for pulse to reach DNC before timestamp expires -> throw away
				{
					++discarded_count;
					continue;
				}
			}

			unsigned int hicann_id = (pulselist[npulse].id >> 9) & 0x7;
			unsigned int neuron_id = pulselist[npulse].id & 0x1ff;
			unsigned int pulse_data = (hicann_id<<24) | (neuron_id << 15) | (curr_tstamp & 0x7fff );
			unsigned int extra_pulse = 0;

			log(Logger::DEBUG0) << "TmBVL1AddressFlip: fill data: FPGA delta time: "<< curr_cycle-prev_reltime << ", HICANN " << hicann_id << ", pulse: " << pulse_data << ", extra: " << extra_pulse;

			jtag->FPGA_fill_playback_fifo(false, (curr_cycle-prev_reltime)<<1, hicann_id, pulse_data, extra_pulse);

			prev_reltime = curr_cycle;
		}

	}

}

bool TmBVL1AddressFlip::execute_experiment(float duration_in_s)
{
	bool read_output_of_repeater = false;

	// Start systime counter if successful
	log(Logger::INFO) << "tmbv_l1addressflip: Enabling systime counter";
	unsigned int curr_ip = jtag->get_ip();
	S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(_hicann_control->GetCommObj());
	if ( (curr_ip != 0) && (comm_ptr != NULL)) // if Ethernet-JTAG available, trigger via Ethernet (without timestamp offset), else use JTAG method (with offset...)
	{
		comm_ptr->trigger_systime(curr_ip,false); // stop first -> necessary when re-initializing
		jtag->HICANN_stop_time_counter();
		jtag->HICANN_start_time_counter();
		comm_ptr->trigger_systime(curr_ip,true);
		log(Logger::INFO) << "Systime counters started synchronously.";
	}
	else
		jtag->FPGA_set_systime_ctrl(1);

	// start experiment execution
	log(Logger::INFO) << "tm_l2pulses: Start: enabling fifos";
	jtag->FPGA_enable_tracefifo();

	// test: send some pulses directly
	//for (unsigned int n=0;n<5;++n)
	//    jtag->FPGA_start_pulse_event(0,0xabc0+n);

	jtag->FPGA_enable_fifoloop();
	jtag->FPGA_enable_pulsefifo();

	read_output_of_repeater = true;

	// BE CAREFUL when enabling the readout of a repeater:
	// if pulses are recorded in the FPGA, there will be config packets, that fill up the trace.
	// also, this may lead to stop the repeaters from being working
	if (read_output_of_repeater)
	{
		//unsigned int rep_id = (156 - 128) /2;
		 for (size_t n_repetitions = 0; n_repetitions<100; ++n_repetitions){
			readout_repeater(_block_of_readout_repeater, RepeaterControl::OUTWARDS, _id_of_readout_repeater);
		 }
	}

	// wait...
	log(Logger::INFO) << "tmbv_l1addressflip: Running experiment for " << duration_in_s << " seconds";
	 usleep(1000000*duration_in_s);

	// stop experiment
	log(Logger::INFO) << "tmbv_l1addressflip: Stopping experiment";
	jtag->FPGA_disable_pulsefifo();
	jtag->FPGA_disable_tracefifo();
	jtag->FPGA_disable_fifoloop();

	return true;
}

bool TmBVL1AddressFlip::execute_experiment(
			float duration_in_s,
			vector< vector<uint> >& times,
			vector< vector<uint> >& nnums
		)
{
	bool read_output_of_repeater = false;

	// Start systime counter if successful
	log(Logger::DEBUG0) << "tmbv_l1addressflip: Enabling systime counter";
	unsigned int curr_ip = jtag->get_ip();
	S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(_hicann_control->GetCommObj());
	if ( (curr_ip != 0) && (comm_ptr != NULL)) // if Ethernet-JTAG available, trigger via Ethernet (without timestamp offset), else use JTAG method (with offset...)
	{
		comm_ptr->trigger_systime(curr_ip,false); // stop first -> necessary when re-initializing
		jtag->HICANN_stop_time_counter();
		jtag->HICANN_start_time_counter();
		comm_ptr->trigger_systime(curr_ip,true);
		log(Logger::DEBUG0) << "Systime counters started synchronously.";
	}
	else
		jtag->FPGA_set_systime_ctrl(1);

	// start experiment execution
	log(Logger::DEBUG0) << "tm_l2pulses: Start: enabling fifos";
	jtag->FPGA_enable_tracefifo();

	// test: send some pulses directly
	//for (unsigned int n=0;n<5;++n)
	//    jtag->FPGA_start_pulse_event(0,0xabc0+n);

	jtag->FPGA_enable_fifoloop();
	jtag->FPGA_enable_pulsefifo();

	read_output_of_repeater = true;

	// BE CAREFUL when enabling the readout of a repeater:
	// if pulses are recorded in the FPGA, there will be config packets, that fill up the trace.
	// also, this may lead to stop the repeaters from being working
	if (read_output_of_repeater)
	{
		//unsigned int rep_id = (156 - 128) /2;
		 for (size_t n_repetitions = 0; n_repetitions<100; ++n_repetitions){
			vector<uint> times_0(3,0);
			vector<uint> nnums_0(3,0);
			readout_repeater(_block_of_readout_repeater, RepeaterControl::OUTWARDS, _id_of_readout_repeater, times_0, nnums_0,/* silent */ 1);
			times.push_back(times_0);
			nnums.push_back(nnums_0);
		 }
	}

	// wait...
	log(Logger::DEBUG0) << "tmbv_l1addressflip: Running experiment for " << duration_in_s << " seconds";
	 usleep(1000000*duration_in_s);

	// stop experiment
	log(Logger::DEBUG0) << "tmbv_l1addressflip: Stopping experiment";
	jtag->FPGA_disable_pulsefifo();
	jtag->FPGA_disable_tracefifo();
	jtag->FPGA_disable_fifoloop();

	return true;
}

void TmBVL1AddressFlip::readout_repeater(repeater_control_location loca, RepeaterControl::rep_direction dir, uint repnr){
	vector<uint> times(3,0);
	vector<uint> nnums(3,0);
	readout_repeater(loca, dir, repnr, times, nnums, 1);
	cout << "Repeater " << repnr << ": ";
	cout << "Received neuron numbers " << dec << nnums[0] << ", " << nnums[1] << ", " << nnums[2];
	cout << " with delays " << times[1]-times[0] << " and " << times[2]-times[1] << " cycles" << endl;
}

void TmBVL1AddressFlip::readout_repeater(repeater_control_location loca, RepeaterControl::rep_direction dir, uint repnr, vector<uint>& times, vector<uint>& nnums, bool silent){
	uint odd=0;
	#if HICANN_V>=2
	if (_repeater_controls[loca]->repaddr(repnr)%2) odd=1;
	#elif HICANN_V==1
	if (repnr%2) odd=1;
	#endif
	_repeater_controls[loca]->conf_repeater(repnr, RepeaterControl::TESTIN, dir, true); //configure repeater in desired block to receive BEG data
	usleep(1000); //time for the dll to lock
	_repeater_controls[loca]->stopin(odd); //reset the full flag
	_repeater_controls[loca]->startin(odd); //start recording received data to channel 0
	usleep(1000);  //recording lasts for ca. 4 µs - 1ms
	
	if (!silent) cout << endl << "Repeater " << repnr << ":" << endl;
	for (int i=0; i<3; i++){
		_repeater_controls[loca]->tdata_read(odd, i, nnums[i], times[i]);
		if (!silent) cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
	}
	_repeater_controls[loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
	_repeater_controls[loca]->stopin(odd);
	_repeater_controls[loca]->tout(repnr, false); //set tout back to 0 to prevent conflicts
}
    

/// currently, transformation of timestamps back to pulse times outputs biological times in ms
void TmBVL1AddressFlip::read_trace_pulses(
			std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
		)
{
	spiketrains.clear();

	unsigned int spk_count = 0;
	unsigned int cycle_count = 0;
	unsigned int prev_tstamp = 0;

	while((!jtag->FPGA_empty_pulsefifo()))
	{
		uint64_t rdata;
		bool is_config_packet;
		jtag->FPGA_get_trace_fifo(rdata, is_config_packet);
		if ( is_config_packet ) {
			log(Logger::INFO) << "read_trace_pulses: received config packet" << Logger::flush;
			continue;
		}


		unsigned int curr_id = (rdata>>15)&0x1ff;
		unsigned int curr_tstamp = rdata&0x7fff;

		if ( (spk_count > 0) && ( prev_tstamp > curr_tstamp+0x2000 ) ) // overflow detected (tolerates unsorted pulse times up to a quarter of the timestamp counter range)
			cycle_count += 1;

		double curr_time = this->starttime + this->clkperiod_dnc*(curr_tstamp + cycle_count*0x8000);
		prev_tstamp = curr_tstamp;


		if (spk_count < 10)
			log(Logger::INFO) << "read_trace_pulses: received data " << (unsigned long)rdata << " -> id=" << curr_id << ", tstamp=" << curr_tstamp << ", time=" << curr_time;

		l2_pulse_address_t curr_address(curr_id);

		spiketrains[curr_address].push_back( curr_time );

		++spk_count;

		if ( (rdata>>63) ) // second pulse available
		{
			rdata = rdata>>32;
			curr_id = (rdata>>15)&0x1ff; // ignore HICANN ID, because this is set in testmode
			curr_tstamp = rdata&0x7fff;

			if ( (spk_count > 0) && ( prev_tstamp > curr_tstamp+0x2000 ) ) // overflow detected (tolerates unsorted pulse times up to a quarter of the timestamp counter range)
				cycle_count += 1;

			curr_time = this->starttime + this->clkperiod_dnc*(curr_tstamp + cycle_count*0x8000);
			prev_tstamp = curr_tstamp;

			l2_pulse_address_t curr_address_2(curr_id);

			spiketrains[curr_address_2].push_back( curr_time );
            ++spk_count;
		}
	}

	log(Logger::INFO) << "read_trace_spikes: " << spk_count << " spikes from " << spiketrains.size() << " addresses found" << Logger::flush;
}


/////////////////////////////////////////////////////////////////
// TESTMODE
/////////////////////////////////////////////////////////////////

bool TmBVL1AddressFlip::test() {
	initialize_system();
	activate_hicann(); // resets everything
	init_layer2();
	set_pll(_pll_mult, _pll_div);

	char c;
	bool cont=true;
	do{
		cout << "Select test option:" << endl;
		cout << "0: Reset everything (all rams)" << endl;
		cout << "1: Set PLL Freqency on Hicann" << endl;
		cout << "2: Select vertical bus id" << endl;
		cout << "3: Select Input Channel (BEG and L2)" << endl;
		cout << "4: Run l1 address flip experiment" << endl;
		cout << "5: Run l1 address flip experiment systematic (Takes about 1 hour)" << endl;
		cout << "x: Exit" << endl;
		cin >> c;

		switch(c){

			case '0':{
				cout << "NOT IMPLEMENTED" << endl;
				cout << "resetting everything" << endl;
				cout << "resetting synapse drivers" << endl;
				_synapse_controls[SCTRL_TOP]->reset_drivers();
				_synapse_controls[SCTRL_BOTTOM]->reset_drivers();
				cout << "resetting crossbars and switches" << endl;
				_l1switch_controls[L1SCTRL_MID_LEFT]->reset();
				_l1switch_controls[L1SCTRL_MID_RIGHT]->reset();
				_l1switch_controls[L1SCTRL_TOP_LEFT]->reset();
				_l1switch_controls[L1SCTRL_BOTTOM_LEFT]->reset();
				_l1switch_controls[L1SCTRL_TOP_RIGHT]->reset();
				_l1switch_controls[L1SCTRL_BOTTOM_RIGHT]->reset();
				cout << "resetting repeater blocks" << endl;
				_repeater_controls[RCTRL_MID_LEFT]->reset();
				_repeater_controls[RCTRL_MID_RIGHT]->reset();
				_repeater_controls[RCTRL_TOP_LEFT]->reset();
				_repeater_controls[RCTRL_BOTTOM_LEFT]->reset();
				_repeater_controls[RCTRL_TOP_RIGHT]->reset();
				_repeater_controls[RCTRL_BOTTOM_RIGHT]->reset();
				cout << "resetting neuron builder" << endl;
				_neuron_builder_control->initzeros();
				cout << "resetting neuron control" << endl;
				_neuron_control->nc_reset();
			} break;

			case '1':{

				uint pll_mult;
				cout << "Select PLL Frequency:" << endl;
				cout << "1:  50 MHz" << endl;
				cout << "2: 100 MHz" << endl;
				cout << "3: 150 MHz" << endl;
				cout << "4: 200 MHz" << endl;
				cout << "5: 250 MHz" << endl << endl;
				cin >> pll_mult;


				if (_pll_mult < 6 && _pll_mult > 0) {
					_pll_mult=pll_mult;
					set_pll(_pll_mult, _pll_div);
				}
				else {
					cout << "You selected a not supported option: " << _pll_mult << "\tPLL is not changed";
				}
			} break;

			case '2':{

				unsigned int vbus_id;
				cout << "Select Vertical Bus ID ( one of [";
				std::vector<uint> vbuses = get_vbuses_for_hbus(_h_bus);
				for (size_t nn = 0; nn < vbuses.size(); ++nn)
					cout << vbuses[nn] << ",";
				cout << "]" << endl;

				cin >> vbus_id;

				if ( !set_v_bus(vbus_id) )
					cout << "this vertical bus can not be connected!" << endl;

			} break;

			case '3':{

				unsigned int channel;
				cout << "Select input channel : 0..7" << endl;
				cin >> channel;

				if ( !set_channel(channel) )
					cout << "got wrong input, nothing changed!" << endl;

			} break;

			case '4':{

				uint period, nnumber1 = 0;
				cout << "Enter the neuron number to test: " << flush;
				cin >> nnumber1;
				cout << "Enter period between events: " << flush;
				cin >> period;
				double clk_cycle = 20.e-9*_pll_div/_pll_mult;

				// 1.) configure begs and mergers
				// only BEG 4 is used
				// input from L2 is merged to the corresponding output register.
				//
				nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
						DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
				//bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1,  1,  1, 1, 1, 1, 1, 1, 1, 1}; // slow enable
				//bool se[23] = {0, 0, 0, 0, 1, 0, 0, 0,  0, 0, 0, 0,  0, 0,  1,  0, 0, 0, 0, 0, 0, 0, 0}; // input select, only BEG 4 active
				//bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 1, 0, 0, 0}; // enable merging, merging dnc channel 4
				
				// default, everything switched off, no merging, all channels go straight.
				bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1,  1,  1, 1, 1, 1, 1, 1, 1, 1}; // slow enable
				bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0,  1, 0, 0, 0,  1, 0,  1,  0, 0, 0, 0, 0, 0, 0, 0}; // input select
				bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0}; // enable merging
				vector<nc_merger> merg(mer, mer+23);
				vector<bool> slow(sl, sl+23);
				vector<bool> select(se, se+23);
				vector<bool> enable(en, en+23);

				// choose input from the desired BEG (select)
				select[_channel] = 1;
				// enable merging in the dnc merger 
				enable[_channel+15] = 1;

				_neuron_control->merger_set(merg, enable, select, slow);
				_neuron_control->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output

				
				// 2.) configure sending repeater
				_repeater_controls[RCTRL_MID_LEFT]->conf_spl1_repeater(_id_of_sending_repeater, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

				// 3.) switch from horizontal to vertical (crossbar right 25 -> _v_bus)
				//L1SCTRL_MID_RIGHT 
				_l1switch_controls[L1SCTRL_MID_RIGHT]->reset();
				_l1switch_controls[L1SCTRL_MID_LEFT]->reset();
				// connects/disconnects vertical and horizontal lines,
				// last argument specifies to connect(true) or not connect
				if (_v_bus >= 128 )
					_l1switch_controls[L1SCTRL_MID_RIGHT]->switch_connect( _h_bus, _v_bus, true);
				else
					_l1switch_controls[L1SCTRL_MID_LEFT]->switch_connect( _h_bus, _v_bus, true);

				_neuron_control->beg_configure(_channel, 0, 2000);
				_neuron_control->beg_set_number(_channel, 0); // address 0
				_neuron_control->beg_off(8);
				_neuron_control->beg_on(_channel); //sends 0

				usleep(3000);  //locking

				bool read_output_of_repeater = false;
				read_output_of_repeater = true;

				// Test for looking wether the repeater is locking
				if (read_output_of_repeater)
				{
					 for (size_t n_repetitions = 0; n_repetitions<10; ++n_repetitions){
						 readout_repeater(_block_of_readout_repeater, RepeaterControl::OUTWARDS, _id_of_readout_repeater);
					 }
				}
				// 2.) configure spl1_control and dnc
				bool dnc_if_direction[8] = {1,1,1,1,1,1,1,1}; // all channels from DNC->HICANN
				bool dnc_if_enable[8]    = {0,0,0,0,0,0,0,0};
				// bool dnc_if_enable[8]    = {0,0,0,0,1,0,0,0}; // only channel 4 enabled

				dnc_if_enable[_channel] = 1;

				// create hicann_cfg, so I can use program_spl1_control for configuration of dnc and dnc_if
				hicann_cfg_t hicann_cfg;
				hicann_cfg.dnc_if_channels_direction = std::vector<bool>(dnc_if_direction, dnc_if_direction+8);
				hicann_cfg.dnc_if_channels_enable    = std::vector<bool>(dnc_if_enable, dnc_if_enable+8);
				program_spl1_control(hicann_cfg);

				// prepare input spikes
				std::map< l2_pulse_address_t, std::vector<double> > spiketrains;

				// Target address is DNC 0, HICANN 0, DNC_IF_CHANNEL 4 and the requested l1 address
				l2_pulse_address_t addr_1;
				addr_1.data.fields.dnc_if_channel = _channel;
				addr_1.data.fields.l1_address = nnumber1;

				// create vector with spiketimes
				// spiketimes are generated as multiple of the chosen clock period.
				std::vector<double> spiketimes; // in hw_time = real time
				unsigned int isi_cycles =  period;
				unsigned int num_spikes = 500;
				double offset = 500e-9;
				for (size_t n= 0; n<num_spikes; ++n) {
					spiketimes.push_back(offset + n * isi_cycles*clk_cycle);
				}
				// add one more spike with offset
				spiketimes.push_back(spiketimes.back()+offset);

				// send spikes to playback fifo
				spiketrains[addr_1] = spiketimes;
				write_playback_pulses(spiketrains);

				// execute experiment:
				// playback spikes and readout the results at the readout repeater
				// Note that the Playback Fifo runs in Loop Mode!!!
				execute_experiment(1);
				log(Logger::INFO) << "tmbv_l1addressflip: config of last experiment was: ";
				log(Logger::INFO) << "\tPLL:\t" << _pll_mult;
				log(Logger::INFO) << "\tCHANNEL:\t" << _channel;
				log(Logger::INFO) << "\tHBUS:\t" << _h_bus;
				log(Logger::INFO) << "\tVBUS:\t" << _v_bus;
				log(Logger::INFO) << "\tNRN_ID:\t" << nnumber1;
				log(Logger::INFO) << "\tPERIOD:\t" << period << Logger::flush;

			} break;
			case '5':
			{
				log(Logger::INFO) << "Running systematic test";
				unsigned int period = 50;
				unsigned int repetitions = 100;
				string filename = "../results/l1_bitflips_bv/results.dat";
				stringstream summary;
				vector<unsigned int> channels;
				for (size_t n_ch = 0; n_ch < 8; ++n_ch) {
					channels.push_back(n_ch);
				}

				summary << "\n====================================================================================";
				summary << "\n====================================================================================";
				summary << "\nSummary:";
				summary << "\n====================================================================================";
				summary << "\nSETTINGS:";
				summary << "\tPLL:\t" << _pll_mult;
				summary << "\tPERIOD:\t" << period << endl;
				summary << "\tRepetitions per address:\t" << repetitions*3 << endl;
				summary << "\tNum Data Points total: \t" << channels.size()*8*64*repetitions*3 << endl;
				summary << "\n====================================================================================";
				summary << "\nMEAN ERROR RATES";
				summary << "\n------------------------------------------------------------------------------------";

				vector<double> mean_error_rates_all_channels;
				for (size_t n_ch=0;n_ch<channels.size(); ++n_ch) {
					set_channel(channels[n_ch]);
					vector<unsigned int> v_buses = get_vbuses_for_hbus(_h_bus);
					vector<double> mean_error_rates(v_buses.size(),0.);

					for( size_t n_vb = 0; n_vb < v_buses.size(); ++n_vb) {
						set_v_bus(v_buses[n_vb]);
						vector < vector <uint> > results(64); // inner vector: received numbers, value holds count
						for( uint address = 0; address < 64; ++address){
							vector< vector<uint> > times;
							vector< vector<uint> > nnums;

							run_address_flip_experiment(
								address, //!< L1 Address (0..63)
								period, //!< period between spikes ( in clock cycles )
								repetitions, //!< number of repetitions
								times,
								nnums
								);

							vector <uint> & result = results[address];
							result = vector<uint>(64,0); // inner vector: received numbers, value holds count
							for(size_t nn = 0; nn < times.size(); ++nn) {
								vector<uint>& ttimes = times[nn];
								vector<uint>& nnnums = nnums[nn];
								//cout << "Received neuron numbers " << dec << nnnums[0] << ", " << nnnums[1] << ", " << nnnums[2];
								//cout << " with delays " << ttimes[1]-ttimes[0] << " and " << ttimes[2]-ttimes[1] << " cycles" << endl;
								if ( ttimes[1]-ttimes[0] > 0 ) {
									result[nnnums[0]]++;
									result[nnnums[1]]++;
								}
								if ( ttimes[2]-ttimes[1] > 0 ) {
									result[nnnums[2]]++;
								}
							}
						}
						for( uint address = 0; address < 64; ++address){
							cout << "Sending Address Number \t" << address << endl;
							for (uint nr=0; nr<results[address].size(); nr++){
								if (results[address][nr]){
									cout << "\tNumber \t" << nr << ":\t" << results[address][nr] << endl;
								}
							}
						}
						mean_error_rates[n_vb] = write_results_to_file(filename,period, results);
					}
					/// Write Summary

					for( size_t n_vb = 0; n_vb < v_buses.size(); ++n_vb) {
						summary << "\nHBUS:\t" << _h_bus << "\tCHANNEL:\t" << _channel<< "\tVBUS:\t" << v_buses[n_vb] << "\t" << mean_error_rates[n_vb];
					}
					summary << "\n------------------------------------------------------------------------------------";
					mean_error_rates_all_channels.insert( mean_error_rates_all_channels.end(), mean_error_rates.begin(),mean_error_rates.end());
				}
				double mean_rate = accumulate(mean_error_rates_all_channels.begin(),mean_error_rates_all_channels.end(),(double)0.)/mean_error_rates_all_channels.size();
				summary << "\nmean:\t\t"<< mean_rate;
				summary << "\n====================================================================================";
				summary << "\n" << flush;
				fstream file;
				file.open(filename.c_str(), fstream::app | fstream::out);
				file << summary.rdbuf();
				file.flush(); file.close();
			} break;
			case 'x':{
				cont=false;
			} break;
		}
	}while(cont);

	return true;
}

bool TmBVL1AddressFlip::run_address_flip_experiment(
		unsigned int address, //!< L1 Address (0..63)
		unsigned int period, //!< period between spikes ( in clock cycles )
		unsigned int repetitions, //!< number of repetitions
		vector< vector<uint> >& times,
		vector< vector<uint> >& nnums
		)
{
	log(Logger::INFO) << "Running Address Flip Experiment for channel " << _channel << ", h_bus " << _h_bus << ", v_bus " << _v_bus << ", address " << address << Logger::flush;
	double clk_cycle = 20.e-9*_pll_div/_pll_mult;

	// 1.) configure begs and mergers
	// only BEG 4 is used
	// input from L2 is merged to the corresponding output register.
	//
	nc_merger mer[23] = {BG0, BG1, BG2, BG3, BG4, BG5, BG6, BG7, L0_0, L0_1, L0_2, L0_3, L1_0, L1_1, L2_0,
			DNC0, DNC1, DNC2, DNC3, DNC4, DNC5, DNC6, DNC7};
	//bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1,  1,  1, 1, 1, 1, 1, 1, 1, 1}; // slow enable
	//bool se[23] = {0, 0, 0, 0, 1, 0, 0, 0,  0, 0, 0, 0,  0, 0,  1,  0, 0, 0, 0, 0, 0, 0, 0}; // input select, only BEG 4 active
	//bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 1, 0, 0, 0}; // enable merging, merging dnc channel 4
	
	// default, everything switched off, no merging, all channels go straight.
	bool sl[23] = {1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1,  1,  1, 1, 1, 1, 1, 1, 1, 1}; // slow enable
	bool se[23] = {0, 0, 0, 0, 0, 0, 0, 0,  1, 0, 0, 0,  1, 0,  1,  0, 0, 0, 0, 0, 0, 0, 0}; // input select
	bool en[23] = {0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0}; // enable merging
	vector<nc_merger> merg(mer, mer+23);
	vector<bool> slow(sl, sl+23);
	vector<bool> select(se, se+23);
	vector<bool> enable(en, en+23);

	// choose input from the desired BEG (select)
	select[_channel] = 1;
	// enable merging in the dnc merger 
	enable[_channel+15] = 1;

	_neuron_control->merger_set(merg, enable, select, slow);
	_neuron_control->outputphase_set(0,0,0,0,0,0,0,0); //important for SPL1 to DNC output

	
	// 2.) configure sending repeater
	_repeater_controls[RCTRL_MID_LEFT]->conf_spl1_repeater(_id_of_sending_repeater, RepeaterControl::SPL1OUT, RepeaterControl::INWARDS, true);

	// 3.) switch from horizontal to vertical (crossbar right 25 -> _v_bus)
	//L1SCTRL_MID_RIGHT 
	_l1switch_controls[L1SCTRL_MID_RIGHT]->reset();
	_l1switch_controls[L1SCTRL_MID_LEFT]->reset();
	// connects/disconnects vertical and horizontal lines,
	// last argument specifies to connect(true) or not connect
	if (_v_bus >= 128 )
		_l1switch_controls[L1SCTRL_MID_RIGHT]->switch_connect( _h_bus, _v_bus, true);
	else
		_l1switch_controls[L1SCTRL_MID_LEFT]->switch_connect( _h_bus, _v_bus, true);

	_neuron_control->beg_configure(_channel, 0, 2000);
	_neuron_control->beg_set_number(_channel, 0); // address 0
	_neuron_control->beg_off(8);
	_neuron_control->beg_on(_channel); //sends 0

	usleep(3000);  //locking

	// 2.) configure spl1_control and dnc
	bool dnc_if_direction[8] = {1,1,1,1,1,1,1,1}; // all channels from DNC->HICANN
	bool dnc_if_enable[8]    = {0,0,0,0,0,0,0,0};
	// bool dnc_if_enable[8]    = {0,0,0,0,1,0,0,0}; // only channel 4 enabled

	dnc_if_enable[_channel] = 1;

	// create hicann_cfg, so I can use program_spl1_control for configuration of dnc and dnc_if
	hicann_cfg_t hicann_cfg;
	hicann_cfg.dnc_if_channels_direction = std::vector<bool>(dnc_if_direction, dnc_if_direction+8);
	hicann_cfg.dnc_if_channels_enable    = std::vector<bool>(dnc_if_enable, dnc_if_enable+8);
	program_spl1_control(hicann_cfg);

	// prepare input spikes
	std::map< l2_pulse_address_t, std::vector<double> > spiketrains;

	// Target address is DNC 0, HICANN 0, DNC_IF_CHANNEL 4 and the requested l1 address
	l2_pulse_address_t addr_1;
	addr_1.data.fields.dnc_if_channel = _channel;
	addr_1.data.fields.l1_address = address;

	// create vector with spiketimes
	// spiketimes are generated as multiple of the chosen clock period.
	std::vector<double> spiketimes; // in hw_time = real time
	unsigned int isi_cycles =  period;
	unsigned int num_spikes = 500;
	double offset = 500e-9;
	for (size_t n= 0; n<num_spikes; ++n) {
		spiketimes.push_back(offset + n * isi_cycles*clk_cycle);
	}
	// add one more spike with offset
	spiketimes.push_back(spiketimes.back()+offset);

	// send spikes to playback fifo
	spiketrains[addr_1] = spiketimes;
	write_playback_pulses(spiketrains);

	// execute experiment:
	// playback spikes and readout the results at the readout repeater
	// Note that the Playback Fifo runs in Loop Mode!!!
	execute_experiment(1.e-6, times, nnums);
	log(Logger::DEBUG0) << "tmbv_l1addressflip: config of last experiment was: ";
	log(Logger::DEBUG0) << "\tPLL:\t" << _pll_mult;
	log(Logger::DEBUG0) << "\tVBUS:\t" << _v_bus;
	log(Logger::DEBUG0) << "\tNRN_ID:\t" << address;
	log(Logger::DEBUG0) << "\tPERIOD:\t" << period << Logger::flush;
}

double TmBVL1AddressFlip::write_results_to_file(
		std::string filename,
		unsigned int period,
		vector< vector<uint> >& result
		)
{
	//calculating error rates
	vector <double> errate (64,0.0);
	for (uint u=0; u<64; u++){ //zeros are not errors!
		// error rate = number of wrong events / number of total events
		//
		//errate[u]=1.0-(((double)result[u][u])/((double)(accumulate(result[u].begin(), result[u].end(), 0)-result[u][0])));
		double denominator = ((double)(accumulate(result[u].begin(), result[u].end(), 0)-result[u][0]));
		if (denominator > 0.)
			errate[u]=1.0-(((double)result[u][u])/denominator);
		else 
			errate[u]=0.;
	}

	fstream file;
	file.open(filename.c_str(), fstream::app | fstream::out);
	file << "\n====================================================================================";
	file << "\nSETTINGS:";
	file << "\tPLL:\t" << _pll_mult;
	file << "\tVBUS:\t" << _v_bus;
	file << "\tHBUS:\t" << _h_bus;
	file << "\tPERIOD:\t" << period << endl;
	file << "____________________________________________________________________________________\n";

	file << "Results columnwise: sent number patern and what was received, (decimal, binary, and event number). Last is the error rate." << endl;
	for (uint u=0; u<64; u++){ //print error rates
		file << setw(2) << u << ":\t" << bino(u,6) << ":\tpttrn.\t|\t";
	}
	file << endl;
	
	for (uint s=0; s<64; s++){
		for (uint i=0; i<64; i++){
			bool fillflag=false;
			for (uint j=0; j<64; j++){
				if ((result[i][j]) && (!fillflag)) {
					file << setw (2) << j << ":\t" << bino(j,6) << ":\t" << setw(6) << result[i][j] << "\t|\t";
					result[i][j]=0;
					fillflag=true;
				}
			}
			if (!fillflag) file << "   \t       \t      \t|\t";
		}
		uint flag=0;
		for (uint g=0; g<64; g++) flag+=accumulate(result[g].begin(), result[g].end(), 0);
		if (!flag) s=64; //empty array => interrupt
		file << endl;
	}
	
	for (uint u=0; u<64; u++){ //print error rates
		file << "ER:\t" << setw(6) << setprecision(4) << errate[u] << "\t      \t|\t";;
	} 
	double  mean_errate = accumulate(errate.begin(), errate.end(),0.)/errate.size();
	
	file << "\nMEAN ERROR RATE:\t"<< mean_errate;
	file << "\n====================================================================================";
	file << "\n" << flush;
	file.flush(); file.close();

	return mean_errate;
}

bool TmBVL1AddressFlip::set_v_bus(
	unsigned int v_bus //!< vertical bus, to which horizontal bus nur 25 connects, (default 140);
		)
{
	if ( v_bus % 32 == _h_bus/2 ) {
		set_readout_repeater(v_bus);
		return  true;
	}
	else return false;
}
bool TmBVL1AddressFlip::set_channel(
	unsigned int channel
		)
{
	if (channel < 8) {
		_channel = channel;
		_id_of_sending_repeater = channel_to_repeater[channel];
		_h_bus = channel_to_hbus[channel];
		return true;
	}
	else return false;
}
std::vector<uint> TmBVL1AddressFlip::get_vbuses_for_hbus(unsigned int hbus)
{
	std::vector<uint> vbuses;
	for (size_t nn = 0; nn < 8; ++nn)
		vbuses.push_back(32*nn + (hbus/2));
	return vbuses;
}



static TmBVL1AddressFlip * test_instance;

class LteeTmBVL1AddressFlipTest: public Listee<Testmode>{
public:
	LteeTmBVL1AddressFlipTest() : Listee<Testmode>(string("tmbv_l1addressflip"),string("testmode reproducing l1 address flips at 150 Mhz PLL and higher")){};
	Testmode * create(){
		test_instance = new TmBVL1AddressFlip();
		return test_instance;
	};
};

LteeTmBVL1AddressFlipTest ListeeTmBVL1AddressFlipTest;

