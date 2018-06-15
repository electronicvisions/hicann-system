// Company         :   kip
// Author          :   Sebastian Millner
// E-Mail          :   smillner@kip.uni-heidelberg.de
//
// Filename        :   neuron_control.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Thu Feb 25 10
// Last Change     :   Fr Mar 26 11
// by              :   akononov
//------------------------------------------------------------------------


#include "common.h" // library includes
#include "s2comm.h"
#include "linklayer.h"

#include "s2ctrl.h"
#include "ctrlmod.h"
#include "hicann_ctrl.h"
#include "neuron_control.h" // neuron control class


#include <boost/assign/list_of.hpp>

using namespace facets;

std::map<nc_merger,std::string>
NeuronControl::merger_names = boost::assign::map_list_of
	(BG0,     "BG0")
	(BG1,     "BG1")
	(BG2,     "BG2")
	(BG3,     "BG3")
	(BG4,     "BG4")
	(BG5,     "BG5")
	(BG6,     "BG6")
	(BG7,     "BG7")
	(L0_0,    "L0_0")
	(L0_1,    "L0_1")
	(L0_2,    "L0_2")
	(L0_3,    "L0_3")
	(L1_0,    "L1_0")
	(L1_1,    "L1_1")
	(L2_0,    "L2_0")
	(DNC0,    "DNC0")
	(DNC1,    "DNC1")
	(DNC2,    "DNC2")
	(DNC3,    "DNC3")
	(DNC4,    "DNC4")
	(DNC5,    "DNC5")
	(DNC6,    "DNC6")
	(DNC7,    "DNC7")
	(MERG_ALL,"MERG_ALL");


using namespace std;
using namespace facets;

//extern Debug dbg;

// *** BEGIN class NeuronControl ***

NeuronControl::NeuronControl(Stage2Ctrl* c, uint ta, uint sa, uint ma):CtrlModule(c,ta,sa,ma) 
	,log(Logger::instance("hicann-system.NeuronControl", Logger::INFO,""))
{
	#if HICANN_V >= 2
	config.resize(20);
	#elif HICANN_V == 1
	config.resize(15);
	#else
	#error Missing code for this HICANN revision.
	#endif
}

NeuronControl::~NeuronControl(){}

void NeuronControl::configure(){
	log(Logger::DEBUG0) << "NeuronControl::configure";
	ci_addr_t adr;
	#if HICANN_V >= 2
	for (int i=0; i<20; i++){
		read_data(i);
		get_read_data(adr,config[i]);
	}
	#elif HICANN_V == 1
	for (int i=0; i<15; i++){
		read_data(i);
		get_read_data(adr,config[i]);
	}
	#else
	#error Missing code for this HICANN revision.
	#endif
}

int NeuronControl::write_data(uint rowaddr, vector<bool> cfg){
	ci_addr_t addr = rowaddr; // address of addressed instance
	ci_data_t data = 0;
	for(uint l=0;l<cfg.size();l++) data += cfg[l] << l;  // shift config bits into data payload
	return write_cmd(addr, data, del);        // ...send
}

int NeuronControl::write_data(uint rowaddr, uint cfg){
	ci_addr_t addr = rowaddr; // address of addressed instance
	ci_data_t data = cfg;
	return write_cmd(addr, data, del);
}

int NeuronControl::read_data(uint rowaddr){
	ci_addr_t addr = rowaddr; // address of addressed instance
	return read_cmd(addr, del);         // ...send
}

void NeuronControl::get_read_data(ci_addr_t& rowaddr, ci_data_t& data)
{
	#ifndef WITHOUT_ARQ
	ci_payload tmp;
	if(get_data(&tmp) == 1){
		log(Logger::DEBUG1) << "NeuronControl::get_read_data: " << tmp ;
		rowaddr = tmp.addr & 0xf;  // max address is 14
		data = tmp.data & 0xffff;  // data width is 16 bit
	}
	else log(Logger::ERROR) << "NeuronControl::get_read_data: fail...";
	#endif // WITHOUT_ARQ
}

void NeuronControl::set_seed(uint seed){
	log(Logger::DEBUG0) << "NeuronControl::set_seed " << seed;
	write_data(nc_seed, seed); // 16 bit seed allowed
	config[nc_seed]=seed;
}

