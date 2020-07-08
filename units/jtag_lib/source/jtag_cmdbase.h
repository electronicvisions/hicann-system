///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate$
// $LastChangedRevision$
// $LastChangedBy$

#ifndef __JTAG_CMDBASE__
#define __JTAG_CMDBASE__

#include <bitset>
#include <cstdio>
#include <iostream>
#include <utility>
#include <vector>
#include <tr1/array>

extern "C" {
#include <stdint.h>
#include <unistd.h>
}

//////////////////////////////////////////////////////////////////////////////////
/// .
class jtag_cmdbase
{
public:
	// JTAG related defines
	static const int IRW_FPGA = 6;
	static const int IRW_K7FPGA = 4;
	static const int IRW_DNC = 6;
	static const int IRW_HICANN = 6;

	// instruction register width (bit)

	static const int CMD_READID = 0x00;
	static const int CMD_CHANNEL_SELECT = 0x01;
	static const int CMD_LVDS_PADS_EN = 0x02;
	static const int CMD_INIT_CTRL = 0x03;
	static const int CMD_CFG_PROTOCOL = 0x04;
	static const int CMD_TIMESTAMP_CTRL = 0x05;
	static const int CMD_LOOPBACK_CTRL = 0x06;
	static const int CMD_BIAS_BYPASS = 0x07;
	static const int CMD_START_LINK = 0x08;
	static const int CMD_STOP_LINK = 0x09;
	static const int CMD_TEST_MODE_EN = 0x0a;
	static const int CMD_MEMORY_TEST_MODE = 0x0b;
	static const int CMD_TEST_MODE_DIS = 0x0c;
	static const int CMD_START_ROUTE_DAT = 0x0d;
	static const int CMD_LVDS_TEST_CTRL = 0x0e;
	static const int CMD_TDO_SELECT = 0x0f;
	static const int CMD_PLL2G_CTRL = 0x10;
	static const int CMD_SET_TX_DATA = 0x11;
	static const int CMD_GET_RX_DATA = 0x12;
	static const int CMD_SET_HEAP_MODE = 0x13;
	static const int CMD_SET_CURRENT_CHAIN = 0x14;
	static const int CMD_SET_TIME_LIMIT = 0x15;
	static const int CMD_SPEED_DETECT_CTRL = 0x16;
	static const int UNUSED28 = 0x17;
	static const int CMD_START_CFG_PKG = 0x18;
	static const int CMD_START_PULSE_PKG = 0x19;
	static const int CMD_READ_STATUS = 0x1a;
	static const int CMD_RESET_CHANNEL = 0x1b;
	static const int UNUSED5 = 0x1c;
	static const int UNUSED32 = 0x1d;
	static const int CMD_SET_DELAY_RX_DATA = 0x21;
	static const int CMD_SET_DELAY_RX_CLK = 0x22;
	static const int UNUSED9 = 0x23;
	static const int CMD_PULSE_PROTOCOL = 0x24;
	static const int UNUSED6 = 0x25;
	static const int UNUSED29 = 0x26;
	static const int CMD_READ_CRC_COUNT = 0x27;
	static const int CMD_RESET_CRC_COUNT = 0x28;
	static const int UNUSED31 = 0x29;
	static const int CMD_READ_HEAP_COUNT = 0x2a;
	static const int CMD_RESET_HEAP_COUNT = 0x2b;
	static const int CMD_SET_LOW_SPEED = 0x2c;
	static const int UNUSED26 = 0x2d;
	static const int CMD_DC_CODING = 0x2e;
	static const int UNUSED10 = 0x2f;
	static const int UNUSED11 = 0x30;
	static const int UNUSED12 = 0x31;
	static const int UNUSED13 = 0x32;
	static const int UNUSED14 = 0x33;
	static const int UNUSED15 = 0x34;
	static const int UNUSED16 = 0x35;
	static const int UNUSED17 = 0x36;
	static const int UNUSED18 = 0x37;
	static const int CMD_CLK_SEL_BY2 = 0x38;
	static const int UNUSED20 = 0x39;
	static const int UNUSED21 = 0x3a;
	static const int UNUSED22 = 0x3b;
	static const int UNUSED23 = 0x3c;
	static const int UNUSED24 = 0x3d;
	static const int UNUSED25 = 0x3e;
	static const int CMD_BYPASS = 0x3f;

	static const int CMD2_SET_TX_DATA = 0x02;
	static const int CMD2_SET_TX_CHANNEL = 0x03;
	static const int CMD2_SET_TX_CTRL = 0x04;
	static const int CMD2_GET_RX_DATA = 0x05;
	static const int CMD2_GET_RX_CHANNEL = 0x06;
	static const int CMD2_GET_STATUS = 0x0a;
	static const int CMD2_SET_FPGA_CTRL = 0x0b;
	static const int CMD2_GET_TRACE_FIFO = 0x0c;
	static const int CMD2_CONT_PULSE_CTRL = 0x0d;
	static const int CMD2_SYSTIME_CTRL = 0x0e;
	static const int CMD2_PULSE_FB_SETTINGS = 0x0f;
	static const int CMD2_DDR32_OB_READ_REQUEST = 0x10;
	static const int CMD2_DDR32_OB_READ_DATA = 0x11;
	static const int CMD2_ARQ_CTRL = 0x12;
	static const int CMD2_ARQ_READ_COUNT = 0x13;
	static const int CMD2_ARQ_READ_DOUT = 0x14;
	static const int CMD2_ARQ_READ_FIFO_STATUS = 0x15;
	static const int CMD2_ARQ_NETWORK_DEBUG = 0x16;
	static const int CMD2_PB_PULSE_COUNT01 = 0x17;
	static const int CMD2_PB_PULSE_COUNT23 = 0x18;
	static const int CMD2_TRACE_COUNTS = 0x19;
	static const int CMD2_DDR32_SO_READ_REQUEST = 0x1a;
	static const int CMD2_DDR32_SO_READ_DATA = 0x1b;
	static const int CMD2_GET_DESIGNVERSION = 0x1c;
	static const int CMD2_ARQ_PERFTEST_SET = 0x1d;
	static const int CMD2_ARQ_PERFTEST_GET = 0x1e;
	static const int CMD2_TESTMODE = 0x30;

	// ************ Kintex7 FPGA commands *********** //

	static const int K7CMD_DNCIF_STATE = 0x1;
	static const int K7CMD_DNCIF_RXCFG = 0x2;
	static const int K7CMD_DNCIF_RXPLS = 0x3;
	static const int K7CMD_DNCIF_CONFIG = 0x4;
	static const int K7CMD_DNCIF_CTRLS = 0x5;
	static const int K7CMD_DNCIF_TXDAT = 0x6;
	static const int K7CMD_DNCIF_DIS_CONFIG = 0x7;
	static const int K7CMD_HICANN_CHNL = 0x8;
	static const int K7CMD_DNCIF_DELAY = 0x9;
	static const int K7CMD_ARQ_READ_DOUT = 0xa;
	static const int K7CMD_ARQ_READ_COUNT = 0xb;
	static const int K7CMD_ARQ_NETWORK_DEBUG = 0xc;
	static const int K7CMD_HICANNIF_NRNFILTER = 0xd;

	// ************ HICANN STATES ******************* //
	static const int CMD3_READID = 0x00;
	static const int CMD3_LVDS_PADS_EN = 0x02;
	static const int CMD3_LINK_CTRL = 0x03;
	static const int CMD3_LAYER1_MODE = 0x04;
	static const int CMD3_SYSTEM_ENABLE = 0x05;
	static const int CMD3_BIAS_CTRL = 0x06;
	static const int CMD3_SET_IBIAS = 0x07;
	static const int CMD3_START_LINK = 0x08;
	static const int CMD3_STOP_LINK = 0x09;
	static const int CMD3_STOP_TIME_COUNT = 0x0a;
	static const int CMD3_READ_SYSTIME = 0x0b;
	static const int CMD3_SET_GLOBAL_RESET = 0x0c;
	static const int CMD3_SET_2XPLS = 0x0d;
	static const int CMD3_RESET_TIME_COUNT = 0x0e;
	static const int CMD3_NRN0_FILTER_CTRL = 0x0f;
	static const int CMD3_PLL2G_CTRL = 0x10;
	static const int CMD3_SET_TX_DATA = 0x11;
	static const int CMD3_GET_RX_DATA = 0x12;
	static const int CMD3_SET_TEST_CTRL = 0x17;
	static const int CMD3_START_CFG_PKG = 0x18;
	static const int CMD3_START_PULSE_PKG = 0x19;
	static const int CMD3_READ_STATUS = 0x1a;
	static const int CMD3_SET_RESET = 0x1b;
	static const int CMD3_REL_RESET = 0x1c;
	static const int CMD3_SET_DELAY_RX_DATA = 0x21;
	static const int CMD3_SET_DELAY_RX_CLK = 0x22;
	static const int CMD3_READ_CRC_COUNT = 0x27;
	static const int CMD3_RESET_CRC_COUNT = 0x28;

