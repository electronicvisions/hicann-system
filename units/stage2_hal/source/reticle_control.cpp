// Company         :   kip
// Author          :   Alexander Kononov
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                 :
// Filename        :   reticle_control.cpp
//                 :
// Create Date     :   Wed Apr  4 2012
// Last Change     :   Fri Jul  6 2012
// by              :   akononov
//------------------------------------------------------------------------

#include <arpa/inet.h> //UDP socket stuff etc.

#include "dnc_control.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "iboardv2_ctrl.h"
#include "reticle_control.h"
#include "s2comm.h"
#include "stage2_conf.h"

#include "HandleCommFreezeError.h"
#include "logger_ncsim_compat.h"

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#ifndef NCSIM
static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("hicann-system.ReticleControl");
#endif

using namespace std;
using namespace facets;

std::multiset<uint> & ReticleControl::instantiated() {
	static std::multiset<uint>* _i = NULL;
	if (!_i)
		_i = new std::multiset<uint>;
	return *_i; // DO NOT DELETE (has to be alive until process termination)
				// talk to Eric if you think you know better
}

void registerHandleCommFreeze()
{
	// stdout redirection (handling recvfrom 11 errors...)
	static HandleCommFreezeError* error = NULL;
	if (!error)
		error = new HandleCommFreezeError;
	// DO NOT DELETE (has to be alive until process termination => talk to Eric
	// if you think you know better)
}

// initialize communication
void ReticleControl::init(bool on_wafer = true)
{
	{
		std::stringstream msg;
		msg << "Initialize connection to FPGA IP " << fpga_ip[0] << "." << fpga_ip[1] << "."
			<< fpga_ip[2] << "." << fpga_ip[3] << ":" << jtag_port;
		visionary_logger::write_to_syslog(msg.str());
	}

	FPGAConnectionId connid(fpga_ip);
	connid.set_jtag_port(jtag_port);

	// CommAccess registers signal handlers but the "freeze handling code"
	// should use no signal handling (otherwise we might call same cleanup code
	// for child too)
	registerHandleCommFreeze(); // because jtaglib might fu** up and hang forever

	// get lock before accessing hardware
	access.reset(new CommAccess(connid, pmu_ip));

	if (arq_mode) {
		LOG4CXX_INFO(logger, "Activating HICANN-ARQ communication mode");
		model = jtag_eth_fpga_arq;
	} else {
		throw std::runtime_error("The non-ARQ communication mode is not compatible with ReticleControl");
	}

	std::shared_ptr<sctrltp::ARQStream> p_hostarq;
	if (model == jtag_eth_fpga_arq) {
		FPGAConnectionId::IPv4 tmp((fpga_ip));
		// FIXME: add option for kintex-based access
		// FIXME: get dnc id nicer!
#ifdef FPGA_BOARD_BS_K7
		p_hostarq.reset(new sctrltp::ARQStream(
			tmp.to_string(), "192.168.0.128", /*listen port*/ 1234, "192.168.0.1",
			/*target port*/ 1234)); // TODO: set correct host IP
#else
		p_hostarq.reset(new sctrltp::ARQStream(
			tmp.to_string(), "192.168.1.2", /*listen port*/ 1234, tmp.to_string().c_str(),
			/*target port*/ 1234));
#endif

		// shut up the HICANN-ARQ
		Stage2Comm::set_fpga_reset(tmp.to_uint(),
			/*enable_core*/ false,
			/*enable_fpgadnc*/ false,
			/*enable_ddr2onboard*/ false,
			/*enable_ddr2sodimm*/ false,
			/*enable_arq*/ true);

		// HICANNs seem to send garbage config packets during reset, drop them (issue #2889)
		size_t dropped_packets = p_hostarq->drop_receive_queue();
		while (dropped_packets != 0) {
			LOG4CXX_ERROR(
				logger, "ReticleControl::init() on FPGA " << tmp.to_string() << " dropped"
				<< dropped_packets << " packet(s) after HostARQ reset (cf. #2889)");
			usleep(100*1000); // 100ms
			dropped_packets = p_hostarq->drop_receive_queue();
		}
	}

	jtag.reset(new myjtag_full(true, /*dnc?*/ !kintex, physically_available_hicanns, 0, kintex));
	if (!jtag->initJtag(jtag_lib_v2::JTAG_ETHERNET))
		throw std::runtime_error("JTAG open failed!");

	// Connectors on cube setup are not as robust as elastomeric connectors on wafer systems
	// -> reduced JTag frequency for cube setups
	size_t const jtag_frequency_in_kHz = on_wafer ? 10000 : 750;

	LOG4CXX_DEBUG(logger, "Setting JTAG frequency to " << jtag_frequency_in_kHz << " kHz");

	if (model == jtag_wafer_fpga) {
		if (!jtag->initJtagV2(
		        jtag->ip_number(fpga_ip[0], fpga_ip[1], fpga_ip[2], fpga_ip[3]), jtag_port,
		        jtag_frequency_in_kHz)) {
			throw std::runtime_error("JTAG init failed!");
		}

		LOG4CXX_INFO(logger, "Initialized JTAG over ETH communication");


		jtag_p2f.reset(new S2C_JtagPhys2Fpga(*access.get(), jtag.get(), on_wafer));
		comm = jtag_p2f.get();
	} else if (model == jtag_eth_fpga_arq) {
		if (!jtag->initJtagV2(p_hostarq, jtag_frequency_in_kHz)) {
			throw std::runtime_error("JTAG init failed!");
		}
		LOG4CXX_INFO(logger, "Initialized JTAG (over Host-ARQ) communication");

		jtag_p2f.reset(new S2C_JtagPhys2FpgaArq(
			*access.get(), jtag.get(), on_wafer, jtag_port - 1700, p_hostarq, kintex));
		comm = jtag_p2f.get();
	} else {
		jtag_p.reset(new S2C_JtagPhys(*access.get(), jtag.get(), on_wafer));
		comm = jtag_p.get();
	}

	fc.reset(new FPGAControl(comm, s_number)); // use reticle sequential number for labeling
	dc.reset(new DNCControl(comm, s_number));
	// available_hicanns represents the physical availabel hicanns in hs-channel numbering
	// because JTAG address space is contiguous (indexing of hicann bitset) we use .count() (number
	// of 1s)
	for (uint i = 0; i < physically_available_hicanns.count(); i++)
		hicann[i].reset(new HicannCtrl(comm, i));
	used_hicanns.reset(); // no HICANNs are in use
}

