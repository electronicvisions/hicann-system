// Company         :   kip
// Author          :   Sebastian Millner
// E-Mail          :   smillner@kip.uni-heidelberg.de
//
// Filename        :   fg_control.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Dez 2009
// Last Change     :   Jan 2009
// by              :   smillner
//------------------------------------------------------------------------
#include "common.h" // library includes
#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "ctrlmod.h"
#include "hicann_ctrl.h"
#include "fg_control.h" // fg control class
using namespace std;
using namespace facets;
using namespace FG_pkg;
//extern Debug dbg;
// *** BEGIN class FGControl ***
const int FGControl::TAG             = 0;
const int FGControl::REG_BASE        = 256;  // bit 8 set
const int FGControl::REG_OP          = REG_BASE+2;
const int FGControl::REG_ADDRINS     = REG_BASE+1;
const int FGControl::REG_SLAVE       = REG_BASE+4;
const int FGControl::REG_BIAS        = REG_BASE+8;
const int FGControl::REG_OP_WIDTH    = 32;   // pos 00010
const int FGControl::REG_BIAS_WIDTH  = 14;   // pos 01000
const int FGControl::REG_SLAVE_BUSY  = 256;  // bit 8
const int FGControl::REG_TEST_NUM    = 10;
const int FGControl::MEM_SIZE        = FG_pkg::fg_lines/2 + FG_pkg::fg_lines % 2;
const int FGControl::MEM_WIDTH       = 20;
const int FGControl::MEM0_BASE       = 0;
const int FGControl::MEM1_BASE       = 128;  // bit 7 set

FGControl::FGControl(fg_loc fgl, Stage2Ctrl *c, uint ta, uint sa, uint ma)
    : CtrlModule(c, ta, sa, ma), log(Logger::instance("hicann-system.FGControl", Logger::INFO, ""))
{
	loc = fgl;
	maxcycle         = FG_pkg::fg_maxcycle;
	currentwritetime = FG_pkg::fg_currentwritetime;
	voltagewritetime = FG_pkg::fg_voltagewritetime;
	readtime         = FG_pkg::fg_readtime;
	acceleratorstep  = FG_pkg::fg_acceleratorstep;
	lines            = FG_pkg::fg_lines;
	pulselength      = FG_pkg::fg_pulselength;
	fg_biasn         = FG_pkg::fg_biasn;
	fg_bias          = FG_pkg::fg_bias;
	groundVM         = 0;
	calib            = 0;
}

FGControl::~FGControl(){}

int FGControl::write_data(uint rowaddr, vector<bool> cfg){
	ci_addr_t addr = rowaddr; // row address of addressed crossbar
	ci_data_t data = 0;
	for(uint l=0;l<cfg.size();l++) data += cfg[l] << l;  // shift config bits into data payload
	return write_cmd(addr, data, del);        // ...send
}

int FGControl::write_data(uint rowaddr, uint cfg){
	ci_addr_t addr = rowaddr; // row address of addressed crossbar
	ci_data_t data = cfg;
	log(Logger::DEBUG2) << "FGControl::Addr = " << addr;
	return write_cmd(addr, data, del);
}

int FGControl::read_data(uint rowaddr){
	ci_addr_t addr = rowaddr; // row address of addressed crossbar
	return read_cmd(addr, del);         // ...send
}

void FGControl::get_read_data_ram(ci_addr_t& rowaddr, ci_data_t& data){
#ifndef WITHOUT_ARQ
	// fg_loc loc_int;
	ci_payload tmp;
	if(get_data(&tmp) == 1){
		log(Logger::DEBUG2) << "FGControl::get_read_data: " << tmp;
		rowaddr = tmp.addr & 0x1ff;
		uint val1 = tmp.data % 1024;
		uint val2 = tmp.data /1024;
		data = tmp.data;
#if HICANN_V >= 2
		data =(val2<<(MEM_WIDTH/2))+val1;
#elif HICANN_V == 1
		data =(revert10bits(val2)<<(MEM_WIDTH/2))+revert10bits(val1);
#else
	#error Missing code for this HICANN revision.
#endif
	}
	else {
		log(Logger::ERROR) << "FGControl::get_read_data: fail...";
	}
#endif // WITHOUT_ARQ

}

