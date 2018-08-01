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
		LOG4CXX_INFO(logger, "Activating HostARQ communication mode");
		model = jtag_eth_fpga_arq;
	} else { // very ugly too
		LOG4CXX_ERROR(logger, "Non HostARQ communication not supported!");
		if (gigabit_on)
			model = jtag_wafer_fpga;
		else
			model = jtag_wafer;
	}
	assert(
		(model == jtag_eth_fpga_arq) &&
		"The communication mode is not compatible with ReticleControl");


	jtag.reset(new myjtag_full(true, /*dnc?*/ !kintex, available_hicanns, 0, kintex));
	if (!jtag->initJtag(jtag_lib_v2::JTAG_ETHERNET))
		throw std::runtime_error("JTAG open failed!");

	mLog(Logger::INFO) << "Setting JTAG frequency to 10000 kHz" << Logger::flush;

	if (model == jtag_wafer_fpga) {
		if (!jtag->initJtagV2(
				jtag->ip_number(fpga_ip[0], fpga_ip[1], fpga_ip[2], fpga_ip[3]), jtag_port,
				/* speed */ 10000)) {
			throw std::runtime_error("JTAG init failed!");
		}

		LOG4CXX_INFO(logger, "Initialized JTAG over ETH communication");


		jtag_p2f.reset(new S2C_JtagPhys2Fpga(*access.get(), jtag.get(), on_wafer));
		comm = jtag_p2f.get();
	} else if (model == jtag_eth_fpga_arq) {
		FPGAConnectionId::IPv4 tmp((fpga_ip));
// FIXME: add option for kintex-based access
// FIXME: get dnc id nicer!
#ifdef FPGA_BOARD_BS_K7
		std::shared_ptr<sctrltp::ARQStream> p_hostarq(new sctrltp::ARQStream(
			tmp.to_string(), "192.168.0.128", /*listen port*/ 1234, "192.168.0.1",
			/*target port*/ 1234)); // TODO: set correct host IP
#else
		std::shared_ptr<sctrltp::ARQStream> p_hostarq(new sctrltp::ARQStream(
			tmp.to_string(), "192.168.1.2", /*listen port*/ 1234, tmp.to_string().c_str(),
			/*target port*/ 1234));
#endif
		if (!jtag->initJtagV2(p_hostarq, /*speed*/ 10000)) {
			throw std::runtime_error("JTAG init failed!");
		}

		LOG4CXX_INFO(logger, "Initialized JTAG over ARQ communication");
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
	for (uint i = 0; i < available_hicanns.count(); i++)
		hicann[i].reset(new HicannCtrl(comm, i));
	used_hicanns.reset(); // no HICANNs are in use
}

/// Used by VerticalSetupPowerBackend::SetupReticle
ReticleControl::ReticleControl(
	bool gigabit_on, ip_t _ip, std::bitset<8> available_hicann, bool _arq_mode, bool _kintex)
	: Stage2Ctrl(NULL, 0),
	  available_hicanns(available_hicann),
	  gigabit_on(gigabit_on),
	  arq_mode(_arq_mode),
	  kintex(_kintex)
{
	LOG4CXX_INFO(
		logger, "Creating ReticleControl instance for reticle " << _ip << " (vertical setup)");

	// create fake reticle to comply with other functionality, set jtag port wether kintex (i.e.
	// CubeSetup) is used
	reticle temp = reticle(0, 0, cartesian_t(0, 0), _ip, (kintex ? 1700 : 1701), 0, 0);

	bool already_existing = isInstantiated(temp.seq_number);
	if (already_existing)
		LOG4CXX_FATAL(logger, "Multiple instances for the same reticle are not allowed :P");

	instantiated().insert(temp.seq_number);

	// initializing all reticle-specific variables
	jtag_port = temp.port;
	x = temp.coord.x;
	y = temp.coord.y;
	fpga_ip[0] = _ip.a;
	fpga_ip[1] = _ip.b;
	fpga_ip[2] = _ip.c;
	fpga_ip[3] = _ip.d;
	s_number = temp.seq_number;
	aoutput = temp.aout;
	pic_num = temp.pic;

	// initializing reticle info for the reticleIDclass
	reticle_info = temp;

	if (!already_existing)
		init(/*on_wafer*/ false);
}

