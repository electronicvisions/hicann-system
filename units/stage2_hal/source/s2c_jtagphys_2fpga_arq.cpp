#include "s2c_jtagphys_2fpga_arq.h"

// for JTAG: named socket
#include <sys/socket.h>
#include <sys/types.h>

#include <boost/chrono.hpp>

using namespace facets;
using namespace std;

// Class S2C_JtagPhys2FpgaArq :

bool S2C_JtagPhys2FpgaArq::jtag_running = false;

S2C_JtagPhys2FpgaArq::S2C_JtagPhys2FpgaArq(
	CommAccess const& access,
	myjtag_full* j,
	bool on_reticle,
	dncid_t dncid,
	bool use_k7fpga,
	std::string shm_name)
	: S2C_JtagPhys2Fpga(access, j, on_reticle, use_k7fpga),
	  dncid(dncid),
#ifdef FPGA_BOARD_BS_K7
	  hostarq(
		  shm_name,
		  "192.168.0.128",
		  /*listen port*/ 1234,
		  "192.168.0.1",
		  /*target port*/ 1234), // to do: set correct host IP
#else
	  hostarq(
		  shm_name,
		  "192.168.1.2",
		  /*listen port*/ 1234,
		  access.getFPGAConnectionId().get_fpga_ip().to_string().c_str(),
		  /*target port*/ 1234), // to do: set correct host IP
#endif
	  hostalctrl(access.getFPGAConnectionId().get_fpga_ip().to_ulong(), 1234),
	  bulk(0),
	  max_bulk(MAX_PDUWORDS),
	  jtag_listener_thread(NULL)
{
	#ifdef NCSIM
	#ifndef FPGA_BOARD_BS_K7
	// set interface to RGMII for FACETS Virtex5 board
	eth_if *pEthIf = eth_if::getInstance();
	pEthIf->setIfType(ETH_IF_RGMII);
	#endif
	#endif
    hostalctrl.setARQStream(&hostarq);
	#ifdef NCSIM
	hostalctrl.setEthernetIF( &(hostarq.pimpl->eth_soft) );
	#endif

	this->jtag_listener_thread = new boost::thread(
		jtag_listener, this, access.getFPGAConnectionId().get_fpga_ip().to_string().c_str());

	for (unsigned int nwait = 0; nwait < 100; ++nwait) {
		if (jtag_running)
			break;

		usleep(10000);
	}
}


S2C_JtagPhys2FpgaArq::~S2C_JtagPhys2FpgaArq()
{
	if (this->jtag_listener_thread != NULL) {
		jtag_running = false;
		// listener thread sometimes keeps waiting for data, force return after timeout
		jtag_listener_thread->try_join_for(boost::chrono::milliseconds(500));
		delete jtag_listener_thread;
	}
}


void S2C_JtagPhys2FpgaArq::initHostAL() {
	log(Logger::INFO) << "Initialisation start: " << /*sc_simulation_time() <<*/ endl;
	if (hostalctrl.isInitialized()) {
		log(Logger::INFO) << "FPGA Connection already initialized";
	}
	else if (hostalctrl.initFPGAConnection(0x112)) {
		log(Logger::INFO) << "TUD_TESTBENCH:NOTE:TESTPASS: Transport Layer initialisation sequence performed successfully." << endl;
	} else {
		log(Logger::ERROR) << "TUD_TESTBENCH:NOTE:TESTFAIL: Transport Layer initialisation sequence failed." << endl;
		log(Logger::ERROR) << "Initialisation end: " << /*sc_simulation_time() <<*/ endl;;
		log(Logger::ERROR) << "Aborting any further tests." << endl;
	}
}