void FGControl::get_read_data(ci_addr_t& rowaddr, ci_data_t& data){
#ifndef WITHOUT_ARQ
	// fg_loc loc_int;
	ci_payload tmp;
	if(get_data(&tmp) == 1){
		log(Logger::DEBUG2) << "FGControl::get_read_data: " << tmp;
		rowaddr = tmp.addr & 0x1ff;
		data = tmp.data;
	}
	else {
		log(Logger::ERROR) << "FGControl::get_read_data: fail...";
	}
#endif // WITHOUT_ARQ
}

#if HICANN_V == 1
uint FGControl::revert10bits(uint val){
	uint result=0;
	for(int i=0; i<10; i++) result |= (val & (1<<i))?(1<<(9-i)):0;
	return result;
}
#endif

int FGControl::write_ram(uint bank, uint addr, uint val1, uint val2){//write data to ram. As two values are written simultaneously, two values must be given.
	uint val;
#if HICANN_V >= 2
	val = (val2<<(MEM_WIDTH/2))+val1;
#elif HICANN_V == 1
	val = (revert10bits(val2)<<(MEM_WIDTH/2))+revert10bits(val1); //revert only needs to be done in first HICANN version
#else
	#error Missing code for this HICANN revision.
#endif
	log(Logger::DEBUG2) << "FGControl::write_ram: Value to be written to ram:" << val << " at address: " << addr;
	return write_data(addr+bank*128, val);
}

int FGControl::initzeros(uint bank){
	for (int i = 0; i<MEM_SIZE; i++) {
		write_ram(bank,i,0,0);
	}
	log(Logger::DEBUG0) << "FGControl::initzeros bank " << bank;
	return 1;
}

int FGControl::initmax(uint bank){
	for (int i = 0; i<MEM_SIZE; i++) {
		write_ram(bank,i,1023,1023);
	}
	log(Logger::DEBUG0) << "FGControl::initmax bank " << bank;
	return 1;
}

int FGControl::initval(uint bank, uint val){
	for (int i = 0; i<MEM_SIZE; i++) {
		write_ram(bank,i,val,val);
	}
	log(Logger::DEBUG0) << "FGControl::initval "<< val << " bank " << bank;
	return 1;
}

int FGControl::initSquare(uint bank, uint val){
	log(Logger::DEBUG0) << " FGControl::initSquare bank " << bank;
	std::stringstream stringstream;
	for (int i = 0; i<MEM_SIZE/2; i++){
		write_ram(bank,i,val,val);
		stringstream << i << "bank" << bank << " [" << val << "," << val << "] ";
	}
	for (int i = MEM_SIZE/2; i<MEM_SIZE; i++){
		write_ram(bank,i,0,0);
		stringstream << i << "bank" << bank << " [0,0] ";
	}
	log(Logger::DEBUG0) << stringstream.str() << Logger::flush;
	return 1;
}

int FGControl::initRamp(uint bank, uint val){
	log(Logger::DEBUG0) << " FGControl::initSquare bank " << bank;
	std::stringstream stringstream;

	// Initial ramp value
	int ramp_value = 0;

	// Create current ramp
	for (int i = 0; i<MEM_SIZE; i++){

		// Calculate new ramp value
		ramp_value = (int)(i*val/MEM_SIZE);

		write_ram(bank,i,ramp_value,ramp_value);
		stringstream << i << "bank" << bank << " [" << ramp_value << "," << ramp_value << "] ";
	}
	log(Logger::DEBUG0) << stringstream.str() << Logger::flush;
	return 1;
}