ReticleControl::ReticleControl(
    size_t seq_number,
    ip_t _ip,
    uint16_t port,
    FPGAConnectionId::IPv4 const pmu_ip,
    std::bitset<8> physically_available_hicanns,
    std::bitset<8> highspeed_hicanns,
    bool on_wafer,
    bool _arq_mode,
    bool _kintex)
    : Stage2Ctrl(NULL, 0),
      physically_available_hicanns(physically_available_hicanns),
      highspeed_hicanns(highspeed_hicanns),
      pmu_ip(pmu_ip),
      on_wafer(on_wafer),
      arq_mode(_arq_mode),
      kintex(_kintex)
{
	LOG4CXX_INFO(
		logger, "Creating ReticleControl instance for reticle IP/FPGA "
					<< _ip << "/" << ((_ip.d - 1) % 32 + int(_ip.d / 32) * 12) << " (on wafer "
					<< _ip.c << ") port " << port);

	// create fake reticle to comply with other functionality

	bool already_existing = isInstantiated(seq_number);
	if (already_existing)
		LOG4CXX_FATAL(logger, "Multiple instances for the same reticle are not allowed :P");

	instantiated().insert(seq_number);
	// initializing all reticle-specific variables
	fpga_ip[0] = _ip.a;
	fpga_ip[1] = _ip.b;
	fpga_ip[2] = _ip.c;
	fpga_ip[3] = _ip.d;
	jtag_port = port;
	s_number = seq_number;

	if (!already_existing)
		init(on_wafer);
}

ReticleControl::~ReticleControl()
{
	LOG4CXX_INFO(logger, "Deleting ReticleControl instance for reticle (" << s_number << ")");
	comm->reset_hicann_arq(physically_available_hicanns);
	if (!(instantiated().erase(s_number))) {
		LOG4CXX_ERROR(
			logger,
			"There was a collision with reticle instances (" << s_number << ") at destructor");
		LOG4CXX_WARN(logger, "Active reticle instances:");
		if (instantiated().empty()) {
			LOG4CXX_WARN(logger, "No reticles have been instantiated so far")
		} else {
			std::stringstream msg;
			msg << "The following reticles have already been instantiated:" << endl;
			multiset<uint>::const_iterator it;
			for (it = instantiated().begin(); it != instantiated().end(); it++)
				msg << "Reticle: " << *it << endl;
			LOG4CXX_WARN(logger, msg.str());
		}
	}
	LOG4CXX_DEBUG(logger, "Deleted ReticleControl instance for reticle (" << s_number << ")");
}

