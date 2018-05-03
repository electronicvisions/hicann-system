//-----------------------------------------------------------------
//
// Copyright (c) 2015 TU-Dresden  All rights reserved.
//
// Unless otherwise stated, the software on this site is distributed
// in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. THERE IS NO WARRANTY FOR THE SOFTWARE,
// TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN OTHERWISE
// STATED IN WRITING THE COPYRIGHT HOLDERS PROVIDE THE SOFTWARE
// "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE ENTIRE
// RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH YOU.
// SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL
// NECESSARY SERVICING, REPAIR OR CORRECTION. IN NO EVENT UNLESS
// REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL ANY
// COPYRIGHT HOLDER, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY
// GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT
// OF THE USE OR INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT
// LIMITED TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES
// SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF THE SOFTWARE TO
// OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER HAS BEEN
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//-----------------------------------------------------------------

// Company           :   TU-Dresden
// Author            :   Stefan Scholze
// E-Mail            :   scholze@iee.et.tu-dresden.de
//
// Filename          :   jtag_ethernet.hpp
// Project Name      :   JTAG Library V2
// Description       :   Ethernet JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | scholze    | 29 Aug 2013   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_ETHERNET_HPP__
#define __JTAG_ETHERNET_HPP__

#include "jtag_eth_socket.hpp"
#include "jtag_if.hpp"
#include "jtag_state.hpp"
#include "jtag_stdint.h"

#define MAX_BUFFER_DEPTH 1400

namespace jtag_lib_v2 {
class jtag_ethernet : public jtag_if
{
private:
	eth_socket_base* m_pEthSocket;
	struct sockaddr_in m_sockAddr;
	uint8_t m_auiSendBuffer[MAX_BUFFER_DEPTH];
	uint8_t m_auiReceiveBuffer[MAX_BUFFER_DEPTH];
	uint16_t m_uiSendSize;
	jtag_state m_jtagState;
	uint16_t m_uiCurrTxSize;
	uint32_t m_uiBufferDepth;
	uint16_t m_uiMajorVersion;
	uint16_t m_uiMinorVersion;
	bool m_bNibbleHigh;
	bool m_bUseSystemC;
	bool m_bRxData;
	bool m_bNamedSocket;
	uint8_t* m_puiTDOBuffer;

	bool sendData(const uint16_t);
	uint16_t receiveData(const uint16_t);
	bool readTDO(const uint16_t, const bool bFinal = true);

	jtag_ethernet();
	jtag_ethernet(const jtag_ethernet&);

public:
	jtag_ethernet(const bool);
	virtual ~jtag_ethernet();
	virtual bool createSession(const char*, const uint16_t);
	virtual bool enumCables(uint8_t&, const bool);
	virtual bool openCable(const uint8_t, const uint32_t, const bool);
	virtual bool closeCable();
	virtual bool setPin(const enum eJtagPin, const bool);
	virtual bool getPin(const enum eJtagPin, bool&);
	virtual bool pulsePin(const eJtagPin, const uint16_t);
	virtual bool shiftDeviceIR(const uint8_t, uint8_t*, uint32_t);
	virtual bool shiftChainIR(uint8_t*, uint32_t);
	virtual bool shiftDeviceDR(const uint8_t, uint8_t*, uint8_t*, uint32_t, const enum eScanMode);
	virtual bool shiftChainDR(uint8_t*, uint8_t*, uint32_t, const enum eScanMode);
	virtual bool navigate(const enum eJtagState, const uint16_t);
	virtual bool setJtagFrequency(const uint32_t);
	virtual bool flushCable();
};

} // namespace jtag_lib_v2

#endif // __JTAG_ETHERNET_HPP__
