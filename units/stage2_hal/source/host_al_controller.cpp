// Company         :   tud
// Author          :   partzsch
// E-Mail          :   Johannes.Partzsch@tu-dresden.de
//
// Filename        :   host_al_controller.h
// Project Name    :   p_facets
// Subproject Name :   s_fpga
// Description     :   <short description>
//
//------------------------------------------------------------------------


#include <iomanip>
#include <stdexcept>

extern "C" {
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
}

#include <boost/asio/ip/address.hpp>

#include "host_al_controller.h"
#include "logger.h"
#include "logger_ncsim_compat.h"

#include "sctrltp/ARQFrame.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#define MAX_TIMESTAMP_CNT 0x8000 // 2**15

#ifndef NCSIM
#include <chrono>
static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("hicann-system.HostAL");
#endif

HostALController::HostALController()
	: target_ip(c_uiRemoteIP),
	  target_port(1234),
	  seq_number_max(4096),
	  fpga_clk_cycle_in_s(8.0e-9),
	  dnc_clk_cycle_in_s(4.0e-9),
	  spikes_to_binfile_enable(false)
{
	this->common_prepare();
}

HostALController::HostALController(uint32_t set_target_ip, uint16_t set_target_port)
	: target_ip(set_target_ip),
	  target_port(set_target_port),
	  seq_number_max(4096),
	  fpga_clk_cycle_in_s(8.0e-9),
	  dnc_clk_cycle_in_s(4.0e-9),
	  spikes_to_binfile_enable(false)
{
	this->common_prepare();
}

#ifndef HOSTAL_WITHOUT_JTAG
void HostALController::setJTAGInterface(myjtag_full* set_jtag)
{
	this->jtag = set_jtag;
}
#endif

void HostALController::setARQStream(sctrltp::ARQStream *set_arq)
{
	this->arq_ptr = set_arq;
}

sctrltp::ARQStream *HostALController::getARQStream()
{
	return this->arq_ptr;
}


void HostALController::setEthernetIF(EthernetIFBase* set_eth)
{
	this->ethernet_if = set_eth;
}


EthernetIFBase* HostALController::getEthernetIF()
{
	return this->ethernet_if;
}

bool HostALController::initEthernetIF()
{
	if (this->ethernet_if == NULL) {
		LOG4CXX_ERROR(logger, "HostALController::initEthernetIF: EthernetIFBase pointer not set.");
		return false;
	}

	bool retval = this->ethernet_if->init(this->target_port);

	return retval;
}


void HostALController::reset()
{
	this->ethernet_init = false;

	this->pulse_loopback_enable = false;
	this->pb_systime_replace_enable = false;
	this->pb_end_address_received = 0;

	// clear pulse/config related stuff
	this->reset_fpga_times();
	this->reset_playback_buffers();
	this->reset_trace_buffers();
	this->reset_hiconf_buffers();
}


void HostALController::reset_playback_buffers()
{
	this->playback_entry_buffer.clear();
	this->last_group_start_in_buffer = -1;
}

void HostALController::reset_trace_buffers()
{
	this->trace_overflow_count = 0;
	this->last_traced_tstamp = 0;
	this->rec_trace_entries = 0;
	this->received_pulses_count = 0;
	this->recpulse_buffer.clear();
}

void HostALController::reset_hiconf_buffers()
{
	this->hiconf_bulk_buffer.clear();
	for (size_t i = 0; i < hiconf_receive_buffer.size(); i++)
		if (!hiconf_receive_buffer.at(i).empty())
			hiconf_receive_buffer.at(i).clear();
}


bool HostALController::isInitialized()
{
	return this->ethernet_init;
}


bool HostALController::initFPGAConnection(unsigned int /*seq_number*/)
{
	LOG4CXX_INFO(logger, "HostALController::initFPGAConnection: Using hostARQ.");
	this->arq_ptr->start();
	return true;
}


uint32_t HostALController::getTargetIP() const
{
	return this->target_ip;
}


uint16_t HostALController::getTargetPort() const
{
	return this->target_port;
}


bool HostALController::sendSingleFrame(
	unsigned int frametype,
	unsigned int pllength,
	void* payload,
	unsigned int /*user_data*/,
	double /*timeout*/)
{
	if (pllength == 0) {
		LOG4CXX_WARN(
			logger,
			"HostALController::sendSingleFrame: trying to send frame without payload (type: 0x"
				<< std::hex << frametype << std::dec << "). Ignoring.");
		return true;
	}


	if (this->arq_ptr == NULL) {
		LOG4CXX_ERROR(logger, "HostALController::sendSingleFrame: hostARQ pointer not set.");
		return false;
	}

	sctrltp::packet curr_pck;
	curr_pck.pid = frametype;
	curr_pck.len = (pllength + 7) / 8; // 64bit words

	LOG4CXX_DEBUG(
		logger, "HostALController::sendSingleFrame: length(bytes): "
					<< pllength << ", length (64bit words): " << curr_pck.len
					<< ", type: " << std::hex << frametype << std::dec);

	memcpy(curr_pck.pdu, payload, curr_pck.len * 8);

#ifdef NCSIM
	while (this->arq_ptr->send_buffer_full())
		wait(10.0, SC_US);
#endif

	if (!this->arq_ptr->send(curr_pck)) {
		LOG4CXX_WARN(logger, "HostALController::sendSingleFrame: ARQ.send() not successful");
		return false;
	}

#ifndef NCSIM
	// JP: Not needed for simulation, as trigger_send is called in separate thread
	this->arq_ptr->trigger_send();
#endif

	return true;
}