void ReticleControl::switchVerticalPower(bool state){
	IBoardV2Ctrl board(jtag.get()); //create IBoard instance

	if (state){
		board.setVoltage(SBData::vdd11,  5.0);
		board.setVoltage(SBData::vdd5,   5.0);
		board.setVoltage(SBData::vdd25,  2.5);
		board.setVoltage(SBData::vdd11,  11.0);
		board.setVoltage(SBData::voh,    0.9);
		board.setVoltage(SBData::vol,    0.7);
		board.setVoltage(SBData::vddbus, 1.2);
		board.setJtag();
		board.setBothMux();
	} else {
		board.setVoltage(SBData::vddbus, 0.0);
		board.setVoltage(SBData::vol, 0.0);
		board.setVoltage(SBData::voh, 0.0);
		board.setVoltage(SBData::vdd11, 5.0);
		board.setVoltage(SBData::vdd25, 0.0);
		board.setVoltage(SBData::vdd5, 0.0);
		board.setVoltage(SBData::vdd11, 0.0);
	}
}

void ReticleControl::setVol(float vol)
{
	IBoardV2Ctrl board(jtag.get()); // create IBoard instance
	board.setVoltage(SBData::vol, vol);
}

void ReticleControl::setVoh(float voh)
{
	IBoardV2Ctrl board(jtag.get()); // create IBoard instance
	board.setVoltage(SBData::voh, voh);
}

void ReticleControl::printThisReticle(){
	cout << "Attributes of this reticle:" << endl;
	cout << "Sequential number: " << s_number
		 << ", \tFPGA IP: " << fpga_ip[0] << "." << fpga_ip[1] << "." << fpga_ip[2] << "."
		 << fpga_ip[3] << ":" << jtag_port << endl;
}

void ReticleControl::printInstantiated()
{
	if (instantiated().empty())
		cout << "No reticles have been instantiated so far" << endl;
	else {
		cout << "The following reticles have already been instantiated:" << endl;
		multiset<uint>::const_iterator it;
		for (it = instantiated().begin(); it != instantiated().end(); it++)
			cout << "Reticle: " << *it << endl;
	}
}

bool ReticleControl::isInstantiated(uint seq_num)
{
	return (instantiated().find(seq_num) != instantiated().end());
}

bool ReticleControl::hicannInit(uint hicann_nr, bool silent, bool return_on_error)
{ // TODO: re-write for initAllConnections stuff...
	bool success = true;

	if (comm->Init(
			comm->dnc2jtag(hicann[hicann_nr]->addr()), /*silent*/ silent,
			/*force_highspeed_init*/ false, return_on_error) != Stage2Comm::ok) {
		LOG4CXX_WARN(
			logger, "Reticle (" << s_number << ")::HICANN " << hicann_nr << " Init Failed. ");
		success = false;
	} else {
		LOG4CXX_INFO(
			logger, "Reticle (" << s_number << ")::HICANN " << hicann_nr << " Init OK. ");
		set_used_hicann(hicann_nr, true);
	}

	return success;
}

bool ReticleControl::hicannInit(std::bitset<8> hicann, std::bitset<8> highspeed_hicann, bool silent, bool return_on_error)
{
	bool success = true;

	if (comm->Init(hicann, highspeed_hicann, /*silent*/ silent, /*force_highspeed_init*/ false, return_on_error) !=
		Stage2Comm::ok) {
		LOG4CXX_WARN(
			logger, "Reticle (" << s_number << ")::HICANNs " << hicann.to_string() <<
			" (HS: " << highspeed_hicanns.to_string() << ") Init Failed. ");
		success = false;
	} else {
		LOG4CXX_INFO(
			logger,
			"Reticle (" << s_number << ")::HICANNs " << hicann.to_string() <<
			" (HS: " << highspeed_hicanns.to_string() << ") Init OK. ");
		for (size_t i = 0; i < hicann.size(); ++i) {
			if (hicann[i])
				set_used_hicann(i, true);
		}
	}

	return success;
}

void ReticleControl::set_used_hicann(uint hicann_number, bool state){
	used_hicanns.set(hicann_number, state);
}

bitset<8> ReticleControl::get_used_hicanns()
{
	return used_hicanns;
}

uint8_t ReticleControl::hicann_number()
{
	return physically_available_hicanns.count();
}

FPGAConnectionId::IPv4::bytes_type const ReticleControl::get_fpga_ip() const
{
	return fpga_ip;
}
