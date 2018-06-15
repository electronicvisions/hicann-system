// Company         :   tud
// Author          :   partzsch
// E-Mail          :   <email>
//
// Filename        :   ethernet_if_base.h
// Project Name    :   p_facets
// Subproject Name :   s_fpga
// Description     :   <short description>
//
//------------------------------------------------------------------------

// interface class for sending and receiving UDP packets

#ifndef __ETHERNET_SOFTWARE_IF_H__
#define __ETHERNET_SOFTWARE_IF_H__


#include <arpa/inet.h>

#include "ethernet_if_base.h"

#ifdef NCSIM
  // simulation mode
  #include "eth_socket.h"
#endif

// pre-declarations
class Logger;

class EthernetSoftwareIF : public EthernetIFBase
{
	public:
	
		EthernetSoftwareIF();
	
		virtual ~EthernetSoftwareIF();

		bool init(const uint16_t uiPort);

		virtual int sendUDP(const uint32_t uiTargetIP, const uint16_t uiPort, const void *pszData, const uint16_t uiDataBytes);

		virtual bool receiveUDP(rx_entry_t &);
		virtual bool hasReceivedUDP();

		virtual void pauseEthernet(double time); // stop execution for specified time (timebase for simulation: 1us, for real operation PC-FPGA: 1ms)

		void end();

	private:
		#ifdef NCSIM
			// simulation mode
              eth_socket socket_if;
		#endif
		int udp_socket; // only needed for host software mode

		struct sockaddr_in remoteServAddr;

		Logger& _log;
};

#endif