bool HostALController::sendFPGAConfigPacket(uint64_t const payload)
{
	if (sendSingleFrame(
			application_layer_packet_types::FPGACONFIG, sizeof(uint64_t), (void*) &payload, 0, 0) !=
		true) {
		throw std::runtime_error("sendFPGAConfigPacket: failed to send config packet");
	}

	// wait for response packet
	sctrltp::packet curr_pck;
#ifndef NCSIM
	auto chrono_start = std::chrono::system_clock::now();
#endif
	// FIXME remove all communication timeouts and let HostARQ handle timeouts with throws
	// (ISSUE:2526)
	time_t const start = time(NULL);
	time_t now = start;
	double const cable_plug_timeout = 15; // seconds
	while (difftime(now, start) < cable_plug_timeout) {
		if (!arq_ptr->received_packet_available()) {
			usleep(50);
			now = time(NULL);
			continue;
		}
		arq_ptr->receive(curr_pck);
#ifndef NCSIM
		if (curr_pck.pid != application_layer_packet_types::FPGACONFIG) {
			throw std::runtime_error(
				"sendFPGAConfigPacket: unexpected response packet type " +
				std::to_string(curr_pck.pid));
		}
		if (curr_pck.len > 1) {
			throw std::runtime_error(
				"sendFPGAConfigPacket: unexpected response packet length " +
				std::to_string(curr_pck.len));
		}

		uint64_t const response_payload = curr_pck.pdu[0];
		if (response_payload != payload) {
			throw std::runtime_error(
				"sendFPGAConfigPacket: unexpected response packet payload " +
				std::to_string(response_payload) + " != " + std::to_string(payload));
		}
		auto chrono_end = std::chrono::system_clock::now();
		auto elapsed =
			std::chrono::duration_cast<std::chrono::microseconds>(chrono_end - chrono_start);
		LOG4CXX_DEBUG(
			logger, "sendFPGAConfigPacket: received response after " +
						std::to_string(elapsed.count()) + "us");
#endif
		return true;
	}
#ifdef NCSIM
	LOG4CXX_FATAL(logger, "sendFPGAConfigPacket: did not receive response packet after " << cable_plug_timeout << "s");
#else
	throw std::runtime_error(
		"sendFPGAConfigPacket: did not receive response packet after " +
		std::to_string(cable_plug_timeout) + "s");
#endif
}

bool HostALController::waitForReceivedUDP(double timeout, double poll_interval)
{
	if (this->ethernet_if == NULL) {
		LOG4CXX_WARN(
			logger, "HostALController::initFPGAConnection: EthernetIFBase pointer not set.");
		return true;
	}

	while (!(this->ethernet_if->hasReceivedUDP())) {
		if (timeout <= 0.0)
			return true;

		if (ethernet_if)
			ethernet_if->pauseEthernet(poll_interval);
		timeout -= poll_interval;
	}

	return false;
}

bool HostALController::sendJTAG(unsigned int pllength, void* payload, double timeout)
{
	bool retval = this->sendSingleFrame(
		application_layer_packet_types::JTAGBULK, pllength, payload, 0, timeout);
	return retval;
}


sctrltp::packet HostALController::getReceivedJTAGData()
{
	this->fill_receive_buffer(false);

	sctrltp::packet curr_pck;
	if (this->jtag_receive_buffer.size()) {
		curr_pck = this->jtag_receive_buffer.front();
		this->jtag_receive_buffer.pop_front();
	} else
		curr_pck.len = 0;

	return curr_pck;
}


bool HostALController::sendI2C(uint64_t /*payload*/, double /*timeout*/)
{
	LOG4CXX_WARN(logger, "HostALController::sendI2C: not implemented yet.Abort.");
	return false;
}

std::vector<pulse_float_t> HostALController::getFPGATracePulses()
{
	this->fill_receive_buffer(false);

	std::vector<pulse_float_t> retval;

	const size_t num_pulses = this->getTraceBufferSize();
	for (size_t np = 0; np < num_pulses; ++np) {
		pulse_t raw_pulse = this->getTracePulse();
		retval.push_back(
			pulse_float_t(raw_pulse.second, raw_pulse.first * this->dnc_clk_cycle_in_s));
	}

	return retval;
}


HostALController::pulse_t HostALController::getTracePulse()
{
	if (this->recpulse_buffer.size() == 0)
		LOG4CXX_FATAL(
			logger, "HostALController::getTracePulse() ["
						<< target_ip << ":" << target_port
						<< "]: "
						   "no pulse in trace buffer. Check for number of buffered pulses with "
						   "getTraceBufferSize() first");

	pulse_t curr_entry = this->recpulse_buffer.front();
	this->recpulse_buffer.pop_front();

	return curr_entry;
}


unsigned int HostALController::getTraceBufferSize()
{
	this->fill_receive_buffer(false);
	return this->recpulse_buffer.size();
}


