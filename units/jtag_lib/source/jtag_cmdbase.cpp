#include "jtag_cmdbase.h"

#include <cstring>
#include <cassert>

using namespace std;

void jtag_cmdbase::SetARQMonitorSegment(jtag_cmdbase::segment_t const set_segment) {
	set_jtag_instr_chain_noexec(CMD2_ARQ_CTRL, pos_fpga);
	uint64_t set_data = (1<<28) | (set_segment.to_ulong()<<26);
	set_jtag_data_chain(set_data, 29, pos_fpga);
}

void jtag_cmdbase::SetARQTimings(unsigned int arbiter_delay, unsigned int master_timeout, unsigned int target_timeout) {
	set_jtag_instr_chain_noexec(CMD2_ARQ_CTRL, pos_fpga);
	uint64_t set_data = (1<<25) | ((arbiter_delay&0x1f)<<20) | ((target_timeout&0x3ff)<<10) | (master_timeout&0x3ff);
	set_jtag_data_chain(set_data, 29, pos_fpga);
}


jtag_cmdbase::arqdout_t jtag_cmdbase::GetARQDout() {
	// 8*6 bit
	//void GetARQDout(uint64_t &dout) {
	set_jtag_instr_chain_noexec(CMD2_ARQ_READ_DOUT, pos_fpga);
	uint64_t tmp = 0;
	get_jtag_data_chain(tmp, 48, pos_fpga);
	jtag_cmdbase::arqdout_t ret;
	for (int i = 0; i < 48; i++)
		ret[i/6] = (tmp >> 6*(i/6)) & 0x3f;
	return ret;
}

jtag_cmdbase::arqdout_t jtag_cmdbase::K7FPGA_GetARQDout()
{
	// 8*6 bit
	set_jtag_instr_chain(K7CMD_ARQ_READ_DOUT, pos_fpga);
	uint64_t tmp = 0;
	get_jtag_data_chain(tmp, 48, pos_fpga);
	jtag_cmdbase::arqdout_t ret;
	for (int i = 0; i < 48; i++)
		ret[i / 6] = (tmp >> 6 * (i / 6)) & 0x3f;
	return ret;
}

jtag_cmdbase::arqdout_t jtag_cmdbase::GetARQDout(segment_t const set_segment) {
	SetARQMonitorSegment(set_segment);
	return GetARQDout();
}

jtag_cmdbase::arqcounters_t jtag_cmdbase::GetARQCounters() {
	uint32_t rcounter, wcounter;
	set_jtag_instr_chain_noexec(CMD2_ARQ_READ_COUNT, pos_fpga);
	uint64_t curr_data = 0x110DADA;
	get_jtag_data_chain(curr_data, 64, pos_fpga);

	rcounter = curr_data >> 32;
	wcounter = curr_data & 0xffffffffull;
	return std::make_pair(rcounter, wcounter);
}

jtag_cmdbase::arqcounters_t jtag_cmdbase::K7FPGA_GetARQCounters() {
	uint32_t rcounter, wcounter;
	set_jtag_instr_chain(K7CMD_ARQ_READ_COUNT, pos_fpga);
	uint64_t curr_data = 0x110DADA;
	get_jtag_data_chain(curr_data, 64, pos_fpga);

	rcounter = curr_data >> 32;
	wcounter = curr_data & 0xffffffffull;
	return std::make_pair(rcounter, wcounter);
}

jtag_cmdbase::arqdebugregs_t jtag_cmdbase::GetARQNetworkDebugReg() {
	set_jtag_instr_chain_noexec(CMD2_ARQ_NETWORK_DEBUG, pos_fpga);
	uint64_t reg;
	get_jtag_data_chain(reg, 64, pos_fpga);
	return std::make_pair(reg>>32, reg);
}

jtag_cmdbase::arqdebugregs_t jtag_cmdbase::K7FPGA_GetARQNetworkDebugReg() {
	set_jtag_instr_chain(K7CMD_ARQ_NETWORK_DEBUG, pos_fpga);
	uint64_t reg;
	get_jtag_data_chain(reg, 64, pos_fpga);
	return std::make_pair(reg>>32, reg);
}

void jtag_cmdbase::GetARQFIFOStatus(bool &write_full, bool &write_almost_full, bool &write_empty, bool &arq_read)
{
	set_jtag_instr_chain_noexec(CMD2_ARQ_READ_FIFO_STATUS, pos_fpga);
	uint64_t tmp = 0;
	get_jtag_data_chain(tmp, 20, pos_fpga);

	write_full = tmp & 0x1;
	write_almost_full = (tmp>>1) & 0x1;
	write_empty = (tmp>>2) & 0x1;
	arq_read = (tmp>>3) & 0x1;
}


void jtag_cmdbase::GetARQFIFOCount(unsigned int &write_count, bool &write_when_full)
{
	set_jtag_instr_chain_noexec(CMD2_ARQ_READ_FIFO_STATUS, pos_fpga);
	uint64_t tmp = 0;
	get_jtag_data_chain(tmp, 20, pos_fpga);

	write_when_full = (tmp>>19) & 0x1;
	write_count = (tmp>>4) & 0x7fff;
}

