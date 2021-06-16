// Company         :   kip
// Author          :   Alexander Kononov
// E-Mail          :   akononov@kip.uni-heidelberg.de
//
// Filename        :   repeater_control.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Thu Jan 20 11
// Last Change     :   Thu Jan 21 11
// by              :   akononov
//------------------------------------------------------------------------

#include "common.h" // library includes
#include "s2comm.h"
#include "linklayer.h"
#include "logger.h"
#include "s2ctrl.h"
#include "ctrlmod.h"
#include "hicann_ctrl.h"
#include "repeater_control.h" // this class

using namespace std;
using namespace facets;

RepeaterControl::RepeaterControl(rep_loc cl, Stage2Ctrl* c, uint ta, uint sa, uint ma):CtrlModule(c,ta,sa,ma)
{
	loc = cl;
	outputflag[0] = 65;
	outputflag[1] = 65;
	startconf = 0; //one cannot read these bits so it is assumed at the beginning that they are 0...
	if (loc == REPL || loc == REPR) {
		config.resize(32);
	}
	else {
		config.resize(64);
	}
}

RepeaterControl::~RepeaterControl(){}

void RepeaterControl::configure(){
	mLog(Logger::DEBUG0) << "RepeaterControl::configure";
	for (unsigned int i=0; i<config.size(); i++){
		config[i]=read_data(repaddr(i));
		if (config[i] & rc_touten){
			outputflag[i%2]=i;
		}
	}
	
}

uint RepeaterControl::repaddr(uint addr) {
#if HICANN_V>=2
	if (loc==REPL || loc==REPBL || loc==REPBR) return addr;
	else if (loc==REPTL || loc==REPTR) return ((addr+1) & 0x3f);
	else if (loc==REPR)	return ((32-addr) & 0x1f);
	else return 0xffffffff; // on error location
#elif HICANN_V==1
	uint ra=0;
	for(int i=0;i<7;i++)ra|= (addr& (1<<i))?(1<<(6-i)):0;
	return ra;
#else
	#error Missing code for this HICANN revision.
#endif
}

uint RepeaterControl::read_data(uint commaddr){
	ci_addr_t addr = commaddr; //cell address / command in the repeater block
	read_cmd(addr,0);  //send the read command
	#ifndef WITHOUT_ARQ
	ci_payload tmp;
	if(get_data(&tmp) == 1){
		uint val = tmp.data & 0xff;  //data width is 8 bit
		mLog(Logger::DEBUG1) << "RepeaterControl::read_data: " << hex << tmp;
		return val;
	}
	return 0xffffffff;  //failure
	#endif // WITHOUT_ARQ
}

uint RepeaterControl::write_data(uint commaddr, uint writedata){
	ci_addr_t addr = commaddr; //cell address / command for the repeater block
	ci_data_t data = writedata;
	return write_cmd(addr, data, 0);
}

rep_loc RepeaterControl::get_loc() {
	return loc;
}

bool RepeaterControl::is_spl1rep(uint rep_nr){ // returns true if rep_nr is a sending repeater
	if (loc!=REPL || rep_nr > 28 || rep_nr%4) return false;
	else return true;
}

void RepeaterControl::reset() {  //clear repeater SRAM
	mLog(Logger::DEBUG0) << "RepeaterControl::reset";
	int rmax = 64;
	if (loc == REPL || loc == REPR) rmax = 32;
	for(int r=0; r<rmax; r++){
		write_data(repaddr(r),0);
		config[r]=0;
	}
	// Design bug in FPGA init, rc_drvresetb is set to 0 but not released to 1 after init
	// -> released here via jtag.
	// cf. https://brainscales-r.kip.uni-heidelberg.de/meetings/531#status-of-user-softwareecm
	startconf=rc_drvresetb; //release driver reset, but keep DLL
	write_data(rc_config, startconf);
	outputflag[0] = 65;
	outputflag[1] = 65;
}

void RepeaterControl::reset_dll() {  //reset DLLs before every driver lock
	mLog(Logger::DEBUG0) << "RepeaterControl::reset_dll";
	write_data(rc_config, rc_dllresetb|rc_drvresetb); //reset DLL, driver
    startconf = rc_dllresetb|rc_drvresetb;
}