bool HostALController::readHICANN(uint64_t /*payload*/, double /*timeout*/)
{
	LOG4CXX_WARN(logger, "HostALController::readHICANN: not implemented yet.Abort.");
	return false;
}


void HostALController::activateBinarySpikeDataFileWrite(bool set_active)
{
	this->spikes_to_binfile_enable = set_active;
}


bool HostALController::sendFPGAPlayback(std::vector<uint32_t>& payload, double timeout)
{
	// write data to binary file
	if (this->spikes_to_binfile_enable) {
		FILE* fout = fopen("playback.bin", "wb");
		if (fout) {
			for (unsigned int nentry = 0; nentry < payload.size(); ++nentry)
				fwrite(&(payload[nentry]), 4, 1, fout);
		}

		fclose(fout);
	}

	uint32_t* p_ptr = &(payload.front());
	bool retval = true;

	unsigned int max_frame = 2 * MAX_PDUWORDS;
	for (unsigned int npos = 0; npos < payload.size(); npos += max_frame) {
		unsigned int framesize_32bit =
			(payload.size() - npos > max_frame) ? (max_frame) : (payload.size() - npos);
		unsigned int userdata = framesize_32bit;
		retval &= this->sendSingleFrame(
			application_layer_packet_types::FPGAPLAYBACK, 4 * framesize_32bit, p_ptr + npos,
			userdata, timeout);
	}

	return retval;
}

bool HostALController::sendPulseList(
	const std::vector<pulse_float_t>& pulses, double fpga_hicann_delay_in_s)
{
	for (unsigned int npulse = 0; npulse < pulses.size(); ++npulse) {
		pulse_float_t pulse = pulses[npulse];
		full_fpga_time_t pulse_time = (full_fpga_time_t)((pulse.time / fpga_clk_cycle_in_s) + 0.5);
		uint16_t hicann_reltime =
			(uint16_t)(((pulse.time + fpga_hicann_delay_in_s) / dnc_clk_cycle_in_s) + 0.5);
		this->addPlaybackPulse(pulse_time, hicann_reltime, pulse.id);
	}
	return this->flushPlaybackPulses();
}


bool HostALController::sendPlaybackConfig(
	double fpga_time, uint64_t payload, unsigned int dnc, unsigned int hicann, double timeout)
{
	int pulse_time = (int) ((fpga_time / fpga_clk_cycle_in_s) + 0.5);
	unsigned int pck_count = 1;

	std::vector<uint32_t> wdata(4, 0);
	wdata[0] = (pck_count << 18) | (1 << 16) | ((pulse_time & 0x3fff) << 1);
	wdata[1] = 0;
	wdata[2] = payload & 0xffffffff;
	wdata[3] = ((payload >> 32) & 0x1ffff) | ((hicann & 0x7) << 27) | ((dnc & 0x3) << 30);


	return this->sendFPGAPlayback(wdata, timeout);
}


void HostALController::addPlaybackPulse(
	full_fpga_time_t fpga_time, full_hicann_time_t hicann_time, pulse_address_t id)
{
	LOG4CXX_DEBUG(
		logger, "HostALController::addPlaybackPulse: Processing new pulse with FPGA time "
					<< std::dec << fpga_time << ", HICANN time " << hicann_time << ", id " << id);

	unsigned int max_pulses_per_group = 184;
	// too small distance -> continue current group
	if ((fpga_time < last_playback_reltime + min_reltime_distance_clks) &&
		(last_playback_reltime > 0) && (last_group_start_in_buffer >= 0) &&
		(((this->playback_entry_buffer[last_group_start_in_buffer]) >> 18) <
		 max_pulses_per_group)) {
		unsigned int pulses_in_group =
			(this->playback_entry_buffer[last_group_start_in_buffer]) >> 18;

		if (pulses_in_group & 0x1) // odd number of pulses up to now -> start new 64bit entry
		{
			this->playback_entry_buffer.push_back(
				(((uint32_t)(id) &0x3fff) << 18) | ((hicann_time & 0x7fff) << 1));
			this->playback_entry_buffer.push_back(0);
		} else {
			this->playback_entry_buffer.back() =
				(((uint32_t)(id) &0x3fff) << 18) | ((hicann_time & 0x7fff) << 1);
		}

		this->playback_entry_buffer[last_group_start_in_buffer] =
			(this->playback_entry_buffer[last_group_start_in_buffer] & 0x3ffff) |
			((pulses_in_group + 1) << 18);

		LOG4CXX_DEBUG(
			logger, "HostALController::addPlaybackPulse: Adding pulse to group at buffer size "
						<< this->last_group_start_in_buffer << ", fpga_time=" << fpga_time
						<< ", last_playback_reltime=" << last_playback_reltime);

		last_playback_reltime += 1; // one clock needed for playing back one pulse
	} else                          // higher fpga_time distance -> start new group
	{
		// handle overflows -> assumes that pulse times are strictly sorted and 64bit integer value
		// does not overflow
		if (((fpga_time - last_playback_reltime) > 0x2000) &&
			((last_playback_reltime >> 14) < (fpga_time >> 14))) // have to generate overflow
		{
			full_fpga_time_t interval_diff = (fpga_time >> 14) - (last_playback_reltime >> 14);

			if ((last_playback_reltime & 0x3fff) > 0x3ffb) // take into account that playback FSM
														   // needs some clks to go into waiting
														   // state
			{
				interval_diff -= 1;
			}

			if (interval_diff > 0x3fff) {
				LOG4CXX_WARN(
					logger, "HostALController::addPlaybackPulse: inter-spike interval exceeds "
							"currently supported number of systime counter overflows: "
								<< interval_diff << ">1023. Truncating interval.");
				interval_diff = 0x3fff;
			}

			LOG4CXX_DEBUG(
				logger,
				"HostALController::addPlaybackPulse: Adding overflow wait packet, delaying for "
					<< interval_diff << " systime counter intervals");

			this->playback_entry_buffer.push_back((1) | ((interval_diff & 0x3fff) << 2));
			this->playback_entry_buffer.push_back(0x0); // for alignment to 64bit
		}

		full_fpga_time_t curr_fpga_time = fpga_time;
		if (curr_fpga_time < last_playback_reltime + min_reltime_distance_clks) {
			curr_fpga_time = last_playback_reltime + min_reltime_distance_clks;
			full_fpga_time_t const delta_fpga_time = curr_fpga_time - fpga_time;
			double const delta_wallclock_time_in_s = delta_fpga_time * fpga_clk_cycle_in_s;
			LOG4CXX_WARN(
			    logger, "HostALController::addPlaybackPulse: Had to shift pulse by "
			                << std::dec << delta_fpga_time << " ("
			                << delta_wallclock_time_in_s * 1e9 << ") FPGA clock cycles (ns) "
			                << "because of too high rate.");
		}

		this->playback_entry_buffer.push_back((1 << 18) | ((curr_fpga_time & 0x3fff) << 1));
		this->playback_entry_buffer.push_back(
			(((uint32_t)(id) &0x3fff) << 18) | ((hicann_time & 0x7fff) << 1));

		this->last_group_start_in_buffer = this->playback_entry_buffer.size() - 2;

		LOG4CXX_DEBUG(
			logger, "HostALController::addPlaybackPulse: Starting new pulse group at buffer size "
						<< this->last_group_start_in_buffer);

		last_playback_reltime = curr_fpga_time;
	}
}


