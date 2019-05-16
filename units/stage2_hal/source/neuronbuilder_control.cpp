// Company         :   kip
// Author          :   Sebastian Millner
// E-Mail          :   smillner@kip.uni-heidelberg.de
//
// Filename        :   neuronBuilder_control.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Mon Jan 11 10
// Last Change     :   Mon May 04 12
// by              :   akononov
//------------------------------------------------------------------------

#include "common.h" // library includes
#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "neuronbuilder_control.h" // neuronBuilder control class

using namespace std;
using namespace facets;

// *** BEGIN class NeuronBuilderControl ***
const uint NeuronBuilderControl::MAXADR = (1 << nspl1_numa) - 1;
const uint NeuronBuilderControl::NREGBASE = MAXADR + 1;
const uint NeuronBuilderControl::OUTREGBASE = MAXADR + 2;
NeuronBuilderControl::iomap_t NeuronBuilderControl::IOmap_;

//binary output in stream
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

NeuronBuilderControl::NeuronBuilderControl(Stage2Ctrl *c, uint ta, uint sa, uint ma)
    : CtrlModule(c, ta, sa, ma),
      log(Logger::instance("hicann-system.NeuronBuilderControl", Logger::INFO, "")) {
	outputcfg_ = std::vector<bool>(2*aoutselect_dw+2,0);
	neuroncfg_ = std::vector<bool>(2+1+2+2*n_numslow+2*n_numfast,0);

	sram_ = std::vector<NBData>(MAXADR+1); // MAXADR is the last address, i.e. the length is +1
	inout_top_= std::vector<InOutData>(MAXADR+1);
	inout_bot_= std::vector<InOutData>(MAXADR+1);

	// set all sram entries to zero
	for (uint i=0; i<=MAXADR; ++i){
	  sram_.at(i).integer = 0;
	  inout_top_.at(i).integer = 0;
	  inout_bot_.at(i).integer = 0;
	}

	// write fixed entries into aoutmap
	// this table reproduces the mapping in sec. 4.3.4 in hicann-doc
	IOmap_[0]  = 0;
	IOmap_[1]  = 1;
	IOmap_[2]  = 2;
	IOmap_[3]  = 3;
	IOmap_[4]  = 4;
	IOmap_[5]  = 5;
	IOmap_[10] = 6;
	IOmap_[8]  = 7;
}

NeuronBuilderControl::~NeuronBuilderControl(){}

int NeuronBuilderControl::write_data(uint rowaddr, std::vector<bool> cfg){
	ci_addr_t addr = rowaddr;
	//addr +=OcpNeuronBuilder.startaddr;

	ci_data_t data = 0;
	for(uint l=0;l<cfg.size();l++) data += cfg[l] << l;  // shift config bits into data payload
	log(Logger::DEBUG1)<<"NeuronBuilderControl::Writing vector " << hex << data;
	return write_cmd(addr, data, del);        // ...send
}

int NeuronBuilderControl::write_data(uint rowaddr, uint cfg){
	ci_addr_t addr = rowaddr;
	//addr +=OcpNeuronBuilder.startaddr;
	ci_data_t data = cfg;
	return write_cmd(addr, data, del);
}

void NeuronBuilderControl::write_sram(){
	log(Logger::DEBUG0) <<"NeuronBuilderControl::Writing sram...";
		for (uint i=0; i<=MAXADR; ++i)
		  write_data(i,sram_.at(i).integer);
}