void RepeaterControl::set_dir(uint rep_nr, bool dir){
	mLog(Logger::DEBUG0) << "RepeaterControl::set_dir nr:" << rep_nr << " dir:" << dir;
	if (dir){
		write_data(repaddr(rep_nr), config[rep_nr] | rc_dir);
		config[rep_nr] |= rc_dir;
	}
	else{
		write_data(repaddr(rep_nr), config[rep_nr] & (rc_dir ^ 0xff));
		config[rep_nr] &= (rc_dir ^ 0xff);
	}
}

void RepeaterControl::set_dir(std::vector<uint> rep_nr, std::vector<bool> dir){
	for (unsigned int i=0; i<rep_nr.size(); i++) set_dir(rep_nr[i], dir[i]);
}

void RepeaterControl::enable(uint rep_nr, bool _switch){
	mLog(Logger::DEBUG0) << "RepeaterControl::enable nr:" << rep_nr << " switch:" << _switch;
	if (_switch){
		write_data(repaddr(rep_nr), config[rep_nr] | rc_recen);
		config[rep_nr] |= rc_recen;
	}
	else{
		write_data(repaddr(rep_nr), config[rep_nr] & (rc_recen ^ 0xff));
		config[rep_nr] &= (rc_recen ^ 0xff);
	}
}

void RepeaterControl::enable(std::vector<uint> rep_nr, std::vector<bool> _switch){
	for (unsigned int i=0; i<rep_nr.size(); i++) enable(rep_nr[i], _switch[i]);
}

void RepeaterControl::tout(uint rep_nr, bool _switch){
/// TODO -- !!! apply V2 changes
	mLog(Logger::DEBUG0) << "RepeaterControl::tout nr:" << rep_nr << " switch:" << _switch;
	if (_switch){ //turn touten on
		if (outputflag[rep_nr%2] == 65 || outputflag[rep_nr%2] == rep_nr){
#if HICANN_V==1
			if ((config[rep_nr] & rc_tinen) && !(is_spl1rep(rep_nr))) {
				tin(rep_nr, false); //in-/output cannot be set at the same time
			}
#endif
			write_data(repaddr(rep_nr), config[rep_nr] | rc_touten);                      //except for sending repeaters
			config[rep_nr] |= rc_touten;
			outputflag[rep_nr%2] = rep_nr;
		}
		else {
			mLog(Logger::ERROR) << "RepeaterControl::tout: Unable to switch touten of repeater " << rep_nr
					<< " on, please switch off touten of repeater nr " << outputflag[rep_nr%2] << " first!";
		}

	}
	else{ //turn touten off
		write_data(repaddr(rep_nr), config[rep_nr] & (rc_touten ^ 0xff));
		config[rep_nr] &= (rc_touten ^ 0xff);
		outputflag[rep_nr%2]=65;
	}
}

void RepeaterControl::tout(std::vector<uint> rep_nr, std::vector<bool> _switch){
	for (unsigned int i=0; i<rep_nr.size(); i++) {
		tout(rep_nr[i], _switch[i]);
	}
}

void RepeaterControl::tin(uint rep_nr, bool _switch){
	mLog(Logger::DEBUG0) << "RepeaterControl::tin nr:" << rep_nr << " switch:" << _switch;
	if (_switch){
#if HICANN_V==1
		if ((config[rep_nr] & rc_tinen) && !(is_spl1rep(rep_nr))) {
			tout(rep_nr, false); //in-/output cannot be set at the same time
		}
#endif
		write_data(repaddr(rep_nr), config[rep_nr] | rc_tinen);                        //except for sending repeaters
		config[rep_nr] |= rc_tinen;
	}
	else{
		write_data(repaddr(rep_nr), config[rep_nr] & (rc_tinen ^ 0xff));
		config[rep_nr] &= (rc_tinen ^ 0xff);
	}
}

void RepeaterControl::tin(std::vector<uint> rep_nr, std::vector<bool> _switch){
	for (unsigned int i=0; i<rep_nr.size(); i++) tin(rep_nr[i], _switch[i]);
}