void NeuronControl::beg_on(uint gen_nr){
	// gen_nr 0-7 are the individual generators, gen_nr=8 means start all generators together
	if (gen_nr > 8) {
		log(Logger::ERROR) << "NeuronControl::beg_on: Illegal generator number";
	} else {
		if (gen_nr == 8){
			log(Logger::DEBUG0) << "NeuronControl::beg_on enable all";
			write_data(nc_randomreset, config[nc_randomreset] | 0xff);
			config[nc_randomreset] |= 0xff;
		}
		else {
			log(Logger::DEBUG0) << "NeuronControl::beg_on enable nr " << gen_nr;
			gen_nr = 1 << gen_nr;
			write_data(nc_randomreset, config[nc_randomreset] | gen_nr);
			config[nc_randomreset] |= gen_nr;
		}
	}
}

void NeuronControl::beg_off(uint gen_nr){
	// gen_nr 0-7 are the individual generators, gen_nr=8 means stop all generators
	if (gen_nr > 8) {
		log(Logger::ERROR) << "NeuronControl::beg_off: Illegal generator number";
	} else {
		if (gen_nr == 8){
			log(Logger::DEBUG0) << "NeuronControl::beg_off disable all";
			write_data(nc_randomreset, config[nc_randomreset] & 0xff00);
			config[nc_randomreset] &= 0xff00;
		}
		else {
			log(Logger::DEBUG0) << "NeuronControl::beg_off disable nr" << gen_nr;
			gen_nr = (1 << gen_nr) ^ 0xffff;
			write_data(nc_randomreset, config[nc_randomreset] & gen_nr);
			config[nc_randomreset] &= gen_nr;
		}
	}
}

void NeuronControl::beg_configure(uint gen_nr, bool poisson, uint period){
	// gen_nr 0-7 are the individual generators, gen_nr=8 means configure all generators
	if (gen_nr > 8) {
		log(Logger::ERROR) << "NeuronControl::beg_off: Illegal generator number";
	} else {
		if (gen_nr == 8){
			log(Logger::DEBUG0) << "NeuronControl::beg_configure all generators";
			log(Logger::DEBUG0) << "\t poisson:" << poisson << ", period:" << period;
			if (poisson){
				write_data(nc_randomreset, config[nc_randomreset] | 0xff00);
				config[nc_randomreset] |= 0xff00;
			}
			else{
				write_data(nc_randomreset, config[nc_randomreset] & 0xff);
				config[nc_randomreset] &= 0xff;
			}
			for (uint i=0; i<8; i++){
				write_data(7+i, period & 0xffff);
				config[7+i] = period & 0xffff;
			}
		}
		else{
			log(Logger::DEBUG0) << "NeuronControl::beg_configure nr" << gen_nr;
			log(Logger::DEBUG0) << "\t poisson:" << poisson << ", period:" << period;
			write_data(7+gen_nr, period & 0xffff);
			config[7+gen_nr] = period & 0xffff;
			if (poisson){
				gen_nr = 1 << (gen_nr + 8);
				write_data(nc_randomreset, config[nc_randomreset] | gen_nr);
				config[nc_randomreset] |= gen_nr;
			}
			else{
				gen_nr = (1 << (gen_nr + 8)) ^ 0xffff;
				write_data(nc_randomreset, config[nc_randomreset] & gen_nr);
				config[nc_randomreset] &= gen_nr;
			}
		}
	}
}

void NeuronControl::beg_set_number(uint gen_nr, uint nnumber){
	log(Logger::DEBUG0) << "NeuronControl::beg_set_number gen:" << gen_nr << " as neuron:" << nnumber;
#if HICANN_V >= 2
	if(nnumber < 64 && gen_nr < 8){ //sanity check
		if (!(gen_nr%2)){ ///even bgens
			config[nc_nnumber+gen_nr/2] &= 0xff00; //clear old value
			config[nc_nnumber+gen_nr/2] |= nnumber; //write new value
			write_data(nc_nnumber+gen_nr/2, config[nc_nnumber+gen_nr/2]);
		}
		else{ ///odd bgens
			config[nc_nnumber+gen_nr/2] &= 0xff; //clear old value
			config[nc_nnumber+gen_nr/2] |= (nnumber << 8); //write new value
			write_data(nc_nnumber+gen_nr/2, config[nc_nnumber+gen_nr/2]);
		}
	}
	else {
		log(Logger::ERROR) << "NeuronControl::beg_set_number: Variable out of range";
	}
#elif HICANN_V == 1
#else
	#error Missing code for this HICANN revision.
#endif
}