bool NeuronBuilderControl::check_sram(){

	unsigned int num_firing_denmems=0, sum=0;
	bool passed = true;

	for (uint i=0; i<=MAXADR; ++i){
		unsigned int check = nbdata(sram_.at(i).bits.fireb,
		                            sram_.at(i).bits.firet,
		                            sram_.at(i).bits.vertical,
		                            sram_.at(i).bits.nmem_top,
		                            sram_.at(i).bits.nmem_bot,
		                            sram_.at(i).bits.spl1);

		log(Logger::DEBUG0) << "NeuronBuilderControl::check_sram: SRAM entry at: " << i
				<< "[fireb:" << sram_.at(i).bits.fireb
				<< ",firet:" << sram_.at(i).bits.firet
				<< ",vertical:" << sram_.at(i).bits.vertical
				<< ",nmem_top:" << sram_.at(i).bits.nmem_top
				<< ",nmem_bot:" << sram_.at(i).bits.nmem_bot
				<< ",spl1:" << sram_.at(i).bits.spl1
				<< "]" << Logger::flush;

		if (check != sram_.at(i).integer){
			log(Logger::ERROR) << "NeuronBuilderControl::check_sram: SRAM entry at address " << i << " seems incorrect! " << check << " - " << sram_.at(i).integer ;
			passed = false;
		}

		// check if invalid nmem entries have been written
		if (i%4!=3){
			if (sram_.at(i).bits.nmem_top || sram_.at(i).bits.nmem_bot){
				log(Logger::ERROR) << "NeuronBuilderControl::check_sram: nmem entry at invalid address " << i << "!";
				passed = false;
			}
		}

		if (i%2!=0){
			if (sram_.at(i).bits.vertical || sram_.at(i).bits.fireb){
				log(Logger::ERROR) << "NeuronBuilderControl::check_sram: vertical/fireb entry at invalid address " << i << "!";
				passed = false;
			}
		}

		if (i%2!=1){
			if (sram_.at(i).bits.firet){
				log(Logger::ERROR) << "NeuronBuilderControl::check_sram: firet entry at invalid address " << i << "!";
				passed = false;
			}
		}

		num_firing_denmems += sram_.at(i).bits.fireb;
		num_firing_denmems += sram_.at(i).bits.firet;

		sum += sram_.at(i).integer;
	}

	log(Logger::INFO) << "Found " << num_firing_denmems << " firing denmems.";

	if (sum==0)
		log(Logger::WARNING) << "NeuronBuilderControl::check_sram: Neuronbuilder SRAM seems empty.";

	return passed;
}

void NeuronBuilderControl::print_sram(ostream &os){
		os << "-------------------------------SRAM CONFIG-----------------------------------" << std::endl;
		os << "legend:\n";
		os << "io:\tinout\n";
		os << "inp:\tinterconnect_neuron_pair\n";
		os << "afl:\tactivate_firing_left\n";
		os << "elfi:\tenable_left_fire_input\n";
		os << "erfi:\tenable_right_fire_input\n";
		os << "afr:\tactivate_firing_right\n";
		os << "v:\tvertical interconnection\n";
		os << "firet:\tfire enable top\n";
		os << "fireb:\tfire enable bottom\n";

		os << "----------------||------||--TOP---------------------------------||--BOTTOM------------------------------||------" << std::endl;

		os << "address  \t|| ";
		os << "spl1"<<
			"\t|| " << "io" <<
			" \t| "  << "inp"<<
			" | "  << "afl" <<
			" | "  << "elfi" <<
			" | "  << "erfi" <<
			" | "  << "afr" <<
			" || " << "io"<<
			" \t| "  << "inp"<<
			" | "  << "afl" <<
			" | "  << "elfi" <<
			" | "  << "erfi" <<
			" | "  << "afr" <<
			" || " << "v" <<
			" | "  << "firet" <<
			" | "  << "fireb" <<
			std::endl;

	for (uint i=0; i<=MAXADR; ++i){
		if (i%4==0)
			os << "-----------------------------------------------------------------------------" << std::endl;

		unsigned int tmp_nmem_top = sram_.at(i).bits.nmem_top;
		unsigned int tmp_nmem_bot = sram_.at(i).bits.nmem_bot;
		PairConfiguration tmp_top, tmp_bot;
		tmp_top.integer = tmp_nmem_top;
		tmp_bot.integer = tmp_nmem_bot;

		unsigned int num=0, spl1=sram_.at(i).bits.spl1;
		for(int j=0; j<6; j++) num |= (spl1 & (1<<j))?(1<<(5-j)):0; //reverse bit order bug

		os << "address " << i << " \t|| ";

		os << num                                      <<
			" \t|| " << tmp_top.bits.inout                                 <<
			" \t|  "  << (tmp_top.bits.interconnect_neuron_pair  ? "c":"0") <<
			"  | "  << (tmp_top.bits.activate_firing_left        ? "l":"0") <<
			"   |  "  << (tmp_top.bits.enable_left_fire_input      ? "l":"0") <<
			"   |  "  << (tmp_top.bits.enable_right_fire_input     ? "r":"0") <<
			"   |  "  << (tmp_top.bits.activate_firing_right       ? "r":"0") <<
			"  ||  " << tmp_bot.bits.inout                                 <<
			"  \t|  "  << (tmp_bot.bits.interconnect_neuron_pair  ? "c":"0") <<
			"  | "  << (tmp_bot.bits.activate_firing_left        ? "l":"0") <<
			"   |  "  << (tmp_bot.bits.enable_left_fire_input      ? "l":"0") <<
			"   |  "  << (tmp_bot.bits.enable_right_fire_input     ? "r":"0") <<
			"   |  "  << (tmp_bot.bits.activate_firing_right       ? "r":"0") <<
			"  ||  " << (sram_.at(i).bits.vertical                ? "v":"0") <<
			"  |  "  << (sram_.at(i).bits.firet                   ? "t":"0") <<
			"  |  "  << (sram_.at(i).bits.fireb                   ? "b":"0") <<
			std::endl;
	}
}

