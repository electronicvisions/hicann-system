// Company         :   kip
// Author          :   Andreas Gruebl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//
// Filename        :   l1switch_control.h
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Tue Jun 24 08
// Last Change     :   Mon Aug 04 08
// by              :   agruebl
//------------------------------------------------------------------------


#include "common.h" // library includes
#include "s2comm.h"
#include "linklayer.h"

#include "s2ctrl.h"
#include "ctrlmod.h"
#include "hicann_ctrl.h"
#include "l1switch_control.h" // Layer 1 switch control class

using namespace std;

//extern Debug dbg;

// *** BEGIN class L1SwitchControl ***

namespace facets {

L1SwitchControl::L1SwitchControl(switch_loc sl, Stage2Ctrl* c, uint ta, uint sa, uint ma):CtrlModule(c,ta,sa,ma) {
	loc = sl;
	if (loc == CBL || loc == CBR) config.resize(64);
	else config.resize(112);
}

L1SwitchControl::~L1SwitchControl(){}


int L1SwitchControl::write_cfg(ci_addr_t rowaddr, vector<bool> cfg){
	ci_data_t data = 0;
	for(uint l=0;l<cfg.size();l++) data += cfg[l] << l;  // shift config bits into data payload

	return write_cmd(rowaddr, data, del);        // ...send
}

int L1SwitchControl::write_cfg(ci_addr_t rowaddr, uint cfg){
	ci_addr_t addr = rowaddr; // row address of addressed crossbar
//	addr += get_offs(loc);

	ci_data_t data = cfg;
	return write_cmd(addr, data, del);
}

int L1SwitchControl::read_cfg(ci_addr_t rowaddr){
	return read_cmd(rowaddr, del);         // ...send
}

void L1SwitchControl::get_read_cfg(ci_addr_t& rowaddr, vector<bool>& cfg)
{
	#ifndef WITHOUT_ARQ

	ci_payload tmp;
	if(get_data(&tmp) == 1){
		log(Logger::DEBUG1) << "L1SwitchControl::get_read_cfg: " << tmp;

		switch_loc rloc = get_loc(tmp.addr);
		
		if(rloc != loc) {
			log(Logger::ERROR) << "L1SwitchControl::get_read_cfg: wrong switch location address in received data!";
			log(Logger::ERROR) << "L1SwitchControl::get_read_cfg: read data: " << hex << tmp;
		}

		rowaddr = tmp.addr & 0x1ff;

		// retrieve bit mask
		uint numbits = (loc==CBL || loc==CBR) ? CB_NUMH/2 : SYN_NUMH; // copied from switchctrl.sv
		uint cd = tmp.data;
		cfg.clear();
		for(uint l=0;l<numbits;l++) {cfg.push_back(cd & 0x1); cd >>= 1;}
		cout << endl;

	}else{
		log(Logger::ERROR) << "L1SwitchControl::get_read_cfg: fail...";
	}

	#endif // WITHOUT_ARQ

}

void L1SwitchControl::get_read_cfg(ci_addr_t& rowaddr, uint& cfg)
{
	#ifndef WITHOUT_ARQ

	ci_payload tmp;
	if(get_data(&tmp) == 1){
		log(Logger::DEBUG1) << "L1SwitchControl::get_read_cfg: " << tmp;

		switch_loc rloc = get_loc(tmp.addr);

		if(rloc != loc) {
			log(Logger::ERROR) << "L1SwitchControl::get_read_cfg: wrong switch location address in received data!";
			log(Logger::ERROR) << "L1SwitchControl::get_read_cfg: read data: " << hex << tmp;
		}

		rowaddr = tmp.addr & 0x1ff;

		cfg = tmp.data;

	}else{
		log(Logger::ERROR) << "L1SwitchControl::get_read_cfg: fail...";
	}

	#endif // WITHOUT_ARQ

}

uint L1SwitchControl::get_offs(switch_loc loc){
	if(loc == SYNTR ) return OcpSyn_tr.startaddr;
	if(loc == SYNTL ) return OcpSyn_tl.startaddr;
	if(loc == SYNBR ) return OcpSyn_br.startaddr;
	if(loc == SYNBL ) return OcpSyn_bl.startaddr;
	if(loc == CBR ) return OcpCb_r.startaddr;
	if(loc == CBL ) return OcpCb_l.startaddr;
	log(Logger::ERROR) << "L1SwitchControl::get_offs: illegal location specified!!!";
	return 0xffffffff;
}

switch_loc L1SwitchControl::get_loc(uint addr){
	uint offs = addr & 0xfffffe00; // minimum is 9 valid address bits for crossbar config: 0x1ff!
	if(offs == OcpSyn_tr.startaddr) return SYNTR;
	if(offs == OcpSyn_tl.startaddr) return SYNTL;
	if(offs == OcpSyn_br.startaddr) return SYNBR;
	if(offs == OcpSyn_bl.startaddr) return SYNBL;
	if(offs == OcpCb_r.startaddr) return CBR;
	if(offs == OcpCb_l.startaddr) return CBL;
	log(Logger::ERROR) << "L1SwitchControl::get_loc: illegal address specified!!!";
	log(Logger::ERROR) << "L1SwitchControl::get_loc: addr: 0x" << hex << addr;
	return CBL;
}
	
uint L1SwitchControl::bitrotate(uint addr) {
	uint ra=0;
	for(int i=0;i<16;i++)ra|= (addr& (1<<i))?(1<<(15-i)):0;
	return ra;
}

void L1SwitchControl::configure(){
	log(Logger::DEBUG0) <<  "L1SwitchControl::configure";
	for (unsigned int i=0; i<config.size(); i++){
		ci_addr_t addr;
		read_cfg(i);
		get_read_cfg(addr, config[i]);
	}
}

void L1SwitchControl::reset(){
	log(Logger::DEBUG0) <<  "L1SwitchControl::reset";
	for (unsigned int i=0; i<config.size(); i++){
		write_cfg(i, 0);
		config[i]=0;
	}
}

uint L1SwitchControl::transline(uint hor_line, uint ver_line){
	int z;
	log(Logger::DEBUG0) <<  "L1SwitchControl::transline h: " << hor_line << " -> v: " << ver_line;

#if HICANN_V >= 2

	if (loc == CBL){
		//evaluates if (h/2-v)/32 is an integer - according to crossbar documentation, and if the given lines are in necessary range
		if (((ver_line - (hor_line/2))%32 == 0) && (hor_line < 64) && (ver_line < 128)){
			z=(ver_line - (hor_line/2))/32;     // /2 due to repeating pattern in a crossbar every 2 sequent lines are the same
			if ((z >= 0) && (z <= 3)) return 1<<z;        //z is in this case the index of the bit that has to be set
			else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
		}
		else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
	}

	if (loc == CBR){
		//evaluates if (h/2-v)/32 is an integer - according to crossbar documentation, and if the given lines are in necessary range
		if (((ver_line - (hor_line/2))%32 == 0) && (hor_line < 64) && (ver_line > 127) && (ver_line < 256)){
			ver_line -= 128;  //in the right crossbar the numbers are 128 higher
			z=(ver_line - (hor_line/2))/32;     // /2 due to repeating pattern in a crossbar every 2 sequent lines are the same
			if ((z >= 0) && (z <= 3)) return 1<<z;        //z is in this case the index of the bit that has to be set
			else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
		}
		else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
	}

#elif HICANN_V == 1

	if (loc == CBL){
		//evaluates if (h-v)/32 is an integer - according to crossbar documentation, and if the given lines are in necessary range
		if (((ver_line - (hor_line & 0x1f))%32 == 0) && (hor_line < 64) && (ver_line < 128)){
			z=(ver_line - (hor_line & 0x1f))/32;     //0x1f due to repeating pattern in a crossbar every 32 lines are the same
			if ((z >= 0) && (z <= 3)) return 1<<z;        //z is in this case the index of the bit that has to be set
			else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
		}
		else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
	}

	if (loc == CBR){
		if (((ver_line - (hor_line & 0x1f))%32 == 0) && (hor_line < 64) && (ver_line > 127) && (ver_line < 256)){
			ver_line -= 128;  //in the right crossbar the numbers are 128 higher
			z=(ver_line - (hor_line & 0x1f))/32;
			if (z >= 0 && z <= 3) return 1<<z;
			else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
		}
		else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
	}

#else
	#error Missing code for this HICANN revision.
#endif

	if (loc == SYNTL || loc == SYNBL){ // these are complicated
		if (hor_line%2) hor_line-=1;   // 2 sequent horizontal lines have same configuration possibilities
		uint mo = ver_line%4;          // 4 sequent vertical lines can be connected to horizontal lines
		if (mo) ver_line-=mo;          // this is the starting vertical line, all calculations are done for it
									   // mo indicates then if other line that the starting line has to be connected
		if (((ver_line + 2*(hor_line & 0x3f) + 4)%32 == 0) && (hor_line < 112) && (ver_line+mo < 128)){
			z=(ver_line + 2*(hor_line & 0x3f) + 4)/32 - (hor_line & 0x3f)/16 - 1; // z is the bit number that has to be set (tricky formula ;-)
			if((z >= 0) && (z <= 3)) return bitrotate(1 << (4*z + mo)); //bit 15 corresponds to the lowest vertical line number
			else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
		}
		else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
	}

	if (loc == SYNTR || loc == SYNBR){
		if (hor_line%2) hor_line-=1;
		uint mo = ver_line%4;
		if (mo) ver_line-=mo;
		if (((ver_line + 2*(hor_line & 0x3f) + 4)%32 == 0) && (hor_line < 112) && (ver_line+mo > 127) && (ver_line+mo < 256)){
			ver_line -= 128;  //in the right switch the numbers are 128 higher
			z=(ver_line + 2*(hor_line & 0x3f) + 4)/32 - (hor_line & 0x3f)/16 - 1;
			if((z >= 0) && (z <= 3)) return bitrotate(1 << (4*z + mo));
			else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
		}
		else {log(Logger::ERROR) << "L1SwitchControl::transline: cannot switch requested lines"; return 0;}
	}

	return 0;
}

void L1SwitchControl::switch_connect(uint horiz_line, uint vert_line, bool connection){
	log(Logger::DEBUG0) <<  "L1SwitchControl::switch_connect h: " << horiz_line << " -> v: " << vert_line << " with " << connection;
	uint con=transline(horiz_line, vert_line);
	if (con){
		if (connection){
			write_cfg(horiz_line, config[horiz_line] | con);
			config[horiz_line] |= con;
		}
		else{
			write_cfg(horiz_line, config[horiz_line] & (con ^ 0xffff));
			config[horiz_line] &= (con ^ 0xffff);
		}
	}
}

void L1SwitchControl::switch_connect(std::vector<uint> horiz_line, std::vector<uint> vert_line, std::vector<bool> connection){
	for (unsigned int i=0; i < horiz_line.size(); i++) {
		switch_connect(horiz_line[i], vert_line[i], connection[i]);
	}
// TODO -- suboptimal implementation, because it executes 1 write cycle per index. Should be optimized to only perform a write cycle
// for the whole line... maybe...
}

void L1SwitchControl::print_config(){
	for (unsigned int i=0; i<config.size(); i++){
		ci_addr_t addr;
		ci_data_t data;
		read_cfg(i);
		get_read_cfg(addr, data);
		cout << " addr " << dec << i << " config in ram: " << hex << config[i] << "\t" << "config on chip: " << data << "\t";
		if (i%2) cout << endl;
	}
	cout << endl << endl;
}

// *** END class L1SwitchControl END ***

}