void HostALController::addPlaybackConfig(
	full_fpga_time_t fpga_time, uint64_t payload, pulse_address_t hicann_id)
{
	unsigned int pck_count = 1;
	this->playback_entry_buffer.push_back(
		(pck_count << 18) | (1 << 16) | ((fpga_time & 0x3fff) << 1));
	this->playback_entry_buffer.push_back(0);
	this->playback_entry_buffer.push_back(payload & 0xffffffff);
	this->playback_entry_buffer.push_back(((payload >> 32) & 0x1ffff) | ((hicann_id & 0x1f) << 17));
}


bool HostALController::flushPlaybackPulses()
{
	bool retval = this->sendFPGAPlayback(this->playback_entry_buffer); // (uses default timeout)
	this->reset_playback_buffers(); // JP: should we clear if retval==false?

	return retval;
}


bool HostALController::setSystime(
	const bool enable, const bool listen_global, const bool expstart_mode, const double /*timeout*/)
{
	uint64_t payload = (1 << 22); // systime write enable bit

	if (enable)
		payload |= (1 << 21);
	if (listen_global)
		payload |= (1 << 20);
	if (expstart_mode)
		payload |= (1 << 19);

	if (pulse_loopback_enable)
		payload |= (1 << 24);
	if (pb_systime_replace_enable)
		payload |= (1 << 23);

	return sendFPGAConfigPacket(payload);
}


bool HostALController::addPlaybackFPGAConfig(
	full_fpga_time_t fpga_time, bool pb_end_marker, bool stop_trace, bool start_trace_read, bool block_trace_recording)
{
	// handle overflows -> assumes that times are strictly sorted and 64bit integer value does not
	// overflow
	if (((fpga_time - last_playback_reltime) > 0x2000) &&
		((last_playback_reltime >> 14) < (fpga_time >> 14))) // have to generate overflow
	{
		full_fpga_time_t interval_diff = (fpga_time >> 14) - (last_playback_reltime >> 14);

		if ((last_playback_reltime & 0x3fff) >
			0x3ffb) // take into account that playback FSM needs some clks to go into waiting state
		{
			interval_diff -= 1;
		}

		if (interval_diff > 0x3fff) {
			LOG4CXX_WARN(
				logger, "HostALController::addPlaybackFPGAConfig: inter-spike interval exceeds "
						"currently supported number of systime counter overflows: "
							<< interval_diff << ">1023. Truncating interval.");
			interval_diff = 0x3fff;
		}

		LOG4CXX_DEBUG(
			logger,
			"HostALController::addPlaybackFPGAConfig: Adding overflow wait packet, delaying for "
				<< interval_diff << " systime counter intervals");

		this->playback_entry_buffer.push_back((1) | ((interval_diff & 0x3fff) << 2));
		this->playback_entry_buffer.push_back(0x0); // for alignment to 64bit
	}

	// generate and send FPGA config packet
	this->playback_entry_buffer.push_back((1 << 18) | (1 << 16) | ((fpga_time & 0x3fff) << 1));
	this->playback_entry_buffer.push_back(0);

	uint32_t config_low = 0;
	if (stop_trace)
		config_low |= 1; // bit 0: stop trace
	if (start_trace_read)
		config_low |= (1 << 1); // bit 1: start trace read
	if (block_trace_recording)
		config_low |= (1 << 2); // bit 2: block trace

	uint32_t config_high = (1 << 31); // bit 63(31): FPGA config
	if (pb_end_marker)
		config_high |= (1 << 30); // bit 62(30): end marker

	this->playback_entry_buffer.push_back(config_low);
	this->playback_entry_buffer.push_back(config_high);

	this->last_playback_reltime = fpga_time;
	return true;
}