void NeuronControl::merger_set(nc_merger merg, bool enable, bool select, bool slow){
	log(Logger::DEBUG0) << "NeuronControl::merger_set merg:" << merger_names[merg] << " [enbl:" << enable << ",sel:" << select << ",slw:" << slow << "]";
#if HICANN_V >= 2
	if (merg > 0x4fff){
		if (enable) {
			config[nc_dncmerger] |= (merg & 0xff);
			write_data(nc_dncmerger, config[nc_dncmerger]);
		}
		else{
			config[nc_dncmerger] &= ((merg & 0xff) ^ 0xffff);
			write_data(nc_dncmerger, config[nc_dncmerger]);
		}
		if (select) {
			config[nc_dncloopb] |= ((merg & 0xff) << 8);
			write_data(nc_dncloopb, config[nc_dncloopb]);
		}
		else{
			config[nc_dncloopb] &= (((merg & 0xff) << 8) ^ 0xffff);
			write_data(nc_dncloopb, config[nc_dncloopb]);
		}
		if (slow) {
			config[nc_dncmerger] |= ((merg & 0xff) << 8);
			write_data(nc_dncmerger, config[nc_dncmerger]);
		}
		else{
			config[nc_dncmerger] &= (((merg & 0xff) << 8) ^ 0xffff);
			write_data(nc_dncmerger, config[nc_dncmerger]);
		}
	}
	else {
		if (enable) {
			config[nc_enable] |= merg;
			write_data(nc_enable, config[nc_enable]);
		}
		else{
			config[nc_enable] &= (merg ^ 0xffff);
			write_data(nc_enable, config[nc_enable]);
		}
		if (select){
			config[nc_select] |= merg;
			write_data(nc_select, config[nc_select]);
		}
		else{
			config[nc_select] &= (merg ^ 0xffff);
			write_data(nc_select, config[nc_select]);
		}
		if (slow){
			config[nc_slow] |= merg;
			write_data(nc_slow, config[nc_slow]);
		}
		else{
			config[nc_slow] &= (merg ^ 0xffff);
			write_data(nc_slow, config[nc_slow]);
		}
	}
#elif HICANN_V == 1
	if (enable) {
		config[nc_enable] |= merg;
		write_data(nc_enable, config[nc_enable]);
	}
	else{
		config[nc_enable] &= (merg ^ 0xffff);
		write_data(nc_enable, config[nc_enable]);
	}
	if (select){
		config[nc_select] |= merg;
		write_data(nc_select, config[nc_select]);
	}
	else{
		config[nc_select] &= (merg ^ 0xffff);
		write_data(nc_select, config[nc_select]);
	}
	if (slow){
		config[nc_slow] |= merg;
		write_data(nc_slow, config[nc_slow]);
	}
	else{
		config[nc_slow] &= (merg ^ 0xffff);
		write_data(nc_slow, config[nc_slow]);
	}
#else
	#error Missing code for this HICANN revision.
#endif
}

void NeuronControl::merger_set(std::vector<nc_merger> merg, std::vector<bool> enable, std::vector<bool> select, std::vector<bool> slow){
	for (unsigned int i=0; i<merg.size(); i++) {
		merger_set(merg[i], enable[i], select[i], slow[i]);
	}
}

void NeuronControl::merger_get(nc_merger merg, bool& enable, bool& select, bool& slow){
	log(Logger::DEBUG0) << "NeuronControl::merger_get merg:" << merg << " [enbl:" << enable << ",sel:" << select << ",slw:" << slow << "]";
	if (config[nc_enable] & merg) {
		enable=true;
	} else {
		enable=false;
	}
	if (config[nc_select] & merg) {
		select=true;
	} else {
		select=false;
	}
	if (config[nc_slow] & merg) {
		slow=true;
	} else {
		slow=false;
	}
}

