// Company         :   tud                      
// Author          :   partzsch            
// E-Mail          :   <email>                	
//                    			
// Filename        :   host_al_controller.h                
// Project Name    :   p_facets
// Subproject Name :   s_fpga            
// Description     :   <short description>            
//                    			
//------------------------------------------------------------------------
#ifndef _HOST_AL_CONTROLLER_H
#define _HOST_AL_CONTROLLER_H

#include <vector>
#include <deque>
#include <tr1/array>

extern "C" {
#include <stdint.h>
}

#include "ethernet_if_base.h"
#include "pulse_float.h"

#ifndef HOSTAL_WITHOUT_JTAG
  #include "common.h" // for myjtag_full
#endif

// ncsim-based compilation uses custom Makefile...
// => don't add path to units/communcation/
#include "sctrltp/ARQStream.h"
#include "sctrltp/ARQFrame.h"


// flags for transport layer
#define TLFLAG_SYN 0x02
#define TLFLAG_ACK 0x10
//#define TLFLAG_FIN 0x01
#define TLFLAG_RST 0x04

// types of application layer packets
// as defined in: "Specification of 
// FACETS/BrainScaleS Stage2 Host <-> 
// FPGA Communication" - Rev 0.1
namespace application_layer_packet_types {
enum e
{
	JTAGBULK = 0x06A4, //!< send and receive a bulk of JTGA nibbles to/from FPGA
	FPGATRACE = 0x0CA5,
	FPGAPLAYBACK = 0x0C5A,
	FPGACONFIG = 0x0C1B,
	HICANNCONFIG = 0x2A1B
};
}


// Ethernet defines
// default target MAC Address
static const unsigned char c_uiRemoteAddress[6] = { 0x00, 0x19, 0x99, 0x12, 0x3B, 0x6C };

// default target IP 192.168.1.17

#ifdef NCSIM
static const uint32_t c_uiRemoteIP = 0xC0A80101;
#else
static const uint32_t c_uiRemoteIP = 0xC0A80111;
#endif

static const unsigned int min_reltime_distance_clks = 6;




template<typename T, size_t offset_msb = 0, size_t offset_lsb = 0>
struct fpga_data_field {

	T get() const {
		return sanitize(data);
	}

	void set(T value) {
		sanitize(value);
		data = (get_high() << (size() + offset_lsb)) | get_low() | (value << offset_lsb);
	}

	T get_high() const {
		// drop all data + lower bits
		size_t const cnt_nonhigh_bits = size() - offset_msb;
		return data >> cnt_nonhigh_bits;
	}

	T get_low() const {
		// drop all data + higher bits
		size_t const cnt_nonlow_bits = size() - offset_lsb;
		return (data << cnt_nonlow_bits) >> cnt_nonlow_bits;
	}

	size_t size() const {
		STATIC_ASSERT(sizeof(T) == 2);
		return sizeof(T);
	}

	size_t bitsize() const {
		return size() - offset_msb - offset_lsb;
	}

private:
	T data;

	T sanitize(T value) {
		return (((data << offset_msb)  // remove upper bits
				>> offset_lsb)  // move back
			>> offset_lsb); // and drop lower bits
	}
} __attribute__((packed));




class HostALController
{
	private:
		struct hicann_config_t
		{
			unsigned int dnc;
			unsigned int hicann;
			uint64_t data;
			
			hicann_config_t() : dnc(0), hicann(0), data(0)
			{}
			
			hicann_config_t(uint64_t set_data,unsigned int set_dnc,unsigned int set_hicann) : dnc(set_dnc), hicann(set_hicann), data(set_data)
			{}
		};

		// FIXME: hotfix (will be replaced by @lpilz's time class)
		typedef uint64_t full_fpga_time_t;
		typedef uint64_t full_hicann_time_t;
		typedef uint16_t pulse_address_t;

		// pair.first  - pulse time stamp in HICANN clk cycles (4ns)
		// pair.second - 14bit pulse address, consisting of DNC ID, HICANN ID, channel ID and neuron address
		typedef std::pair<full_hicann_time_t, pulse_address_t> pulse_t;

	public:
		HostALController();

		HostALController(uint32_t set_target_ip, uint16_t set_target_port);

	#ifndef HOSTAL_WITHOUT_JTAG
		/// set Ethernet-JTAG controller to use
		void setJTAGInterface(myjtag_full *set_jtag);
	#endif

		void setARQStream(sctrltp::ARQStream *set_arq);

		sctrltp::ARQStream *getARQStream();

		/// set the Ethernet interface to use.
		/// should be a derived class of EthernetIFBase
		void setEthernetIF(EthernetIFBase *set_eth);

