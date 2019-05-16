// Company         :   kip
// Author          :   Andreas Gruebl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//
// Filename        :   synapse_control.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Tue Jun 24 08
// Last Change     :   Tue May 30 11
// by              :   akononov
//------------------------------------------------------------------------

#include "common.h" // library includes
#include "s2comm.h"
#include "linklayer.h"
#include "logger.h"
#include "s2ctrl.h"
#include "ctrlmod.h"
#include "hicann_ctrl.h"
#include "synapse_control.h"

using namespace std;

namespace facets {

SynapseControl::SynapseControl(syn_loc sl, Stage2Ctrl* c, uint ta, uint sa, uint ma) :
	CtrlModule(c, ta, sa, ma),
	log(Logger::instance("hicann-system.SynapseControl", Logger::INFO,""))
{
	loc = sl;
}

SynapseControl::~SynapseControl(){}

void SynapseControl::reset_dll(){
	log(Logger::DEBUG0) << "SynapseControl::configure";
#if HICANN_V >= 2
	write_data(sc_cnfgreg, (0<<sc_cfg_dllresetb_p)|(0xf<<sc_cfg_predel_p)|(0xf<<sc_cfg_endel_p)|(0xf<<sc_cfg_oedel_p)|(0x2<<sc_cfg_wrdel_p));
	write_data(sc_ctrlreg, (1<<sc_newcmd_p)|sc_cmd_idle);
#elif HICANN_V == 1
	write_data(sc_cnfgreg, (0<<sc_cfg_dllresetb_p)|(3<<sc_cfg_predel_p)|(5<<sc_cfg_endel_p));
#else
	#error Missing code for this HICANN revision.
#endif
}

void SynapseControl::configure(){
	log(Logger::DEBUG0) << "SynapseControl::configure";
#if HICANN_V >= 2
	//write_data(sc_cnfgreg, (0<<sc_cfg_dllresetb_p)|(0xf<<sc_cfg_predel_p)|(0xf<<sc_cfg_endel_p)|(0xf<<sc_cfg_oedel_p)|(0x2<<sc_cfg_wrdel_p));//added this line for reseting dll
	write_data(sc_cnfgreg, (3<<sc_cfg_dllresetb_p)|(0xf<<sc_cfg_predel_p)|(0xf<<sc_cfg_endel_p)|(0xf<<sc_cfg_oedel_p)|(0x2<<sc_cfg_wrdel_p));
	write_data(sc_ctrlreg, (1<<sc_newcmd_p)|sc_cmd_idle);
#elif HICANN_V == 1
	write_data(sc_cnfgreg, (3<<sc_cfg_dllresetb_p)|(3<<sc_cfg_predel_p)|(5<<sc_cfg_endel_p));
#else
	#error Missing code for this HICANN revision.
#endif
}

uint SynapseControl::todriver(uint data, uint adr) {
#if HICANN_V >= 2
	if((adr%4)>1) data<<=8;
	return data;
#elif HICANN_V == 1
	uint sd=0;
	for(int i=0; i<8; i++) sd|= (data& (1<<i))?(1<<(7-i)):0;
	if(!(adr&1)) sd<<=8;
	return sd;
#else
	#error Missing code for this HICANN revision.
#endif
}

uint SynapseControl::fromdriver(uint sd, uint adr) {
#if HICANN_V >= 2
	if((adr%4)>1) sd>>=8;
	sd = sd & 0xff;
	return sd;
#elif HICANN_V == 1
	uint data;
	for(int i=0;i<8;i++) data|= (sd& (1<<i))?(1<<(7-i)):0;
	if(!(adr&1)) data>>=8;
	return data;
#else
	#error Missing code for this HICANN revision.
#endif
}

void SynapseControl::todecoder(uint &top, uint &bot) {
	std::bitset<4> topb(top & 0xf);
	std::bitset<4> botb(bot & 0xf);
	std::bitset<2> temp; // temporarily stores the bits from the top row

	temp[0] = topb[0];
	temp[1] = topb[1];

	topb[0] = botb[3];
	topb[1] = topb[2];
	topb[2] = botb[2];

	botb[3] = temp[1];
	botb[2] = botb[0];
	botb[0] = botb[1];
	botb[1] = temp[0];

	top=topb.to_ulong() & 0xf;
	bot=botb.to_ulong() & 0xf;
}

void SynapseControl::fromdecoder(uint &top, uint &bot) {
	std::bitset<4> topb(top & 0xf);
	std::bitset<4> botb(bot & 0xf);
	std::bitset<2> temp; // temporarily stores the bits from the top row

	temp[0] = topb[0];
	temp[1] = topb[2];

	topb[0] = botb[1];
	topb[2] = topb[1];
	topb[1] = botb[3];

	botb[1] = botb[0];
	botb[0] = botb[2];
	botb[2] = temp[1];
	botb[3] = temp[0];

	top=topb.to_ulong() & 0xf;
	bot=botb.to_ulong() & 0xf;
}

bool SynapseControl::busy(){
#if HICANN_V >= 2
	if ((read_data(sc_status) & (sc_slice_busy | sc_syndrv_busy | sc_auto_busy))) return true;
	else return false;
#elif HICANN_V == 1
	return false;
#else
	#error Missing code for this HICANN revision.
#endif
}

bool SynapseControl::autobusy(){
#if HICANN_V >= 2
	if ((read_data(sc_status) & sc_auto_busy)) return true;
	else return false;
#elif HICANN_V == 1
	return false;
#else
	#error Missing code for this HICANN revision.
#endif
}

bool SynapseControl::arraybusy(){
#if HICANN_V >= 2
	if ((read_data(sc_status) & sc_slice_busy)) return true;
	else return false;
#elif HICANN_V == 1
	return false;
#else
	#error Missing code for this HICANN revision.
#endif
}

bool SynapseControl::driverbusy(){
#if HICANN_V >= 2
	if ((read_data(sc_status) & sc_syndrv_busy)) return true;
	else return false;
#elif HICANN_V == 1
	return false;
#else
	#error Missing code for this HICANN revision.
#endif
}

uint SynapseControl::read_data(uint commaddr){
	ci_addr_t addr = commaddr; //address in the synapse block
	read_cmd(addr, Stage2Comm::synramdelay);  //send the read command
#ifndef WITHOUT_ARQ
	ci_payload tmp;
	if(get_data(&tmp) == 1){
		uint val = tmp.data & 0xffffffff;  //data width is 32 bit
		log(Logger::DEBUG1) << "SynapseControl::read_data: " << hex << tmp;
		return val;
	}
	return 0xffffffff;  //return this on failure
#endif // WITHOUT_ARQ
}

uint SynapseControl::write_data(uint commaddr, uint writedata){
	ci_addr_t addr = commaddr;
	ci_data_t data = writedata;
	return write_cmd(addr, data, Stage2Comm::synramdelay);
}

void SynapseControl::write_weight(uint trow, uint tcol, uint val){

	if (trow < 224 && tcol < 256){
		vector<uint> data(32,0);
		read_row(trow, data, false);
		uint col = tcol/8; //position of the column in the vector (data[col])
		uint mod = tcol%8; //position of weight as 4-bit chunk in data[col]
		val = (val & 0xf) << (4*mod); //prepare new value
		data[col] &= (~(0xf << (4*mod))); //erase the old value
		data[col] |= val; //write new value
		write_weight(trow, data);
	}
	else {
		log(Logger::ERROR) << "SynapseControl::write_weight: Specified entry does not exist";
	}
}

uint SynapseControl::read_weight(uint trow, uint tcol){
	if (trow < 224 && tcol < 256){
		vector<uint> data(32,0);
		read_row(trow, data, false);
		uint col = tcol/8; //position of the column in the vector (data[col])
		uint mod = tcol%8; //position of weight as 4-bit chunk in data[col]
		return (data[col] & (0xf << (4*mod))) >> (4*mod);
	}
	else {
		log(Logger::ERROR) << "SynapseControl::read_weight: Specified entry does not exist";
		return 0xffffffff;
	}
}

void SynapseControl::write_decoder(uint trow, uint tcol, uint val){
	if (trow < 224 && tcol < 256){
		vector<uint> top(32,0);
		vector<uint> bot(32,0);
		uint ttop, tbot, topflag=0;
		// now we have to read out both rows of the same synapse driver
#if HICANN_V>=2
		if (trow%2==0){
			read_row(trow, bot, true);
			read_row(trow+1, top, true);
		}
		else{
			topflag = 1;
			read_row(trow, top, true);
			read_row(trow-1, bot, true);
		}
#elif HICANN_V==1
		if (trow%4==0 || trow%4==1){
			read_row(trow, bot, true);
			read_row(trow+2, top, true);
		}
		else{
			topflag = 1;
			read_row(trow, top, true);
			read_row(trow-2, bot, true);
		}
#else
	#error Missing code for this HICANN revision.
#endif
		uint col = tcol/8; //position of the column in the vector (data[col])
		uint mod = tcol%8; //position of decoder value as 4-bit chunk in data[col]
		ttop=(top[col] & (0xf << (4*mod))) >> (4*mod);
		tbot=(bot[col] & (0xf << (4*mod))) >> (4*mod);

		fromdecoder(ttop, tbot); //switch to "normal" format
		if (topflag) ttop= (val & 0xf);
		else tbot= (val & 0xf);

		todecoder(ttop, tbot); //switch back to decoder format

		ttop = (ttop & 0xf) << (4*mod); //prepare new values
		tbot = (tbot & 0xf) << (4*mod); //prepare new values

		top[col] &= (~(0xf << (4*mod))); //erase the old values
		bot[col] &= (~(0xf << (4*mod))); //erase the old values

		top[col] |= ttop; //write new value
		bot[col] |= tbot; //write new value

#if HICANN_V>=2
		if (topflag){
			write_row(trow, top, true);
			write_row(trow-1, bot, true);
		}
		else{
			write_row(trow, bot, true);
			write_row(trow+1, top, true);
		}
#elif HICANN_V==1
		if (topflag){
			write_row(trow, top, true);
			write_row(trow-2, bot, true);
		}
		else{
			write_row(trow, bot, true);
			write_row(trow+2, top, true);
		}
#else
	#error Missing code for this HICANN revision.
#endif
	}
	else log(Logger::ERROR) << "SynapseControl::write_decoder: Specified entry does not exist";
}

uint SynapseControl::read_decoder(uint trow, uint tcol){
	if (trow < 224 && tcol < 256){
		uint ttop, tbot, topflag=0;
		vector<uint> top(32,0);
		vector<uint> bot(32,0);

		// now we have to read out both rows of the same synapse driver
#if HICANN_V>=2
		if (trow%2==0){
			read_row(trow, bot, true);
			read_row(trow+1, top, true);
		}
		else{
			topflag = 1;
			read_row(trow, top, true);
			read_row(trow-1, bot, true);
		}
#elif HICANN_V==1
		if (trow%4==0 || trow%4==1){
			read_row(trow, bot, true);
			read_row(trow+2, top, true);
		}
		else{
			topflag = 1;
			read_row(trow, top, true);
			read_row(trow-2, bot, true);
		}
#else
	#error Missing code for this HICANN revision.
#endif

		uint col = tcol/8; //position of the column in the vector (data[col])
		uint mod = tcol%8; //position of decoder value as 4-bit chunk in data[col]
		ttop=(top[col] & (0xf << (4*mod))) >> (4*mod);
		tbot=(bot[col] & (0xf << (4*mod))) >> (4*mod);
		fromdecoder(ttop, tbot);

		if (topflag) return ttop;
		else return tbot;
	}
	else {
		log(Logger::ERROR) << "SynapseControl::read_decoder: Specified entry does not exist";
		return 0xffffffff;
		}
}

void SynapseControl::write_decoder(uint bot_row_adr, vector<uint> data_bot, vector<uint> data_top){
	if (data_bot.size()!=32 || data_top.size()!=32) {
		log(Logger::ERROR) << "SynapseControl::write_decoder: Data vector has wrong length";
	}
	else if (bot_row_adr > 222) {
		log(Logger::ERROR) << "SynapseControl::write_decoder: Specified row does not exist";
	}
	else{
		//swap the bits between vectors to make everything right
		for(int x=0; x<32; x++){ //every vector
			uint ttop, tbot, newtop=0, newbot=0; //temporary variables

			for(int i=0; i<8; i++){ //for every 4-bit word swap the bits
				ttop = (data_top[x] & (0xf << (4*i))) >> (4*i);
				tbot = (data_bot[x] & (0xf << (4*i))) >> (4*i);
				todecoder(ttop, tbot);
				newtop |= ttop << (4*i);
				newbot |= tbot << (4*i);
			}
			data_top[x]=newtop;
			data_bot[x]=newbot;
		}

#if HICANN_V >= 2
		write_row(bot_row_adr, data_bot, 1); //bottom row
		write_row(bot_row_adr+1, data_top, 1); //top row
#elif HICANN_V == 1
		write_row(bot_row_adr, data_bot, 1); //bottom row
		write_row(bot_row_adr+2, data_top, 1); //top row
#else
	#error Missing code for this HICANN revision.
#endif
	}
}

void SynapseControl::read_decoder(uint bot_row_adr, vector<uint> &data_bot, vector<uint> &data_top){
	if (data_bot.size()!=32 || data_top.size()!=32) log(Logger::ERROR) << "SynapseControl::read_decoder: Data vector has wrong length";
	else if (bot_row_adr > 222) log(Logger::ERROR) << "SynapseControl::read_decoder: Specified row does not exist";
	else{
#if HICANN_V >= 2
		read_row(bot_row_adr, data_bot, 1); //bottom row
		read_row(bot_row_adr+1, data_top, 1); //top row
#elif HICANN_V == 1
		read_row(bot_row_adr, data_bot, 1); //bottom row
		read_row(bot_row_adr+2, data_top, 1); //top row
#else
	#error Missing code for this HICANN revision.
#endif

		//swap the bits between vectors to make everything right
		for(int x=0; x<32; x++){ //every vector
			uint ttop, tbot, newtop=0, newbot=0; //temporary variables

			for(int i=0; i<8; i++){ //for every 4-bit word swap the bits
				ttop = (data_top[x] & (0xf << (4*i))) >> (4*i);
				tbot = (data_bot[x] & (0xf << (4*i))) >> (4*i);
				fromdecoder(ttop, tbot);
				newtop |= ttop << (4*i);
				newbot |= tbot << (4*i);
			}
			data_top[x]=newtop;
			data_bot[x]=newbot;
		}
	}
}

void SynapseControl::write_weight(uint row, vector<uint> data){
	if (data.size()!=32) log(Logger::ERROR) << "SynapseControl::write_weight: Data vector has wrong length";
	else if (row > 224) log(Logger::ERROR) << "SynapseControl::write_weight: Specified row does not exist";
	else write_row(row, data, 0);
}

void SynapseControl::read_weight(uint row, vector<uint> &data){
	if (data.size()!=32) log(Logger::ERROR) << "SynapseControl::read_weight: Data vector has wrong length";
	else if (row > 224) log(Logger::ERROR) << "SynapseControl::read_weight: Specified row does not exist";
	else read_row(row, data, false);
}

void SynapseControl::write_row(uint row, vector<uint> data, bool decoder){
	// vector "data" mapping: bits [3..0] of data[0] contain bits [3..0] of synapse 0 and so on...
	if (data.size()!=32) log(Logger::ERROR) << "SynapseControl::write_row: Data vector has wrong length";
	else if (row > 223) log(Logger::ERROR) << "SynapseControl::write_row: Specified row does not exist";
	else{
#if HICANN_V >= 2
		for(int w=0; w<32; w++){
			uint sd=data[w];
			data[w]=0;
			//this is also due to a bug (single synapse 4-bit chunks are flipped)
			for(int i=0; i<32; i++) data[w] |= (sd & (1<<i))?(1<<(31-i)):0;
		}

		for (int colset=0; colset < 8; colset++){
			for (int i=0; i<4; i++){
				write_data(sc_synin+i, data[8*i+colset]);
			}
			if(decoder) write_data(sc_ctrlreg, sc_cmd_wdec| (row<<sc_adr_p) | (colset<<sc_colset_p) | (1<<sc_newcmd_p));
			else write_data(sc_ctrlreg, sc_cmd_write| (row<<sc_adr_p) | (colset<<sc_colset_p) | (1<<sc_newcmd_p));
			while(arraybusy()); //wait until not busy
		}

		#elif HICANN_V == 1
		for(int w=0; w<32; w++){
			uint sd=0;
			// due to a bug, bit order has to be rotated
			for(int i=0; i<32; i++) sd |= (data[w]& (1<<i))?(1<<(31-i)):0;
			write_data(sc_synw + w, sd);
		}

		bool nc=read_data(sc_ctrlreg) & (1<<sc_newcmd_p);
		nc = !nc; //invert new command flag
		if(decoder) write_data(sc_ctrlreg, sc_cmd_wdec| (row<<sc_adr_p)|(nc<<sc_newcmd_p));
		else write_data(sc_ctrlreg, sc_cmd_write| (row<<sc_adr_p)|(nc<<sc_newcmd_p));
#else
	#error Missing code for this HICANN revision.
#endif
	}
}

void SynapseControl::read_row(uint row, vector<uint> &data, bool decoder){
	if (data.size()!=32) log(Logger::ERROR) << "SynapseControl::read_row: Data vector has wrong length";
	else if (row > 223) log(Logger::ERROR) << "SynapseControl::read_row: Specified row does not exist";
	else{
#if HICANN_V >= 2
		if (decoder) write_data(sc_ctrlreg, sc_cmd_st_dec | (1<<sc_newcmd_p) | (row<<sc_adr_p)); //open row for reading
		else write_data(sc_ctrlreg, sc_cmd_st_rd | (1<<sc_newcmd_p) | (row<<sc_adr_p));
		while(arraybusy()); //wait until not busy

		for (int colset=0; colset < 8; colset++){
			if (decoder) write_data(sc_ctrlreg, sc_cmd_rdec | (colset<<sc_colset_p) | (1<<sc_newcmd_p) | (row<<sc_adr_p)); //issue read command
			else write_data(sc_ctrlreg, sc_cmd_read | (colset<<sc_colset_p) | (1<<sc_newcmd_p) | (row<<sc_adr_p));
			while(arraybusy()); //wait until not busy
			for (int i=0; i<4; i++){ //read out SYNOUT register
				uint sd=read_data(sc_synout+i);
				data[8*i+colset]=0;
				//this is also due to a bug (single synapse 4-bit chunks are flipped)
				for(int j=0; j<32; j++) data[8*i+colset] |= (sd & (1<<j))?(1<<(31-j)):0;
			}
		}

		write_data(sc_ctrlreg, sc_cmd_close | (1<<sc_newcmd_p) | (row<<sc_adr_p)); //close row
		while(arraybusy()); //wait until not busy

#elif HICANN_V == 1
		bool nc = read_data(sc_ctrlreg) & (1<<sc_newcmd_p);
		nc = !nc; //invert new command flag

		if (decoder) write_data(sc_ctrlreg, sc_cmd_rdec | (row<<sc_adr_p) | (nc<<sc_newcmd_p));
		else write_data(sc_ctrlreg, sc_cmd_read | (row<<sc_adr_p) | (nc<<sc_newcmd_p));

		log(Logger::DEBUG1) << "SynapseControl::read_row: Reading weight/decoder data";
		for(int i=0; i<32; i++){
			uint sd=0;
			data[i]=read_data(sc_synd+i);
			// due to a bug, bit order has to be rotated
			for(int w=0; w<32; w++) sd |= (data[i]& (1<<w))?(1<<(31-w)):0;
			data[i]=sd;
		}
#else
	#error Missing code for this HICANN revision.
#endif
	}
}

void SynapseControl::drv_config(uint drv_nr, bool top_ex, bool top_in,
	bool bot_ex, bool bot_in, bool enable, bool loc_input, bool top_input){

	log(Logger::DEBUG0) << "SynapseControl::drv_config: drv_nr:" << drv_nr << " [top_ex:"<< top_ex
					<< ", top_in:" << top_in << ", bot_ex:" << bot_ex << ", bot_in:" << bot_in
					<< ", enable:" << enable << ", loc_input:" << loc_input << "]";

	uint top=0, bottom=0;
	if (top_ex) top|=sc_senx;
	if (top_in) top|=sc_seni;
	if (bot_ex) bottom|=sc_senx;
	if (bot_in) bottom|=sc_seni;
	if (enable) bottom|=sc_cbot_en;
	if (loc_input) bottom|=sc_cbot_locin;
	if (top_input) bottom|=sc_cbot_topin;

#if HICANN_V >= 2
	write_data(sc_encfg+drv_nr, todriver(bottom, drv_nr));
	write_data(sc_encfg+drv_nr+1, todriver(top, drv_nr+1));
#elif HICANN_V == 1
	write_data(sc_encfg+drv_nr, todriver(bottom, drv_nr));
	write_data(sc_encfg+drv_nr+2, todriver(top, drv_nr+2));
#else
	#error Missing code for this HICANN revision.
#endif
}

void SynapseControl::drv_config(uint drv_nr, bool top_ex, bool top_in, bool bot_ex, bool bot_in, bool enable,
	bool loc_input, bool top_input, bool stdf_enable, bool dep, uint top_selgm, uint bot_selgm, uint cap){
	
	cap&=0x7; //three bits
	bot_selgm&=0x3; //two bits
	top_selgm&=0x3;
	
	uint top=0, bottom=0;
	if (top_ex) top|=sc_senx;
	if (top_in) top|=sc_seni;
	if (dep)    top|=sc_ctop_dep;
	top|=top_selgm << 2;
	top|=cap << 4;
	
	if (bot_ex)      bottom|=sc_senx;
	if (bot_in)      bottom|=sc_seni;
	if (enable)      bottom|=sc_cbot_en;
	if (stdf_enable) bottom|=sc_cbot_enstdf;
	if (loc_input)   bottom|=sc_cbot_locin;
	if (top_input)   bottom|=sc_cbot_topin;
	bottom|=bot_selgm << 2;

#if HICANN_V >= 2
	write_data(sc_encfg+drv_nr, todriver(bottom, drv_nr));
	write_data(sc_encfg+drv_nr+1, todriver(top, drv_nr+1));
#elif HICANN_V == 1
	write_data(sc_encfg+drv_nr, todriver(bottom, drv_nr));
	write_data(sc_encfg+drv_nr+2, todriver(top, drv_nr+2));
#else
	#error Missing code for this HICANN revision.
#endif
}

void SynapseControl::drv_set_gmax(uint drv_nr, uint selbot, uint seltop, uint fracbot, uint fractop){
	if (fractop > 15 || fracbot > 15) log(Logger::ERROR) << "SynapseControl::drv_set_gmax: illegal divisor value" << flush;
	if (seltop > 3 || selbot > 3) log(Logger::ERROR) << "SynapseControl::drv_set_gmax: illegal select value" << flush;
	if (drv_nr > 224) log(Logger::ERROR) << "SynapseControl::drv_set_gmax: illegal driver number" << flush;

	log(Logger::DEBUG0) << "SynapseControl::drv_set_gmax: drv_nr:" << drv_nr << " [V_gmax_bot: " << selbot << ", V_gmax_top: " << seltop << ", fraction_bot: " << fracbot << ", fraction_top: " << fractop << "]";
	
	fracbot |= fracbot << 4;
	fractop |= fractop << 4;
	selbot = selbot << 2;
	seltop = seltop << 2;
			
#if HICANN_V >= 2
	read_data(sc_encfg+drv_nr); //read out the current driver configuration (needs to be modified)
	uint cfgbot = fromdriver(read_data(sc_encfg+drv_nr),sc_encfg+drv_nr); //actual read cycle
	read_data(sc_encfg+drv_nr+1); //dummy read cycle
	uint cfgtop = fromdriver(read_data(sc_encfg+drv_nr),sc_encfg+drv_nr+1); //actual read cycle

	cfgbot &= 0xf3; //delete the bits of previous configuration
	cfgtop &= 0xf3; //delete the bits of previous configuration
	cfgbot |= selbot; //write new configuration
	cfgtop |= seltop, //write new configuration

	write_data(sc_encfg+drv_nr, todriver(cfgbot, drv_nr)); //write new configuration to the chip
	write_data(sc_encfg+drv_nr+1, todriver(cfgtop, drv_nr+1));

	write_data(sc_engmax+drv_nr, todriver(fracbot, drv_nr)); //write gmax divisors to the chip
	write_data(sc_engmax+drv_nr+1, todriver(fractop, drv_nr+1));
#elif HICANN_V == 1
	uint cfgbot = read_data(sc_encfg+drv_nr);
	uint cfgtop = read_data(sc_encfg+drv_nr+2);

	cfgbot &= 0xf3; //delete the bits of previous configuration
	cfgtop &= 0xf3; //delete the bits of previous configuration
	cfgbot |= selbot; //write new configuration
	cfgtop |= seltop, //write new configuration

	write_data(sc_encfg+drv_nr, todriver(cfgbot, drv_nr)); //write new configuration to the chip
	write_data(sc_encfg+drv_nr+2, todriver(cfgtop, drv_nr+2));

	write_data(sc_engmax+drv_nr, todriver(fracbot, drv_nr)); //write gmax divisors to the chip
	write_data(sc_engmax+drv_nr+2, todriver(fractop, drv_nr+2));
#else
	#error Missing code for this HICANN revision.
#endif
}

void SynapseControl::gmax_frac_set(uint drv_nr, uint fracbot, uint fractop){
	if (fractop > 15 || fracbot > 15) log(Logger::ERROR) << "SynapseControl::gmax_frac_set: illegal divisor value" << flush;
	if (drv_nr > 224) log(Logger::ERROR) << "SynapseControl::gmax_frac_set: illegal driver number" << flush;

	fracbot |= fracbot << 4;
	fractop |= fractop << 4;

	write_data(sc_engmax+drv_nr, todriver(fracbot, drv_nr)); //write gmax divisors to the chip
	write_data(sc_engmax+drv_nr+1, todriver(fractop, drv_nr+1));
}

void SynapseControl::gmax_frac_set(uint drv_nr, uint fracbotexc, uint fracbotinh, uint fractopexc, uint fractopinh){
	if (fractopexc > 15 || fracbotexc > 15 || fractopinh > 15 || fracbotinh > 15)
		log(Logger::ERROR) << "SynapseControl::gmax_frac_set: illegal divisor value" << flush;
	if (drv_nr > 224) log(Logger::ERROR) << "SynapseControl::gmax_frac_set: illegal driver number" << flush;

	uint fractop, fracbot;
	fracbot = (fracbotexc | (fracbotinh << 4)) & 0xff;
	fractop = (fractopexc | (fractopinh << 4)) & 0xff;

	write_data(sc_engmax+drv_nr, todriver(fracbot, drv_nr)); //write gmax divisors to the chip
	write_data(sc_engmax+drv_nr+1, todriver(fractop, drv_nr+1));
}

void SynapseControl::drv_set_pdrv(uint drv_nr, uint pdrvbot, uint pdrvtop){
	log(Logger::DEBUG0) << "SynapseControl::drv_set_pdrv: drv_nr:" << drv_nr << " [pdrvbot:"<< pdrvbot
				<< ", pdrvtop:" << pdrvtop  << "]";
#if HICANN_V >= 2
	write_data(sc_endrv+drv_nr, todriver(pdrvbot, drv_nr));
	write_data(sc_endrv+drv_nr+1, todriver(pdrvtop, drv_nr+1));
#elif HICANN_V == 1
	write_data(sc_endrv+drv_nr, todriver(pdrvbot, drv_nr));
	write_data(sc_endrv+drv_nr+2, todriver(pdrvtop, drv_nr+2));
#else
	#error Missing code for this HICANN revision.
#endif
}

void SynapseControl::reset_drivers(){
	log(Logger::DEBUG0) << "SynapseControl::reset_drivers: Resetting synapse drivers";
	for (uint r=0; r<224; r++){
		write_data(sc_encfg+r, todriver(0, sc_encfg+r));
		write_data(sc_endrv+r, todriver(0, sc_endrv+r));
		write_data(sc_engmax+r, todriver(0, sc_engmax+r));
	}
}

void SynapseControl::reset_all(){
	log(Logger::DEBUG0) << "SynapseControl::reset_all: Resetting synapse arrays and drivers, stand by...";
	vector<uint> zeros(32,0); //one row, all weights 0, decoder 0
	for (uint r=0; r<224; r++){
		write_weight(r, zeros); //reset weights
#if HICANN_V >= 2
		if ((r%4==0) || (r%4==2)) write_decoder(r, zeros, zeros); //reset decoder values
#elif HICANN_V == 1
		if ((r%4==0) || (r%4==1)) write_decoder(r, zeros, zeros); //reset decoder values
#else
	#error Missing code for this HICANN revision.
#endif
		write_data(sc_encfg+r, todriver(0, sc_encfg+r));
		write_data(sc_endrv+r, todriver(0, sc_endrv+r));
		write_data(sc_engmax+r, todriver(0, sc_engmax+r));
	}
}

void SynapseControl::preout_set(uint drv_nr, uint pout0, uint pout1, uint pout2, uint pout3){
	///this function depends on global enable bits being zero!!!
	log(Logger::DEBUG0) << "SynapseControl::preout_set: drv_nr:" << drv_nr << " [pout0:"<< pout0
			<< ", pout1:" << pout1  << ", pout2:" << pout2 << ", pout3:" << pout2 << "]";
	if (pout0 > 3 || pout1 > 3 || pout2 > 3 || pout3 > 3) {
		log(Logger::ERROR) << "SynapseControl::preout_set: Preout value out of range";
	}
	else if (drv_nr > 224) {
		log(Logger::ERROR) << "SynapseControl::preout_set: Specified driver does not exist";
	}
	else{
		uint top, bottom, p0, p1, p2, p3;
		p0 = (pout0 & 0x1) | ((pout0 & 0x2) << 1);
		p1 = ((pout1 & 0x1) << 3) | (pout1 & 0x2);
		p2 = (pout2 & 0x1) | ((pout2 & 0x2) << 1);
		p3 = ((pout3 & 0x1) << 3) | (pout3 & 0x2);
		top = (p3 << 4) | p1;
		bottom = (p2 << 4) | p0;
		drv_set_pdrv(drv_nr, bottom, top);
	}
	log(Logger::DEBUG0) << "SynapseControl::preout_set: drv_nr:" << drv_nr << " [pout0:"<< pout0
			<< ", pout1:" << pout1  << ", pout2:" << pout2 << ", pout3:" << pout2 << "]";
}

void SynapseControl::read_driver(uint addr, uint &cfgbot, uint &pdrvbot, uint &gmaxbot, uint &cfgtop, uint &pdrvtop, uint &gmaxtop){
#if HICANN_V >= 2
	read_data(sc_encfg+addr); //dummy read cycle
	cfgbot  = fromdriver(read_data(sc_encfg+addr),sc_encfg+addr); //actual read cycle
	read_data(sc_endrv+addr); //dummy read cycle
	pdrvbot = fromdriver(read_data(sc_endrv+addr),sc_endrv+addr); //actual read cycle
	read_data(sc_engmax+addr); //dummy read cycle
	gmaxbot = fromdriver(read_data(sc_endrv+addr),sc_endrv+addr); //actual read cycle

	read_data(sc_encfg+addr+1); //dummy read cycle
	cfgtop  = fromdriver(read_data(sc_encfg+addr+1),sc_encfg+addr+1); //actual read cycle
	read_data(sc_endrv+addr+1); //dummy read cycle
	pdrvtop = fromdriver(read_data(sc_endrv+addr+1),sc_endrv+addr+1); //actual read cycle
	read_data(sc_engmax+addr+1); //dummy read cycle
	gmaxtop = fromdriver(read_data(sc_endrv+addr+1),sc_endrv+addr+1); //actual read cycle
#endif
}

void SynapseControl::print_config(){
	for (int i=0; i<224; i+=1){
#if HICANN_V >= 2
		if (i%4==0 || i%4==2){
			int cfg, drv, gmx;
			read_data(sc_encfg+i); //dummy read cycle
			cfg = fromdriver(read_data(sc_encfg+i),sc_encfg+i); //actual read cycle
			read_data(sc_endrv+i); //dummy read cycle
			drv = fromdriver(read_data(sc_endrv+i),sc_endrv+i); //actual read cycle
			read_data(sc_engmax+i); //dummy read cycle
			gmx = fromdriver(read_data(sc_endrv+i),sc_endrv+i); //actual read cycle
			cout << "Driver " << dec << setw(3) << i << ": ";
			cout << "Bot: Config:" << hex << setw(3) << cfg << " Predrv:" << setw(3)
				<< drv << " Gmax:" << setw(3) << gmx;
			read_data(sc_encfg+i+1); //dummy read cycle
			cfg = fromdriver(read_data(sc_encfg+i),sc_encfg+i+1); //actual read cycle
			read_data(sc_endrv+i+1); //dummy read cycle
			drv = fromdriver(read_data(sc_endrv+i),sc_endrv+i+1); //actual read cycle
			read_data(sc_engmax+i+1); //dummy read cycle
			gmx = fromdriver(read_data(sc_endrv+i),sc_endrv+i+1); //actual read cycle
			cout << " Top: Config:" << setw(3) << cfg << " Predrv:" << setw(3) << drv
				<< " Gmax:" << setw(3) << gmx << endl;
			cout << dec;
		}
#elif HICANN_V == 1
		if (i%4==0 || i%4==1){
			cout << "Driver " << dec << i << ": " << endl;
			cout << "Bot: Config: " << hex << read_data(sc_encfg+i) << " Predrv: " << read_data(sc_endrv+i) << " Gmax: " << read_data(sc_engmax+i);
			cout << " Top: Config: " << read_data(sc_encfg+i+2) << " Predrv: " << read_data(sc_endrv+i+2) << " Gmax: " << read_data(sc_engmax+i+2) << endl;
			cout << dec;
		}
#else
	#error Missing code for this HICANN revision.
#endif
	}
}

void SynapseControl::print_weight(){
#if HICANN_V >= 2
	std::vector<uint> data(32,0);
	for (int i=0; i<224; i++){
		read_weight(i, data);

		cout << "Row " << setw(3) << i << ": " << hex;
		for (int j=0; j<32; j++){
			if (j==16) cout << endl << "         ";
			for (int k=0; k<32; k+=4){
				cout << ((data[j]&(0xf<<k))>>k);
			}
		}
		cout << dec << endl;
	}
#endif
}

void SynapseControl::print_decoder(){
#if HICANN_V >= 2
	std::vector<uint> databot(32,0), datatop(32,0);
	for (int i=0; i<224; i+=2){
		read_decoder(i, databot, datatop);

		cout << "Row " << setw(3) << i << ": " << hex;
		for (int j=0; j<32; j++){
			if (j==16) cout << endl << "         ";
			for (int k=0; k<32; k+=4){
				cout << ((databot[j]&(0xf<<k))>>k);
			}
		}
		cout << dec << endl;

		cout << "Row " << setw(3) << i+1 << ": " << hex;
		for (int j=0; j<32; j++){
			if (j==16) cout << endl << "         ";
			for (int k=0; k<32; k+=4){
				cout << ((datatop[j]&(0xf<<k))>>k);
			}
		}
		cout << dec << endl;
	}
#endif
}

void SynapseControl::set_sram_timings(uint read_delay, uint setup_precharge, uint write_delay){
	if (read_delay >= (1<<8)) throw runtime_error("read_delay parameter too large (size is 8 bit in hardware). Fail.");
	if (setup_precharge >= (1<<4)) throw runtime_error("setup_precharge parameter too large (size is 4 bit in hardware). Fail.");
	if (write_delay >= (1<<4)) throw runtime_error("write_delay parameter too large (size is 4 bit in hardware). Fail.");

	write_data((1<<addrmsb_syndrvctrl) + (1<<cnfgadr_syndrvctrl)     , (read_delay&0xff));
	write_data((1<<addrmsb_syndrvctrl) + (1<<cnfgadr_syndrvctrl) + 1 , (((setup_precharge&0xf)<<4) + (write_delay&0xf)));
}

void SynapseControl::get_sram_timings(uint& read_delay, uint& setup_precharge, uint& write_delay){
	uint addr0 = (1<<addrmsb_syndrvctrl) + (1<<cnfgadr_syndrvctrl);
	uint addr1 = (1<<addrmsb_syndrvctrl) + (1<<cnfgadr_syndrvctrl) + 1;

	uint rd_data = read_data(addr0);
	read_delay = rd_data&0xff;

	rd_data = read_data(addr1);
	setup_precharge = (rd_data>>4)&0xf;
	write_delay     = rd_data&0xf;
}

} // namespace facets