S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Init(int hicann_jtag_nr, bool silent, bool force_highspeed_init, bool return_on_error){
	initHostAL();
	S2C_JtagPhys2FpgaArq::Commstate returnval = S2C_JtagPhys2Fpga::Init(hicann_jtag_nr, silent, force_highspeed_init, return_on_error);

	// enable FPGA ARQ (has been disabled during above init):
	set_fpga_reset(jtag->get_ip(), false, false, false, false, /*ARQ:*/false);

	// FIXME: make configurable
	if (k7fpga)
		jtag->K7FPGA_SetARQTimings(0xff, 0x0c8, 0x032);
	else
		jtag->SetARQTimings(0xff, 0x0c8, 0x032);

	return returnval;
}


S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Init(std::bitset<8> hicann, bool silent, bool force_highspeed_init, bool return_on_error){
	initHostAL();
	S2C_JtagPhys2FpgaArq::Commstate returnval = S2C_JtagPhys2Fpga::Init(hicann, silent, force_highspeed_init, return_on_error);

	// enable FPGA ARQ (has been disabled during above init):
	set_fpga_reset(jtag->get_ip(), false, false, false, false, /*ARQ:*/false);

	// FIXME: make configurable
	if (k7fpga)
		jtag->K7FPGA_SetARQTimings(0xff, 0x0c8, 0x032);
	else
		jtag->SetARQTimings(0xff, 0x0c8, 0x032);

	return returnval;
}


S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Flush() {
	if (bulk != 0)
		hostalctrl.sendHICANNConfigBulk();
	bulk = 0;
	hostalctrl.sendHICANNConfigBulk();

	// we do error handling via exceptions now FIXME
	return S2C_JtagPhys2Fpga::ok;
}

S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Clear() {
	// non-senseical with real arq... we would have to re-sync the window
	return S2C_JtagPhys2Fpga::ok;
}


// fpga_ip and sysstart_port not required in this version - inherited from overridden S2C_JtagPhys2Fpga implementation
void S2C_JtagPhys2FpgaArq::trigger_systime(unsigned int const /*fpga_ip*/, bool const enable, bool const listen_global, uint const /*sysstart_port*/)
{
    hostalctrl.setSystime(enable, listen_global, false);
}


void S2C_JtagPhys2FpgaArq::trigger_experiment(unsigned int const /*fpga_ip*/, bool const enable, bool const listen_global, uint const /*sysstart_port*/)
{
    hostalctrl.setSystime(enable, listen_global, true);
}


int S2C_JtagPhys2FpgaArq::issueCommand(uint hicann_nr, uint tagid, ci_payload *data, uint /*del*/) {
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

	// explicit flush if ethernet frame is full
	if ((++bulk % max_bulk) == 0) {
		log(Logger::DEBUG0) << "flushing frame, was full " << bulk << endl;
		Flush();
	}
	return 1; // same as in s2comm::issueCommand()
}
	
int S2C_JtagPhys2FpgaArq::recvData(uint hicann_nr, uint tagid, ci_payload *data) {
	// we have to flush all the write commands first
	Flush();
	uint64_t tmp = 0;
	try {
		tmp = hostalctrl.getReceivedHICANNConfig(dncid, jtag2dnc(hicann_nr), tagid);
	} catch (std::underflow_error e) {
		log(Logger::ERROR) << *this << endl;
		throw e;
	}
	data->addr = (tmp>>32) & 0xffff;
	data->data = static_cast<ci_data_t>(tmp);
	return 1; // FIXME l1switch_control.cpp seems to be fubar?
}


S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Send(IData /*d*/, uint /*del*/) {
	throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + "User called software-arq stuff but this is ARQ-based comm");
}


S2C_JtagPhys2FpgaArq::Commstate S2C_JtagPhys2FpgaArq::Receive(IData & /*d*/) {
	throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + "User called software-arq stuff but this is ARQ-based comm");
}