		/// get the current Ethernet interface.
		EthernetIFBase *getEthernetIF();

		/// initialize Ethernet socket.
		bool initEthernetIF();

		/// clear all receive buffers and init flags.
		/// should be called when FPGA is reset.
		void reset();
		
		/// find out if FPGA connection has been initialized before.
		bool isInitialized();

		/// intialize the host-FPGA connection.
		bool initFPGAConnection(unsigned int seq_number = 0);

		/// return the target IP of the current FPGA connection.
		uint32_t getTargetIP() const;

		/// return the target port of the current FPGA connection.
		uint16_t getTargetPort() const;

		/// send single AL frame.
		/// if timeout > 0: wait timeout for acknowledge, else: return directly
		bool sendSingleFrame( 
			unsigned int frametype,  //!< type identifier of the frame
			unsigned int pllength,  //!< number of bytes in the payload
			void *payload,  //!< pointer to memory area with payload data
			unsigned int user_data=0,  //!< lower 16 bit of the frame type header, if 0: insert number of 32bit packets in payload
			double const timeout=500.0 //!< time [in ms] after which function returns if no acknowledge was received
			);

		// send FPGA config packet and block until response packet was sent by FPGA
		// response packet is copy of the sent config packet
		bool sendFPGAConfigPacket(uint64_t const payload);

		/// wait until next UDP packet is received.
		/// returns true if no packet occured until timeout
		bool waitForReceivedUDP(double timeout=0.0, double poll_interval = 1.0);

		/// TODO: ME: Define protocol procedures for Host and FPGA side.
		/// TODO: ME: Add descriptions for usage.

		/// send JTAG packet.
		/// first 32bit of payload contains length: number of 32bit entries to come
		/// remainder of payload are JTAG nibbles as provided by JTAGLib V1 or V2
		bool sendJTAG(unsigned int pllength, void* payload, double timeout = 100.0);

		/// return next received hostARQ packet of JTAG type.
		/// if nothing was received, an empty packet (length 0) is returned
		sctrltp::packet getReceivedJTAGData();

		/// send I2C packet.
		///
		bool sendI2C(uint64_t payload, double timeout = 100.0);

		/// poll DDR2 TRACE data.
		/// 
		std::vector<pulse_float_t> getFPGATracePulses();

		/// get next pulse from trace buffer in host - used from HALBE.
		/// function looks for pulses that have arrived at the host first
		pulse_t getTracePulse();
		
		/// get number of trace pulses that are currently in host buffer - used from HALBE.
		/// function looks for pulses that have arrived at the host first
		unsigned int getTraceBufferSize();

		/// activate writing of spike data to file in binary format
		void activateBinarySpikeDataFileWrite(bool set_active = true);

		/// send data to the FPGA playback memory
		/// 
		bool sendFPGAPlayback(std::vector<uint32_t> &payload, double timeout=100.0);

		/// send list of pulses to FPGA playback memory.
		/// manages conversion to timestamps and management of timestamp overflows
		/// pulses must be sorted in time, times denote the release time at the FPGA in seconds (system time scale, not biological) and must be positive
		bool sendPulseList(const std::vector<pulse_float_t> & pulses, double fpga_hicann_delay_in_s = 500.0e-9);
		
		/// send configuration packet to playback memory.
		/// manages conversion to timestamps and management of timestamp overflows, however,
		/// it must be ensured that the sending time is sorted with previous/subsequent pulses
		/// time denotes release time at the FPGA in seconds (system time scale, not biological) and must be positive
		/// the single configuration packet is sent directly to the FPGA in a separate UDP frame
		bool sendPlaybackConfig(double fpga_time, uint64_t payload, unsigned int dnc=0, unsigned int hicann=0, double timeout=100.0);
		
		
		/// add single pulse to buffer - used from HALBE.
		/// fpga_time is given in FPGA clk cycles (8ns)
		/// hicann_time is given in DNC/HICANN clock cycles (4ns)
		/// id is a 14bit address, consisting of DNC ID, HICANN ID, channel ID and neuron address
		void addPlaybackPulse(full_fpga_time_t fpga_time, full_hicann_time_t hicann_time, pulse_address_t id);


		/// add single configuration packet to buffer - used from HALBE.
		/// fpga_time is given in FPGA clk cycles (8ns)
		/// hicann_id is a 5bit address, consisting of DNC ID and HICANN ID
		void addPlaybackConfig(full_fpga_time_t fpga_time, uint64_t payload, pulse_address_t hicann_id);

		
		/// send pulse buffer to FPGA - used from HALBE.
		bool flushPlaybackPulses();