	// UHEI states
	static const int CMD3_PLL_FAR_CTRL = 0x29;
	static const int CMD3_ARQ_CTRL = 0x30;
	static const int CMD3_ARQ_CTRL_RESET = 0x01;
	static const int CMD3_ARQ_CTRL_LOOPBACK = 0x02;
	static const int CMD3_ARQ_TXPCKNUM = 0x31;
	static const int CMD3_ARQ_RXPCKNUM = 0x32;
	static const int CMD3_ARQ_RXDROPNUM = 0x33;
	static const int CMD3_ARQ_TXTOVAL = 0x34;
	static const int CMD3_ARQ_RXTOVAL = 0x35;
	static const int CMD3_ARQ_TXTONUM = 0x36;
	static const int CMD3_ARQ_RXTONUM = 0x37;

	// address meanings in i2c master
	static const int ADDR_PRESC_L = 0x0;
	static const int ADDR_PRESC_M = 0x1;
	static const int ADDR_CTRL = 0x2;
	static const int ADDR_RXREG = 0x3;
	static const int ADDR_STATUS = 0x4;
	static const int ADDR_TXREG = 0x5;
	static const int ADDR_CMDREG = 0x6;

	// ************ JTAG to I2C instructions (in FPGA) *************** //
	static const int JTAG_I2C_DR_WIDTH = 14; // data register width for i2c-chain in FPGA;
	/*** UHEI I2C stuff ***/
	static const int CMD_I2C_SET_DATA_EXEC =
		0x20; // set i2c address, data and rw bit and execute transfer
	static const int CMD_I2C_GET_DATA = 0x21; // read back i2c data

public:
	std::vector<uint32_t> irwidth;
	uint32_t chain_length;
	uint32_t num_hicanns; /// number of hicanns in the jtag chain
	uint32_t irw_total;

	uint32_t pulsefifo_mode;   // saves playback fifo mode
	uint32_t tracefifo_mode;   // saves trace fifo mode
	uint32_t fifoloop_mode;    // saves loop fifo mode
	uint32_t filter_nrn0_mode; // saves trace fifo filter mode

	uint32_t pos_fpga;
	uint32_t pos_dnc;
	uint32_t pos_hicann;

	// for Kintex7 HICANN interface JTAG
	unsigned int hicannif_conf[8];
	unsigned int hicannif_nr;

	// availbale hicanns in hs-channel (here called dnc) numbering
	std::bitset<8> physical_available_hicanns;
	unsigned int hicann_num;
	bool const is_kintex;

	uint32_t get_irw(uint32_t pos) { return irwidth[pos]; }

protected:
	// init function for overloaded constructor
	void jtag_cmdbase_init(bool fpga_avail, bool dnc_avail, unsigned int hicann_addr)
	{
		irwidth.clear();

		for (unsigned int i = 0; i < hicann_num; i++)
			irwidth.push_back(IRW_HICANN);

		if (dnc_avail)
			irwidth.push_back(IRW_DNC);

		if (fpga_avail) {
			if (is_kintex)
				irwidth.push_back(IRW_K7FPGA);
			else
				irwidth.push_back(IRW_FPGA);
		}

		chain_length = fpga_avail + dnc_avail + hicann_num;
		num_hicanns = hicann_num;

		if (is_kintex)
			irw_total = fpga_avail * IRW_K7FPGA + hicann_num * IRW_HICANN;
		else
			irw_total = fpga_avail * IRW_FPGA + dnc_avail * IRW_DNC + hicann_num * IRW_HICANN;

		if (fpga_avail)
			pos_fpga = hicann_num + dnc_avail;
		else
			pos_fpga = chain_length; // set to unused value if not in chain

		if (dnc_avail)
			pos_dnc = hicann_num;
		else
			pos_dnc = chain_length; // set to unused value if not in chain

		pos_hicann = hicann_addr;

		pulsefifo_mode = 0;

		hicannif_nr = 0;
		for (unsigned int nif = 0; nif < 8; ++nif)
			hicannif_conf[nif] = 0x0004000c;
	}

public:
	//////////////////////////////////////////////////////////////////////////////
	/// .
	// supply info wether FPGA/DNC is present, number of HICANNs in chain and
	// the HICANN in chain to talk with
	jtag_cmdbase(
		bool fpga_avail,
		bool dnc_avail,
		std::bitset<8> available_hicanns,
		unsigned int hicann_addr,
		bool k7fpga = false)
		: pulsefifo_mode(0),
		  tracefifo_mode(0),
		  fifoloop_mode(0),
		  filter_nrn0_mode(0),
		  physical_available_hicanns(available_hicanns),
		  hicann_num(available_hicanns.count()),
		  is_kintex(k7fpga)
	{
		jtag_cmdbase_init(fpga_avail, dnc_avail, hicann_addr);
	}

	// deprecated ctor with number of hicanns in jtagchain instead of bitset
	jtag_cmdbase(
		bool fpga_avail,
		bool dnc_avail,
		unsigned int hicann_nr,
		unsigned int hicann_addr,
		bool k7fpga = false)
		: pulsefifo_mode(0),
		  tracefifo_mode(0),
		  fifoloop_mode(0),
		  filter_nrn0_mode(0),
		  hicann_num(hicann_nr), is_kintex(k7fpga)
	{
		for (unsigned int i = 0; i < hicann_nr; i++) {
			physical_available_hicanns.set(i);
		}
		jtag_cmdbase_init(fpga_avail, dnc_avail, hicann_addr);
	}

	virtual ~jtag_cmdbase() {}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual bool reset_jtag(void) = 0;

	//////////////////////////////////////////////////////////////////////////////
	/// .
	virtual void execute_instr(void) = 0;

	//////////////////////////////////////////////////////////////////////////////
	/// .

	virtual void set_jtag_instr_chain(unsigned int command, unsigned char position) = 0;
	virtual void set_jtag_instr_chain_noexec(unsigned int command, unsigned char position) = 0;

	virtual void set_jtag_data_chain(
		uint64_t wdata, unsigned char width, unsigned char position) = 0;
	virtual void set_jtag_data_chain_noexec(
		uint64_t wdata, unsigned char width, unsigned char position) = 0;
	virtual void set_jtag_hicanns_data_chain(uint64_t wdata) = 0;

	virtual void set_get_jtag_data_chain(
		uint64_t wdata, uint64_t& rdata, unsigned int count, unsigned char position) = 0;
	virtual void set_get_jtag_data_chain(
		const std::vector<bool> wdata, std::vector<bool>& rdata, unsigned char position) = 0;

	virtual void get_jtag_data_chain(
		uint64_t& rdata, unsigned char width, unsigned char position) = 0;
	virtual void get_jtag_all_data_chain(std::vector<uint64_t>& rdata, unsigned char width) = 0;

	virtual void set_hicanns_ir_chain(unsigned char cmd) = 0;
	virtual void set_all_ir_chain(unsigned char cmd) = 0;

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_link(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD_START_LINK, pos_dnc);