unsigned int HostALController::getPlaybackEndAddress()
{
	this->fill_receive_buffer(false);
	return this->pb_end_address_received;
}


bool HostALController::sendSingleHICANNConfig(
	uint64_t payload, unsigned int dnc, unsigned int hicann, double timeout)
{
	unsigned int userdata = 2; // length=2 (2x32bit portions)
	unsigned int dest = (dnc & 0x3) << 3 | (hicann & 0x7);
	payload = (payload & 0x3ffffffffffffull) |
			  ((uint64_t)(dest) << 59); // data is 49bit plus 1bit ARQ tag, destination in MSBs
	return this->sendSingleFrame(
		application_layer_packet_types::HICANNCONFIG, 8, &payload, userdata, timeout);
}


void HostALController::addHICANNConfigBulk(uint64_t payload, unsigned int dnc, unsigned int hicann)
{
	this->hiconf_bulk_buffer.push_back(hicann_config_t(payload, dnc, hicann));
}


bool HostALController::sendHICANNConfigBulk(double timeout)
{
	unsigned int max_config_per_frame = MAX_PDUWORDS;

	// currently rather ineffective: copy data to new send buffer
	unsigned int pck_count = this->hiconf_bulk_buffer.size();
	unsigned int remaining_pck_count = pck_count;
	unsigned int pck_buffer_pos = 0;

	if (remaining_pck_count)
		LOG4CXX_DEBUG(logger, "About to send HICANN config bulk with " << pck_count << " packets");

	bool retval = true;
	while (remaining_pck_count > 0) {
		// determine current frame size
		unsigned int curr_pck_count = remaining_pck_count;
		if (remaining_pck_count > max_config_per_frame)
			curr_pck_count = max_config_per_frame;

		unsigned int userdata = 2 * curr_pck_count; // (2x32bit portions)
		std::vector<uint64_t> payload;

		LOG4CXX_DEBUG(
			logger,
			"  -> Sending HICANN config frame with " << curr_pck_count << " config packets:");
		for (unsigned int npck = 0; npck < curr_pck_count; ++npck) {
			int const dnc = hiconf_bulk_buffer.at(pck_buffer_pos + npck).dnc & 0x3;
			int const hic = hiconf_bulk_buffer.at(pck_buffer_pos + npck).hicann & 0x7;
			bool const is_read = hiconf_bulk_buffer.at(pck_buffer_pos + npck).data & 0x80000000ull;
			unsigned int dest = (hiconf_bulk_buffer.at(pck_buffer_pos + npck).dnc & 0x3) << 3 |
								(hiconf_bulk_buffer.at(pck_buffer_pos + npck).hicann & 0x7);
			uint64_t curr_payload =
				(hiconf_bulk_buffer.at(pck_buffer_pos + npck).data & 0x3ffffffffffffull) |
				(static_cast<uint64_t>(dest) << 59);
			// printf("        packet % 2d: %016llX\n",npck,(unsigned long long)curr_payload);
			LOG4CXX_DEBUG(
				logger, "        packet "
							<< npck << ": " << std::hex
							<< (unsigned long long) curr_payload // TODO fill payload with zeros
							<< " (dnc " << dnc << ", hicann " << hic << ", "
							<< (is_read ? "read" : "write") << ")");
			payload.push_back(curr_payload); // data is 49bit plus 1bit ARQ tag, destination in MSBs
		}

		uint64_t* p_ptr = &(payload.front());
		retval &= this->sendSingleFrame(
			application_layer_packet_types::HICANNCONFIG, 8 * curr_pck_count, p_ptr, userdata,
			timeout);

		pck_buffer_pos += curr_pck_count;
		remaining_pck_count -= curr_pck_count;
	}
	this->hiconf_bulk_buffer.clear();

	return retval;
}