		/// set system time trigger in FPGA.
		bool setSystime(bool const enable, bool const listen_global, bool const expstart_mode, double const timeout=100.0);

		/// add data end marker to FPGA playback.
		bool addPlaybackFPGAConfig(full_fpga_time_t fpga_time, bool pb_end_marker, bool stop_trace, bool start_trace_read);

		/// returns memory address of last acknowledged end-of-data marker from playback (zero when no acknowledge was received).
		unsigned int getPlaybackEndAddress();

		/// send single HICANN config packet.
		bool sendSingleHICANNConfig(uint64_t payload, unsigned int dnc=0, unsigned int hicann=0, double timeout=100.0);		

		/// add HICANN config packet to bulk of packets.
		void addHICANNConfigBulk(uint64_t payload, unsigned int dnc=0, unsigned int hicann=0);
		
		/// send bulk of config packets.
		/// attention: does not check on maximum allowed length of frame
		bool sendHICANNConfigBulk(double timeout=100.0);


		/// return next HICANN config packet from the receive buffer for the specified HICANN.
		/// fills packet into data, returns false if no data is available
		/// FIXME: extreme high timeout to workaround floating gate problems
		uint64_t getReceivedHICANNConfig(unsigned int dnc, unsigned int hicann, unsigned int tagid, double timeout=20000.0);

		/// return number of HICANN config packets that are available in host receive buffer for the specified HICANN
		unsigned int getReceivedHICANNConfigCount(unsigned int dnc, unsigned int hicann, unsigned int tagid);

	// experiment control

		/// start Playback and Trace memory simultaneously
		bool startPlaybackTrace(double timeout=100.0);

		/// stop monitoring by Trace memory
		bool stopTrace(double timeout=100.0);

		/// start DDR2 trace memory readout
		///  
		bool startTraceRead(double timeout=100.0);			


		/// set internal loopback of pulses in FPGA
		bool setFPGAPulseLoopback(bool loopback_enable, double timeout=100.0);

		/// set replace mode, where the timestamp fields of playback pulses are set to the system time counter at the time of their release
		bool setPlaybackSystimeReplacing(bool replace_enable, double timeout=100.0);

		/// resets last_playback_reltime to 0

		void reset_fpga_times();

	private:
		void common_prepare();
		void fill_receive_buffer(bool with_jtag=true);

		/// reset all trace buffers.
		/// called by startTraceRead and reset()
		void reset_trace_buffers();

		/// reset all playback buffers.
		/// called by flushPlaybackPulses and reset()
		void reset_playback_buffers();

		/// reset all hicann config buffers (send and receive).
		/// called by reset()
		void reset_hiconf_buffers();

	private:
		EthernetIFBase *ethernet_if;
		char unsigned const *target_mac; // should not be changed by the class
		uint32_t const target_ip;
		uint16_t const target_port;
		bool ethernet_init;
		
	#ifndef HOSTAL_WITHOUT_JTAG
		myjtag_full *jtag;
	#endif

		sctrltp::ARQStream *arq_ptr;

		std::vector<hicann_config_t> hiconf_bulk_buffer; /// buffer for hicann config bulks
		std::vector<uint32_t> playback_entry_buffer; /// buffer for playback entries. Is filled via addPlaybackPulse() or addPlaybackConfig() and cleared in flushPlaybackPulses()
		full_fpga_time_t last_playback_reltime; /// release time of last pulse in playback memory in FPGA clk cycles(=8ns)
		int last_group_start_in_buffer;

		// receive buffer for JTAG data
		std::deque<sctrltp::packet> jtag_receive_buffer;

		// receive buffers for HICANN config
		std::tr1::array< std::deque<uint64_t>, 64> hiconf_receive_buffer;
		
		// receive buffer for traced pulses
		uint64_t received_pulses_count; /// number of received pulses so far. used for debug output
		std::deque<pulse_t> recpulse_buffer;
		uint64_t trace_overflow_count; /// number of counter overflows in trace memory occured so far.
		full_hicann_time_t last_traced_tstamp; /// holds the last traced time stamp (hicann clk cycles). Use to determine counter overflows.
		unsigned int rec_trace_entries; /// number of trace DDR2 entries received 
		unsigned int pb_end_address_received; /// last acknowledged end address of playback content, as requested by playback end-of-data marker

		double const fpga_clk_cycle_in_s;
		double const dnc_clk_cycle_in_s;

		bool pulse_loopback_enable;
		bool pb_systime_replace_enable;
        
        bool spikes_to_binfile_enable;
};

#endif // _HOST_AL_CONTROLLER_H