void NeuronBuilderControl::enable_firing(unsigned int denmem_i){
	assert(denmem_i<=MAXADR);
	const bool is_top = denmem_i <= (MAXADR/2);
	const bool is_left = (denmem_i%2)==0;
	unsigned int address_to_write, tmp_i;
	PairConfiguration tmp_pc;

	if (is_top){
		// 1: set firet, enabling firing under a spl1 address
		address_to_write = denmem_i*2+1;
		sram_.at(address_to_write).bits.firet = 1;
		// 2: enable firing for the denmem
		address_to_write = denmem_i/2*4+3; // maps to 3,3,7,7,1,1,...
		tmp_i = sram_.at(address_to_write).bits.nmem_top;
		tmp_pc.integer = tmp_i;
		if (is_left)
			tmp_pc.bits.activate_firing_left = 1;
		else
			tmp_pc.bits.activate_firing_right = 1;
		sram_.at(address_to_write).bits.nmem_top = tmp_pc.integer;
	}
	else{ // is bottom
		// 1: set firet, enabling firing under a spl1 address
		address_to_write = (denmem_i-(MAXADR+1)/2)*2;
		sram_.at(address_to_write).bits.fireb = 1;
		// 2: enable firing for the denmem
		address_to_write = (denmem_i-(MAXADR+1)/2)/2*4+3; // maps to 3,3,7,7,1,1,...
		tmp_i = sram_.at(address_to_write).bits.nmem_bot;
		tmp_pc.integer = tmp_i;
		if (is_left)
			tmp_pc.bits.activate_firing_left = 1;
		else
			tmp_pc.bits.activate_firing_right = 1;
		sram_.at(address_to_write).bits.nmem_bot = tmp_pc.integer;
	}
}

void NeuronBuilderControl::set_spl1address(unsigned int denmem_i, unsigned int spl1){
	assert(denmem_i<=MAXADR);
	const bool is_top = denmem_i <= (MAXADR/2);
	unsigned int address_to_write;

	unsigned int num=0;
	for(int i=0; i<6; i++) num |= (spl1 & (1<<i))?(1<<(5-i)):0; //reverse bit order bug
	spl1=num;

	if (is_top)
		address_to_write = (denmem_i%256)*2 + 1;
	else
		address_to_write = (denmem_i%256)*2;

	sram_.at(address_to_write).bits.spl1 = spl1;
}

void NeuronBuilderControl::connect_vertical(unsigned int denmem_i){
	log(Logger::DEBUG0) <<"vertically connect denmem " << denmem_i << " and " << denmem_i+256;
	assert(denmem_i<=MAXADR/2);
	sram_.at(denmem_i*2).bits.vertical = 1;
}

