#ifndef _FPGA_CTRL_H_
#define _FPGA_CTRL_H_


#include "s2ctrl.h"
#include "s2comm.h"
#include "sctrltp/ARQStream.h"

namespace facets {



class FPGAControl : public Stage2Ctrl
{
	public:
		FPGAControl(Stage2Comm *comm_class, unsigned int set_fpga_id);

		/// send a single L2 pulse 	to a HICANN chip.
		/// Bit 14:0  time stamp
		/// Bit 23:15 neuron id (3bit for SPL1 channel, 6bit target l1 address
		void sendSingleL2Pulse(unsigned char channel, uint64 l2pulse){
			GetCommObj()->jtag->FPGA_start_pulse_event(channel, l2pulse);
		}
		
		/// retrieve the current content of the data rx register from DNC
		void getFpgaRxData(uint64_t& rxdata){
			GetCommObj()->jtag->FPGA_get_rx_data(rxdata);
		}

		// trigger (soft) FPGA reset
		void reset();

		// Spike counter for Playback and Trace Memory
		uint64_t get_trace_pulse_count() const;
		uint64_t get_pb_pulse_count() const;

		// Number of sent and received packets pov HICANNARQ to HICANN
		uint64_t get_network_debug_reg_rx() const;
		uint64_t get_network_debug_reg_tx() const;

		// Number of sent and received packets pov HICANNARQ to Uplink, i.e. host
		uint64_t get_ul_packet_cnt_r() const;
		uint64_t get_ul_packet_cnt_w() const;

		// Git hash of bitfile
		uint32_t get_bitfile_git_hash() const;

		// Git dirty flag
		bool get_git_dirty_flag() const;

		// Flag true, if PB module should release spikes/config faster than the
		// DDR-memory allows
		bool get_pb_release_error() const;

		// Flag true, if PB module sends Config faster than the HICANN-ARQ can process
		// resulting in a fifo overflow
		bool get_pb2arq_fifo_overflow() const;

		bool get_crc_counter() const;

		// Prints all stats module information
		void print_all_stats() const;

		// static versions for direct access via arq_stream
		static uint64_t get_trace_pulse_count(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static uint64_t get_pb_pulse_count(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static uint64_t get_network_debug_reg_rx(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static uint64_t get_network_debug_reg_tx(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static uint64_t get_ul_packet_cnt_r(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static uint64_t get_ul_packet_cnt_w(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static uint32_t get_bitfile_git_hash(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static bool get_git_dirty_flag(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static bool get_pb_release_error(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static bool get_pb2arq_fifo_overflow(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);
		static bool get_crc_counter(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const);

	private:
		virtual std::string ClassName() { return "FPGAControl"; };
		static uint64_t readStat(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr, unsigned int const stat_id);
		unsigned int fpga_id;
		sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* m_arq_ptr;

};



}  // end of namespace facets

#endif //_DNC_CTRL_H_

