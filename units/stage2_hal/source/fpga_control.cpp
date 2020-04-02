#ifndef _FPGA_CTRL_CPP_
#define _FPGA_CTRL_CPP_


#include "fpga_control.h"
#include "host_al_controller.h"
#include "logger.h"
#include "logger_ncsim_compat.h"
#ifndef HICANN_DESIGN
	#include "s2c_jtagphys_2fpga.h"
	#include "s2c_jtagphys_2fpga_arq.h"
#endif

#ifndef NCSIM
static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("hicann-system.FPGACtrl");
#endif

namespace facets {

#ifndef SYSTIME_PERIOD_NS
	#define SYSTIME_PERIOD_NS 4.0
#endif

FPGAControl::FPGAControl(Stage2Comm *comm_class, unsigned int set_fpga_id)
	: Stage2Ctrl(comm_class,set_fpga_id),
	  fpga_id(set_fpga_id)
{
	S2C_JtagPhys2FpgaArq* tmp = dynamic_cast<S2C_JtagPhys2FpgaArq*>(comm);
	m_arq_ptr = NULL;
	if (tmp) {
		m_arq_ptr = tmp->getHostAL()->getARQStream();
	}
}

void FPGAControl::reset() {
#ifndef HICANN_DESIGN
	unsigned int curr_ip = GetCommObj()->jtag->get_ip();
	//std::cout << GetCommObj() << std::endl;
	//std::cout << typeid(GetCommObj()).name() << std::endl;
	S2C_JtagPhys2Fpga *comm_ptr = dynamic_cast<S2C_JtagPhys2Fpga *>(GetCommObj());
	if (comm_ptr != NULL)                                                                                                                                                                                                                
	{
		comm_ptr->set_fpga_reset(curr_ip, true, true, true, true, true); // set reset
		#ifdef NCSIM
		#else
		usleep(100); // wait: optional, to see LED flashing - can be removed
		#endif
		comm_ptr->set_fpga_reset(curr_ip, false, false, false, false, false); // release reset
		#ifdef NCSIM
		wait(120, SC_US); // wait for resetted memory controllers to come up again -> crucial for simulation!
		#endif
		//printf("FPGAControl::reset: performed soft FPGA reset for IP %X.\n", curr_ip);
	} else
		printf("FPGAControl::reset: ### error ###: bad s2comm pointer; "
		       "did not perform soft FPGA reset. This is expected for non-ARQ communication modes\n");
#else
	printf("FPGAControl::reset: ### warning ###: soft FPGA reset not implemented in HICANN-only testbench\n");
#endif
}

uint64_t FPGAControl::readStat(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr, unsigned int const stat_id)
{
	sctrltp::packet curr_pck;
	curr_pck.pid = PTYPE_STATS;
	curr_pck.len = 1;
	curr_pck.pdu[0]= stat_id;

	while (arq_ptr->send_buffer_full())
		//wait for empty tx entry to send
		usleep(100);

	// FIXME: not safe in multi thread case
	// send packet
	if ( !arq_ptr->send(curr_pck) ) {
		throw std::runtime_error("FPGAControl::readStat(): could not send ARQ packet");
	}

	// receive packet
	// sum up all the sleeps until timeout is reached
	size_t accumulated_sleep = 0;
	size_t const timeout = 100; //in ms
	// sleep timings in us (back-off to longer times; don't use >= 1s!)
	std::tr1::array<size_t, 8> const sleep_intervals = {5, 10, 100, 500, 1000, 5000, 10000, 100000};
	size_t sleep_interval_idx = 0;

	uint64_t stat_value;

	while (true) {
		if (! arq_ptr->received_packet_available()) {
			// nothing in HostARQ layer, let's sleep
			int ret;
			timespec towait;
			towait.tv_sec = 0; // we don't support sleeping >= 1s
			towait.tv_nsec = sleep_intervals[sleep_interval_idx] * 1000;
			accumulated_sleep += sleep_intervals[sleep_interval_idx];
			do {
				ret = nanosleep(&towait, NULL);
			} while (ret == EINTR); // sleep again if interrupted ;)
			if (ret > 0) {
				LOG4CXX_ERROR(logger, "FPGAController::readStat: "
					<< "Too much caffeine; cannot sleep (ret was " << ret << ")?!?!?");
				break;
			}
			if (sleep_interval_idx + 1 < sleep_intervals.size())
				sleep_interval_idx++;
			if (accumulated_sleep > timeout*1000) {
				throw std::underflow_error("FPGAController::readStat(): No stat packet received");
			}
		} else {
			//packet received
			arq_ptr->receive(curr_pck);

			//check packet type
			if(curr_pck.pid == PTYPE_STATS) {
				if(curr_pck.len == 1) {
					stat_value = curr_pck.pdu[0];
					return stat_value;
				} else {
					throw std::runtime_error("FPGAControl::readStat(): received stat answer packet longer than one 64 bit word");
				}
			} else {
				std::stringstream debug_msg;
				debug_msg << "FPGAControl::readStat(): received wrong packet type: ";
				debug_msg << "expected " << PTYPE_STATS << " but received " << curr_pck.pid
				          << std::endl;
				/* FIXME: Blocking (i.e. issue read and wait for received) functions rely on idle
				 * connection! We should enforce it or make this function async (issue read here
				 * and evaluate result in receive loop). */
				throw std::runtime_error(debug_msg.str());
			}
		}
	}
	return 0;
}

uint64_t FPGAControl::get_trace_pulse_count(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_trace_pulse_count(): arq_ptr is NULL");
	uint64_t counter = readStat(arq_ptr, 0);
	return counter;
}

uint64_t FPGAControl::get_pb_pulse_count(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_pb_pulse_count(): arq_ptr is NULL");
	uint64_t counter = readStat(arq_ptr, 1);
	return counter;
}

uint64_t FPGAControl::get_network_debug_reg_rx(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_network_debug_reg_rx(): arq_ptr is NULL");
	uint64_t counter = readStat(arq_ptr, 2);
	return counter;
}

uint64_t FPGAControl::get_network_debug_reg_tx(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_network_debug_reg_tx(): arq_ptr is NULL");
	uint64_t counter = readStat(arq_ptr, 3);
	return counter;
}

uint64_t FPGAControl::get_ul_packet_cnt_r(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_ul_packet_cnt_r(): arq_ptr is NULL");
	uint64_t counter = readStat(arq_ptr, 4);
	return counter;
}

uint64_t FPGAControl::get_ul_packet_cnt_w(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_ul_packet_cnt_w(): arq_ptr is NULL");
	uint64_t counter = readStat(arq_ptr, 5);
	return counter;
}

bool FPGAControl::get_git_dirty_flag(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_git_dirty_flag(): arq_ptr is NULL");
	uint64_t stats = readStat(arq_ptr, 6);
	stats = stats & 0x1;
	return stats;
}

uint32_t FPGAControl::get_bitfile_git_hash(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_bitfile_git_hash(): arq_ptr is NULL");
	return readStat(arq_ptr, 6) >> 32;
}

bool FPGAControl::get_pb_release_error(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_pb_release_error(): arq_ptr is NULL");
	uint64_t stats = readStat(arq_ptr, 7);
	return stats & 0x1;
}

bool FPGAControl::get_pb2arq_fifo_overflow(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_pb2arq_fifo_overflow(): arq_ptr is NULL");
	uint64_t stats = readStat(arq_ptr, 7);
	return (stats & 0x2) >> 1;
}

bool FPGAControl::get_crc_counter(sctrltp::ARQStream<sctrltp::ParametersFcpBss1>* const arq_ptr)
{
	if (!arq_ptr)
		throw std::runtime_error("FPGAControl::get_crc_counter(): arq_ptr is NULL");
	uint64_t stats = readStat(arq_ptr, 8);
	return stats;
}

uint64_t FPGAControl::get_trace_pulse_count() const
{
	return get_trace_pulse_count(m_arq_ptr);
}

uint64_t FPGAControl::get_pb_pulse_count() const
{
	return get_pb_pulse_count(m_arq_ptr);
}

uint64_t FPGAControl::get_network_debug_reg_rx() const
{
	return get_network_debug_reg_rx(m_arq_ptr);
}

uint64_t FPGAControl::get_network_debug_reg_tx() const
{
	return get_network_debug_reg_tx(m_arq_ptr);
}

uint64_t FPGAControl::get_ul_packet_cnt_r() const
{
	return get_ul_packet_cnt_r(m_arq_ptr);
}

uint64_t FPGAControl::get_ul_packet_cnt_w() const
{
	return get_ul_packet_cnt_w(m_arq_ptr);
}

bool FPGAControl::get_git_dirty_flag() const
{
	return get_git_dirty_flag(m_arq_ptr);
}

uint32_t FPGAControl::get_bitfile_git_hash() const
{
	return get_bitfile_git_hash(m_arq_ptr);
}

bool FPGAControl::get_pb_release_error() const
{
	return get_pb_release_error(m_arq_ptr);
}

bool FPGAControl::get_pb2arq_fifo_overflow() const
{
	return get_pb2arq_fifo_overflow(m_arq_ptr);
}

bool FPGAControl::get_crc_counter() const
{
	return get_crc_counter(m_arq_ptr);
}

void FPGAControl::print_all_stats() const
{
		uint64_t const tracepc =  get_trace_pulse_count();
		uint64_t const pbpc = get_pb_pulse_count();
		uint64_t const debug_reg_rx = get_network_debug_reg_rx();
		uint64_t const debug_reg_tx = get_network_debug_reg_tx();
		uint64_t const ulr = get_ul_packet_cnt_r();
		uint64_t const ulw = get_ul_packet_cnt_w();
		bool const dirty = get_git_dirty_flag();
		uint32_t const hash = get_bitfile_git_hash();
		bool const pbrelerror = get_pb_release_error();
		bool const fifooverflow = get_pb2arq_fifo_overflow();
		bool crc = get_crc_counter();
		LOG4CXX_INFO(logger,
			std::dec << "FPGA Stats:\n"
			<< "Trace Pulse Count: " << tracepc << " | PB pulse count: " << pbpc
			<< "\nUplink packet counter reads: " << ulr << " | writes: " << ulw
			<< "\nNetwork debug register reads: " << debug_reg_rx << " | writes: " << debug_reg_tx
			<< "\nGit hash: 0x" << hash << " | Dirty flag: " << dirty
			<< "\nPb release error: " << pbrelerror << " | fifo overflow: " << fifooverflow
			<< "\nCRC counter: " << std::hex << crc);
}

}  // end of namespace facets

#endif //_DNC_CTRL_H_