void NeuronBuilderControl::connect_horizontal_top(unsigned int denmem_id, int direction){

	unsigned int address_to_write;
	PairConfiguration nmem;
	unsigned int tmp_i;

	// undirected connection
	if (denmem_id%2==0){
		address_to_write = denmem_id*2+3;
		tmp_i = sram_.at(address_to_write).bits.nmem_top;
		nmem.integer = tmp_i;
		nmem.bits.interconnect_neuron_pair = 1;
		sram_.at(address_to_write).bits.nmem_top = nmem.integer;
		return;
	}

	// direction to the right, i.e. "enable_left_fire_input"
	if (direction == 1){
		address_to_write = (denmem_id+1)*2+3;
		tmp_i = sram_.at(address_to_write).bits.nmem_top;
		nmem.integer = tmp_i;
		nmem.bits.enable_left_fire_input = 1;

		// determine if that neuron is set to fire, then enable also firing between denmems
		if (sram_.at(denmem_id*2+1).bits.firet)
			nmem.bits.activate_firing_left=1;
		sram_.at(address_to_write).bits.nmem_top = nmem.integer;
		return;
	}

	// direction to the left, i.e. "enable_right_fire_input"
	if (direction == -1){
		address_to_write = denmem_id*2+1;
		tmp_i = sram_.at(address_to_write).bits.nmem_top;
		nmem.integer = tmp_i;
		nmem.bits.enable_right_fire_input = 1;

		// determine if our neuron is set to fire, then enable also firing between denmems
		if (sram_.at(denmem_id*2+3).bits.firet)
			nmem.bits.activate_firing_right=1;
		sram_.at(address_to_write).bits.nmem_top = nmem.integer;
		return;
	}

	assert(0); // we should never reach here
}

void NeuronBuilderControl::connect_horizontal_bottom(unsigned int denmem_id, int direction){

	unsigned int address_to_write;
	PairConfiguration nmem;
	unsigned int tmp_i;

	// undirected connection
	if (denmem_id%2==0){
		address_to_write = denmem_id*2+3;
		tmp_i = sram_.at(address_to_write).bits.nmem_bot;
		nmem.integer = tmp_i;
		nmem.bits.interconnect_neuron_pair = 1;
		sram_.at(address_to_write).bits.nmem_bot = nmem.integer;
		return;
	}

	// direction to the right, i.e. "enable_left_fire_input"
	if (direction == 1){
		address_to_write = (denmem_id+1)*2+3;
		tmp_i = sram_.at(address_to_write).bits.nmem_bot;
		nmem.integer = tmp_i;
		nmem.bits.enable_left_fire_input = 1;

		// determine if that neuron is set to fire, then enable also firing between denmems
		if (sram_.at(denmem_id*2).bits.fireb)
			nmem.bits.activate_firing_left = 1;
		sram_.at(address_to_write).bits.nmem_bot = nmem.integer;
		return;
	}

	// direction to the left, i.e. "enable_right_fire_input"
	if (direction == -1){
		address_to_write = denmem_id*2+1;
		tmp_i = sram_.at(address_to_write).bits.nmem_bot;
		nmem.integer = tmp_i;
		nmem.bits.enable_right_fire_input = 1;

		// determine if our neuron is set to fire, then enable also firing between denmems
		if (sram_.at(denmem_id*2+2).bits.fireb)
			nmem.bits.activate_firing_right=1;
		sram_.at(address_to_write).bits.nmem_bot = nmem.integer;
		return;
	}

	assert(0); // we should never reach here
}


void NeuronBuilderControl::set_aout(unsigned int denmem_id){
	log(Logger::INFO) << "Setting analog out for denmem " << denmem_id;
	const bool is_top = denmem_id <= (MAXADR/2);
	const bool is_left = (denmem_id%2)==0;
	unsigned int address_to_write, tmp_nmem;
	PairConfiguration tmp_pc;
	log(Logger::INFO) << "Setting analog out for denmem " << denmem_id;

	if (is_top){
		address_to_write = denmem_id/2*4+3; // maps to 3,3,7,7,11,11,...
		if (is_left){
			for(uint nadd=3; nadd<512; nadd+=4){
				inout_top_.at(nadd).bits.aout_l = 0; //unset all readout bits corresponding to line first
			}
			inout_top_.at(address_to_write).bits.aout_l = 1;
		} else {
			for(uint nadd=3; nadd<512; nadd+=4){
				inout_top_.at(nadd).bits.aout_r = 0; //unset all readout bits corresponding to line first
			}
			inout_top_.at(address_to_write).bits.aout_r = 1;
		}
		for(uint nadd=3; nadd<512; nadd+=4){
			tmp_nmem = sram_.at(nadd).bits.nmem_top;
			tmp_pc.integer = tmp_nmem;
			tmp_pc.bits.inout = IOmap_.at(inout_top_.at(nadd).integer);
			sram_.at(nadd).bits.nmem_top = tmp_pc.integer;
		}
		
	}
	else{ //bottom
		address_to_write = (denmem_id-(MAXADR+1)/2)/2*4+3;
		if (is_left){
			for(uint nadd=3; nadd<512; nadd+=4){
				inout_bot_.at(nadd).bits.aout_l = 0; //unset all readout bits corresponding to line first
			}
			inout_bot_.at(address_to_write).bits.aout_l = 1;
		} else {
			for(uint nadd=3; nadd<512; nadd+=4){
				inout_bot_.at(nadd).bits.aout_l = 0; //unset all readout bits corresponding to line first
			}
			inout_bot_.at(address_to_write).bits.aout_r = 1;
		}
		for(uint nadd=3; nadd<512; nadd+=4){
			tmp_nmem = sram_.at(nadd).bits.nmem_bot;
			tmp_pc.integer = tmp_nmem;
			tmp_pc.bits.inout = IOmap_.at(inout_bot_.at(nadd).integer);
			sram_.at(nadd).bits.nmem_bot = tmp_pc.integer;
		}
	}
	log(Logger::INFO)<< sram_.at(address_to_write).bits.nmem_top;

}

