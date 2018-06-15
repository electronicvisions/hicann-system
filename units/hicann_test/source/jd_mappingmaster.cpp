
#include "dnc_control.h"
#include "fpga_control.h"
#include "s2c_jtagphys_2fpga.h" //jtag-fpga library
#include "spl1_control.h" //spl1 control class 
#include "iboardv2_ctrl.h" //iBoard controll class

// _host_al_control stuff
#include "ethernet_software_if.h"
#include "host_al_controller.h"

#include <RCF/Idl.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/TcpEndpoint.hpp>
#include <RCF/FilterService.hpp>
#include <RCF/ZlibCompressionFilter.hpp>
#include <RCF/SessionObjectFactoryService.hpp>

#include "jd_mappingmaster.h"
#include "jd_rcf_experiment.h"

#include <cassert>
#include <algorithm>

struct Experiment; // forward declaration

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

//const int JDMappingMaster::TAG = 0;
const double JDMappingMaster::clkperiod_dnc = 4.0e-9; // in seconds
const double JDMappingMaster::clkperiod_fpga = 8.0e-9; // in seconds
const size_t JDMappingMaster::playback_fifo_size;

JDMappingMaster::JDMappingMaster():
	Testmode()
	,_host_al_control(NULL)
	,_eth_stim(NULL)
	,_host_al_intialized(false)
{}
JDMappingMaster::~JDMappingMaster()
{
	if(_host_al_control!=NULL)
		delete _host_al_control;
	if(_eth_stim!=NULL)
		delete _eth_stim;
}

void JDMappingMaster::initialize_system()
{
	// ugly: needs to set already here for infering if we are on wafer or not
	_hicann_control = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];

	// numbering valid for both setups
	unsigned int hicann_jtag_nr = _hicann_control->addr();
	this->hicann_nr      = jtag->pos_dnc-1-hicann_jtag_nr; // store this for pulse processing
	jtag->set_hicann_pos(hicann_jtag_nr);

	uint32_t version = jtag->GetFPGADesignVersion();
	log(Logger::INFO) << "FPGA Version: " << version << endl;

	// indexing in DNC (i.e. pulse addresses) and JTAG indexing are different (only applies if several HICANNs are present in the system)
	log(Logger::INFO) << "HICANN IDs: DNC links: " << hicann_nr << ", JTAG: " << hicann_jtag_nr;

	S2C_JtagPhys2Fpga *comm_ptr = getL2CommPtr();

	// get DNC id
	uint16_t jtag_port = comm_ptr->getConnId().get_jtag_port();
	log(Logger::INFO) << "initialize_system: jtag_port =  " << jtag_port << Logger::flush;
	if (jtag_port >= 1700 && jtag_port < 1704)
		dnc_id = jtag_port - 1700;
	else
		log(Logger::ERROR) << "initialize_system: jtag_port=  " << jtag_port << "out of range. Cannot detect dnc id "<< Logger::flush;

    // soft FPGA reset example - works similar to reset of timestamp counters
	//unsigned int curr_ip = jtag->get_ip();
    // set_fpga_reset(unsigned int fpga_ip, bool enable_core=false, bool enable_fpgadnc=false, bool enable_ddr2onboard=false, bool enable_ddr2sodimm=false,
	//                        bool enable_arq=false, uint reset_port=1801)
	//comm_ptr->set_fpga_reset(curr_ip,true,false,true,true,false); // set reset of core logic
	//comm_ptr->set_fpga_reset(curr_ip,false,false,false,false,false); // release reset of core logic, but leave FPGA-ARQ in reset
	// BV: doing the reset twice brings more stability (FIXME)
	//comm_ptr->set_fpga_reset(curr_ip,true,false,true,true,true); // set reset of core logic
	//comm_ptr->set_fpga_reset(curr_ip,false,false,false,false,true); // release reset of core logic, but leave FPGA-ARQ in reset
	
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

	initialize_host_al_controller();

	// this sets the PLL to the default values during experiment run
	// TODO should this move to initialize_iboardv2(..) ?
	set_pll(pll_setting_default::PLL_MULT_EXPERIMENT, pll_setting_default::PLL_DIV_EXPERIMENT);
}

void JDMappingMaster::initialize_host_al_controller() {
	if (_host_al_intialized ) {
		log(Logger::INFO) << "Host AL Controller already intialized.";
		return;
	}
	// HostAL stuff copied from tmag_switchramtest
	unsigned int curr_ip = _hicann_control->GetCommObj()->jtag->get_ip();
	unsigned int al_port = 1234;
	log(Logger::INFO) << "IP for HostALController: " << hex << curr_ip << ", port: " << al_port << endl;
	// TODO: make sure host al controller is created only once.
	_host_al_control = new HostALController(curr_ip,al_port);
	_eth_stim = new EthernetSoftwareIF ;
	_host_al_control->setEthernetIF(_eth_stim);
	_host_al_control->initEthernetIF();

	// initialise transport layer; set initial sequence number
	log(Logger::INFO) << "Initialisation start: " << /*sc_simulation_time() <<*/ endl;
	if ( _host_al_control->initFPGAConnection(0x112) )
		log(Logger::INFO) << "TUD_TESTBENCH:NOTE:TESTPASS: Transport Layer initialisation sequence performed successfully." << endl;
	else
	{
		log(Logger::ERROR) << "TUD_TESTBENCH:NOTE:TESTFAIL: Transport Layer initialisation sequence failed." << endl;
		log(Logger::ERROR) << "Initialisation end: " << /*sc_simulation_time() <<*/ endl;;
		log(Logger::ERROR) << "Aborting any further tests." << endl;
	}
	_host_al_intialized = true;
}

void JDMappingMaster::initialize_common() {

	/////////////////////////////////////////////////////////////////
	// check if system setup is correct
	/////////////////////////////////////////////////////////////////
	if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT])
		throw std::runtime_error("object 'chip' in testmode not set, abort");

	/////////////////////////////////////////////////////////////////
	// get the pointers to the macro control blocks
	/////////////////////////////////////////////////////////////////
	_dnc_control = (DNCControl*) chip[FPGA_COUNT]; // use DNC
	_fpga_control = (FPGAControl*) chip[0]; // use FPGA


	// FPGA reset
	_fpga_control->reset();

	// JTAG reset
	jtag->reset_jtag();

	// verify jtag functionality
	if (jtag->read_id(jtag->pos_fpga) != 0xbabebeef)
		throw std::runtime_error("got wrong jtag id for fpga after reset");
	//if (jtag->read_id(jtag->pos_dnc) != 0xTODOTODO)
	//	throw std::runtime_error("got wrong jtag id for dnc after reset");
	if (jtag->read_id(jtag->pos_hicann) != 0x14849434)
		throw std::runtime_error("got wrong jtag id for hicann after reset");

	// DNC reset
	jtag->FPGA_set_fpga_ctrl(0x1);
	usleep(500); // ugly, but AG says: DLLs or other stuff needs lots of time to init

	/////////////////////////////////////////////////////////////////
	// get the HICANN controllog(Logger::INFO) << "HICANN communication Init()";
	/////////////////////////////////////////////////////////////////
	_hicann_control = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0]; // do not insert hicannr here, because we only have one HICANN

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
}