void RepeaterControl::conf_repeater(uint rep_nr, rep_mode mode, rep_direction dir, bool _switch){
//  TODO -- clean up modes, direction with switch/cases
	///////////////////////////////
	// DEBUG
	///////////////////////////////
	std::stringstream modestring;
	switch (mode) {
		case TESTIN:
			modestring << "TESTIN";
			break;
		case TESTOUT:
			modestring << "TESTOUT";
			break;
		case FORWARD:
			modestring << "FORWARD";
			break;
		default:
			modestring << "invalid.";
			break;
	}
	std::stringstream dirstring;
	switch (dir) {
		case INWARDS:
			dirstring << "INWARDS";
			break;
		case OUTWARDS:
			dirstring << "OUTWARDS";
			break;
		default:
			dirstring << "invalid.";
			break;
	}

	mLog(Logger::DEBUG0) << "RepeaterControl::conf_repeater nr:" << rep_nr << " mode:" << modestring.str() << " dir:" << dirstring.str() << " sw:" << _switch;

	///////////////////////////////
	// FUNCTION
	///////////////////////////////

	if (mode==TESTIN) {
		tout(rep_nr, _switch);      //set test output to receive packets
	}
	else if (mode==TESTOUT){
		if (is_spl1rep(rep_nr)) {
			mLog(Logger::ERROR) << "RepeaterControl::conf_repeater: Sending repeaters cannot connect to the test output circuit";
		}
		tin(rep_nr, _switch); //set test input to send data
	}
	else if (mode==FORWARD){
		tout(rep_nr, false);
		tin(rep_nr, false);
	}
	else {
		mLog(Logger::ERROR) << "RepeaterControl::conf_repeater: Invalid repeater mode";
	}
#if HICANN_V>=2
	if (dir==OUTWARDS){
		set_dir(rep_nr, false);   //deactivate internal driver
		enable(rep_nr, _switch);  //switch external driver
	}
	else{
		enable(rep_nr, false);    //deactivate external driver
		set_dir(rep_nr, _switch); //switch internal driver
	}
#elif HICANN_V==1
	if (dir==OUTWARDS){
		if(_switch)	{
			set_dir(rep_nr, false); //direction out of the chip (input comes from within)
		}
	}
	else{
		if(_switch) {
			set_dir(rep_nr, true);  //direction into the chip (input comes from outside)
		}
	}
	enable(rep_nr, _switch);   //enable the receiver
#else
	#error Missing code for this HICANN revision.
#endif
}

void RepeaterControl::conf_spl1_repeater(uint rep_nr, rep_mode mode, rep_direction dir, bool _switch){
//  TODO -- clean up modes, direction with switch/cases
	///////////////////////////////
	// DEBUG
	///////////////////////////////
	std::stringstream modestring;
	switch (mode) {
		case TESTIN:
			modestring << "TESTIN";
			break;
		case SPL1OUT:
			modestring << "SPL1OUT";
			break;
		case FORWARD:
			modestring << "FORWARD";
			break;
		default:
			modestring << "undef.";
			break;
	}
	std::stringstream dirstring;
	switch (dir) {
		case INWARDS:
			dirstring << "INWARDS";
			break;
		case OUTWARDS:
			dirstring << "OUTWARDS";
			break;
		default:
			dirstring << "undef.";
			break;
	}

	mLog(Logger::DEBUG0) << "RepeaterControl::conf_spl1_repeater nr:" << rep_nr << " mode:" << modestring.str() << " dir:" << dirstring.str() << " sw:" << _switch;

	///////////////////////////////
	// FUNCTION
	///////////////////////////////
	if (!is_spl1rep(rep_nr)) {
		mLog(Logger::ERROR) << "RepeaterControl::conf_spl1_repeater: Not a sending repeater";
	}

	if (mode==TESTIN) {
		tout(rep_nr, _switch);      //set test output to receive packets
	}
	else if (mode==SPL1OUT){
		tout(rep_nr, false);   //disable test output
			tin(rep_nr, _switch);     //switch spl1 output from neuroncontrol
	}
	else if (mode==FORWARD){
		tout(rep_nr, false);
		tin(rep_nr, false);
	}
	else {
		mLog(Logger::ERROR) << "RepeaterControl::conf_spl1_repeater: Invalid repeater mode";
	}

	if (dir==INWARDS){
		set_dir(rep_nr, _switch); //switch internal driver
		enable(rep_nr, false); //disable external driver
	}
	else{
		enable(rep_nr, _switch);   //switch external driver
		set_dir(rep_nr, false);    //disable internal driver
	}

}