void NeuronBuilderControl::set_currentin(unsigned int denmem_id){
	log(Logger::INFO)<< "Setting current in for denmem " << denmem_id;
	const bool is_top = denmem_id <= (MAXADR/2);
	const bool is_left = (denmem_id%2)==0;
	unsigned int address_to_write;

	if (is_top){
		address_to_write = denmem_id/2*4+3; // maps to 3,3,7,7,1,1,...
		if (is_left)
			inout_top_.at(address_to_write).bits.currentin_l = 1;
		else
			inout_top_.at(address_to_write).bits.currentin_r = 1;
		sram_.at(address_to_write).bits.nmem_top = IOmap_.at(inout_top_.at(address_to_write).integer);
	}
	else{
		address_to_write = (denmem_id-(MAXADR+1)/2)/2*4+3;
		if (is_left)
			inout_bot_.at(address_to_write).bits.currentin_l = 1;
		else
			inout_bot_.at(address_to_write).bits.currentin_r = 1;
		sram_.at(address_to_write).bits.nmem_bot = IOmap_.at(inout_bot_.at(address_to_write).integer);
	}
	log(Logger::INFO)<< sram_.at(address_to_write).bits.nmem_top;

}

int NeuronBuilderControl::read_data(uint rowaddr){
	ci_addr_t addr = rowaddr; // row address of addressed crossbar
	//addr +=OcpNeuronBuilder.startaddr;
	return read_cmd(addr, del);         // ...send
}

int NeuronBuilderControl::writeTRD(uint trd){
	return write_data((1<<nspl1_numa)+0x10, trd);
}

int NeuronBuilderControl::readTRD(){
	return read_data((1<<nspl1_numa)+0x10);
}

int NeuronBuilderControl::writeTSUTWR(uint data){
	return write_data((1<<nspl1_numa)+0x11, data);
}

int NeuronBuilderControl::readTSUTWR(){
	return read_data((1<<nspl1_numa)+0x11);
}

int NeuronBuilderControl::initzeros(){
	for (uint i = 0; i <= MAXADR; i++){
		write_data(i,0);
	}
	log(Logger::DEBUG0)<<"NeuronBuilderControls::initzeros ...";
	return 1;
}

int NeuronBuilderControl::configNeuronsGbl(){
	log(Logger::DEBUG1)<<"NeuronBuilderControl:Writing neuron builder control config vector";
	return write_data(NREGBASE,neuroncfg_);
}

int NeuronBuilderControl::configOutputs(){
	log(Logger::DEBUG1)<<"NeuronBuilderControl:Writing neuron builder control output vector";
	return write_data(OUTREGBASE,outputcfg_);
}

int NeuronBuilderControl::setOutputMux(uint op1, uint op2){
	for(int i=2*aoutselect_dw-1; i>=0; i--){
		outputcfg_[i] = 0;
	}
	if(op1 <aoutselect_dw){
		outputcfg_[2*aoutselect_dw-1-op1]=1;
	}
	if(op2 < aoutselect_dw){
		outputcfg_[aoutselect_dw-op2-1]=1;
	}
	log(Logger::DEBUG0) << "NeuronBuilderControls::setOutputMux: [op1:" << op1 << ",op2:" << op2 << "]";
	return configOutputs();
}

