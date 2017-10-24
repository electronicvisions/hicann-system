#include "s2c_jtagphys_2fpga_arq.h"

// for JTAG: named socket
#include <sys/socket.h>
#include <sys/types.h>

#include <boost/chrono.hpp>

using namespace facets;
using namespace std;

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("hicann-system.S2C_JtagPhys2FpgaArq");


S2C_JtagPhys2FpgaArq::S2C_JtagPhys2FpgaArq(
	CommAccess const& access,
	myjtag_full* j,
	bool on_reticle,
	dncid_t dncid,
	std::shared_ptr<sctrltp::ARQStream> arq_ptr,
	bool use_k7fpga)
	: S2C_JtagPhys2Fpga(access, j, on_reticle, use_k7fpga),
	  dncid(dncid),
	  p_ARQStream(arq_ptr),
	  hostalctrl(access.getFPGAConnectionId().get_fpga_ip().to_ulong(), 1234),
	  bulk(0),
	  max_bulk(MAX_PDUWORDS)
{
#ifdef NCSIM
#ifndef FPGA_BOARD_BS_K7
	// set interface to RGMII for FACETS Virtex5 board
	eth_if* pEthIf = eth_if::getInstance();
	pEthIf->setIfType(ETH_IF_RGMII);
#endif
#endif
	hostalctrl.setARQStream(p_ARQStream.get());
#ifdef NCSIM
	hostalctrl.setEthernetIF(&(p_ARQStream.get()->pimpl->eth_soft));

	log(Logger::INFO) << "Initialisation start: " << /*sc_simulation_time() <<*/ endl;
	if (hostalctrl.initFPGAConnection(0x112)) {
		log(Logger::INFO) << "TUD_TESTBENCH:NOTE:TESTPASS: Transport Layer initialisation sequence "
							 "performed successfully.";
	}
#endif
}


S2C_JtagPhys2FpgaArq::~S2C_JtagPhys2FpgaArq()
{
}

S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Init(
	int hicann_jtag_nr, bool silent, bool force_highspeed_init, bool return_on_error)
{
	S2C_JtagPhys2FpgaArq::Commstate returnval =
		S2C_JtagPhys2Fpga::Init(hicann_jtag_nr, silent, force_highspeed_init, return_on_error);

	if (link_states.none()) {
		log(Logger::ERROR) << "Untested code path if no highspeed links are used.";
		throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + ": Not implemented/untested code path.");
	}
	// enable FPGA ARQ (has been disabled during above init):
	log(Logger::INFO) << "S2C_JtagPhys2FpgaArq::Init: Enabling HICANN-ARQ in FPGA";
	set_fpga_reset(jtag->get_ip(), false, false, false, false, /*ARQ:*/ false);

	// FIXME: make configurable
	if (!k7fpga)
		jtag->SetARQTimings(0xff, 0x0c8, 0x032);

	return returnval;
}

S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Init(
	std::bitset<8> hicann, bool silent, bool force_highspeed_init, bool return_on_error)
{
	return Init(hicann, hicann, silent, force_highspeed_init, return_on_error);
}

S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Init(
	std::bitset<8> hicann, std::bitset<8> highspeed_hicann,
	bool silent, bool force_highspeed_init, bool return_on_error)
{
	S2C_JtagPhys2FpgaArq::Commstate returnval =
		S2C_JtagPhys2Fpga::Init(hicann, highspeed_hicann, silent, force_highspeed_init, return_on_error);

	// maybe enable FPGA ARQ (has been disabled during above init):
	if (link_states.none()) {
		log(Logger::INFO) << "Not reenabling FPGAs HICANN-ARQ because we do not use highspeed links.";
	} else {
		// FIXME: Here be per-chip reset toggeling...
		log(Logger::INFO) << "Reenabling FPGAs HICANN-ARQ.";
		set_fpga_reset(jtag->get_ip(), false, false, false, false, /*ARQ:*/ false);
		// FIXME: make configurable
		if (!k7fpga)
			jtag->SetARQTimings(0xff, 0x0c8, 0x032);
	}

	return returnval;
}


S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Flush()
{
	if (bulk != 0)
		hostalctrl.sendHICANNConfigBulk();
	bulk = 0;
	hostalctrl.sendHICANNConfigBulk();

	// we do error handling via exceptions now FIXME
	return S2C_JtagPhys2Fpga::ok;
}

S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Clear()
{
	// non-senseical with real arq... we would have to re-sync the window
	return S2C_JtagPhys2Fpga::ok;
}


// fpga_ip and sysstart_port not required in this version - inherited from overridden
// S2C_JtagPhys2Fpga implementation
void S2C_JtagPhys2FpgaArq::trigger_systime(
	unsigned int const /*fpga_ip*/,
	bool const enable,
	bool const listen_global,
	uint const /*sysstart_port*/)
{
	hostalctrl.setSystime(enable, listen_global, false);
}