void NeuronControl::outputphase_set(bool reg0, bool reg1, bool reg2, bool reg3, bool reg4, bool reg5, bool reg6, bool reg7){
	log(Logger::DEBUG0) << "NeuronControl::outputphase_set";
	log(Logger::DEBUG0) << "\t [reg0:" << reg0 << ",reg1:" << reg1 << ",reg2:" << reg2 << ",reg3:" << reg3 << ",reg4:"
			<< reg4 << ",reg5:" << reg5 << ",reg6:" << reg6 << ",reg7:" << reg7 << "]";
	// construct a vector to write into the register
	uint data=reg0+reg1*2+reg2*4+reg3*8+reg4*16+reg5*32+reg6*64+reg7*128;
	write_data(nc_phase, data);
	config[nc_phase] = data;
}

void NeuronControl::dnc_enable_set(bool dnc0, bool dnc1, bool dnc2, bool dnc3, bool dnc4, bool dnc5, bool dnc6, bool dnc7){
	log(Logger::DEBUG0) << "NeuronControl::dnc_enable_set";
	log(Logger::DEBUG0) << "\t [ch0:" << dnc0 << ",ch1:" << dnc1 << ",ch2:" << dnc2 << ",ch3:" << dnc3 << ",ch4:"
			 << dnc4 << ",ch5:" << dnc5 << ",ch6:" << dnc6 << ",ch7:" << dnc7 << "]";
	// construct a vector to write into the register
	uint newcon=(dnc0+dnc1*2+dnc2*4+dnc3*8+dnc4*16+dnc5*32+dnc6*64+dnc7*128) << 8;
	uint data = (config[nc_dncloopb] & 0xff) | newcon;
	write_data(nc_dncloopb, data);
	config[nc_dncloopb] = data;
}

void NeuronControl::loopback_on(uint source, uint end){
	//sanity check: greater channel number must be odd and channels have to be sequent
	log(Logger::DEBUG0) << "NeuronControl::loopback_on [HCin:" << source << "->HCout:" << end <<"]";
	if ((max(source,end)%2) && (max(source,end)-min(source,end) == 1) && (max(source,end) < 8)){
		uint clear = (0x3 << min(source,end)) ^ 0xffff;
		uint newcon = 1 << end;
		uint data = (config[nc_dncloopb] & clear) | newcon;
		write_data(nc_dncloopb, data);
		config[nc_dncloopb] = data;
	}
	else {
		log(Logger::ERROR) << "NeuronControl::loopback_on: cannot enable loopback for given channels";
	}
}

void NeuronControl::loopback_off(uint number){ // input number of an involved channel, number=8 turns off all loopbacks
	if (number < 9){
		if (number == 8) {
			write_data(nc_dncloopb, config[nc_dncloopb] & 0xff00);
			log(Logger::DEBUG0) << "NeuronControl::loopback_off for all channels";
		} else{
			log(Logger::DEBUG0) << "NeuronControl::loopback_off for channel " << number;
			if (number%2) number--;
			uint clear = (0x3 << number) ^ 0xffff;
			write_data(nc_dncloopb, config[nc_dncloopb] & clear);
			config[nc_dncloopb] &= clear;
		}
	}
	else {
		log(Logger::ERROR) << "NeuronControl::loopback_off: channel out of range";
	}
}

void NeuronControl::nc_reset(){
	log(Logger::DEBUG0) << "NeuronControl::nc_reset";
#if HICANN_V>=2
	uint maxreg=20;
#elif HICANN_V==1
	uint maxreg=15;
#else
	#error Missing code for this HICANN revision.
#endif
	for (unsigned int i=0; i<maxreg; i++){
		write_data(i,0);
		config[i]=0;
	}
}

void NeuronControl::print_config(){
	#if HICANN_V>=2
	uint maxreg=20;
	#elif HICANN_V==1
	uint maxreg=15;
	#else
	#error Missing code for this HICANN revision.
	#endif
	for (unsigned int i=0; i<maxreg; i++){
		ci_addr_t addr;
		ci_data_t data;
		read_data(i);
		get_read_data(addr, data);
		cout << " addr " << dec << i << " config in ram: " << hex << config[i] << "\t" << "config on chip: " << data << "\t";
		if (i%2) cout << endl;
	}
	cout << endl << endl;
}