uint64_t HostALController::getReceivedHICANNConfig(
	unsigned int dnc, unsigned int hicann, unsigned int tagid, double const timeout)
{
	int ret;

	if ((dnc > 3) || (hicann > 7)) {
		LOG4CXX_ERROR(
			logger,
			"HostALController::getReceivedHICANNConfig: Invalid DNC or HICANN specified: DNC "
				<< dnc << ", HICANN " << hicann << ". Ignoring request.");
#ifndef NCSIM
		LOG4CXX_FATAL(
			logger, "HostALController::getReceivedHICANNConfig" << target_ip << ":" << target_port
																<< "]: "
																   "DNC or HICANN ID out of range");
#endif
		return 0;
	}

	unsigned int curr_id = ((dnc & 0x3) << 4) | ((hicann & 0x7) << 1) | (tagid & 0x1);

	// sum up all the sleeps until timeout is reached
	size_t accumulated_sleep = 0;

	// sleep timings in us (back-off to longer times; don't use >= 1s!)
	std::tr1::array<size_t, 10> const sleep_intervals = {5,    10,    100,   500,    1000,
	                                                     5000, 10000, 50000, 100000, 500000};

	// sleep if nothing received
	for (size_t sleep_interval_idx = 0; hiconf_receive_buffer.at(curr_id).size() == 0;) {
		if (!arq_ptr->received_packet_available()) {
// nothing in HostARQ layer, let's sleep
#ifdef NCSIM
			size_t const sleep_interval = 1; // sleep 1us in sim
			// assume much longer sleep times for sim
			accumulated_sleep += 1000 * sleep_interval;
			this->ethernet_if->pauseEthernet(sleep_interval);
#else
			timespec towait;
			towait.tv_sec = 0; // we don't support sleeping >= 1s
			towait.tv_nsec = sleep_intervals[sleep_interval_idx] * 1000;
			accumulated_sleep += sleep_intervals[sleep_interval_idx];
			do {
				ret = nanosleep(&towait, NULL);
			} while (ret == EINTR); // sleep again if interrupted ;)
			if (ret > 0) {
				LOG4CXX_ERROR(
					logger, "HostALController::getReceivedHICANNConfig: "
								<< "Too much caffeine; cannot sleep (ret was " << ret << ")?!?!?");
				break;
			}
			if (sleep_interval_idx + 1 < sleep_intervals.size())
				sleep_interval_idx++;
#endif
		}

		// recheck now after sleeping
		fill_receive_buffer(true);
		if (hiconf_receive_buffer.at(curr_id).size() > 0) {
			sleep_interval_idx = 0;           // reset sleep interval
			if (accumulated_sleep > 100000) { // previous max wait time
				// Ensure to log this in syslog by using error
				LOG4CXX_ERROR(
				    logger, "HostALController::getReceivedHICANNConfig() ["
				                << arq_ptr->get_remote_ip() << ":" << target_port
				                << "]: "
				                   "No data in receive buffer "
				                << curr_id << " after waiting " << accumulated_sleep
				                << " us. Cf. #2354");
			}
			break;
		}

		if (accumulated_sleep > timeout * 1000) {
			LOG4CXX_ERROR(
			    logger, "HostALController::getReceivedHICANNConfig ["
			                << arq_ptr->get_remote_ip() << ":" << target_port
			                << "]: No config packet received for DNC " << dnc << ", HICANN "
			                << hicann << " after waiting " << accumulated_sleep << " us.");
#ifndef NCSIM
			LOG4CXX_FATAL(
				logger, "HostALController::getReceivedHICANNConfig() ["
							<< target_ip << ":" << target_port
							<< "]: "
							   "No HICANN config packet received");
#endif

			return 0;
		}
	}

	// maybe a vector is more efficient, but costs memory space
	uint64_t data = hiconf_receive_buffer.at(curr_id).front();
	hiconf_receive_buffer.at(curr_id).pop_front();
	return data;
}


unsigned int HostALController::getReceivedHICANNConfigCount(
	unsigned int dnc, unsigned int hicann, unsigned int tagid)
{
	if ((dnc > 3) || (hicann > 7)) {
		LOG4CXX_ERROR(
			logger,
			"HostALController::getReceivedHICANNConfig: Invalid DNC or HICANN specified: DNC "
				<< dnc << ", HICANN " << hicann << ". Ignoring request.");
#ifndef NCSIM
		throw std::out_of_range("DNC or HICANN ID out of range");
#endif

		return 0;
	}

	unsigned int curr_id = ((dnc & 0x3) << 4) | ((hicann & 0x7) << 1) | (tagid & 0x1);

	this->fill_receive_buffer(false);
	return hiconf_receive_buffer.at(curr_id).size();
}


bool HostALController::startPlaybackTrace(double /*timeout*/)
{
	this->reset_fpga_times(); // have to reset counter for following experiments

	uint64_t payload = (1 << 29);
	if (pulse_loopback_enable)
		payload |= (1 << 24);
	if (pb_systime_replace_enable)
		payload |= (1 << 23);

	return sendFPGAConfigPacket(payload);
}


bool HostALController::stopTrace(double /*timeout*/)
{
	uint64_t payload = (1 << 27);
	if (pulse_loopback_enable)
		payload |= (1 << 24);
	if (pb_systime_replace_enable)
		payload |= (1 << 23);

	return sendFPGAConfigPacket(payload);
}


bool HostALController::startTraceRead(double /*timeout*/)
{
	this->reset_trace_buffers();

	uint64_t payload = (1 << 26);
	if (pulse_loopback_enable)
		payload |= (1 << 24);
	if (pb_systime_replace_enable)
		payload |= (1 << 23);

	LOG4CXX_DEBUG(
		logger, "setPlaybackSystimeReplacing: content=" << std::hex << payload << std::dec);

	return sendFPGAConfigPacket(payload);
}