/// Used by WaferPowerBackend::SetupReticle
ReticleControl::ReticleControl(
	size_t seq_number,
	size_t pow_number,
	ip_t _ip,
	uint16_t port,
	FPGAConnectionId::IPv4 const pmu_ip,
	bool gigabit_on,
	bool _arq_mode,
	bool _kintex)
	: Stage2Ctrl(NULL, 0),
	  pmu_ip(pmu_ip),
	  gigabit_on(gigabit_on),
	  arq_mode(_arq_mode),
	  kintex(_kintex)
{
	LOG4CXX_INFO(
		logger, "Creating ReticleControl instance for reticle IP/FPGA "
					<< _ip << "/" << ((_ip.d - 1) % 32 + int(_ip.d / 32) * 12) << " (on wafer "
					<< _ip.c << ") port " << port);

	// create fake reticle to comply with other functionality
	reticle temp = reticle(seq_number, pow_number, cartesian_t(0, 0), _ip, port, 0, 0);

	bool already_existing = isInstantiated(temp.seq_number);
	if (already_existing)
		LOG4CXX_FATAL(logger, "Multiple instances for the same reticle are not allowed :P");

	instantiated().insert(temp.seq_number);
	available_hicanns.set();
	// initializing all reticle-specific variables
	x = temp.coord.x;
	y = temp.coord.y;
	fpga_ip[0] = _ip.a;
	fpga_ip[1] = _ip.b;
	fpga_ip[2] = _ip.c;
	fpga_ip[3] = _ip.d;
	jtag_port = port;
	s_number = temp.seq_number;
	aoutput = temp.aout;
	pic_num = temp.pic;

	// initializing reticle info for the reticleIDclass
	reticle_info = temp;

	if (!already_existing)
		init(/*on_wafer*/ true);
}

ReticleControl::~ReticleControl()
{
	LOG4CXX_INFO(logger, "Deleting ReticleControl instance for reticle (" << x << "," << y << ")");
	jtag_p->reset_hicann_arq(available_hicanns);
	if (!(instantiated().erase(s_number))) {
		LOG4CXX_ERROR(
			logger,
			"There was a collision with reticle instances (" << x << "," << y << ") at destructor");
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
	LOG4CXX_DEBUG(logger, "Deleted ReticleControl instance for reticle (" << x << "," << y << ")");
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
	cout << "Sequential number: " << s_number << ", \tPower number: " << p_number
		 << ", \tCartesian coordinates: (" << x << "," << y << "), \tAout number: " << aoutput
		 << ", \tFPGA IP: " << fpga_ip[0] << "." << fpga_ip[1] << "." << fpga_ip[2] << "."
		 << fpga_ip[3] << ":" << jtag_port << ", \tPIC: " << pic_num << endl;
}

ReticleControl::reticleIDclass ReticleControl::reticleID(ReticleControl::IDtype id) const
{
	return reticleIDclass(reticle_info, id);
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
			logger, "Reticle (" << x << "," << y << ")::HICANN " << hicann_nr << " Init Failed. ");
		success = false;
	} else {
		LOG4CXX_INFO(
			logger, "Reticle (" << x << "," << y << ")::HICANN " << hicann_nr << " Init OK. ");
		set_used_hicann(hicann_nr, true);
	}

	return success;
}

bool ReticleControl::hicannInit(std::bitset<8> hicann, bool silent, bool return_on_error)
{
	bool success = true;

	if (comm->Init(hicann, /*silent*/ silent, /*force_highspeed_init*/ false, return_on_error) !=
		Stage2Comm::ok) {
		LOG4CXX_WARN(
			logger, "Reticle (" << x << "," << y << ")::HICANNs " << hicann.to_string()
								<< " Init Failed. ");
		success = false;
	} else {
		LOG4CXX_INFO(
			logger,
			"Reticle (" << x << "," << y << ")::HICANNs " << hicann.to_string() << " Init OK. ");
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
	return available_hicanns.count();
}

FPGAConnectionId::IPv4::bytes_type const ReticleControl::get_fpga_ip() const
{
	return fpga_ip;
}

uint8_t ReticleControl::get_fpga_dnc_channel_id() const
{
	uint8_t tmp = jtag_port - 1700;
	if (tmp > 3)
		throw logic_error("ReticleControl::get_fpga_dnc_channel_id(): jtag_port is not in range "
						  "[1700, 1703]. Unable to detect FPGA-DNC channel");
	return tmp;
}