void JDMappingMaster::initialize_highspeed() {

	/////////////////////////////////////////////////////////////////
	// init the FPGA, DNC, HICANN communication
	/////////////////////////////////////////////////////////////////

	log(Logger::INFO) << "\tHICANN communication Init()";

	if (_hicann_control->GetCommObj()->Init(_hicann_control->addr()) != Stage2Comm::ok)
		throw std::runtime_error("HICANN communication Init() failed, abort");

	S2C_JtagPhys2Fpga *comm_ptr = getL2CommPtr();
	if (comm_ptr->get_transmission_errors()>20)
		throw std::runtime_error("too many highspeed transmission errors");

	// TODO (@BV): Is this correct for multi-fpga systems? If no, split out into iboard/wafer.
	_dnc_control->setTimeStampCtrl(0x00); // disable the DNC timestamp handling

	log_ids();
}


/// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
/// init sequence taken from tmak_iboardv2
void JDMappingMaster::initialize_iboardv2(unsigned int hicannr)
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
	// get the pointers to the macro control blocks
	/////////////////////////////////////////////////////////////////
	_iboard_control = new IBoardV2Ctrl(conf, jtag, 0); //create an iboard control instance

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

	// initialize common part (shared by wafer/iboard)
	initialize_common();

	// initialize highspeed (shared by wafer/iboard)
	initialize_highspeed();

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

}

void JDMappingMaster::initialize_waferscale_system(unsigned int hicannr) {
    // initializing stuff which seems to be necessary
	log(Logger::INFO) << "setting up waferscale system";
	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

	// initialize common part (shared by wafer/iboard)
	initialize_common();

	// TODO: maybe wafer-reset here?

	// initialize highspeed (shared by wafer/iboard)
	initialize_highspeed();

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
}

void JDMappingMaster::set_pll(uint multiplicator, uint divisor){
	uint pdn = 0x1;
	uint frg = 0x1;
	uint tst = 0x0;
	jtag->HICANN_set_pll_far_ctrl(divisor, multiplicator, pdn, frg, tst);
}

void JDMappingMaster::log_ids(){
	uint64_t id=0xf;
	jtag->read_id(id, jtag->pos_fpga);
	log(Logger::INFO) << "FPGA ID: 0x" << hex << id;
	jtag->read_id(id, jtag->pos_dnc);
	log(Logger::INFO) << "DNC ID: 0x" << hex << id;
	jtag->read_id(id, jtag->pos_hicann);
	log(Logger::INFO) << "HICANN ID: 0x" << hex << id;

}

void JDMappingMaster::reset_arq(){
	throw std::runtime_error("there's no ARQ reset anymore... use Init()");
	// Init() resets FPGA/HICANN ARQ stuff
}

S2C_JtagPhys2Fpga * JDMappingMaster::getL2CommPtr(){
	S2C_JtagPhys2Fpga * comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(_hicann_control->GetCommObj());
	if (comm_ptr == NULL)
		throw std::runtime_error("wrong communication method -- did you try \"-bje2f\"?");
	return comm_ptr;
}