int FGControl::initval1(uint bank, uint val1,uint val2){
	write_ram(bank,0,val1,val2);
	log(Logger::DEBUG0) << "FGControl::initval1 bank " << bank << " [" << val1 << "," << val2 << "] ";
#if HICANN_V >= 2
	for (int i = 1; i<MEM_SIZE; i++) {
		write_ram(bank,i,val2,val2);
	}
#elif HICANN_V == 1
	for (int i = 1; i<MEM_SIZE-1; i++) {
		write_ram(bank,i,val2,val2);
	}
	write_ram(bank,MEM_SIZE-1,val1,val2);
#else
	#error Missing code for this HICANN revision.
#endif
	return 1;
}

int FGControl::initArray(uint bank, std::vector<int> val){
	log(Logger::DEBUG0)<<"FGControl::initArray ";
	std::stringstream stringstream;
	for (int i = 0; i < MEM_SIZE-1; i += 1){
		write_ram(bank,i,val[2*i],val[2*i+1]);
		stringstream << i << " bank " << bank <<  " [" << val[2*i] << "," << val[2*i+1] << "] " ;
#if HICANN_V >= 2
		if(i==MEM_SIZE-2) {
			write_ram(bank, i+1, val[2*i+2], 0);  // very last cell (130, 129th in array) does not exist...
			stringstream << "correcting "<< i << " bank " << bank <<  " [" << val[2*i] << "," << val[2*i] << "] ";
		}
#elif HICANN_V == 1
		if(i==MEM_SIZE-2) {
			write_ram(bank, i, val[0], val[1]);  //this corrects a bug in Floating gate controller - last and first cell must be equal!!!!!
			stringstream << "correcting "<< i << " bank " << bank <<  " [" << val[0] << "," << val[1] << "] ";
		}
#else
		#error Missing code for this HICANN revision.
#endif
	}
	log(Logger::DEBUG0) << stringstream.str() << Logger::flush;
	return 1;
}

int FGControl::initArray(uint bank, std::vector<int> val, int neuron){
	write_ram(bank,0,val[0],val[1]);// line zero must be initialized for global stuff.
	if(neuron == 0) {
		return 1; // done as neuron 0 is already written.
	}
	neuron += 1; //address offset between fg_array doe to global values
	write_ram(bank,neuron/2,val[neuron],val[neuron+1]); //always programming two neurons as doesn't make a difference in write time.
	log(Logger::DEBUG0) << "FGControl::initArray "  << "neuron/2" << neuron << " bank " << " [" << val[neuron] << "," << val[neuron+1] << "]";
#if HICANN_V == 1
	write_ram(bank,MEM_SIZE-1,val[0],val[1]); //this corrects a bug in Floating gate controler - last and first cell must be equal!!!!!
#endif
	return 1;
}

int FGControl::read_ram(uint bank, uint addr){
	return read_data(addr+bank*128);
}

int FGControl::write_biasingreg(){
	uint data = 0;
	//buildup data vector
	data += calib << (2*FG_pkg::fg_bias_dw+FG_pkg::fg_pulselength_dw+1);
	data += groundVM << (2*FG_pkg::fg_bias_dw+FG_pkg::fg_pulselength_dw);
	data += pulselength << 2*FG_pkg::fg_bias_dw;
	data += fg_biasn << FG_pkg::fg_bias_dw;
	data += fg_bias;

	std::stringstream logstring;
	log(Logger::DEBUG0) << "FGControl::write biasingreg...";
	logstring << "\t calib: " << calib;
	logstring << ", groundVM: " << groundVM;
	logstring << ", pulselength: " << pulselength;
	logstring << ", fg_biasn: " << fg_biasn;
	logstring << ", fg_bias: " << fg_bias;
	log(Logger::DEBUG0) << logstring.str() << Logger::flush;
	return write_data(REG_BIAS, data);
}

