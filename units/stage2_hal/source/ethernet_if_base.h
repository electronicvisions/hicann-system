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

#ifndef __ETHERNET_IF_BASE_H__
#define __ETHERNET_IF_BASE_H__

#include <stdint.h>
#include <cstddef>
#include <vector>

// return type for received UDP frames
typedef struct rx_entry
{
  uint32_t  uiSourceIP;
  uint16_t  uiSourcePort;
  uint16_t  uiDestPort;
  std::vector<char> pData;
  
  rx_entry() : uiSourceIP(0), uiSourcePort(0), uiDestPort(0)
  {}

  // length in terms of T-units
  template<typename T>
  size_t len() const {
    return pData.size() / sizeof(T);
  }

  template<typename T>
  T const * word_access() const {
    return reinterpret_cast<T const *>(&(*pData.begin()));
  }

} rx_entry_t;

// base class for UDP communication
class EthernetIFBase
{
	public:
		virtual ~EthernetIFBase()
		{}

		virtual bool init(const uint16_t uiPort) = 0;

		virtual int sendUDP(const uint32_t uiTargetIP, const uint16_t uiPort, const void *pszData, const uint16_t uiDataBytes) = 0;

		virtual bool receiveUDP(rx_entry_t &) = 0;
		virtual bool hasReceivedUDP() = 0;

		virtual void pauseEthernet(double time) = 0; // stop execution for specified time (timebase for simulation: 1us, for real operation PC-FPGA: 1ms)

};

#endif


