///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate$
// $LastChangedRevision$
// $LastChangedBy$

#ifndef __JTAG_CMDBASE_FPGADNCHICANN_FULL__
#define __JTAG_CMDBASE_FPGADNCHICANN_FULL__

#include <vector>
#include <arpa/inet.h>
#include "jtag_access.hpp"
#include "jtag_cmdbase.h"
#include "logger.h"
extern "C" {
#include <stdint.h>
}


//////////////////////////////////////////////////////////////////////////////////
/// .
class jtag_cmdbase_fpgadnchicann_full
	: public jtag_cmdbase
	, public jtag_lib_v2::jtag_access
{
protected:
	// "new" logger class
	Logger& log;

public:
	// shortcuts
	typedef uint32_t uint;
	typedef uint64_t uint64;

	//////////////////////////////////////////////////////////////////////////////
	/// .
	// supply info wether FPGA/DNC is present, bitset of physical availabe HICANNs (HS Channel
	// numbering), the HICANN in chain to talk with and if Kintex7 is used
	jtag_cmdbase_fpgadnchicann_full(
		bool fpga_avail,
		bool dnc_avail,
		std::bitset<8> available_hicanns,
		uint hicann_addr,
		bool k7fpga = false)
		: jtag_cmdbase(fpga_avail, dnc_avail, available_hicanns, hicann_addr, k7fpga),
		  log(Logger::instance("hicann-system.jtag_cmdbase_fpgadnchicann_full", Logger::INFO, "")),
		  fpga_ip(0)
	{}

	// depricated ctor with number of hicanns in jtagchain instead ob bitset
	jtag_cmdbase_fpgadnchicann_full(
		bool fpga_avail, bool dnc_avail, uint hicann_nr, uint hicann_addr, bool k7fpga = false)
		: jtag_cmdbase(fpga_avail, dnc_avail, hicann_nr, hicann_addr, k7fpga),
		  log(Logger::instance("hicann-system.jtag_cmdbase_fpgadnchicann_full", Logger::INFO, "")),
		  fpga_ip(0)
	{}

	jtag_cmdbase_fpgadnchicann_full()
		: jtag_cmdbase(
			  false,
			  false,
			  1,
			  0,
			  false), // default: no FPGA, no DNC, 1 HICANN at addr 0, not on kintex
		  log(Logger::instance("hicann-system.jtag_cmdbase_fpgadnchicann_full", Logger::INFO, "")),
		  fpga_ip(0)
	{}

	~jtag_cmdbase_fpgadnchicann_full() {}


private:
	uint32_t fpga_ip;

public:
	unsigned int get_ip() { return fpga_ip; }

	unsigned int ip_number(unsigned char v3, unsigned char v2, unsigned char v1, unsigned char v0)
	{
		return (v3 << 0x18 | v2 << 0x10 | v1 << 0x08 | v0);
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_hicann_pos(uint hicann_addr)
	{
		pos_hicann = hicann_addr;
		return;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	uint get_hicann_pos() const { return pos_hicann; }

	//////////////////////////////////////////////////////////////////////////////
	/// .
	bool reset_jtag(void)
	{
		this->resetJtag();
		return true;
	}

	bool initJtagV2(unsigned int ip, unsigned int port, unsigned int speed)
	{
		struct in_addr addr;
		addr.s_addr = htonl(ip);
		fpga_ip = ip;
		char* szRemoteIP = inet_ntoa(addr);

		if (!this->createSession(szRemoteIP, port))
			return false;

		uint8_t uiCables;
		if (!this->enumCables(uiCables))
			return false;

		if (!this->openCable(0, speed, true))
			return false;

		if (chain_length != hicann_num + 1)
			return false;

		if (!this->setDeviceCount(chain_length))
			return false;

		if (!this->setDeviceInfo(0, 4, 0x1c56c007))
			return false;

		for (unsigned int nhicann = 0; nhicann < hicann_num; ++nhicann) {
			if (!this->setDeviceInfo(nhicann + 1, IRW_HICANN, 0x14849434))
				return false;
		}

		for (unsigned int ndev = 0; ndev < chain_length; ++ndev) {
			uint64_t curr_id = 0;
			this->read_id(curr_id, ndev);
		}

		return true;
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_instr_chain(unsigned int command, unsigned char position)
	{
		this->setJtagInstr(chain_length - 1 - position, command);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_instr_chain_noexec(unsigned int command, unsigned char position)
	{
		this->setJtagInstr(chain_length - 1 - position, command);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void execute_instr(void) { this->navigateState(jtag_lib_v2::RUN_TEST_IDLE, 1); }

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_data_chain(uint64_t wdata, unsigned char width, unsigned char position)
	{
		this->setJtagData(chain_length - 1 - position, wdata, width, true);
	}


	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_jtag_data_chain_noexec(uint64_t wdata, unsigned char width, unsigned char position)
	{
		this->setJtagData(chain_length - 1 - position, wdata, width, false);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// Shift through all jtag_dr registers to access all HICANNs in parallel
	void set_jtag_hicanns_data_chain(uint64_t wdata)
	{
		std::vector<uint64_t> data_vect;
		std::vector<uint8_t> length_vect;
		for (int j = chain_length-1; j >= 0; --j) {
			if (((int)pos_fpga != j) && ((int)pos_dnc != j)) {
				data_vect.push_back(wdata);
				length_vect.push_back(64);
			} else {
				data_vect.push_back(1);
				length_vect.push_back(1);
			}
		}

		this->setJtagDataAll(&data_vect[0], &length_vect[0], true);
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void get_jtag_data_chain(uint64_t& rdata, unsigned char width, unsigned char position)
	{
		this->getJtagData(chain_length - 1 - position, rdata, width);
	}

	// TODO: fill FIXME?
	void get_jtag_all_data_chain(std::vector<uint64_t>& /*rdata*/, unsigned char /*width*/)
	{
		// throw std::runtime_error("Not implemented");
		std::cerr << "Not implemented: " << __PRETTY_FUNCTION__ << std::endl;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .
	void set_get_jtag_data_chain(
		uint64_t wdata, uint64_t& rdata, unsigned int count, unsigned char position)
	{
		this->setGetJtagData(chain_length - 1 - position, wdata, rdata, count);
	}

	////////////////////////////////////////////////////////////////////////////
	/// Special JTAG function to set all JTAG_IR of HICANNs with same value
	void set_hicanns_ir_chain (unsigned char cmd)
	{
		std::vector<uint32_t> cmd_vect;
		for (int j = chain_length-1; j >= 0; --j) {
			if (((int)pos_fpga != j) && ((int)pos_dnc != j)) {
				cmd_vect.push_back(cmd);
			} else {
				cmd_vect.push_back((1 << get_irw(j)) - 1); // all cmd bits 1 -> bypass command
			}
		}

		this->setJtagInstrAll(&cmd_vect[0]);
	}

	// TODO: fill FIXME?
	void set_all_ir_chain(unsigned char /*cmd*/)
	{
		// throw std::runtime_error("Not implemented");
		std::cerr << "Not implemented: " << __PRETTY_FUNCTION__ << std::endl;
	}

	//////////////////////////////////////////////////////////////////////////////
	/// .

	void set_get_jtag_data_chain(
		const std::vector<bool> wdata, std::vector<bool>& rdata, unsigned char position)
	{
		this->setGetJtagData(chain_length - 1 - position, wdata, rdata, wdata.size());
	}
};

#endif //__JTAG_CMDBASE_FPGADNCHICANN_FULL__