bool HostALController::setFPGAPulseLoopback(bool loopback_enable, double /*timeout*/)
{
	if (this->pulse_loopback_enable != loopback_enable) {
		this->pulse_loopback_enable = loopback_enable;

		uint64_t payload = 0;
		if (pulse_loopback_enable)
			payload |= (1 << 24);
		if (pb_systime_replace_enable)
			payload |= (1 << 23);

		return sendFPGAConfigPacket(payload);
	}

	return true;
}

bool HostALController::setPlaybackSystimeReplacing(bool replace_enable, double /*timeout*/)
{
	if (this->pb_systime_replace_enable != replace_enable) {
		this->pb_systime_replace_enable = replace_enable;

		uint64_t payload = 0;
		if (pulse_loopback_enable)
			payload |= (1 << 24);
		if (pb_systime_replace_enable)
			payload |= (1 << 23);

		return sendFPGAConfigPacket(payload);
	}

	return true;
}

bool HostALController::setHicannARQLoopback(bool loopback_enable, double timeout)
{
	uint64_t payload = 0;
	if (loopback_enable)
		payload |= 1;

	return this->sendSingleFrame(
		application_layer_packet_types::FPGADEBUG, 8, &payload, 1, timeout);
}


void HostALController::reset_fpga_times()
{
	pb_end_address_received = 0;
	last_playback_reltime = 0;
}

// private methods

void HostALController::common_prepare()
{
	this->ethernet_if = NULL;

#ifndef HOSTAL_WITHOUT_JTAG
	this->jtag = NULL;
#endif

	this->target_mac = c_uiRemoteAddress;

	this->reset();
}