void JDMappingMaster::activate_hicann(){

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

void JDMappingMaster::program_hicann(
	const hicann_cfg_t & config
	)
{
	log(Logger::INFO) << "stepwise program() HICANN ...";
	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";

	program_floating_gates(config);
	program_neuron_builder(config);
	program_neuron_control(config);
	program_repeaters(config);
	program_l1switches(config);
	program_synapse_control(config);
	program_spl1_control(config);

	log(Logger::INFO) << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
}

/////////////////////////////////////////////////////////////////
// FloatingGateControl
/////////////////////////////////////////////////////////////////

void JDMappingMaster::activate_floating_gate_control(){

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

void JDMappingMaster::program_floating_gates(const hicann_cfg_t & config) {

	log(Logger::INFO) << "\tprogram() Floating Gate Control";

	// set PLL to 100 MHz for a more accuratre programming of the floating gates
	set_pll(pll_setting_default::PLL_MULT_FGPROGRAMMING, pll_setting_default::PLL_DIV_FGPROGRAMMING);

	// Determine which Floating-Gate Blocks have to be programmed.
	bool configure_top_block = false;
	bool configure_bottom_block = false;

	// If there is any neuron that can fire, the corresponding block is programmed.
	for (std::map< unsigned int, unsigned int >::const_iterator it = config.firing_denmems_and_addresses.begin();
	     it != config.firing_denmems_and_addresses.end();
	     ++it){
		unsigned int denmem_id = it->first;
		if (denmem_id < 256 )
			configure_top_block = true;
		else
			configure_bottom_block = true;
	}

	// If there is any vertical connection between denmems -> programm both blocks
	for (unsigned int i=0; i< config.denmem_switches_vertical.size(); ++ i){
		if (config.denmem_switches_vertical.at(i)) {
			configure_top_block = true;
			configure_bottom_block = true;
			break;
		}
	}

	if (config.program_floating_gates) {
		log(Logger::INFO) << "\tprogramming blocks: ";
		if (configure_top_block)
			log << "top, ";
		if (configure_bottom_block)
			log << "bottom, ";
		if (!(configure_top_block || configure_bottom_block))
			log(Logger::INFO) << "\tNote: no Floating Gates will be programmed!: ";


		/////////////////////////////////////////////////////////////////
		// Write Current & Voltage FG values
		/////////////////////////////////////////////////////////////////
		const unsigned int fg_num_start = configure_top_block ? 0 :(FG_pkg::fg_num/2);
		const unsigned int fg_num_stop = configure_bottom_block ? FG_pkg::fg_num : (FG_pkg::fg_num/2);
		log(Logger::INFO) << "\tDEBUG: fg_num_start = " << fg_num_start << ", -stop = " << fg_num_stop;

		// Write currents lines
		// starting at line 1 as this is the first current!!!!!
		// ops need biases before they work.
		log(Logger::INFO) << "\t\twriting current values";
		for( uint fg_line = 1; fg_line < NUMBER_FG_LINES; fg_line += 2) {
			for(uint fg_num = fg_num_start; fg_num < fg_num_stop; fg_num += 1) {
				std::vector<int> values(NUMBER_FG_COLUMNS);
				values[0] = config.global_floating_gates[fg_num][fg_line];
				for(int fg_col = 1; fg_col < NUMBER_FG_COLUMNS; ++fg_col)
					values[fg_col] = config.neuron_floating_gates[fg_num][fg_col-1][fg_line];
				write_floating_gate_line(fg_num, fg_line, values);
				if(log.getLevel() < Logger::DEBUG0) {
					std::cout << "." << std::flush;
				}
			}
		}
		if(log.getLevel() < Logger::DEBUG0) {
			std::cout << std::endl << std::flush;
		}

		// Write voltages lines
		log(Logger::INFO) << "\t\twriting voltage values";
		for(uint fg_line = 0; fg_line < NUMBER_FG_LINES; fg_line += 2) {
			for(uint fg_num = fg_num_start; fg_num < fg_num_stop; fg_num += 1) {
				std::vector<int> values(NUMBER_FG_COLUMNS);
				values[0] = config.global_floating_gates[fg_num][fg_line];
				for(int fg_col = 1; fg_col < NUMBER_FG_COLUMNS; ++fg_col)
					values[fg_col] = config.neuron_floating_gates[fg_num][fg_col-1][fg_line];
				write_floating_gate_line(fg_num, fg_line, values);
				std::cout << "." << std::flush;
			}
		}
		std::cout << std::endl << std::flush;
	}
	else {
		log(Logger::INFO) << "\t\tFloating Gate programming skipped upon request by user";
	}

	/////////////////////////////////////////////////////////////////
	// Write Current Stimuli
	/////////////////////////////////////////////////////////////////

	if (configure_top_block) {
		if(config.current_input_enable_and_target_denmem_top_left.first){
			write_current_stimulus(
				/* bank */ 0,
				config.current_stimulus_top_left,
				config.current_stimulus_top_left_pulselength,
				JDMappingMaster::FGCTRL_TOP_LEFT,
				/* stimulate_continuous */ true);
		}

		if(config.current_input_enable_and_target_denmem_top_right.first){
			write_current_stimulus(
				/* bank */ 0,
				config.current_stimulus_top_right,
				config.current_stimulus_top_right_pulselength,
				JDMappingMaster::FGCTRL_TOP_RIGHT,
				/* stimulate_continuous */ true);
		}
	}

	if (configure_bottom_block) {
		if(config.current_input_enable_and_target_denmem_bottom_left.first){
			write_current_stimulus(
				/* bank */ 0,
				config.current_stimulus_bottom_left,
				config.current_stimulus_bottom_left_pulselength,
				JDMappingMaster::FGCTRL_BOTTOM_LEFT,
				/* stimulate_continuous */ true);
		}

		if(config.current_input_enable_and_target_denmem_bottom_right.first){
			write_current_stimulus(
				/* bank */ 0,
				config.current_stimulus_bottom_right,
				config.current_stimulus_bottom_right_pulselength,
				JDMappingMaster::FGCTRL_BOTTOM_RIGHT,
				/* stimulate_continuous */ true);
		}
	}

	// reset PLL to normal experiment operation
	set_pll(pll_setting_default::PLL_MULT_EXPERIMENT, pll_setting_default::PLL_DIV_EXPERIMENT);
}

void JDMappingMaster::write_floating_gate_line(uint fg_num, uint fg_line, vector<int> values){

	assert(values.size() == NUMBER_FG_COLUMNS);
	log(Logger::DEBUG0) << "\twrite floating gate line number " << fg_line << " in block " << fg_num << " with values: ";
	log << "\nglobal: " << values[0] << "\nneurons:";
	for ( size_t n_column = 1; n_column < values.size(); ++n_column ) {
		if (n_column % 32 == 1 ) log << "\n";
		log <<  values[n_column] << " ";
	}

	// FGControl::program_cells(line, bank, up/down);
	// bank number, programming data is written to

	// use two banks -> values can be written although fg is still working
	// here write is done by consecutive write not using two banks.
	// doing so should increase the speed

	// FGControl::isBusy()
	// wait until fg_is not busy anymore -
	// normaly one should do other stuff than just polling busy in this time

	/// TODO - ME: add enums for banks and locations
	_floating_gate_controls[fg_num]->initArray(/*bank*/ 0, values);
	while(_floating_gate_controls[fg_num]->isBusy());

	_floating_gate_controls[fg_num]->program_cells(fg_line, /*bank*/ 0, 0); // programm down
	while(_floating_gate_controls[fg_num]->isBusy());

	_floating_gate_controls[fg_num]->program_cells(fg_line, /*bank*/ 0, 1); // programm up
	while(_floating_gate_controls[fg_num]->isBusy());
}

void JDMappingMaster::write_current_stimulus(
	unsigned int bank,
	std::vector<unsigned int> const & current,
	unsigned int pulselength,
	unsigned int fg_num,
	bool stimulate_continuous)
{

	log(Logger::DEBUG0) << "JDMappingMaster::write_current_stimulus";
	log(Logger::DEBUG0) << "\t [bank:" << bank << ",fg_num:" << fg_num << ",continuous:" << stimulate_continuous << "]" << std::endl;

	assert (current.size() == NUMBER_FG_COLUMNS);
	assert (FGControl::MEM_SIZE == NUMBER_FG_COLUMNS/2 + NUMBER_FG_COLUMNS % 2);
	assert (fg_num < 4);

	while(_floating_gate_controls[fg_num]->isBusy());
	std::stringstream currentvals;
	log(Logger::DEBUG0) << "\t writing values" << Logger::flush;
	for(size_t i = 0; i < (size_t)FGControl::MEM_SIZE - 1; ++i) {
		cout << "[" << current[2 * i]
				  << "," << current[2 * i + 1]<< "] ";
		_floating_gate_controls[fg_num]->write_ram(bank, i,
					  current[2 * i], current[2 * i + 1]);
	}
	// There is no entry with index 129 -> write 128 twice
	{
		size_t i = (size_t)FGControl::MEM_SIZE - 1;
		cout << "[" << current[2 * i ] << ", ] ";
		_floating_gate_controls[fg_num]->write_ram(bank, i,
					  current[2 * i], current[2 * i]);
	}
	std::cout << std::endl;

	/*
	// avoid writing over the end of /current/
	size_t last_address = FGControl::MEM_SIZE - 1;
	if (last_address * 2 + 1 < current.size()) {
		_floating_gate_controls[fg_num]->write_ram(bank, last_address,
					  current[last_address * 2],
					  current[last_address * 2 + 1]);
	}
	else {
		_floating_gate_controls[fg_num]->write_ram(bank, last_address,
					  current[last_address * 2],
					  current[last_address * 2]);
	}
	*/

	_floating_gate_controls[fg_num]->set_pulselength(pulselength);
	_floating_gate_controls[fg_num]->write_biasingreg();
	if (stimulate_continuous)
		_floating_gate_controls[fg_num]->stimulateNeuronsContinuous(bank);
	else
		_floating_gate_controls[fg_num]->stimulateNeurons(bank);
}

/////////////////////////////////////////////////////////////////
// Neuron Builder
/////////////////////////////////////////////////////////////////

void JDMappingMaster::activate_neuron_builder_control(){

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

void JDMappingMaster::program_neuron_builder(const hicann_cfg_t & config){

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
uint JDMappingMaster::build_neuron_builder_data(
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

void JDMappingMaster::activate_single_denmem(uint add) {

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

void JDMappingMaster::activate_neuron_control(){

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

void JDMappingMaster::program_neuron_control(const hicann_cfg_t & config){

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

void JDMappingMaster::activate_repeater_control(){
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

void JDMappingMaster::program_repeaters(const hicann_cfg_t & config)
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

void JDMappingMaster::configure_repeater_block(
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
void JDMappingMaster::activate_l1switch_control(){

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

void JDMappingMaster::program_l1switches(const hicann_cfg_t & config){

	log(Logger::INFO) << "\tprogram() L1 Switch  Control";

	//////////////////////////////
    // cross bars
    //////////////////////////////
	configure_l1_switch( JDMappingMaster::L1SCTRL_MID_LEFT, config.crossbar_left);
	configure_l1_switch( JDMappingMaster::L1SCTRL_MID_RIGHT, config.crossbar_right);

	//////////////////////////////
    // select switches
    //////////////////////////////
	configure_l1_switch( JDMappingMaster::L1SCTRL_TOP_LEFT, config.syndriver_switch_top_left);
	configure_l1_switch( JDMappingMaster::L1SCTRL_BOTTOM_LEFT, config.syndriver_switch_bottom_left);
	configure_l1_switch( JDMappingMaster::L1SCTRL_TOP_RIGHT, config.syndriver_switch_top_right);
	configure_l1_switch( JDMappingMaster::L1SCTRL_BOTTOM_RIGHT, config.syndriver_switch_bottom_right);
}

void JDMappingMaster::configure_l1_switch(
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

void JDMappingMaster::activate_synapse_control(){

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

void JDMappingMaster::program_synapse_control(const hicann_cfg_t & config){

	log(Logger::INFO) << "\tprogram() Synapse Control";

	////////////////////////////////////
	// synaptic weights
	////////////////////////////////////

	// write full rows of synaptic weigths
	// bot_row_adr -- row to write (0...223)
	// weights -- vector of 32 elements each element containing
	// the 4 bit weights for 8 subsequent synapses with
	// synapse 8 in the LSBs of an vector element
//	_synapse_controls[SCTRL_TOP]->write_weight((unit)row,(vector<uint>)weights);
//	_synapse_controls[SCTRL_BOTTOM]->write_weight((unit)row,(vector<uint>)weights);

	log(Logger::INFO) << "\t\twriting weights top";
	unsigned int num_rows_written = 0;
	for(size_t n_row = 0; n_row < config.synaptic_weights_top.size(); ++n_row) {
		// write row only if corrensponding syndriver is enabled
		if ( (n_row%4 < 2 && config.syndriver_config_top_left[ n_row/4].enable )
		 	|| (n_row%4 >= 2 && config.syndriver_config_top_right[ n_row/4].enable ))
		{
			log(Logger::DEBUG0) << "\t\t\twriting row " << n_row;
			// compress weight vector, check for size and write
			std::vector< unsigned int > values_to_write;
			compress_synapse_row_values(config.synaptic_weights_top[n_row], values_to_write);
			for(size_t n_w = 0; n_w < values_to_write.size(); ++n_w)
				log << " " << std::hex << values_to_write[n_w];
			assert(values_to_write.size() == 32);
			_synapse_controls[SCTRL_TOP]->write_weight(n_row,values_to_write);
			++num_rows_written;
		}
	}

	log(Logger::INFO) << "\t\t" <<num_rows_written << " rows written.";

	log(Logger::INFO) << "\t\twriting weights bottom";
	num_rows_written = 0;
	for(size_t n_row = 0; n_row < config.synaptic_weights_bottom.size(); ++n_row) {
		// write row only if corrensponding syndriver is enabled
		if ( (n_row%4 < 2 && config.syndriver_config_bottom_left[ n_row/4].enable )
		 	|| (n_row%4 >= 2 && config.syndriver_config_bottom_right[ n_row/4].enable ))
		{
			log(Logger::DEBUG0) << "\t\t\twriting row " << n_row;
			// compress weight vector, check for size and write
			std::vector< unsigned int > values_to_write;
			compress_synapse_row_values(config.synaptic_weights_bottom[n_row], values_to_write);
			for(size_t n_w = 0; n_w < values_to_write.size(); ++n_w)
				log << " " << std::hex << values_to_write[n_w];
			assert(values_to_write.size() == 32);
			_synapse_controls[SCTRL_BOTTOM]->write_weight(n_row,values_to_write);
			++num_rows_written;
		}
	}
	log(Logger::INFO) << "\t\t" <<num_rows_written << " rows written.";

	////////////////////////////////////
	// synapse matrix address decoders
	////////////////////////////////////

	// write two rows of synapse decoders
	// row -- row to write (0...223)
	// data_bot, data_top -- vectors of 32 elements each element containing
	// the 4 bit address corresponding to the 4 LSB of an spl1-address
	// for 8 subsequent address decoders with address decoder 8 in the LSBs
	// of an vector element
//	_synapse_controls[SCTRL_TOP]->write_decoder((uint)bot_row_adr, (vector<uint>)data_bot, (vector<uint>)data_top);
//	_synapse_controls[SCTRL_BOTTOM]->write_decoder((uint)bot_row_adr, (vector<uint>)data_bot, (vector<uint>)data_top);

	log(Logger::INFO) << "\t\twriting decoder addresses top";
	num_rows_written = 0;
	for(size_t n_bot_row = 0; n_bot_row < config.synaptic_decoders_top.size(); n_bot_row+=2 ) {
		// two rows have to be written at the same time
		// write rows only if corrensponding syndriver is enabled
		if ( (n_bot_row%4 < 2 && config.syndriver_config_top_left[ n_bot_row/4].enable )
		 	|| (n_bot_row%4 >= 2 && config.syndriver_config_top_right[ n_bot_row/4].enable ))
		{
			log(Logger::DEBUG0) << "\t\t\twriting rows " << n_bot_row << " and " << n_bot_row + 1;
			// compress vectors, check for size and write
			std::vector< unsigned int > values_to_write_bottom_row;
			std::vector< unsigned int > values_to_write_top_row;
			compress_synapse_row_values(config.synaptic_decoders_top[n_bot_row], values_to_write_bottom_row);
			compress_synapse_row_values(config.synaptic_decoders_top[n_bot_row + 1], values_to_write_top_row);
			log(Logger::DEBUG0) << "\t\t\tbot row: ";
			for(size_t n_w = 0; n_w < values_to_write_bottom_row.size(); ++n_w)
				log << " " << std::hex << values_to_write_bottom_row[n_w];
			log(Logger::DEBUG0) << "\t\t\ttop row: ";
			for(size_t n_w = 0; n_w < values_to_write_top_row.size(); ++n_w)
				log << " " << std::hex << values_to_write_top_row[n_w];
			assert(values_to_write_bottom_row.size() == 32);
			assert(values_to_write_top_row.size() == 32);
			_synapse_controls[SCTRL_TOP]->write_decoder(n_bot_row, values_to_write_bottom_row,values_to_write_top_row);
			num_rows_written += 2;
		}
	}
	log(Logger::INFO) << "\t\t" <<num_rows_written << " rows written.";

	log(Logger::INFO) << "\t\twriting decoder addresses bottom";
	num_rows_written = 0;
	for(size_t n_bot_row = 0; n_bot_row < config.synaptic_decoders_bottom.size(); n_bot_row+=2 ) {
		// two rows have to be written at the same time
		// write rows only if corrensponding syndriver is enabled
		if ( (n_bot_row%4 < 2 && config.syndriver_config_bottom_left[ n_bot_row/4].enable )
		 	|| (n_bot_row%4 >= 2 && config.syndriver_config_bottom_right[ n_bot_row/4].enable ))
		{
			log(Logger::DEBUG0) << "\t\t\twriting rows " << n_bot_row << " and " << n_bot_row + 1;
			// compress vectors, check for size and write
			std::vector< unsigned int > values_to_write_bottom_row;
			std::vector< unsigned int > values_to_write_top_row;
			compress_synapse_row_values(config.synaptic_decoders_bottom[n_bot_row], values_to_write_bottom_row);
			compress_synapse_row_values(config.synaptic_decoders_bottom[n_bot_row + 1], values_to_write_top_row);
			log(Logger::DEBUG0) << "\t\t\tbot row: ";
			for(size_t n_w = 0; n_w < values_to_write_bottom_row.size(); ++n_w)
				log << " " << std::hex << values_to_write_bottom_row[n_w];
			log(Logger::DEBUG0) << "\t\t\ttop row: ";
			for(size_t n_w = 0; n_w < values_to_write_top_row.size(); ++n_w)
				log << " " << std::hex << values_to_write_top_row[n_w];
			assert(values_to_write_bottom_row.size() == 32);
			assert(values_to_write_top_row.size() == 32);
			_synapse_controls[SCTRL_BOTTOM]->write_decoder(n_bot_row, values_to_write_bottom_row,values_to_write_top_row);
			num_rows_written += 2;
		}
	}
	log(Logger::INFO) << "\t\t" <<num_rows_written << " rows written.";

	////////////////////////////////////
	// synapse drivers
	////////////////////////////////////

	log(Logger::INFO) << "\t\tprogramming syndrivers top left";
	_repeater_controls[RCTRL_TOP_LEFT]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
	for( size_t n_syndr = 0; n_syndr < config.syndriver_config_top_left.size(); ++n_syndr ) {
		if (config.syndriver_config_top_left[ n_syndr ].enable ) {
			log(Logger::INFO) << "\t\t\tdriver " << n_syndr << " is enabled";
			// driver address is n_syndr*4
			configure_synapse_driver(SCTRL_TOP, n_syndr*4, config.syndriver_config_top_left[ n_syndr ] );
		}
	}

	log(Logger::INFO) << "\t\tprogramming syndrivers top right";
	_repeater_controls[RCTRL_TOP_RIGHT]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
	for( size_t n_syndr = 0; n_syndr < config.syndriver_config_top_right.size(); ++n_syndr ) {
		if (config.syndriver_config_top_right[ n_syndr ].enable ) {
			log(Logger::INFO) << "\t\t\tdriver " << n_syndr << " is enabled";
			// driver address is n_syndr*4 + 2
			configure_synapse_driver(SCTRL_TOP, n_syndr*4+2, config.syndriver_config_top_right[ n_syndr ] );
		}
	}

	log(Logger::INFO) << "\t\tprogramming syndrivers bottom left";
	_repeater_controls[RCTRL_BOTTOM_LEFT]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
	for( size_t n_syndr = 0; n_syndr < config.syndriver_config_bottom_left.size(); ++n_syndr ) {
		if (config.syndriver_config_bottom_left[ n_syndr ].enable ) {
			log(Logger::INFO) << "\t\t\tdriver " << n_syndr << " is enabled";
			// driver address is n_syndr*4
			configure_synapse_driver(SCTRL_BOTTOM, n_syndr*4, config.syndriver_config_bottom_left[ n_syndr ] );
		}
	}

	log(Logger::INFO) << "\t\tprogramming syndrivers bottom right";
	_repeater_controls[RCTRL_BOTTOM_RIGHT]->reset_dll(); //do this before locking the driver's DLL (resets the DLLs)
	for( size_t n_syndr = 0; n_syndr < config.syndriver_config_bottom_right.size(); ++n_syndr ) {
		if (config.syndriver_config_bottom_right[ n_syndr ].enable ) {
			log(Logger::INFO) << "\t\t\tdriver " << n_syndr << " is enabled";
			// driver address is n_syndr*4 + 2
			configure_synapse_driver(SCTRL_BOTTOM, n_syndr*4+2, config.syndriver_config_bottom_right[ n_syndr ] );
		}
	}

	////////////////////////////////////
	// STDP Program
	////////////////////////////////////

	// TODO -- STDP

}

void
JDMappingMaster::compress_synapse_row_values(
		const std::vector< unsigned char > & values,
		std::vector<unsigned int> & compressed_values
)
{
	compressed_values.resize(values.size()/8);

	unsigned int eight_values = 0;
	for( size_t n_col = 0; n_col <  values.size(); ++n_col) {
		uint mod = n_col%8; //position of weight as 4-bit chunk in data[col]
		uint val = (values[n_col] & 0xf) << (4*mod); //prepare new value
		eight_values |= val; //write new value
		if (n_col%8 == 7){
			compressed_values[ n_col/8 ] = eight_values;
			eight_values=0;
		}
	}
}

void
JDMappingMaster::configure_synapse_driver(
		synapse_control_location sctrl_loc, //!< synapse block (SCTRL_TOP or SCTRL_BOTTOM )
		unsigned int drv_nr, //!< driver number, corresponds to bottom synapse line of this syndriver
		const syndriver_cfg_t & syndr_config //!< struct holding the syndriver config
)
{
	// enabling and connecting the synapse drivers
	//
	// as each syndriver drives two synapse rows its number follows the numbering
	// of its bottom row, hence a syndriver on the left or right side of the
	// synapse matrix each forth row starting with 0 on the left and 2 on the right side
	// drv_nr -- number of the driver according to the above mentioned scheme
	// top_/bot_ex -- connect the top/bottom row to an excitatory input column of a dendritic membrane
	// top_/bot_in -- connect the top/bottom row to a inhibitory input column of a dendritic membrane
	// enable -- enable the syn driver
	// loc_input -- enable input from layer 1
	_synapse_controls[sctrl_loc]->drv_config(
			drv_nr,
			syndr_config.top_row_cfg.senx, // top_ex
			syndr_config.top_row_cfg.seni, // top_in
			syndr_config.bottom_row_cfg.senx, // bot_ex
			syndr_config.bottom_row_cfg.seni, // bot_in
			syndr_config.enable, // enable
			syndr_config.locin,  // loc_input
			syndr_config.topin   // top_input
			);

	// configuring GMAX
	//
	// as each syndriver drives two synapse rows its number follows the numbering
	// of its bottom row, hence a syndriver on the left or right side of the
	// synapse matrix each forth row starting with 0 on the left and 2 on the right side
	//
	// drv_nr -- number of the driver according to the above mentioned scheme
	// gmaxbot/gmaxtop -- V_max divisor values for bottom and top row
	// gmax[7:4]: V_max is divided by this number, when connected to inhibitory input of neurons
	// gmax[3:0]: V_max is divided by this number, when connected to excitatory input of neurons
	//
	// note: as this represents "divisors", on can not use a value of 0,
	// hence possible values are (1..15) with a default of 1 (the highest conductance)

	// building gmax config bytes for bottom and top
	unsigned int gmax_bottom = syndr_config.bottom_row_cfg.gmax_div;
	unsigned int gmax_top = syndr_config.top_row_cfg.gmax_div;
	unsigned char selgmax_bottom = syndr_config.bottom_row_cfg.selgmax;
	unsigned char selgmax_top = syndr_config.top_row_cfg.selgmax;
	_synapse_controls[sctrl_loc]->drv_set_gmax(drv_nr, selgmax_bottom, selgmax_top, gmax_bottom, gmax_top );

	// configuring PRE-DRV,
	//
	// i.e. configure the 2 MSB of the spl1-address
	// a preout (aka strobe line) should listen to:
	//
	// as each syndriver drives two synapse rows its number follows the numbering
	// of its bottom row, hence a syndriver on the left or right side of the
	// synapse matrix each forth row starting with 0 on the left and 2 on the right side
	//
	// drv_nr -- number of the driver according to the above mentioned scheme
	// pout0/1/2/3 -- specify a 2 bit MSB combination (0...3)

	_synapse_controls[sctrl_loc]->preout_set(
			drv_nr,
			syndr_config.bottom_row_cfg.preout_even,
			syndr_config.top_row_cfg.preout_even,
			syndr_config.bottom_row_cfg.preout_odd,
			syndr_config.top_row_cfg.preout_odd);

	// TODO -- MISSING
	// * STP
}

/////////////////////////////////////////////////////////////////
// SPL1 Control
/////////////////////////////////////////////////////////////////

void JDMappingMaster::activate_spl1_control(){
	log(Logger::INFO) << "\tactivate() SPL1 Control";
	_spl1_control->write_reset();
}

void JDMappingMaster:: program_spl1_control(const hicann_cfg_t & config){
	log(Logger::INFO) << "\tprogram() SPL1 Control";

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

	spl1_cfg = dnc_if_enbl; // store config
	_spl1_control->write_cfg(dnc_if_enbl);

}

/////////////////////////////////////////////////////////////////
// PULSES
/////////////////////////////////////////////////////////////////

bool JDMappingMaster::init_layer2()
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
	S2C_JtagPhys2Fpga *comm_ptr = getL2CommPtr();
#else
	log(Logger::INFO) << "Initializing DNC version 1";
    
	uint64_t jtagid;
	jtag->read_id(jtagid,jtag->pos_fpga); // avoid re-resetting of FPGA (would be unsynchronized to HICANN)

	if (jtagid != 0xbabebeef)
		throw std::runtime_error("jtagid fucked up");
	//	jtag->reset_jtag();


	//////////////////////////////////////////
	// FPGA<->DNC LVDS
	//////////////////////////////////////////

	log(Logger::INFO) << "JDMappingMaster::init_layer2 initialize FPGA<->DNC LVDS links" << Logger::flush;

	// initialize FPGA<->DNC LVDS links (this may take a while!

	S2C_JtagPhys2Fpga *comm_ptr = getL2CommPtr();
	if (comm_ptr == NULL)
    {
    	log(Logger::INFO) << "Warning: wrong communication model used for highspeed L2 transmission. Please use option -bje2f";
        return false;
    }

	// TODO (@BV): AG says that Init() does all dnc init!?!
	//if (comm_ptr->fpga_dnc_init(0,true) != 1)
    //	throw std::runtime_error("fpga_dnc_init failed");

	// TODO (@BV): Should be known after Init() call
	//if (comm_ptr->get_transmission_errors()>20)
	//	throw std::runtime_error("too many highspeed transmission errors");

#endif // DNC_V

	//////////////////////////////////////////
	// System Timer
	//////////////////////////////////////////

	log(Logger::INFO) << "JDMappingMaster::init_layer2 enabling systime counter" << Logger::flush;

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

void JDMappingMaster::program_layer2(pcs_cfg_t pcs_config)
{
	log(Logger::INFO) << "PCS configuration:";
    log(Logger::INFO) << "  DNC loopback: " << pcs_config.enable_dnc_loopback;
    log(Logger::INFO) << "  HICANN loopback: " << pcs_config.enable_hicann_loopback;
    log(Logger::INFO) << "  Playback loopmode: "<< pcs_config.enable_playback_loop;
    log(Logger::INFO) << "  FPGA Pulse feedback: " << pcs_config.enable_fpga_feedback;

	if (pcs_config.enable_dnc_loopback)
	{
		// set DNC loopback
		jtag->DNC_set_loopback(0xff);
		_dnc_control->setDirection(0xA9); // set transmission directions in DNC (for HICANN 0 only)
		log(Logger::INFO) << "DNC pulse loopback enabled";
	}
	else if (pcs_config.enable_hicann_loopback)
	{
		// disable DNC loopback
		jtag->DNC_set_loopback(0x00);

		_dnc_control->setDirection(0xA9); // set transmission directions in DNC (for HICANN 0 only)

		spl1_cfg = 0x0A9ff; // store config
		_spl1_control->write_cfg(0x0A9ff); // transmission direction on HICANN side, no timestamp handling (MSB=0)
		_neuron_control->dnc_enable_set(1,1,1,1,1,1,1,1); // enable DNC input to SPL1 merger tree (channel 0 to 7!)
		// set HICANN loopback
		_neuron_control->loopback_on(0,1);
		_neuron_control->loopback_on(3,2);
		_neuron_control->loopback_on(5,4);
		_neuron_control->loopback_on(7,6);
		log(Logger::INFO) << "HICANN pulse loopback enabled";
	}
    else
    {
	    jtag->DNC_set_loopback(0x00);
		_neuron_control->loopback_off(0);
		_neuron_control->loopback_off(2);
		_neuron_control->loopback_off(4);
		_neuron_control->loopback_off(6);
    }

	if (pcs_config.enable_playback_loop)
	{
		// enable loop mode of playback FIFO (TO DO)
		log(Logger::INFO) << "jd_mappingmaster: Using continuous pulse generation instead of playback loop mode";
		//jtag->FPGA_set_cont_pulse_ctrl(enable, channel, poisson?, delay, seed, nrn_start, char nrn_end, hicann_nr)
		jtag->FPGA_set_cont_pulse_ctrl(1, 0, 1, 128, 0x0, 0, 63, hicann_nr);
	}

	// settings for pulse feedback in FPGA
	// jtag command: SetPulseFeedback(unsigned int ch_in, unsigned int ch_out0, unsigned int ch_out1, unsigned int delay0, unsigned int delay1, bool enable = true)
	jtag->SetPulseFeedback(pcs_config.feedback_in,pcs_config.feedback_out0,pcs_config.feedback_out1,
	                       pcs_config.feedback_delay0,pcs_config.feedback_delay1,pcs_config.enable_fpga_feedback);

    comm->Flush();
}

/// pulses are written to the FPGA via JTAG_Ethernet bridge to the FPGA Playback FIFO
/// this includes transformation of pulse times to timestamp values; currently transformation assumes biological times in ms
void JDMappingMaster::write_playback_pulses(
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
		if (false && dnc_id != curr_addr.get_dnc_id()) {
			curr_addr.set_dnc_id(dnc_id);
			log(Logger::DEBUG0) << "write_playback_pulses: overriding dnc_id to " << dnc_id << Logger::flush;
		}
        // ignore all IDs from HICANN upwards, because used only for single HICANN experiments - ID is set in testmode
		//unsigned int curr_id = ((this->hicann_nr&0x7)<<9) | (curr_addr.address&0x1ff);
		// unsigned int curr_id = curr_addr.data.integer & 0xfff ;
		// neuron + channel + hicann + dnc
		//    6   +   3     +   3    +  2  = 14
		// INCLUDE DNC
		unsigned int curr_id = curr_addr.data.integer & 0x3fff ;

		for (unsigned int npulse=0;npulse<times.size();++npulse)
			pulselist.push_back(pulse_float_t(curr_id,times[npulse]));
    }

	std::sort(pulselist.begin(),pulselist.end());

	log(Logger::INFO) << "JDMappingMaster::write_playback_pulses: Send " << pulselist.size() << " pulses:";
	if (log.willBeLogged(Logger::DEBUG1))
		for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
			log(Logger::DEBUG1) << "  id=" << pulselist[npulse].id << ", time=" <<pulselist[npulse].time;


	bool use_playback_fifo = false; // or external Memory via HostAL
	if ( use_playback_fifo ) {

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

				log(Logger::INFO) << "JDMappingMaster: fill data: FPGA delta time: "<< curr_cycle-prev_reltime << ", HICANN " << hicann_id << ", pulse: " << pulse_data << ", extra: " << extra_pulse;

				jtag->FPGA_fill_playback_fifo(false, (curr_cycle-prev_reltime)<<1, hicann_id, pulse_data, extra_pulse);

				prev_reltime = curr_cycle;
			}

		}
	}
	else { // using HostALController
		_host_al_control->sendPulseList(pulselist);
	}
}

bool JDMappingMaster::execute_experiment(float duration_in_s)
{
	// enable DNC<->HICANN 
	log(Logger::INFO) << "Starting pulsepulse packet flow DNC<->HICANN" << Logger::flush;
	_spl1_control->write_cfg(spl1_cfg);

	// Start systime counter if successful
	log(Logger::INFO) << "jd_mappingmaster: Enabling systime counter";
	unsigned int curr_ip = jtag->get_ip();
	S2C_JtagPhys2Fpga *comm_ptr = getL2CommPtr();
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

	// Trace fifo is automatically enabled with pulse fifo
	// jtag->FPGA_enable_tracefifo();

	// test: send some pulses directly
	//for (unsigned int n=0;n<5;++n)
	//    jtag->FPGA_start_pulse_event(0,0xabc0+n);

	//jtag->FPGA_enable_fifoloop();
	//jtag->FPGA_enable_pulsefifo();
	
	// AL experiment trigger
	_host_al_control->startPlaybackTrace();

// TODO -- have a progress bar
//

	// wait...
	log(Logger::INFO) << "jd_mappingmaster: Running experiment for " << duration_in_s << " seconds";
	 usleep(1000000*duration_in_s);

	// stop experiment
	log(Logger::INFO) << "jd_mappingmaster: Stopping experiment";
	//jtag->FPGA_disable_pulsefifo();
	//jtag->FPGA_disable_tracefifo();

	_host_al_control->stopTrace();

	// disable DNC<->HICANN links to prevent FIFO from intercepting ARQ packets
	log(Logger::INFO) << "Stopping packet flow DNC<->HICANN" << Logger::flush;
	_spl1_control->write_cfg(0x0ff00);

	for (size_t ndnc = 0; ndnc < 4; ++ndnc) {
		uint32_t count_current_dnc = jtag->GetSentPlaybackCount(ndnc);
		log(Logger::INFO) << "\tDNC " << ndnc << ":\t" << count_current_dnc;
	}

	unsigned int trace_pulse_count = jtag->GetTracePulseCount();
	unsigned int trace_entry_count = jtag->GetTraceEntryCount();
    log(Logger::INFO) << "Traced pulse count: " << trace_pulse_count << ", stored in " << trace_entry_count << "memory entries";

	log(Logger::INFO) << Logger::flush;

	return true;
}

/// currently, transformation of timestamps back to pulse times outputs biological times in ms
void JDMappingMaster::read_trace_pulses(
			std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
		)
{
	spiketrains.clear();

	unsigned int spk_count = 0;

	bool use_trace_fifo = false;
	if (use_trace_fifo) {
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
			curr_address.set_dnc_id(dnc_id);

			spiketrains[curr_address].push_back( curr_time );

			++spk_count;

			if ( (rdata>>63) ) // second pulse available
			{
				rdata = rdata>>32;
				curr_id = (rdata>>15)&0xfff; // ignore HICANN ID, because this is set in testmode
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

		log(Logger::INFO) << "Resetting all FIFOs" << Logger::flush;
		jtag->FPGA_reset_tracefifo(); //this is always a good idea after using FIFOs
	}
	else { // using DDR2 memory
		std::vector<pulse_float_t> pulselist;
		// Trigger the FPGA to send spike data to the HOST.

		//_host_al_control->setJTAGInterface(jtag);
		//_host_al_control->startTraceReadJTAG();

		_host_al_control->startTraceRead();

		// Then poll to read the received Pulses
		// We don't know, when all events have arrived at the HOST
		size_t num_trials_wo_pulses = 5;
		for (size_t trial = 0; trial<num_trials_wo_pulses; ++trial) {
			std::vector<pulse_float_t> tmp = _host_al_control->getFPGATracePulses();
			if (tmp.size() > 0) {
				pulselist.insert(pulselist.end(),tmp.begin(),tmp.end());
				trial = 0;
				spk_count+=tmp.size();
			}
		}
		// convert into return format
		std::sort(pulselist.begin(),pulselist.end());

		std::vector<pulse_float_t>::const_iterator it_p = pulselist.begin();
		std::vector<pulse_float_t>::const_iterator it_p_end = pulselist.end();
		for ( ; it_p != it_p_end; ++it_p) {
			// TODO: set any coordinates here, e.g. FPGA, wafer etc.
			// TODO: if dnc_id has been altered before -> change it back
			l2_pulse_address_t curr_address(it_p->id);
			spiketrains[curr_address].push_back(it_p->time);
		}
		// TODO:
		// Do we need to reset something in the FPGA? like abov the tracefifo?
	}

	log(Logger::INFO) << "read_trace_spikes: " << spk_count << " spikes from " << spiketrains.size() << " addresses found" << Logger::flush;
}

/////////////////////////////////////////////////////////////////
// RCF
/////////////////////////////////////////////////////////////////

void JDMappingMaster::start_rcf() {
	using namespace RCF;

	RcfInitDeinit rcfInit;
	RcfServer* _rpc_server = new RCF::RcfServer(RCF::TcpEndpoint(server_port));
	std::cout << "starting RCF server" << std::endl;

	// set max msg size
	_rpc_server->getServerTransport().setMaxMessageLength(msg_size);

	SessionObjectFactoryServicePtr sofs(new SessionObjectFactoryService());
	_rpc_server->addService(sofs);

	// bind functions to service
	sofs->bind<I_Experiment, Experiment>("Experiment");

	// compression?
	RCF::FilterServicePtr filterServicePtr(new RCF::FilterService());
	filterServicePtr->addFilterFactory( RCF::FilterFactoryPtr(new RCF::ZlibStatefulCompressionFilterFactory()));
	_rpc_server->addService(filterServicePtr);

	_rpc_server->startInThisThread();
	//server->waitForStopEvent();
}

void JDMappingMaster::stop_rcf() {
	_rpc_server->stop();
}

/////////////////////////////////////////////////////////////////
// TESTMODE
/////////////////////////////////////////////////////////////////

bool JDMappingMaster::test() {
	start_rcf(); // blocks until it gets stop event
	return true;
}

static JDMappingMaster * test_instance;

class LteeJDMappingTest: public Listee<Testmode>{
public:
	LteeJDMappingTest() : Listee<Testmode>(string("jd_mappingmaster"),string("RCF Server for HICANN configuration & experiment control")){};
	Testmode * create(){
		test_instance = new JDMappingMaster();
		return test_instance;
	};
};

LteeJDMappingTest ListeeJDMappingTest;

struct Experiment {

	/// function to reset the system.
	void reset() {
		cout << "RCF: remote called Experiment::reset()" << endl;
		test_instance->initialize_system();
		test_instance->activate_hicann();
	}

	void stop() {
		cout << "RCF: remote called Experiment::stop()" << endl;
		test_instance->stop_rcf();
	}


	/// initialize layer 2 (separated from reset to be able to re-call it during an experiment sweep).
	bool init() {
		cout << "RCF: remote called Experiment::init()" << endl;
		bool result = test_instance->init_layer2();
		return result;
    }

	/// function to configure a HICANN.
	void config(
	    hicann_cfg_t hicann_cfg //!< an HICANN config object
	    )
    {
		cout << "RCF: remote called Experiment::config(...)" << endl;
		cout << "received config for hicann @ " << hicann_cfg.xcoord << " " << hicann_cfg.ycoord << endl;

		/////////////////////////////////////////////////////////////////
		// Configure HICANN
		/////////////////////////////////////////////////////////////////

        test_instance->program_hicann(hicann_cfg);

        cout << "config had " << (hicann_cfg.finished ? "" : "not ") << "finished." << endl;


	}

	/// function to configure one PCS (=FPGA+4DNCs)
	void config_layer2(
		pcs_cfg_t psc_cfg //!< a PCS config object
		)
	{
		cout << "RCF: remote called Experiment::config_layer2()" << endl;
		test_instance->program_layer2(psc_cfg);
	}

	/// function to transmit spike trains to the FPGA.
	void set_input_spikes(
	    std::map< l2_pulse_address_t, std::vector<double> > spiketrains  //!< an input spike train in hw time
	    )
	{
		cout << "RCF: remote called Experiment::set_input_spikes()" << endl;

	    std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st= spiketrains.begin();
	    std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st_end = spiketrains.end();

		for(;it_st != it_st_end; ++it_st)
		{
			l2_pulse_address_t addr = it_st->first;
			cout << "Target: \t" << std::dec;
			cout << "wafer_id: " << addr.data.fields.wafer_id << ", ";
			cout << "fpga_id: " << addr.data.fields.fpga_id << ", ";
			cout << "dnc_id: " <<  addr.data.fields.dnc_id << ", ";
			cout << "hicann_id: " << addr.data.fields.hicann_id << ", ";
			cout << "dnc_if_channel: " <<  addr.data.fields.dnc_if_channel<< endl;
			cout << "l1_address: " <<  addr.data.fields.l1_address << endl;
			cout << "Nr of Spikes: " << it_st->second.size() << endl;
		}

		if (spiketrains.size() > 0)
			test_instance->write_playback_pulses(spiketrains);
	}

	/// function to get spike trains to the FPGA.
	void get_recorded_spikes(
	    std::map< l2_pulse_address_t, std::vector<double> > & spiketrains //!< a recorded spike train in hw time
	    )
	{
		cout << "RCF: remote called Experiment::get_recorded_spikes()" << endl;
		test_instance->read_trace_pulses(spiketrains);
   	}

	bool run(
			double duration //!< duration in hw time
			)
	{
		cout << "RCF: remote called Experiment::run()" << endl;
		// TODO: include duration
		bool success_val = test_instance->execute_experiment(duration);
		// return false if something went wrong.
		return success_val;
	}

};