void RepeaterControl::tdata_write(uint channel, uint entry, uint neuron_nr, uint time){

	if (entry < 3 && channel < 2){
#if HICANN_V >= 2
		if (loc==REPBL || loc==REPBR) {
			channel=!(channel & 1); // bug: channels are swapped in lower repeaters
		}
#endif
		uint hb, lb;
		lb = ((time >> 2) & 0xff);
		hb = (((time & 0x3) << 6) | (neuron_nr & 0x3f));
		if (channel%2){
			write_data(rc_td1+2*entry, hb);
			write_data(rc_td1+2*entry+1, lb);
		}
		else {
			write_data(rc_td0+2*entry, hb);
			write_data(rc_td0+2*entry+1, lb);
		}
		mLog(Logger::DEBUG1) << "RepeaterControl::tdata_write channel:" << channel << " entry:" << entry
					<< " neuron:" << neuron_nr << " time:" << time;
	} else {
		mLog(Logger::ERROR) << "RepeaterControl::tdata_write: Illegal entry or channel number";
	}
}

void RepeaterControl::tdata_read(uint channel, uint entry, uint& neuron_nr, uint& time){

	if (entry < 3 && channel < 2){
#if HICANN_V >= 2
		if (loc==REPBL || loc==REPBR) {
			channel=!(channel & 1); // bug: channels are swapped in lower repeaters
		}
#endif
		uint hb, lb;
		if (channel%2){
			hb = read_data(rc_td1+2*entry);
			lb = read_data(rc_td1+2*entry+1);
		}
		else {
			hb = read_data(rc_td0+2*entry);
			lb = read_data(rc_td0+2*entry+1);
		}
#if HICANN_V == 1
		uint nn=0, n = hb & 0x3f; //temporary variables
		for(uint i=0; i<6; i++){  //flip the bits in neuron number (HICANN bug!)
			nn <<= 1;
			if(n & (1 << i)) nn |= 1;
		}
		neuron_nr = nn;
#else
		neuron_nr = hb & 0x3f;
#endif
		time = gtob((lb << 2) | ((hb & 0xc0) >> 6));

		mLog(Logger::DEBUG1) << "RepeaterControl::tdata_read channel:" << channel << " entry:" << entry
					<< " neuron:" << neuron_nr << " time:" << time;
	}
	else {
		mLog(Logger::ERROR) << "RepeaterControl::tdata_read: Illegal entry or channel number";
	}
}

void RepeaterControl::startin(uint channel){

	if (channel > 1) {
		mLog(Logger::ERROR) << "RepeaterControl::startin: Illegal channel number";
	}
	else {
#if HICANN_V >= 2
		if (loc==REPBL || loc==REPBR) {
			channel=!(channel & 1); // bug: channels are swapped in lower repeaters
		}
#endif
		mLog(Logger::DEBUG1) << "RepeaterControl::startin channel:" << channel;
		if (channel){
			write_data(rc_config, rc_start_tdi1|startconf);
			startconf |= rc_start_tdi1;
		}
		else {
			write_data(rc_config, rc_start_tdi0|startconf);
			startconf |= rc_start_tdi0;
		}
	}
}

void RepeaterControl::stopin(uint channel){

	if (channel > 1) {
		mLog(Logger::ERROR) << "RepeaterControl::stopin: Illegal channel number";
	}
	else {
#if HICANN_V >= 2
		if (loc==REPBL || loc==REPBR) {
			channel=!(channel & 1); // bug: channels are swapped in lower repeaters
		}
#endif
		mLog(Logger::DEBUG1) << "RepeaterControl::stopin channel:" << channel;
		if (channel){
			write_data(rc_config, startconf & (rc_start_tdi1 ^ 0xff));
			startconf &= (rc_start_tdi1 ^ 0xff);
		}
		else {
			write_data(rc_config, startconf & (rc_start_tdi0 ^ 0xff));
			startconf &= (rc_start_tdi0 ^ 0xff);
		}
	}
}