int NeuronBuilderControl::setOutputEn(bool op1, bool op2){
	outputcfg_[aoutselect_dw*2+1]=op1;
	outputcfg_[aoutselect_dw*2]=op2;
	log(Logger::DEBUG0) << "NeuronBuilderControls::setOutputEn: [en1:" << op1 << ",en1:" << op2 << "]";
	return configOutputs();
}

void NeuronBuilderControl::get_read_data(ci_addr_t& rowaddr, ci_data_t& data)
{
#ifndef WITHOUT_ARQ
	ci_payload tmp;
	if(get_data(&tmp) == 1){
		log(Logger::DEBUG2)<< "NeuronBuilderControl::get_read_data: " << tmp;
		rowaddr = tmp.addr & 0x1ff;
		data = tmp.data;
	}else{
		log(Logger::DEBUG2)<< "NeuronBuilderControl::get_read_data: fail...";
	}
#endif // WITHOUT_ARQ
}

int NeuronBuilderControl::write_nmem(uint addr, uint data){
	uint addr_int = 4* addr + 3;
	uint data_int = data << spl1_numd;
	return write_data(addr_int, data_int);
}

int NeuronBuilderControl::read_nmem(uint addr){
	return read_data(4 * addr + 3);
}

void NeuronBuilderControl::get_read_data_nmem(ci_addr_t& addr, ci_data_t& data){
	get_read_data(addr, data);
	data = data >> spl1_numd;
	data = data & 0xffff;
	addr = (addr - 3) / 4;
}

int NeuronBuilderControl::setNeuronGl(bool slow0, bool slow1, bool fast0, bool fast1){
	neuroncfg_[2]=fast1;
	neuroncfg_[n_numfast + 2] = fast0;
	neuroncfg_[2*n_numfast + 2] = slow1;
	neuroncfg_[2*n_numfast  + n_numslow + 2] = slow0;
	log(Logger::DEBUG0)<<"NeuronBuilderControls::setNeuronGl: [slow0:" << slow0 <<",slow1:"<< slow1 <<",fast0:"<< fast0 <<",fast1:"<< fast1 <<"]";
	return configNeuronsGbl();
}

int NeuronBuilderControl::setNeuronGladapt(bool slow0, bool slow1, bool fast0, bool fast1){
	neuroncfg_[1]=fast1;
	neuroncfg_[n_numfast + 1] = fast0;
	neuroncfg_[2*n_numfast + 1] = slow1;
	neuroncfg_[2*n_numfast  + n_numslow + 1] = slow0;
	log(Logger::DEBUG0)<<"NeuronBuilderControls::setNeuronGladapt: [slow0:" << slow0 <<",slow1:"<< slow1 <<",fast0:"<< fast0 <<",fast1:"<< fast1 <<"]";
	return configNeuronsGbl();
}

int NeuronBuilderControl::setNeuronRadapt(bool slow0, bool slow1, bool fast0, bool fast1){
	neuroncfg_[0]=fast1;
	neuroncfg_[n_numfast + 0] = fast0;
	neuroncfg_[2*n_numfast + 0] = slow1;
	neuroncfg_[2*n_numfast  + n_numslow + 0] = slow0;
	log(Logger::DEBUG0)<<"NeuronBuilderControls::setNeuronGladapt: [slow0:" << slow0 <<",slow1:"<< slow1 <<",fast0:"<< fast0 <<",fast1:"<< fast1 <<"]";;
	return configNeuronsGbl();
}

int NeuronBuilderControl::setNeuronBigCap(bool n0, bool n1){
	neuroncfg_[2*n_numfast + 2 * n_numslow] = n1;
	neuroncfg_[2*n_numfast  + 2 * n_numslow + 1] = n0;
	log(Logger::DEBUG0)<<"NeuronBuilderControls::setNeuronBigCap: [n0:" << n0 <<",n1:"<< n1 <<"]";
	return configNeuronsGbl();
}