		set_jtag_data_chain(wdata, 9, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_stop_link(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD_STOP_LINK, pos_dnc);

		set_jtag_data_chain(wdata, 9, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_channel(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_CHANNEL_SELECT, pos_dnc);

		set_jtag_data_chain(wdata, 4, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_tx_data(uint64_t wdata)
	{
		set_jtag_instr_chain_noexec(CMD_SET_TX_DATA, pos_dnc);

		set_jtag_data_chain(wdata, 64, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool DNC_get_rx_data(T& rdata)
	{
		set_jtag_instr_chain_noexec(CMD_GET_RX_DATA, pos_dnc);
		// set_jtag_instr_chain(CMD_GET_RX_DATA,pos_dnc);

		get_jtag_data_chain(rdata, 64, pos_dnc);
		// get_jtag_data_chain(rdata,64,pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_lvds_pads_en(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD_LVDS_PADS_EN, pos_dnc);

		set_jtag_data_chain(wdata, 9, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// enable loopback mode for HICANN channels.
	/// 0    :    disable
	/// 1    :    enable looopback
	bool DNC_set_loopback(unsigned char wdata)
	{
		uint64_t channel = (uint64_t) wdata;
		set_jtag_instr_chain(CMD_LOOPBACK_CTRL, pos_dnc);

		set_jtag_data_chain(channel, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// set heap mode for 8 memories (equals layer1 channel direction) in 8 HICANN channels
	/// 0    :    routing memory, receive layer 1 channel from HICANN
	/// 1    :    memory is heap, transmit pulses to HICANN l1 channel
	bool DNC_set_l1direction(uint64_t wdata)
	{
		uint64_t channel = (uint64_t) wdata;
		set_jtag_instr_chain(CMD_SET_HEAP_MODE, pos_dnc);

		set_jtag_data_chain(channel, 64, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_write_routing_dat(void)
	{
		set_jtag_instr_chain(CMD_START_ROUTE_DAT, pos_dnc);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// GHz PLL control vector.
	/// bit 0    :    switch PLL: 0:2GHz 1:1GHz
	/// bit 1    :    enable VCO
	/// bit 2    :    enable 1GHz
	/// bit 3    :    enable 500MHz
	bool DNC_set_GHz_pll_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_PLL2G_CTRL, pos_dnc);

		set_jtag_data_chain(wdata, 4, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_cfg_pkg(void)
	{
		set_jtag_instr_chain(CMD_START_CFG_PKG, pos_dnc);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_pulse_pkg(void)
	{
		set_jtag_instr_chain(CMD_START_PULSE_PKG, pos_dnc);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_read_status(uint64_t& rdata)
	{
		set_jtag_instr_chain(CMD_READ_STATUS, pos_dnc);
		get_jtag_data_chain(rdata, 8, pos_dnc);
		return true;
	}

	bool DNC_read_channel_sts(unsigned char channel, unsigned char& status)
	{
		DNC_set_channel(channel);
		uint64_t status_tmp;
		DNC_read_status(status_tmp);
		status = (unsigned char) status_tmp;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool DNC_read_crc_count(T& rdata)
	{
		uint64_t read_val;
		set_jtag_instr_chain(CMD_READ_CRC_COUNT, pos_dnc);
		get_jtag_data_chain(read_val, 8, pos_dnc);
		rdata = (T) read_val;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_reset_crc_count(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_RESET_CRC_COUNT, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool DNC_read_heap_count(T& rdata)
	{
		set_jtag_instr_chain(CMD_READ_HEAP_COUNT, pos_dnc);
		get_jtag_data_chain(rdata, 64, pos_dnc);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_reset_heap_count(uint64_t wdata)
	{
		set_jtag_instr_chain(CMD_RESET_HEAP_COUNT, pos_dnc);
		set_jtag_data_chain(wdata, 64, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_reset(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_RESET_CHANNEL, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_pulse_protocol(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_PULSE_PROTOCOL, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_bias_bypass(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_BIAS_BYPASS, pos_dnc);
		set_jtag_data_chain(wdata, 1, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_memory_test_mode(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_MEMORY_TEST_MODE, pos_dnc);
		set_jtag_data_chain(wdata, 1, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_config_protocol(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_CFG_PROTOCOL, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_time_limit(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_SET_TIME_LIMIT, pos_dnc);
		set_jtag_data_chain(wdata, 10, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Offers possibility to disable use of timestamp in a pulse event.
	/// This allows direct and low latency pulse event transmission.
	/// 1 bit for each HICANN connection.
	bool DNC_set_timestamp_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_TIMESTAMP_CTRL, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_lowspeed_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_SET_LOW_SPEED, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_speed_detect_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_SPEED_DETECT_CTRL, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_dc_coding_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD_DC_CODING, pos_dnc);
		set_jtag_data_chain(wdata, 8, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_test_mode_en(void)
	{
		set_jtag_instr_chain(CMD_TEST_MODE_EN, pos_dnc);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_test_mode_dis(void)
	{
		set_jtag_instr_chain(CMD_TEST_MODE_DIS, pos_dnc);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	///	bool set_data_delay(uint64 wdata, uint64 *rdata)
	///  wdata.size() = 144
	///  8 times 6 bit + 16 times 6 bit
	///  143--------------------------------------------------0
	///  link0 link1 link2 link3 link4 link5 link6 link7 link8
	///  FPGA channel 15 -----> channel 0

	bool DNC_set_data_delay(const std::vector<bool>& wdata, std::vector<bool>& rdata)
	{
		set_jtag_instr_chain(CMD_SET_DELAY_RX_DATA, pos_dnc);

		set_get_jtag_data_chain(wdata, rdata, pos_dnc);
		// set_get_jtag_data(wdata,rdata,5);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool bypass(const T wdata, unsigned int length, T& rdata)
	{
		set_jtag_instr_chain(CMD_BYPASS, chain_length - 1);

		set_get_jtag_data_chain(wdata, rdata, length, chain_length - 1);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_pulse(unsigned char channel, uint64_t data)
	{
		DNC_set_channel(channel);
		DNC_set_tx_data(data);
		DNC_start_pulse_pkg();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_config(unsigned char channel, uint64_t data)
	{
		DNC_set_channel(channel);
		DNC_set_tx_data(data);
		DNC_start_cfg_pkg();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_start_routing(unsigned char channel, uint64_t data)
	{
		DNC_set_channel(channel);
		DNC_set_tx_data(data);
		DNC_write_routing_dat();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_init_ctrl(unsigned int wdata)
	{
		set_jtag_instr_chain(CMD_INIT_CTRL, pos_dnc);

		set_jtag_data_chain(wdata, 17, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool DNC_set_test_lvds_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD_LVDS_TEST_CTRL, pos_dnc);

		set_jtag_data_chain(wdata, 9, pos_dnc);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Start of Jtag Chain with 2 elements functions.
	bool read_id(uint64_t& rdata, unsigned char chip_nr)
	{
		set_jtag_instr_chain_noexec(CMD_READID, chip_nr);
		get_jtag_data_chain(rdata, 32, chip_nr);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Start of Jtag Chain with 2 elements functions.
	uint64_t read_id(unsigned char chip_nr)
	{
		uint64_t rdata;
		set_jtag_instr_chain_noexec(CMD_READID, chip_nr);
		get_jtag_data_chain(rdata, 32, chip_nr);

		return rdata;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Read JTAG IDs of all devices in chain with one instruction/data cycle
	std::vector<uint64_t> read_all_ids()
	{
		std::vector<uint64_t> rdata;
		set_all_ir_chain(CMD_READID);
		get_jtag_all_data_chain(rdata, 64);

		return rdata;
	}

	////////////////////////////////////////////////////////////////////////////
	/// Manual HICANN reset (pulsed)
	bool HICANN_chip_reset(void)
	{
		set_jtag_instr_chain(CMD3_SET_GLOBAL_RESET, pos_hicann);
		execute_instr();

		return true;
	}

	////////////////////////////////////////////////////////////////////////////
	/// Manual link reset SET
	bool HICANN_set_if_reset(void)
	{
		set_jtag_instr_chain(CMD3_SET_RESET, pos_hicann);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Manual link reset RELEASE
	bool HICANN_rel_if_reset(void)
	{
		set_jtag_instr_chain(CMD3_REL_RESET, pos_hicann);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_lvds_pads_en(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_LVDS_PADS_EN, pos_hicann);
		set_jtag_data_chain(wdata, 1, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_start_link()
	{
		set_jtag_instr_chain(CMD3_START_LINK, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_stop_link()
	{
		set_jtag_instr_chain(CMD3_STOP_LINK, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_set_pls64(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD3_SET_2XPLS, pos_hicann);
		set_jtag_data_chain(wdata, 1, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_start_cfg_pkg()
	{
		set_jtag_instr_chain(CMD3_START_CFG_PKG, pos_hicann);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_start_pulse_pkg()
	{
		set_jtag_instr_chain(CMD3_START_PULSE_PKG, pos_hicann);
		execute_instr();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool HICANN_read_status(T& rdata)
	{
		set_jtag_instr_chain(CMD3_READ_STATUS, pos_hicann);
		get_jtag_data_chain(rdata, 8, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool HICANN_read_crc_count(T& count)
	{
		uint64_t crc_data;
		set_jtag_instr_chain(CMD3_READ_CRC_COUNT, pos_hicann);
		get_jtag_data_chain(crc_data, 8, pos_hicann);
		count = crc_data;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_reset_crc_count()
	{
		set_jtag_instr_chain(CMD3_RESET_CRC_COUNT, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Offers restart of system counter after counter is stopped via JTAG.
	/// After release via JTAG an additial start signal rfom sys_start pin is required.
	bool HICANN_start_time_counter()
	{
		unsigned int one = 0;
		set_jtag_instr_chain(CMD3_STOP_TIME_COUNT, pos_hicann);
		set_jtag_data_chain(one, 1, pos_hicann);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Offers possibility to stop systime counter after it has been started.
	bool HICANN_stop_time_counter()
	{
		unsigned int zero = 1;
		set_jtag_instr_chain(CMD3_STOP_TIME_COUNT, pos_hicann);
		set_jtag_data_chain(zero, 1, pos_hicann);
		return true;
	}

	/// reset all time counters of all connected HICANNs for hypothetic HICANN version 3
	bool HICANN_reset_all_time_counters()
	{
		set_hicanns_ir_chain((unsigned char) CMD3_RESET_TIME_COUNT);
		return true;
	}

	/// reset all time counters of all connected HICANNs for HICANN version 2
	bool HICANNv2_reset_all_time_counters()
	{
		set_hicanns_ir_chain((unsigned char) CMD3_STOP_TIME_COUNT);
		set_jtag_hicanns_data_chain(0x8000000000000000ULL);
		set_hicanns_ir_chain((unsigned char) CMD3_STOP_TIME_COUNT);
		set_jtag_hicanns_data_chain(0x0000000000000000ULL);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Offers possibility to stop systime counter after it has been started.
	bool HICANN_restart_time_counter()
	{
		HICANN_stop_time_counter();
		HICANN_start_time_counter();
#ifdef NCSIM
		printf("Waiting for Start signal\n");
#else
		printf("Press the start button to restart the systime counters (then ENTER)\n");
		getchar();
#endif
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_set_tx_data(uint64_t wdata)
	{
		set_jtag_instr_chain_noexec(CMD3_SET_TX_DATA, pos_hicann);
		set_jtag_data_chain(wdata, 64, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool HICANN_get_rx_data(uint64_t& rdata)
	{
		set_jtag_instr_chain_noexec(CMD3_GET_RX_DATA, pos_hicann);
		get_jtag_data_chain(rdata, 64, pos_hicann);
		// set_jtag_instr_chain (CMD3_GET_RX_DATA, pos_hicann);
		// get_jtag_data_chain (rdata,64,pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Readout of systime counter.(ATTENTION: Very slow)
	template <typename T>
	bool HICANN_read_systime(T& rdata)
	{
		set_jtag_instr_chain(CMD3_READ_SYSTIME, pos_hicann);
		get_jtag_data_chain(rdata, 15, pos_hicann);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Each bit selects one type of DNC packet
	/// bit 0 : pulse_enable
	/// bit 1 : config_enable
	/// bit 2 : routing_enable
	/// bit 3 : dnc_cfg_enable
	/// bit 4 : ram_heap_enable
	/// bit 5 : limit_enable
	/// bit 6 : get_dnc_status
	/// bit 7 : get_dnc_crc
	/// bit 8 : start dnc_if
	/// bit 9 : master = 1 / slave = 0
	bool FPGA_start_dnc_packet(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD2_SET_TX_CTRL, pos_fpga);
		set_jtag_data_chain(wdata, 10, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_set_channel(unsigned int wdata)
	{
		set_jtag_instr_chain(CMD2_SET_TX_CHANNEL, pos_fpga);
		set_jtag_data_chain(wdata, 24, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_testmode(unsigned int wdata)
	{
		set_jtag_instr_chain(CMD2_TESTMODE, pos_fpga);
		set_jtag_data_chain(wdata, 2, pos_fpga);

		return true;
	}

	////////////////////////////////////////////////////////////////////////////
	/// GHz PLL control vector.
	/// bit 0    :    enable VCO
	/// bit 1    :    enable 1GHz
	/// bit 2    :    enable 500MHz
	bool HICANN_set_GHz_pll_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD3_PLL2G_CTRL, pos_hicann);
		set_jtag_data_chain(wdata, 3, pos_hicann);

		return true;
	}

	////////////////////////////////////////////////////////////////////////////
	/// Controls the neuron 0 filter for each layer 1 channel in spl1
	bool hicannif_nrn0_filter_ctrl(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD3_NRN0_FILTER_CTRL, pos_hicann);
		set_jtag_data_chain(wdata, 8, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// FPGA control vector.
	/// bit 0    :    A_SYNC_RESET
	bool FPGA_set_fpga_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD2_SET_FPGA_CTRL, pos_fpga);
		set_jtag_data_chain(wdata, 12, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_set_tx_data(uint64_t wdata)
	{
		set_jtag_instr_chain_noexec(CMD2_SET_TX_DATA, pos_fpga);
		set_jtag_data_chain(wdata, 64, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_set_systime_ctrl(uint64_t wdata)
	{
		set_jtag_instr_chain_noexec(CMD2_SYSTIME_CTRL, pos_fpga);
		set_jtag_data_chain(wdata, 1, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_get_status(uint64_t& rdata)
	{
		set_jtag_instr_chain(CMD2_GET_STATUS, pos_fpga);
		get_jtag_data_chain(rdata, 14, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_get_crc(unsigned char& rdata)
	{
		uint64_t state;
		FPGA_get_status(state);
		rdata = (unsigned char) (state >> 6);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool FPGA_get_rx_channel(T& rdata)
	{
		set_jtag_instr_chain(CMD2_GET_RX_CHANNEL, pos_fpga);
		get_jtag_data_chain(rdata, 27, pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool FPGA_get_rx_data(T& rdata)
	{
		set_jtag_instr_chain_noexec(CMD2_GET_RX_DATA, pos_fpga);
		get_jtag_data_chain(rdata, 64, pos_fpga);
		// set_jtag_instr_chain (CMD2_GET_RX_DATA, pos_fpga);
		// get_jtag_data_chain (rdata,64,pos_fpga);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Control for continious pulse event creation
	/// bit 0      :   enable generation
	/// bit 1      :   poisson random on/off
	/// bit 9:2    :   Layer 1 channel
	/// bit 15:10  :   first neuron number
	/// bit 21:16  :   last neuron number
	/// bit 24:22  :   selects one of 8 HICANNs
	/// bit 38:25  :   mean time delay
	/// bit 62:39  :   seed
	bool FPGA_set_cont_pulse_ctrl(
		unsigned char start,
		unsigned char channel,
		unsigned char poisson,
		unsigned short delay,
		unsigned int seed,
		unsigned char nrn_start,
		unsigned char nrn_end,
		unsigned char hicann)
	{
		// initialize FPGA JTAG TX data with 0's to avoid wrong SPl1 content
		FPGA_set_tx_data(0);
		set_jtag_instr_chain(CMD2_CONT_PULSE_CTRL, pos_fpga);

		uint64_t wdata = 0;
		wdata |= (((uint64_t) start & 0x1) << 0);
		wdata |= (((uint64_t) poisson & 0x1) << 1);
		wdata |= (((uint64_t) channel & 0xff) << 2);
		wdata |= (((uint64_t) nrn_start & 0x3f) << 10);
		wdata |= (((uint64_t) nrn_end & 0x3f) << 16);
		wdata |= (((uint64_t) hicann & 0x7) << 22);
		wdata |= (((uint64_t)(delay - 2) & 0x3fff) << 25);
		wdata |= (((uint64_t) seed & 0xffffff) << 39);

		set_jtag_data_chain(wdata, 63, pos_fpga);

		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_start_pulse_event(unsigned char channel, uint64_t wdata)
	{
		FPGA_set_channel(channel);
		FPGA_set_tx_data(wdata);
		FPGA_start_dnc_packet((0x1 << 9) + (0x1 << 8) + (0x1 & 0xff));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_start_config_packet(unsigned char channel, uint64_t wdata)
	{
		FPGA_set_channel(channel);
		FPGA_set_tx_data(wdata);
		FPGA_start_dnc_packet((0x1 << 9) + (0x1 << 8) + (0x2 & 0xff));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_cfg_dnc_l1direction(unsigned char channel, uint64_t wdata)
	{
		FPGA_set_channel(1 << channel);
		FPGA_set_tx_data(wdata);
		FPGA_start_dnc_packet((0x1 << 9) + (0x1 << 8) + (0x10 & 0xff));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_write_playback_fifo(uint64_t wdata)
	{
		FPGA_set_tx_data(wdata);
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			((unsigned int) 1 << 11));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_enable_pulsefifo()
	{
		pulsefifo_mode = 1;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_disable_pulsefifo()
	{
		pulsefifo_mode = 0;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_enable_tracefifo()
	{
		tracefifo_mode = 1;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_disable_tracefifo()
	{
		tracefifo_mode = 0;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_enable_filter_tracefifo()
	{
		filter_nrn0_mode = 1;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_disable_filter_tracefifo()
	{
		filter_nrn0_mode = 0;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_enable_fifoloop()
	{
		fifoloop_mode = 1;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_disable_fifoloop()
	{
		fifoloop_mode = 0;
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Reads last FIFO entry and does a pop afterwards
	template <typename T>
	bool FPGA_get_trace_fifo(unsigned int& systime, T& rdata)
	{
		systime = 0;
		uint64_t tmp = 0;
		set_jtag_instr_chain(CMD2_GET_TRACE_FIFO, pos_fpga);
		get_jtag_data_chain(rdata, 64, pos_fpga);
		FPGA_get_rx_channel(tmp);
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5) + (0x1 << 10));
		execute_instr();
		systime = (tmp >> 11) & 0xffffffff;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Reads last FIFO entry and does a pop afterwards
	template <typename T>
	bool FPGA_get_trace_fifo(T& rdata)
	{
		set_jtag_instr_chain(CMD2_GET_TRACE_FIFO, pos_fpga);
		get_jtag_data_chain(rdata, 64, pos_fpga);
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5) + (0x1 << 10));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Reads last FIFO entry, determines whether it is a config_packet  and does a pop afterwards
	template <typename T>
	bool FPGA_get_trace_fifo(T& rdata, bool& is_config_packet)
	{
		uint64_t tmp = 0;
		set_jtag_instr_chain(CMD2_GET_TRACE_FIFO, pos_fpga);
		get_jtag_data_chain(rdata, 64, pos_fpga);
		FPGA_get_rx_channel(tmp);
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5) + (0x1 << 10));
		execute_instr();
		is_config_packet = (tmp >> 26) & 0x1;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool FPGA_reset_tracefifo()
	{
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (fifoloop_mode << 8) + (1 << 7) + (pulsefifo_mode << 6) +
			(tracefifo_mode << 5));
		execute_instr();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	template <typename T>
	bool FPGA_get_rx_packet(uint64_t channel, T& rdata)
	{
		FPGA_get_rx_channel(channel);
		FPGA_get_rx_data(rdata);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Returns Trace FIFO empty
	bool FPGA_empty_pulsefifo()
	{
		uint64_t state = 0;
		FPGA_get_status(state);
		return ((state >> 5) & 0x1);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Controls pulse feedback module for HICANN 0
	bool SetPulseFeedback(
		unsigned int ch_in,
		unsigned int ch_out0,
		unsigned int ch_out1,
		unsigned int delay0,
		unsigned int delay1,
		bool enable = true)
	{
		uint64_t wdata = 0;
		wdata |= delay1 & 0x7fff;
		wdata |= (delay0 & 0x7fff) << 15;
		wdata |= ((uint64_t)(ch_out1 & 0x7)) << 30;
		wdata |= ((uint64_t)(ch_out0 & 0x7)) << 33;
		wdata |= ((uint64_t)(ch_in & 0x7)) << 36;
		if (enable)
			wdata |= ((uint64_t)(1)) << 39;

		set_jtag_instr_chain_noexec(CMD2_PULSE_FB_SETTINGS, pos_fpga);
		set_jtag_data_chain(wdata, 40, pos_fpga);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Read DDR2 onboard memory
	/// Maximum size with one call: 63 entries (each 64bit)
	std::vector<uint64_t> ReadDDROnboard(unsigned int addr, unsigned int size)
	{
		// request
		uint64_t req_data = (uint64_t)(addr) << 6 | (size & 0x3f);
		set_jtag_instr_chain_noexec(CMD2_DDR32_OB_READ_REQUEST, pos_fpga);
		set_jtag_data_chain(req_data, 31, pos_fpga);

		// read
		std::vector<uint64_t> read_data;
		for (unsigned int ndata = 0; ndata < (size & 0x3f); ++ndata) {
			set_jtag_instr_chain_noexec(CMD2_DDR32_OB_READ_DATA, pos_fpga);
			uint64_t curr_data = 0x110DADA;
			get_jtag_data_chain(curr_data, 64, pos_fpga);
			read_data.push_back(curr_data);
		}

		return read_data;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Read DDR2 SODIMM memory
	/// Maximum size with one call: 63 entries (each 64bit)
	std::vector<uint64_t> ReadDDRSODIMM(unsigned int addr, unsigned int size)
	{
		// request
		uint64_t req_data = (uint64_t)(addr) << 6 | (size & 0x3f);
		set_jtag_instr_chain_noexec(CMD2_DDR32_SO_READ_REQUEST, pos_fpga);
		set_jtag_data_chain(req_data, 34, pos_fpga);

		// read
		std::vector<uint64_t> read_data;
		for (unsigned int ndata = 0; ndata < (size & 0x3f); ++ndata) {
			set_jtag_instr_chain_noexec(CMD2_DDR32_SO_READ_DATA, pos_fpga);
			uint64_t curr_data = 0x110DADA;
			get_jtag_data_chain(curr_data, 64, pos_fpga);
			read_data.push_back(curr_data);
		}

		return read_data;
	}

	typedef std::bitset<2> segment_t;
	typedef std::tr1::array<unsigned short, 8> arqdout_t;
	typedef std::pair<uint32_t, uint32_t> arqcounters_t;
	typedef std::pair<uint32_t, uint32_t> arqdebugregs_t;

	void SetARQMonitorSegment(segment_t const set_segment);
	void SetARQTimings(
		unsigned int arbiter_delay, unsigned int master_timeout, unsigned int target_timeout);
	arqdout_t GetARQDout();
	arqdout_t K7FPGA_GetARQDout();
	arqdout_t GetARQDout(segment_t const set_segment);
	arqcounters_t GetARQCounters();
	arqcounters_t K7FPGA_GetARQCounters();
	arqdebugregs_t GetARQNetworkDebugReg();
	arqdebugregs_t K7FPGA_GetARQNetworkDebugReg();
	void GetARQFIFOStatus(
		bool& write_full, bool& write_almost_full, bool& write_empty, bool& arq_read);
	void GetARQFIFOCount(unsigned int& write_count, bool& write_when_full);

	// enable/configure ARQ performance test module
	void SetARQPerformanceTest(bool enable, unsigned int num_packets, unsigned int client_array)
	{
		uint64_t wdata = (((uint64_t)(client_array)) << 32) | (num_packets);
		if (enable)
			wdata |= (uint64_t)(1) << 48;

		set_jtag_instr_chain_noexec(CMD2_ARQ_PERFTEST_SET, pos_fpga);
		set_jtag_data_chain(wdata, 49, pos_fpga);
	}

	bool GetARQPerformanceTestResults(unsigned int& test_time_push, unsigned int& test_time_pop)
	{
		uint64_t read_data = 0;
		set_jtag_instr_chain_noexec(CMD2_ARQ_PERFTEST_GET, pos_fpga);
		get_jtag_data_chain(read_data, 64, pos_fpga);

		test_time_pop = read_data & 0x7fffffff;
		test_time_push = (read_data >> 31) & 0x7fffffff;
		bool retval = (read_data >> 62) & 0x1;

		return retval;
	}

	/// Read number of pulses sent by DDR2-Playback memory
	/// return value contains a 32bit counter, for all pulses sent to the given DNC
	uint32_t GetSentPlaybackCount(unsigned int dnc_id)
	{
		uint64_t read_data = 0;

		if (dnc_id >> 1)
			set_jtag_instr_chain_noexec(CMD2_PB_PULSE_COUNT23, pos_fpga);
		else
			set_jtag_instr_chain_noexec(CMD2_PB_PULSE_COUNT01, pos_fpga);

		get_jtag_data_chain(read_data, 64, pos_fpga);

		if (dnc_id & 1)
			read_data >>= 32;

		return (read_data & 0xffffffff);
	}

	/// Read number of 64bit entries received by DDR2-Trace memory
	unsigned int GetTraceEntryCount()
	{
		uint64_t read_data = 0;
		set_jtag_instr_chain_noexec(CMD2_TRACE_COUNTS, pos_fpga);
		get_jtag_data_chain(read_data, 64, pos_fpga);

		return (read_data & 0xffffffff);
	}

	/// Read number of pulses that were written to DDR2-Trace memory
	unsigned int GetTracePulseCount()
	{
		uint64_t read_data = 0;
		set_jtag_instr_chain_noexec(CMD2_TRACE_COUNTS, pos_fpga);
		get_jtag_data_chain(read_data, 64, pos_fpga);

		return (read_data >> 32);
	}

	/// Read design version of FPGA firmware
	unsigned int GetFPGADesignVersion()
	{
		uint64_t read_data = 0;
		set_jtag_instr_chain_noexec(CMD2_GET_DESIGNVERSION, pos_fpga);
		get_jtag_data_chain(read_data, 32, pos_fpga);

		return read_data;
	}

	//////////////////////////////////////////////////////////////////////////
	//  hicann : one of 8 HICANNs
	//  addr   : 8x64 entries, 8 corresponds with l1 channel, 64 with nrn ID
	//  data   : 15 bit timestamp delta time (length of connection to target)
	bool FPGA_set_routing_data(unsigned char hicann, unsigned short addr, unsigned short data)
	{
		uint64_t data_route = ((uint64_t)(addr & 0x1ff) << 15) + (uint64_t)(data & 0x7fff);
		FPGA_set_channel(0x8 + (hicann & 0x7));
		FPGA_set_tx_data(data_route);
		FPGA_start_dnc_packet((0x1 << 9) + (0x1 << 8) + (0x4 & 0xff));
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	//  hicann : enable setting for HICANNs
	//  data   : 10 bit time limit [14:5] forwarding pulse event to HICANN
	bool FPGA_set_limit_data(unsigned char hicann, unsigned short data)
	{
		uint64_t data_route = (uint64_t)(data & 0x3ff);
		FPGA_set_channel(hicann & 0xff);
		FPGA_set_tx_data(data_route);
		FPGA_start_dnc_packet((0x1 << 9) + (0x1 << 8) + (0x20 & 0xff));
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	//  Send DNC confifuration packet for HICANN channels
	//  valids     : which of the configuration words is used
	//  chnl_reset : 8 bit reset for each hicann connection one bit
	//  init_mode  : 16 bit init setttings for hicann connection, 2bit for each link
	//  dc_bal     : 8 bit dc_balance mode for hicann connection
	//  proto_cfg  : 8 bit handshake protocol for configuration packets
	//  proto_pls  : 8 bit handshake protocol for pulse event packets
	//  mode500    : 8 bit enable for 500Mhz mode
	//  timestamp  : 8 bit timestamp mode in DNC channels
	//  crc_reset  : 8 bit reset for crc counter
	bool FPGA_set_dnc_cfg(
		unsigned char valids,
		unsigned char chnl_reset,
		unsigned short init_mode,
		unsigned char dc_bal,
		unsigned char proto_cfg,
		unsigned char proto_pls,
		unsigned char mode500,
		unsigned char timestamp,
		unsigned char crc_reset)
	{
		unsigned int data0 = (((uint32_t) proto_cfg) << 24) | (((uint32_t) proto_pls) << 16) |
							 (((uint32_t) mode500) << 8) | (((uint32_t) timestamp) << 0);
		unsigned int data1 = (((uint32_t) chnl_reset) << 24) | (((uint32_t) init_mode) << 8) |
							 (((uint32_t) dc_bal) << 0);
		unsigned int crc_valid = (crc_reset != 0) ? 0x01 : 0x00;
		FPGA_set_channel(valids | crc_valid);
		FPGA_set_tx_data((((uint64_t) data1) << 32) | ((uint64_t) data0));
		FPGA_start_dnc_packet((0x1 << 9) + (0x1 << 8) + (0x08 & 0xff));
		return true;
	}

	bool sendDncTimestampMode(unsigned char val)
	{
		this->FPGA_set_dnc_cfg(0x02, 0, 0, 0, 0, 0, 0, val, 0);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////
	// Fill FPGA playback fifo with data
	// cfg    : if True, write a config packet, else write a pulse
	// delay  : The number of dnc cycles to wait after the last pulse, before this packet/pulse is
	// released in the fpga.
	//          One cycle usually equals 4 ns, however, the last bit is ignored, as the FPGA runs
	//          with an 8 ns clock
	// hicann : the target hicann, only used when cfg is true
	// data   : if cfg = False, the data format is as follows (from MSB to LSB):
	//          12 bit address (3 bit hicann, 3 bit dnc if channel, 6 bit neuron address) and a 15
	//          bit delta_time. When a pulse is released, a pulse is generated with the given
	//          address and a 15 bit release timestamp: timestamp = ((curr_fpga_cycle << 1) +
	//          delta_time )& 0x7fff The delta_time is added to the current fpga clock cycle! the
	//          result is a dnc/hicann release timestamp
	// data1  : ??
	bool FPGA_fill_playback_fifo(
		bool cfg, unsigned short delay, unsigned char hicann, uint64_t data, unsigned int data1 = 0)
	{
		unsigned int channel = 0;
		uint64_t wdata = 0;
		if (cfg) {
			channel |= 1 << 23;
			channel |= hicann & 0x7;
			channel |= (((unsigned int) delay & 0x7fff) << 8);
			wdata = data;
		} else {
			channel |= (((unsigned int) delay & 0x7fff) << 8);
			wdata |= data & 0x07ffffff;
			wdata |= ((uint64_t) data1 & 0xffffffff) << 32;
		}
		FPGA_set_channel(channel);
		FPGA_set_tx_data(wdata);
		FPGA_set_fpga_ctrl(
			(filter_nrn0_mode << 9) + (pulsefifo_mode << 6) + ((unsigned int) 1 << 11));
		execute_instr();
		return true;
	}

	bool HICANN_set_bias(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_BIAS_CTRL, pos_hicann);
		set_jtag_data_chain(wdata, 6, pos_hicann);

		return true;
	}

	bool HICANN_set_test(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_SET_TEST_CTRL, pos_hicann);
		set_jtag_data_chain(wdata, 4, pos_hicann);

		return true;
	}

	bool HICANN_set_reset(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_SYSTEM_ENABLE, pos_hicann);

		set_jtag_data_chain(wdata, 1, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////
	// link_settings[0] = init_ctrl.auto_init
	// link_settings[1] = init_ctrl.init_master
	// link_settings[2] = proto_cfg
	// link_settings[3] = proto_pls
	// link_settings[4] = dc_bal_ctrl
	// link_settings[5] = lowspeed_mode
	// link_settings[6] = use_timestamp
	// link_settings[7] = loopback_en
	// link_settings[8] = use_speed_detect
	bool HICANN_set_link_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_LINK_CTRL, pos_hicann);

		set_jtag_data_chain(wdata, 9, pos_hicann);

		return true;
	}

	template <typename T>
	bool HICANN_set_clk_delay(T wdata, T& rdata)
	{
		set_jtag_instr_chain(CMD3_SET_DELAY_RX_CLK, pos_hicann);
		set_get_jtag_data_chain(wdata, rdata, 6, pos_hicann);

		return true;
	}

	template <typename T>
	bool HICANN_set_data_delay(T wdata, T& rdata)
	{
		set_jtag_instr_chain(CMD3_SET_DELAY_RX_DATA, pos_hicann);
		set_get_jtag_data_chain(wdata, rdata, 6, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	///  Access to current bias block in tx1rx1 macro
	///  wdata.size() = 15
	///  1 times 15 bit
	///  current set: 	14--------------0
	///               	TX_PAD RX_PAD DES
	template <typename T>
	bool HICANN_set_ibias(const T& wdata, T& rdata)
	{
		set_jtag_instr_chain(CMD3_SET_IBIAS, pos_hicann);

		set_get_jtag_data_chain(wdata, rdata, 15, pos_hicann);
		// set_get_jtag_data(wdata,rdata,15);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Configuration of the direction of the Layer1 busses
	/// Towards L1=1 Towards L2=0
	/// Default=0
	bool HICANN_set_layer1_mode(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD3_LAYER1_MODE, pos_hicann);

		set_jtag_data_chain(wdata, 8, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Sets 2in1 mode for pulse event packet transmission.
	/// Default=1
	bool HICANN_set_double_pulse(unsigned char wdata)
	{
		set_jtag_instr_chain(CMD3_SET_2XPLS, pos_hicann);
		set_jtag_data_chain(wdata, 1, pos_hicann);

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Collection of functions for pulse packet transmission
	bool HICANN_start_pulse_packet(uint64_t wdata)
	{
		HICANN_set_tx_data(wdata);
		HICANN_start_pulse_pkg();

		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Controll access to Faraday PLL
	bool HICANN_set_pll_far_ctrl(unsigned short wdata)
	{
		set_jtag_instr_chain(CMD3_PLL_FAR_CTRL, pos_hicann);
		set_jtag_data_chain(wdata, 15, pos_hicann);

		return true;
	}

	// PLL control register defined in "jtag_hicann.sv"
	bool HICANN_set_pll_far_ctrl(
		uint32_t ms, uint32_t ns, uint32_t pdn, uint32_t frange, uint32_t test)
	{
		int wdata = ((ms & 0x3f) << 9) | ((ns & 0x3f) << 3) | ((pdn & 0x01) << 2) |
					((frange & 0x01) << 1) | ((test & 0x01) << 0);
		return HICANN_set_pll_far_ctrl(wdata);
	}

	//////////////////////////////////////////////////////////////////////////////
	// ARQ control functions
	//////////////////////////////////////////////////////////////////////////////

	// write control reg
	bool HICANN_arq_write_ctrl(unsigned short wdata0, unsigned short wdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_CTRL, pos_hicann);
		unsigned long wdata = ((unsigned long) wdata0 << 16) | wdata1;
		set_jtag_data_chain(wdata, 32, pos_hicann);
		return true;
	}

	// write tx timeout
	bool HICANN_arq_write_tx_timeout(unsigned short wdata0, unsigned short wdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_TXTOVAL, pos_hicann);
		unsigned long wdata = ((unsigned long) wdata0 << 16) | wdata1;
		set_jtag_data_chain(wdata, 32, pos_hicann);
		return true;
	}

	// write rx timeout
	bool HICANN_arq_write_rx_timeout(unsigned short wdata0, unsigned short wdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXTOVAL, pos_hicann);
		unsigned long wdata = ((unsigned long) wdata0 << 16) | wdata1;
		set_jtag_data_chain(wdata, 32, pos_hicann);
		return true;
	}

	// read tx timeout num
	bool HICANN_arq_read_tx_timeout_num(unsigned short& rdata0, unsigned short& rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_TXTONUM, pos_hicann);
		uint64_t rdata;
		get_jtag_data_chain(rdata, 32, pos_hicann);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read tx timeout num
	bool HICANN_arq_read_rx_timeout_num(unsigned short& rdata0, unsigned short& rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXTONUM, pos_hicann);
		uint64_t rdata;
		get_jtag_data_chain(rdata, 32, pos_hicann);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read tx packet num
	bool HICANN_arq_read_tx_packet_num(unsigned short& rdata0, unsigned short& rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_TXPCKNUM, pos_hicann);
		uint64_t rdata;
		get_jtag_data_chain(rdata, 32, pos_hicann);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read rx packet num
	bool HICANN_arq_read_rx_packet_num(unsigned short& rdata0, unsigned short& rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXPCKNUM, pos_hicann);
		uint64_t rdata;
		get_jtag_data_chain(rdata, 32, pos_hicann);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	// read rx drop num
	bool HICANN_arq_read_rx_drop_num(unsigned short& rdata0, unsigned short& rdata1)
	{
		set_jtag_instr_chain(CMD3_ARQ_RXDROPNUM, pos_hicann);
		uint64_t rdata;
		get_jtag_data_chain(rdata, 32, pos_hicann);
		rdata0 = rdata & 0xffff;
		rdata1 = rdata >> 16;
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////
	/// Collections of functions for OCP packet transmission
	bool HICANN_submit_ocp_packet(uint64_t wdata)
	{
		HICANN_set_test(0xc);
		HICANN_set_tx_data(wdata);
		HICANN_start_cfg_pkg();

		HICANN_set_test(0x0);
		return true;
	}

	/// Returns last received data packet, requires that last packet is the requested one.
	bool HICANN_receive_ocp_packet(uint64_t& rdata)
	{
		HICANN_get_rx_data(rdata);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Collection of functions for pulse packet transmission to layer 1
	bool HICANN_start_l1_pulse_packet(uint64_t wdata)
	{
		HICANN_set_test(0xa);
		HICANN_set_tx_data(wdata);
		HICANN_start_pulse_pkg();
		HICANN_set_test(0x0);

		return true;
	}

	//#####################################################################
	// I2C specific stuff
	// in case other devices are present in chain, set themm all to BYPASS
	// before executing any of the following
	// all commands rely on the fact the FPGA is first device in chain

	// ***********************************************
	bool FPGA_i2c_setctrl(unsigned char ctrladdr, unsigned char ctrlreg)
	{
		set_jtag_instr_chain(CMD_I2C_SET_DATA_EXEC, pos_fpga);

		uint32_t data = 1 << 13 | ctrladdr << 8 | ctrlreg;
		set_jtag_data_chain(data, JTAG_I2C_DR_WIDTH, pos_fpga);

		return true;
	}

	// ***********************************************
	bool FPGA_i2c_getctrl(unsigned char ctrladdr, unsigned char& ctrlreg)
	{
		set_jtag_instr_chain(CMD_I2C_SET_DATA_EXEC, pos_fpga);

		uint32_t data = 0 << 13 | ctrladdr << 8;
		set_jtag_data_chain(data, JTAG_I2C_DR_WIDTH, pos_fpga);

		set_jtag_instr_chain(CMD_I2C_GET_DATA, pos_fpga);

		uint64_t ctrlreg_tmp;
		get_jtag_data_chain(ctrlreg_tmp, 8, pos_fpga);
		ctrlreg = (unsigned char) ctrlreg_tmp;
		return true;
	}

	// ***********************************************
	bool FPGA_i2c_byte_write(unsigned char data, bool start, bool stop, bool nack)
	{
		FPGA_i2c_setctrl(ADDR_TXREG, data);

		uint32_t cmd = 0 | start << 7 | stop << 6 | 1 << 4 | nack << 3;
		FPGA_i2c_setctrl(ADDR_CMDREG, cmd);

		usleep(2000);

		return true;
	}

	// ***********************************************
	bool FPGA_i2c_byte_read(bool start, bool stop, bool nack, unsigned char& data)
	{
		uint32_t cmd = 0 | start << 7 | stop << 6 | 2 << 4 | nack << 3;
		FPGA_i2c_setctrl(ADDR_CMDREG, cmd);

		usleep(2000);

		FPGA_i2c_getctrl(ADDR_RXREG, data);

		return true;
	}

	// end I2C specific stuff
	//#####################################################################

	//////////////////////////////////////////////////////////////////////////////
	/// Help function for bool std::vector print out
	void printBoolVect(std::vector<bool> v)
	{
		unsigned int hexval = 0;
		for (unsigned int nbit = v.size(); nbit > 0;) {
			--nbit;
			if (v[nbit])
				hexval += (1 << (nbit % 4));

			if ((nbit % 4) == 0) {
				printf("%X", hexval);
				hexval = 0;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Help function for bool std::vector print out
	unsigned char getVecDelay(unsigned char channel, std::vector<bool> v)
	{
		unsigned char hexval = 0;
		for (int nbit = v.size(); nbit > 0;) {
			--nbit;
			if ((nbit < ((channel + 1) * 6)) && (nbit >= (channel * 6))) {
				if (v[nbit])
					hexval += (1 << (nbit % 6));
			}
		}
		return hexval;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void printDelayFPGA(const std::vector<bool>& v)
	{
		printDelayFPGA(v, std::cout);
		std::cout << std::endl;
	}

	template <typename T>
	void printDelayFPGA(const std::vector<bool>& v, T& out)
	{
		out << ("15;14;13;12;11;10;9;8;7;6;5;4;3;2;1;0\n");
		unsigned int hexval = 0;
		for (unsigned int nbit = 6 * 16; nbit > 0;) {
			--nbit;
			if (v[nbit])
				hexval += (1 << (nbit % 6));

			if ((nbit % 6) == 0) {
				out << std::dec << hexval << ";";
				hexval = 0;
			}
		}
	}

	// Kintex7 FPGA JTAG commands

	///////////////////////////
	// Status bits
	// bit      0: IDLE + pulse event idle
	// bit      1: link uninitialized
	// bit      2: last packet witht CRC error
	// bit      3: 500 MHz communication speed (fix)
	// bit      4: receives valid configuration packet
	// bit      5: receives valid pulse event packet
	// bit      6: IDLE + configuration idle
	// bit      7: last received pulse packet contained two events
	// bits  15:8: CRC count
	// bits 29:16: FPGA systime counter
	// bits 35:30: current state of highspeed initialization state machine
	// bits 43:36: current rx data
	// bits 51:44: current tx data
	// bits 63:52: number of dropped pulses at FPGA TX FIFO
	bool K7FPGA_get_hicannif_status(uint64_t& status)
	{
		set_jtag_instr_chain(K7CMD_DNCIF_STATE, pos_fpga);
		get_jtag_data_chain(status, 64, pos_fpga);
		return true;
	}

	bool K7FPGA_get_hicannif_rx_cfg(uint64_t& cfg_packet)
	{
		set_jtag_instr_chain(K7CMD_DNCIF_RXCFG, pos_fpga);
		get_jtag_data_chain(cfg_packet, 64, pos_fpga);
		return true;
	}

	template <typename T>
	bool K7FPGA_get_dncif_rx_pls(T& pls_packet)
	{
		set_jtag_instr_chain(K7CMD_DNCIF_RXPLS, pos_fpga);
		get_jtag_data_chain(pls_packet, 24, pos_fpga);
		return true;
	}

	template <typename T>
	bool K7FPGA_get_hicannif_data_delay(T& delay_val)
	{
		set_jtag_instr_chain(K7CMD_DNCIF_DELAY, pos_fpga);
		get_jtag_data_chain(delay_val, 5, pos_fpga);
		delay_val &= 0x1f;
		return true;
	}

	///////////////////////////
	// Set link configuration
	// bit      0: start_link
	// bit      1: loopback_en
	// bit      2: auto_init
	// bit      3: init_master
	// bit      4: use_timestamp
	// bit      5: auto_limit_i
	// bit      6: pulse_protocol
	// bit      7: config_protocol
	// bit  15: 8: heap_mode / l1 direction
	// bit  26:16: limit
	// bit     27: dc_coding
	// bit     28: invert rx_data (8Bit deserialized data)
	template <typename T>
	bool K7FPGA_set_hicannif_config(T config)
	{
		hicannif_conf[hicannif_nr] = config;
		set_jtag_instr_chain(K7CMD_DNCIF_CONFIG, pos_fpga);
		set_jtag_data_chain(config, 29, pos_fpga);
		return true;
	}

	///////////////////////////
	// Start link action
	// bit      0: tx_pulse_event_en
	// bit      1: tx_config_data_en
	// bit      2: routing_data_en
	// bit      3: channel_reset
	// bit      4: crc_count_rst
	// bit      5: reset tx dropped pulse count
	template <typename T>
	bool K7FPGA_set_hicannif_control(T control)
	{
		set_jtag_instr_chain(K7CMD_DNCIF_CTRLS, pos_fpga);
		set_jtag_data_chain(control, 6, pos_fpga);
		return true;
	}

	template <typename T>
	bool K7FPGA_set_hicannif_tx_data(T txdata)
	{
		set_jtag_instr_chain(K7CMD_DNCIF_TXDAT, pos_fpga);
		set_jtag_data_chain(txdata, 64, pos_fpga);
		return true;
    }

	template <typename T>
	bool K7FPGA_set_hicannif_data_delay(T delay_val)
	{
		unsigned int write_val = (1 << 5) | (delay_val & 0x1f);
		set_jtag_instr_chain(K7CMD_DNCIF_DELAY, pos_fpga);
		set_jtag_data_chain(write_val, 6, pos_fpga);
		return true;
	}

	bool K7FPGA_invert_rx_data(bool invert)
	{
		hicannif_conf[hicannif_nr] = hicannif_conf[hicannif_nr] & ~(0x1<<28);
		hicannif_conf[hicannif_nr] = hicannif_conf[hicannif_nr] | ((unsigned int)invert<<28);
		K7FPGA_set_hicannif_config(hicannif_conf[hicannif_nr]);
		return true;
	}

	bool K7FPGA_start_fpga_link()
	{
		hicannif_conf[hicannif_nr] = hicannif_conf[hicannif_nr] | 0x1;
		K7FPGA_set_hicannif_config(hicannif_conf[hicannif_nr]);
		return true;
	}

	bool K7FPGA_stop_fpga_link()
	{
		hicannif_conf[hicannif_nr] = hicannif_conf[hicannif_nr] & ~((unsigned int)0x1);
		K7FPGA_set_hicannif_config(hicannif_conf[hicannif_nr]);
		return true;
	}

	bool K7FPGA_disable_hicannif_config_output()
	{
		set_jtag_instr_chain(K7CMD_DNCIF_DIS_CONFIG, pos_fpga);
		set_jtag_data_chain(1, 1, pos_fpga);
		return true;
	}

	bool K7FPGA_enable_hicannif_config_output()
	{
		set_jtag_instr_chain(K7CMD_DNCIF_DIS_CONFIG, pos_fpga);
		set_jtag_data_chain(0, 1, pos_fpga);
		return true;
	}

	void K7FPGA_set_hicannif(unsigned char hicann_id)
	{
		set_jtag_instr_chain(K7CMD_HICANN_CHNL, pos_fpga);
		set_jtag_data_chain(hicann_id, 3, pos_fpga);
		hicannif_nr = hicann_id;
	}

	unsigned char K7FPGA_get_hicannif() { return hicannif_nr; }

	// configure neuron address filter, located behind each HICANN interface in FPGA.
	// affects all pulses delivered by that hicann_if (9bit)
	// (set with K7FPGA_set_hicannif before using this command)
	// masked means the corresponding bit has a 0 in the filter mask
	// pos. filter mask: all pulses with at least one masked address bit active are filtered
	// neg. filter mask: all pulses with all masked address bits unset
	// example: all pulses with addr 0 on all SPL1 channels are filtered with neg. mask 9'b111000000
	void K7FPGA_set_neuron_addr_filter(unsigned int pos_filter_mask, unsigned int neg_filter_mask)
	{
		set_jtag_instr_chain(K7CMD_HICANNIF_NRNFILTER, pos_fpga);
		set_jtag_data_chain(
		    (((pos_filter_mask & 0x1ff) << 9) + (neg_filter_mask & 0x1ff)), 18, pos_fpga);
	}
};

#endif //__JTAG_CMDBASE__