void S2C_JtagPhys2FpgaArq::trigger_experiment(
	unsigned int const /*fpga_ip*/,
	bool const enable,
	bool const listen_global,
	uint const /*sysstart_port*/)
{
	hostalctrl.setSystime(enable, listen_global, true);
}


int S2C_JtagPhys2FpgaArq::issueCommand(uint hicann_nr, uint tagid, ci_payload* data, uint del)
{
	if (!link_states[jtag2dnc(hicann_nr)]) {
		LOG4CXX_DEBUG(logger, "diverting issueCommand to JTAG-based HICANN-ARQ");
		return S2C_JtagPhys2Fpga::issueCommand(hicann_nr, tagid, data, del);
	}

	// clang-format off
	hostalctrl.addHICANNConfigBulk(
		/* 64 bit "payload": */
		/* fast  */ (static_cast<uint64_t>(tagid)                           << 49) |
		/* write */ (static_cast<uint64_t>(data->write)                     << 48) |
		/* addr  */ (static_cast<uint64_t>(data->addr)                      << 32) |
		/* data  */ ((data->write) ? static_cast<uint32_t>(data->data) : (1 << 31)),
		/* opt dnc id */ dncid,
		/* opt hicann id */ jtag2dnc(hicann_nr)
		/* opt timeout */
	);
	// clang-format on

	// explicit flush if ethernet frame is full
	if ((++bulk % max_bulk) == 0) {
		log(Logger::DEBUG0) << "flushing frame, was full " << bulk;
		Flush();
	}
	return 1; // same as in s2comm::issueCommand()
}

int S2C_JtagPhys2FpgaArq::recvData(uint hicann_nr, uint tagid, ci_payload* data)
{
	if (!link_states[jtag2dnc(hicann_nr)]) {
		LOG4CXX_DEBUG(logger, "diverting recvData to JTAG-based HICANN-ARQ");
		return S2C_JtagPhys2Fpga::recvData(hicann_nr, tagid, data);
	}

	// we have to flush all the write commands first
	Flush();
	uint64_t tmp = 0;
	try {
		tmp = hostalctrl.getReceivedHICANNConfig(dncid, jtag2dnc(hicann_nr), tagid);
	} catch (std::underflow_error const& e) {
		log(Logger::ERROR) << *this;
		throw e;
	}
	data->addr = (tmp >> 32) & 0xffff;
	data->data = static_cast<ci_data_t>(tmp);
	return 1; // FIXME l1switch_control.cpp seems to be fubar?
}

namespace facets {

std::ostream& operator<<(std::ostream& o, S2C_JtagPhys2FpgaArq const& s)
{
	// please note, the jtag commands to read-out the state registers are not
	// always built into the firmware

	jtag_cmdbase::arqcounters_t cntrs;
	uint32_t rcounter, wcounter;
	jtag_cmdbase::arqdebugregs_t dregs;

	if (s.k7fpga) {
		jtag_cmdbase::arqdout_t dout = s.jtag->K7FPGA_GetARQDout();
		o << "ARQ Sequence counters after experiment (ARQDout): "
		  << " " << dec << dout[0] << " " << dout[1] << " " << dout[2] << " " << dout[3] << " "
		  << dout[4] << " " << dout[5] << " " << dout[6] << " " << dout[7];
		cntrs = s.jtag->K7FPGA_GetARQCounters();
		dregs = s.jtag->K7FPGA_GetARQNetworkDebugReg();
		rcounter = cntrs.first, wcounter = cntrs.second;
	} else /*Virtex-5*/ {
		// get all arq seq numbers from all (4) segments
		for (int ii = 0; ii < 4; ii++) {
			jtag_cmdbase::arqdout_t dout = s.jtag->GetARQDout(ii);
			o << "ARQ Sequence counters after experiment (ARQDout): "
			  << " " << dec << dout[0] << " " << dout[1] << " " << dout[2] << " " << dout[3] << " "
			  << dout[4] << " " << dout[5] << " " << dout[6] << " " << dout[7];
		}
		cntrs = s.jtag->GetARQCounters();
		dregs = s.jtag->GetARQNetworkDebugReg();
		rcounter = cntrs.first, wcounter = cntrs.second;
	}

	o << "ARQ read (from HW) counter: " << dec << (rcounter & 0x7fffffff) << " (read fifo "
	  << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
	  << "ARQ write (to HW) counter: " << (wcounter & 0x7fffffff) << " ("
	  << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)";
	o << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
	o << "\n(for details how to interpret this error, see "
		 "https://brainscales-r.kip.uni-heidelberg.de:6443/visions/pl/638gkrfbjtgwzjqaxr94t1hgpr "
		 ")\n";
	return o;
}

} // facets