int NeuronBuilderControl::setNeuronReset(bool n0, bool n1){
	neuroncfg_[2*n_numfast + 2 * n_numslow + 3] = n1;
	neuroncfg_[2*n_numfast  + 2 * n_numslow + 4] = n0;
	log(Logger::DEBUG0)<<"NeuronBuilderControls::setNeuronReset: [n0:" << n0 <<",n1:"<< n1 <<"]";
	return configNeuronsGbl();
}


int NeuronBuilderControl::setSpl1Reset(bool r){
	neuroncfg_[2*n_numfast + 2 * n_numslow + 2] = r;
	log(Logger::DEBUG0)<<"NeuronBuilderControls::setSpl1Reset: [r:" << r <<"]";
	return configNeuronsGbl();
}

void NeuronBuilderControl::print_config(){
	ci_addr_t addr;
	ci_data_t config[4];
	uint spl1[4]; // ECM: too many bits?

	cout << "WARNING: The SPL1 address bit order is reverse on the hardware (Bug)! Spl1addr is also shown in decimal" << endl;
	for(size_t i = 0; i < MAXADR + 1; i++){
		read_data(i);
		get_read_data(addr, config[i%4]);
		if (i!=addr) throw runtime_error("Returned address does not match requested. Fail.");

		if (i%4==3) {
			for(int k=0; k<4; k++){
				uint num=0;
				spl1[k]=config[k]&0x3f;
				for(int j=0; j<6; j++) num |= (spl1[k] & (1<<j))?(1<<(5-j)):0; //revert bit order
				spl1[k]=num;
			}

			cout << "Address " << dec << setw(3) << i-3 << " - " << setw(3) << i << ": "
				<< "Spl1addr: " << setw(2) << spl1[0] << ", " << setw(2) << spl1[1]
				<< ", " << setw(2) << spl1[2] << ", " << setw(2) << spl1[3] << "."
				<< " NmemTop: " << hex << bino(((config[3]&0x3fc0)>>6),8)
				<< ". NmemBot: " << bino(((config[3]&0x3fc000)>>14),8)
				<< ". VertConnect: " << ((config[0]&0x400000)>>22) << ", " << ((config[2]&0x400000)>>22)
				<< ". FireEnable: " << ((config[0]&0x1000000)>>24) << ", " << ((config[1]&0x800000)>>23)
				<< ", " << ((config[2]&0x1000000)>>24) << ", " << ((config[3]&0x800000)>>23) << dec << endl;
		}
	}
	cout << "WARNING: The SPL1 address bit order is reverse on the hardware (Bug)! Spl1addr is also shown in decimal" << endl;
}

void NeuronBuilderControl::set_sram_timings(uint read_delay, uint setup_precharge, uint write_delay){
	if (read_delay >= (1<<8)) throw runtime_error("read_delay parameter too large (size is 8 bit in hardware). Fail.");
	if (setup_precharge >= (1<<4)) throw runtime_error("setup_precharge parameter too large (size is 4 bit in hardware). Fail.");
	if (write_delay >= (1<<4)) throw runtime_error("write_delay parameter too large (size is 4 bit in hardware). Fail.");

	write_data((1<<addrmsb_nrnbuilder) + (1<<cnfgadr_nrnbuilder)     , (read_delay&0xff));
	write_data((1<<addrmsb_nrnbuilder) + (1<<cnfgadr_nrnbuilder) + 1 , (((setup_precharge&0xf)<<4) + (write_delay&0xf)));
}

void NeuronBuilderControl::get_sram_timings(uint& read_delay, uint& setup_precharge, uint& write_delay){
	ci_addr_t addr0 = (1<<addrmsb_nrnbuilder) + (1<<cnfgadr_nrnbuilder);
	ci_addr_t addr1 = (1<<addrmsb_nrnbuilder) + (1<<cnfgadr_nrnbuilder) + 1;

	read_data(addr0);
	read_data(addr1);

	ci_data_t rd_data;
	get_read_data(addr0, rd_data);
	read_delay = rd_data&0xff;

	get_read_data(addr1, rd_data);
	setup_precharge = (rd_data>>4)&0xf;
	write_delay     = rd_data&0xf;
}


void NeuronBuilderControl::reset(){
	for (uint i = 0; i <= OUTREGBASE; i++) write_data(i, 0);
}
// *** END class NeuronBuilderControl END ***