int FGControl::write_operationreg(){
	uint data = 0;
	//buildup data vector
	data += currentwritetime << 26;
	data += voltagewritetime << 20;
	data += acceleratorstep << 14;
	data += readtime << 8;
	data += maxcycle;

	std::stringstream logstring;
	log(Logger::DEBUG0) << "FGControl::write operationreg...";
	logstring << "\t currentwritetime: " << currentwritetime;
	logstring << ", voltagewritetime: " << voltagewritetime;
	logstring << ", acceleratorstep: " << acceleratorstep;
	logstring << ", readtime: " << readtime;
	logstring << ", maxcycle: " << maxcycle;
	log(Logger::DEBUG0) << logstring.str() << Logger::flush;

	return write_data(REG_OP, data);
}

int FGControl::write_biasingreg(uint val){
	return write_data(REG_BIAS, val);
}

int FGControl::write_operationreg(uint val){
	return write_data(REG_OP, val);
}

int FGControl::write_instructionreg(uint collumn, uint line, uint bank, FG_pkg::ControlInstruction instruction){
	uint data = 0;
	//buildup data vector
	data += instruction<< 14;
	data += bank <<13;
	data += line <<5;
	data += collumn;
	return write_data(REG_ADDRINS, data);
}

int FGControl::program_cells(uint collumn, uint bank, uint dir){
	if (dir > 0)
		return write_instructionreg(collumn, 0, bank, FG_pkg::writeUp);
	else
		return write_instructionreg(collumn, 0, bank, FG_pkg::writeDown);
}

int FGControl::readout_cell(uint collumn, uint line){
	return write_instructionreg(collumn, line, 0, FG_pkg::read);
}

int FGControl::get_next_false(){
	write_instructionreg(0,0,0, FG_pkg::getNextFalse);
	read_data(REG_SLAVE);
	ci_data_t value;
	ci_addr_t addr;
	get_read_data(addr, value);
	return value & 0x0ff;
}

int FGControl::stimulateNeurons(int bank){
	log(Logger::DEBUG0) << "FGControl::stimulateNeurons set bank " << bank;
	return write_instructionreg(0,0,bank, FG_pkg::stimulateNeurons);
}

int FGControl::stimulateNeuronsContinuous(int bank){
	log(Logger::DEBUG0) << "FGControl::stimulateNeuronsContinuous set bank " << bank;
	return write_instructionreg(0,0,bank, FG_pkg::stimulateNeuronsContinuous);
}

bool FGControl::isError(){
	read_data(REG_SLAVE);
	ci_data_t value;
	ci_addr_t addr;
	get_read_data(addr, value);
	return value & 0x200;
}

bool FGControl::isBusy(){
	read_data(REG_SLAVE);
	ci_data_t value;
	ci_addr_t addr;
	get_read_data(addr, value);
	return value & 0x100;
}

uint  FGControl::get_regslave(){
	read_data(REG_SLAVE);
	ci_data_t value;
	ci_addr_t addr;
	get_read_data(addr, value);
	return value;
}

int FGControl::programLine(uint row, uint val0, uint val1){
	//write all cells down to zero first
	//
	while(isBusy());                  //TODO this has to be done in a different way
	initval1(0,val0,val1);
	//programm down
//	programm_cells(row,0,0);
//	while(isBusy());
	//program up
	program_cells(row,0,1);
/*	while(isBusy());
	program_cells(row,0,1);
	while(isBusy());
	program_cells(row,0,1);*/
	return 1;

}

uint FGControl::get_maxcycle(){
	return maxcycle;
}

void FGControl::set_maxcycle(uint data){
	maxcycle = data;
}

uint FGControl::get_currentwritetime(){
	return currentwritetime;
}

void FGControl::set_currentwritetime(uint data){
	currentwritetime = data;
}

uint FGControl::get_voltagewritetime(){
	return voltagewritetime;
}