namespace facets {

void S2C_JtagPhys2FpgaArq::jtag_listener(S2C_JtagPhys2FpgaArq* my_caller, const char* fpga_ip)
{
	const unsigned int JTAG_MTU_BYTE = 1440;

	// initialize named socket for JTAG
	int jtag_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (jtag_socket < 0) {
		my_caller->log(Logger::ERROR) << "Could not create named socket for JTAG.";
		return;
	}

	struct sockaddr_un namesock;
	namesock.sun_family = AF_UNIX;
	strcpy(namesock.sun_path, fpga_ip);
	unlink(namesock.sun_path);

	if (bind(jtag_socket, (struct sockaddr*) &namesock, sizeof(struct sockaddr_un)) < 0) {
		my_caller->log(Logger::ERROR) << "Could not bind named socket for JTAG." << endl;
		close(jtag_socket);
		return;
	}

	if (listen(jtag_socket, 1) < 0) {
		my_caller->log(Logger::ERROR) << "Socket listen failed." << endl;
		close(jtag_socket);
		return;
	}

	my_caller->log(Logger::INFO) << "Named socket created successfully. ID: " << (jtag_socket)
								 << ". Waiting for JTAG to connect...";
	jtag_running = true;

	struct sockaddr_un remote;
	socklen_t remote_sock_length = 0;
	int in_sock = accept(jtag_socket, (struct sockaddr*) &remote, &remote_sock_length);
	if (in_sock < 0) {
		my_caller->log(Logger::ERROR) << "Error during socket accept.";
		close(jtag_socket);
		return;
	}

	my_caller->log(Logger::INFO) << "JTAG detected. Now listening to data...";

	// main listener loop
	char rec_buf[JTAG_MTU_BYTE];
	while (jtag_running) {
		// test if something was sent by software-JTAG
		int rec_len = recv(in_sock, rec_buf + 4, JTAG_MTU_BYTE - 4, MSG_DONTWAIT);
		if (rec_len > 0) {
			*((uint32_t*) (rec_buf)) = (rec_len + 3) / 4;
			for (unsigned int nword = 1; nword < (static_cast<unsigned int>(rec_len) + 7) / 4; ++nword)
				*((uint32_t*) (rec_buf) + nword) = ntohl(*((uint32_t*) (rec_buf) + nword));

			my_caller->getHostAL()->sendJTAG(rec_len + 4, rec_buf);
		}

		// test if something was sent back from FPGA-JTAG
		sctrltp::packet jtag_rec = my_caller->getHostAL()->getReceivedJTAGData();
		if (jtag_rec.len) {
			unsigned int jtag_len_32bit = *((uint32_t*) (jtag_rec.pdu));

			for (unsigned int nword = 1; nword < jtag_len_32bit + 1; ++nword)
				*((uint32_t*) (jtag_rec.pdu) + nword) =
					htonl(*((uint32_t*) (jtag_rec.pdu) + nword));

			if (send(in_sock, (uint32_t*) (jtag_rec.pdu) + 1, 4 * jtag_len_32bit, MSG_DONTWAIT) <
				0) {
				my_caller->log(Logger::ERROR) << "Error during sending to named socket.";
			}
		}

		usleep(1000);
	}

	// clean-up
	my_caller->log(Logger::INFO) << "Closing named socket with ID " << jtag_socket;
	close(jtag_socket);
}


std::ostream & operator<< (std::ostream & o, S2C_JtagPhys2FpgaArq const & s) {
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

	o << "ARQ read (from HW) counter: " << dec <<
		(rcounter & 0x7fffffff)
		<< " (read fifo " << ((rcounter & 0x80000000) ? "not empty) " : "empty) ")
		<< "ARQ write (to HW) counter: " <<
		(wcounter & 0x7fffffff)
		<< " (" << ((wcounter & 0x80000000) ? "no data" : "data") << " available to write)";
	o << "Debug Registers: down: " << dec << dregs.first << " up: " << dregs.second;
	o << "\n(for details how to interpret this error, see https://brainscales-r.kip.uni-heidelberg.de:6443/visions/pl/638gkrfbjtgwzjqaxr94t1hgpr )\n";
	return o;
}

} // facets