void RepeaterControl::startout(uint channel){
	if (channel > 1) {
		mLog(Logger::ERROR) << "RepeaterControl::startout: Illegal channel number";
	}
	else {
#if HICANN_V >= 2
		if (loc==REPBL || loc==REPBR) {
			channel=!(channel & 1); // bug: channels are swapped in lower repeaters
		}
#endif
		mLog(Logger::DEBUG1) << "RepeaterControl::startout channel:" << channel;
		if (channel){
			write_data(rc_config, rc_start_tdo1|startconf);
			startconf |= rc_start_tdo1;
		}
		else {
			write_data(rc_config, rc_start_tdo0|startconf);
			startconf |= rc_start_tdo0;
		}
	}
}
void RepeaterControl::stopout(uint channel){
	if (channel > 1) {
		mLog(Logger::ERROR) << "RepeaterControl::stopout: Illegal channel number";
	}
	else {
#if HICANN_V >= 2
		if (loc==REPBL || loc==REPBR) {
			channel=!(channel & 1); // bug: channels are swapped in lower repeaters
		}
#endif
		mLog(Logger::DEBUG1) << "RepeaterControl::stopout channel:" << channel;
		if (channel){
			write_data(rc_config, startconf & (rc_start_tdo1 ^ 0xff));
			startconf &= (rc_start_tdo1 ^ 0xff);
		}
		else {
			write_data(rc_config, startconf & (rc_start_tdo0 ^ 0xff));
			startconf &= (rc_start_tdo0 ^ 0xff);
		}
	}
}

bool RepeaterControl::fullflag(uint channel){
	if (channel > 1) {
		mLog(Logger::ERROR) << "RepeaterControl::fullflag: Illegal channel number";
		return false;
	}
#if HICANN_V >= 2
	if (loc==REPBL || loc==REPBR) {
		channel=!(channel & 1); // bug: channels are swapped in lower repeaters
	}
#endif
	mLog(Logger::DEBUG1) << "RepeaterControl::fullflag channel:" << channel;
	uint data = read_data(rc_status) & 0x3;
	if (data & (1 << channel)) return true;
	else return false;
}

void RepeaterControl::set_sram_timings(uint read_delay, uint setup_precharge, uint write_delay){
	if (read_delay >= (1<<8)) throw runtime_error("read_delay parameter too large (size is 8 bit in hardware). Fail.");
	if (setup_precharge >= (1<<4)) throw runtime_error("setup_precharge parameter too large (size is 4 bit in hardware). Fail.");
	if (write_delay >= (1<<4)) throw runtime_error("write_delay parameter too large (size is 4 bit in hardware). Fail.");

	write_data((1<<addrmsb_repctrl) + (1<<cnfgadr_repctrl)     , (read_delay&0xff));
	write_data((1<<addrmsb_repctrl) + (1<<cnfgadr_repctrl) + 1 , (((setup_precharge&0xf)<<4) + (write_delay&0xf)));
}

void RepeaterControl::get_sram_timings(uint& read_delay, uint& setup_precharge, uint& write_delay){
	uint addr0 = (1<<addrmsb_repctrl) + (1<<cnfgadr_repctrl);
	uint addr1 = (1<<addrmsb_repctrl) + (1<<cnfgadr_repctrl) + 1;

	uint rd_data = read_data(addr0);
	read_delay = rd_data&0xff;

	rd_data = read_data(addr1);
	setup_precharge = (rd_data>>4)&0xf;
	write_delay     = rd_data&0xf;
}

uint RepeaterControl::gtob(uint time) {
	uint b, t=time&0x3ff;
	b=t&0x200;
	for(int i=8;i>=0;i--){b |= (t ^ (b>>1) )&(1<<i);}
	return b;
}

void RepeaterControl::print_config(){
	cout << "WARNING! The repeater addresses below are PHYSICAL addresses!" << endl;
	for (unsigned int i=0; i<config.size(); i++){
		cout << " addr " << dec << i << " config in ram: " << hex << config[i] << "\t" << "config on chip: " << read_data(i) << "\t";
		if (i%2) cout << endl;
	}
	cout << dec << endl << endl;
}