void FGControl::set_voltagewritetime(uint data){
	voltagewritetime = data;
}

uint FGControl::get_readtime(){
	return readtime;
}

void FGControl::set_readtime(uint data){
	readtime = data;
}

uint FGControl::get_acceleratorstep(){
	return acceleratorstep;
}

void FGControl::set_acceleratorstep(uint data){
	acceleratorstep = data;
}

uint FGControl::get_pulselength(){
	return pulselength;
}

void FGControl::set_pulselength(uint data){
	pulselength = data;
}

uint FGControl::get_fg_biasn(){
#if HICANN_V >= 2
	return fg_biasn;
#elif HICANN_V == 1
	// bit order is reverse!
	uint result = (fg_biasn & 8)/8 * 1
				+ (fg_biasn & 4)/4 * 2
				+ (fg_biasn & 2)/2 * 4
				+ (fg_biasn & 1)   * 8;
	return result;
#else
	#error Missing code for this HICANN revision.
#endif
}

void FGControl::set_fg_biasn(uint data){
#if HICANN_V >= 2
	//here, the bit order is reverse IN HARDWARE (while reading)
	fg_biasn = data;
#elif HICANN_V == 1
	// bit order is reverse!
	uint result = (data & 8)/8 * 1
				+ (data & 4)/4 * 2
				+ (data & 2)/2 * 4
				+ (data & 1)   * 8;
	fg_biasn = result;
#else
	#error Missing code for this HICANN revision.
#endif
}

uint FGControl::get_fg_bias(){
#if HICANN_V >= 2
	return fg_bias;
#elif HICANN_V == 1
	// bit order is reverse!
	uint result = (fg_bias & 8)/8 * 1
				+ (fg_bias & 4)/4 * 2
				+ (fg_bias & 2)/2 * 4
				+ (fg_bias & 1)   * 8;
	return result;
#else
	#error Missing code for this HICANN revision.
#endif
}

void FGControl::set_fg_bias(uint data){
#if HICANN_V >= 2
	//here, the bit order is reverse IN HARDWARE (while reading)
	fg_bias = data;
#elif HICANN_V == 1
	// bit order is reverse!
	uint result = (data & 8)/8 * 1
				+ (data & 4)/4 * 2
				+ (data & 2)/2 * 4
				+ (data & 1)   * 8;
	fg_bias = result;
#else
	#error Missing code for this HICANN revision.
#endif
}

uint FGControl::get_calib(){
	return calib;
}

void FGControl::set_calib(uint data){
	calib = data;
}

void FGControl::reset(){
	for (int i = 0; i < 512; i++) write_data(i, 0);
}

void FGControl::print_config(){
	ci_addr_t addr;
	ci_data_t data;
	cout << "FG Controller " << loc << " contains:" << endl;
	for (int i = 0; i < 65; i++){ //RAM bank 0
		read_data(i);
		get_read_data(addr, data);
		cout << "addr " << setw(3) << addr << ": " << hex << setw(5) << data << "  |  " << dec;
		if (i%6 == 5) cout << endl;
	}
	cout << endl;
	for (int i = 128; i < 193; i++){ //RAM bank 1
		read_data(i);
		get_read_data(addr, data);
		cout << "addr " << setw(3) << addr << ": " << hex << setw(5) << data << "  |  " << dec;
		if (i%6 == 1) cout << endl;
	}
	cout << endl;
	read_data(REG_BIAS);
	get_read_data(addr, data);
	cout << "Bias register:        " << hex << setw(8) << data << dec << endl;
	read_data(REG_OP);
	get_read_data(addr, data);
	cout << "Operation register:   " << hex << setw(8) << data << dec << endl;
	read_data(REG_ADDRINS);
	get_read_data(addr, data);
	cout << "Instruction register: " << hex << setw(8) << data << dec << endl;
	cout << endl;
}

// *** END class FGControl END ***
