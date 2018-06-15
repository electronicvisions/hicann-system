// Company         :   tud                      
// Author          :   partzsch            
// E-Mail          :   <email>                	
//                    			
// Filename        :   spinn_controller.h                
// Project Name    :   p_facets
// Subproject Name :   s_fpga            
// Description     :   <short description>            
//                    			
//------------------------------------------------------------------------
#ifndef _SPINN_CONTROLLER_H
#define _SPINN_CONTROLLER_H

#include <stdint.h>
#include <vector>
#include <list>

#include <boost/scoped_ptr.hpp>

#include "ethernet_if_base.h"


class SpinnController
{

	public:
		SpinnController(uint32_t set_target_ip = 0xC0A80101, uint16_t set_config_port=1850, uint16_t set_pulse_port=1851);

		/// return the target IP of the current FPGA connection.
		uint32_t getTargetIP();

		/// return the target port of the current FPGA connection.
		uint16_t getConfigPort();

		/// return the target port of the current FPGA connection.
		uint16_t getPulsePort();


	// configuration functions
		
		/// clear local buffer for routing memory entries
		void clearRoutingEntries();
		
		/// set single entry for routing memory
		void setRoutingEntry(unsigned int addr, unsigned int value);

		/// actually send the local routing memory entries to the FPGA
		/// returns true if successful
		bool writeRoutingMemory();


		/// set input pulse multiplier (4bit).
		/// returns true if successful
		bool writePulseMultiplier(unsigned int pulse_multiplier);

		/// set downsample count (10bit).
		/// returns true if successful
		bool writeDownSampleCount(unsigned int downsample_count);


		/// set port that the SpiNNaker pulse interface reacts on.
		/// returns true if successful
		bool writePulsePort(unsigned int pulse_port);

		/// set sending behaviour: if active, FPGA sends all pulses from the HICANNs to the given IP;
		/// else, it waits for incoming packets and sends pulses to the source of the first packet.
		/// returns true if successful
		bool writeSenderSettings(bool is_active, uint32_t target_ip, uint16_t target_port);

		/// configure generation of output addresses.
		/// single_hicann_en: enable single HICANN mode: only send spikes from the requested HICANN, address is truncated to 9bit, i.e. handled as if it was coming from HICANN 0
		/// single_hicann_addr: address of HICANN in single HICANN mode
		/// addr_offset: offset that is added to all addresses; addresses are truncated to 14bit
		/// input_addr_mode: 0-default, 1-reverse byte order before handing address to routing memory
		/// output_addr_mode: 0-default, 1-reverse byte order, 2-shift address to upper 16bit, 3-shift and reverse byte order (address in LSBs, but two bytes swapped)
		/// returns true if successful
		bool writeOutputAddressConfiguration(bool single_hicann_en, unsigned int single_hicann_addr, unsigned int addr_offset, bool input_addr_mode, unsigned int output_addr_mode);



	// pulse functions		
		/// add pulse to pulse packet.
		void addPulse(unsigned int addr);
		
		/// send current list of pulses to FPGA.
		/// returns true if successful
		bool sendPulses();


		/// send current list of pulses to FPGA, using specified target port.
		/// returns true if successful
		bool sendPulsesToPort(unsigned int pport);		

		/// return next received pulse; if no pulse was received, return -1
        int getReceivedPulse();

	private:
		boost::scoped_ptr< EthernetIFBase > config_if;
		boost::scoped_ptr< EthernetIFBase > pulse_if;
		uint32_t const target_ip;
		uint16_t const config_port;
		uint16_t const pulse_port;
		std::vector<uint32_t> routingbuf;
		std::vector<uint32_t> pulsebuf;
        std::list<uint32_t> recpulses; // FIFO for received pulses
};

#endif // _SPINN_CONTROLLER_H