void HostALController::fill_receive_buffer(bool /*with_jtag*/)
{
	static size_t pck_count = 0;
	sctrltp::packet curr_pck;
	while (arq_ptr->receive(curr_pck)) {
		pck_count++;

		LOG4CXX_DEBUG(
			logger,
			"fill_receive_buffer: received hostARQ packet with " << curr_pck.len << " entries.");
		unsigned int frame_type = curr_pck.pid;

		if (curr_pck.len) {
			switch (frame_type) {
				case application_layer_packet_types::HICANNCONFIG: {
					LOG4CXX_DEBUG(
						logger, "HostALController: received frame "
									<< pck_count - 1 << " with ARQ data (number of 64-bit entries: "
									<< curr_pck.len << ").");

					for (unsigned int ndata = 0; ndata < curr_pck.len;
						 ++ndata) // we access 64-bit entries!
					{
						LOG4CXX_DEBUG(
							logger, "HostAL Raw packet: " << std::setfill('0') << std::setw(8)
														  << std::hex << curr_pck.pdu[ndata]);
						// HostARQ already converted 64-bit entries to host order

						union hicann_cfgdata
						{
							uint64_t raw;
							struct
							{
								uint64_t hicann_data : 49;
								uint64_t tag : 1;
								uint64_t _dummy1 : 9;
								uint64_t hicann : 3;
								uint64_t dnc : 2;
							} bitfield;
						} cfgdata;
						STATIC_ASSERT(sizeof(hicann_cfgdata) == sizeof(uint64_t));

						cfgdata.raw = curr_pck.pdu[ndata];

						LOG4CXX_DEBUG(
							logger, "  -> Received config packet from DNC "
										<< cfgdata.bitfield.dnc << ", HICANN "
										<< 7 - cfgdata.bitfield.hicann << ", Tag: "
										<< cfgdata.bitfield.tag << "): " << std::setfill('0')
										<< std::setw(16) << std::hex << curr_pck.pdu[ndata]);

						union FIXME_id
						{
							uint8_t raw;
							struct
							{
								uint8_t tag : 1; // LSB!
								uint8_t hicann : 3;
								uint8_t dnc : 2;
							} bitfield;
						} curr_id;
						STATIC_ASSERT(sizeof(FIXME_id) == sizeof(uint8_t));

						curr_id.raw = 0;
						curr_id.bitfield.tag = cfgdata.bitfield.tag;
						curr_id.bitfield.hicann = cfgdata.bitfield.hicann;
						curr_id.bitfield.dnc = cfgdata.bitfield.dnc;
						hiconf_receive_buffer.at(curr_id.raw)
							.push_back(cfgdata.bitfield.hicann_data);
					}
					break;
				}

				case application_layer_packet_types::FPGATRACE: {
					LOG4CXX_DEBUG(
						logger,
						"HostALController: received frame with trace data (number of entries: "
							<< curr_pck.len << ").");

					LOG4CXX_DEBUG(
						logger, "  -> pulses in buffer so far: " << received_pulses_count);

					rec_trace_entries += curr_pck.len;

					LOG4CXX_DEBUG(
						logger, "Number of trace DDR2 entries received (including current frame): "
									<< rec_trace_entries);

					for (unsigned int ndata = 0; ndata < curr_pck.len; ++ndata) {
						union FIXME_uglytimepulse
						{
							uint64_t raw;
							struct
							{
								uint64_t timestamp1 : 15; // LSB
								uint64_t label1 : 14;
								uint64_t fpga_msb1 : 1;
								uint64_t bit1 : 1;
								uint64_t bit2 : 1;
								uint64_t timestamp2 : 15;
								uint64_t label2 : 14;
								uint64_t fpga_msb2 : 1;
								uint64_t bit3 : 1;
								uint64_t bit4 : 1;
							} double_pulse;
							struct
							{
								uint64_t timestamp_ov : 15;
								uint64_t label_ov : 14;
								uint64_t fpga_msb_ov : 1;
								uint64_t ov_pulse_ignore : 1;
								uint64_t _pad_ov : 1;
								uint64_t overflow : 31;
								uint64_t ov_packet : 1;
							} single_pulse;
						} timepulse;
						STATIC_ASSERT(sizeof(FIXME_uglytimepulse) == sizeof(uint64_t));
						timepulse.raw = curr_pck.pdu[ndata];

						LOG4CXX_DEBUG(
							logger, "HostAL Raw packet: " << std::setfill('0') << std::setw(8)
														  << std::hex << curr_pck.pdu[ndata]);

						// handle both
						for (size_t ii = 0; ii < 2; ii++) {
							full_fpga_time_t curr_tstamp = timepulse.double_pulse.timestamp1;
							unsigned int curr_id = timepulse.double_pulse.label1;
							bool fpga_msb = timepulse.double_pulse.fpga_msb1;
							if (ii == 1) {
								curr_tstamp = timepulse.double_pulse.timestamp2;
								curr_id = timepulse.double_pulse.label2;
								fpga_msb = timepulse.double_pulse.fpga_msb2;
							}

							// handle pulse in overflow packet
							if (timepulse.single_pulse.ov_packet) {
								if ((ii == 0) ||
									(timepulse.single_pulse.ov_pulse_ignore)) // use second round of
																			  // loop if pulse is
																			  // contained in packet
									continue;

								curr_tstamp = timepulse.single_pulse.timestamp_ov;
								curr_id = timepulse.single_pulse.label_ov;
								fpga_msb = timepulse.single_pulse.fpga_msb_ov;
							}


							bool tstamp_msb = curr_tstamp >> 14;


							LOG4CXX_DEBUG(
								logger, "HostALController info: received pulse packet "
											<< received_pulses_count << " from trace memory: "
											<< "id=0x" << std::hex << curr_id << ", tstamp=0x"
											<< std::hex << curr_tstamp << ", tstamp msb: "
											<< tstamp_msb << " fpga msb: " << fpga_msb);

							last_traced_tstamp = curr_tstamp;

							full_fpga_time_t curr_inttime =
								(uint64_t)(curr_tstamp) +
								trace_overflow_count * MAX_TIMESTAMP_CNT; // FIXME

							// for pulse contained in overflow packet: revert increment for overflow
							// counter
							bool ignore_early_pulse = false;
							if ((!fpga_msb) && tstamp_msb) // detect special case that HICANN
														   // timestamp was registered before
														   // overflow, but pulse arrives in FPGA
														   // after overflow and an overflow packet
														   // was generated
														   // -> undo last overflow for that pulse
							{
								if (curr_inttime >= MAX_TIMESTAMP_CNT)
									curr_inttime -= MAX_TIMESTAMP_CNT;
								else
									ignore_early_pulse = true;
							}

							double curr_time = curr_inttime * dnc_clk_cycle_in_s;
							LOG4CXX_DEBUG(
								logger, "  Extraced time: " << curr_time << ", id: " << std::dec
															<< curr_id << ", overflow count: "
															<< trace_overflow_count);

							// Old bug where trace memory potentially stored pulse twice while
							// sending overflow packet, should not occure anymore
							// TODO 2016-04-27: remove check when its absolutly sure that the bug is
							// fixed
							if (recpulse_buffer.size() &&
								(recpulse_buffer.back().second == curr_id) &&
								((recpulse_buffer.back().first == curr_inttime) ||
								 (recpulse_buffer.back().first + MAX_TIMESTAMP_CNT ==
								  curr_inttime))) {
								LOG4CXX_WARN_BACKTRACE(
									logger, "HostALController::fill_receive_buffer(): Received "
											"pulse twice ISSUE 2022");
								LOG4CXX_WARN(
									logger, "in received hostARQ packet with frame type "
												<< curr_pck.pid << " and " << curr_pck.len
												<< " entries.");
								continue;
							}

							if (!ignore_early_pulse) {
								this->received_pulses_count++;
								this->recpulse_buffer.push_back(pulse_t(curr_inttime, curr_id));
							}
						}

						if (timepulse.single_pulse.ov_packet) // overflow bit on
						{
							LOG4CXX_DEBUG(
								logger, "Overflow packet with value 0x"
											<< std::hex << timepulse.single_pulse.overflow
											<< " received");
							trace_overflow_count += 1;
						}
					}
					break;
				}

				case application_layer_packet_types::FPGAPLAYBACK: {
					this->pb_end_address_received = curr_pck.pdu[0];
					LOG4CXX_DEBUG(
						logger,
						"HostALController: received end-of-data message from playback with address "
							<< std::dec << this->pb_end_address_received);
					break;
				}

				case application_layer_packet_types::JTAGBULK: {
					this->jtag_receive_buffer.push_back(curr_pck);
					break;
				}

				default: {
					LOG4CXX_ERROR(
						logger, "HostALController: unknown frame type of received data: "
									<< std::hex << frame_type << ". Ignoring data.");
					break;
				}
			}
		}
	}
}
