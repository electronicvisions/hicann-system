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
// Filename          :   jtag_ethernet.cpp
// Project Name      :   JTAG Library V2
// Description       :   Ethernet JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | scholze    | 29 Aug 2013   |  initial version
// ----------------------------------------------------------------

#include "jtag_ethernet.hpp"
#include "jtag_extern.h"
#include "jtag_logger.hpp"
#include "jtag_stdint.h"
#include "sctrltp/us_sctp_defs.h"

#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <chrono>

#include <boost/format.hpp>

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable : 4800)
#endif
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#endif

#define DEFAULT_BUFFER_DEPTH 256
#define MIN_BUFFER_DEPTH 16

#define ETH_IDCODE 0xC007FACE
#define CMD_ETH_IDCODE 0x1DC0DE00
#define CMD_JTAG 0x0C
#define CMD_SINGLE 0x3A
#define CMD_BULK 0x33
#define ARQ_JTAG_TIMEOUT 400000 // us

jtag_lib_v2::jtag_ethernet::jtag_ethernet(const bool bUseSystemC)
	: m_pEthSocket(0),
	  m_uiCurrTxSize(0),
	  m_uiBufferDepth(DEFAULT_BUFFER_DEPTH),
	  m_uiMajorVersion(0),
	  m_uiMinorVersion(0),
	  m_pARQStream(NULL),
	  m_bNibbleHigh(false),
	  m_bUseSystemC(bUseSystemC),
	  m_bRxData(false),
	  m_bNamedSocket(false)
{}

bool jtag_lib_v2::jtag_ethernet::createSession(std::shared_ptr<sctrltp::ARQStream> arq_stream)
{
	/* release socket if it is already created */
	if (this->m_pEthSocket) {
		delete this->m_pEthSocket;
	}

	this->m_pARQStream = arq_stream;

	/* mark libraries open */
	this->setSessionOpen(true);

	return true;
}

bool jtag_lib_v2::jtag_ethernet::createSession(const char* szAddr, const uint16_t uiPort)
{
	/* check target address */
	if (!szAddr) {
		jtag_logger::sendMessage(MSG_ERROR, "createSession: Invalid address specified.\n");
		return false;
	}

	/* check if we want to use named socket */
	this->m_bNamedSocket = false;
	if (uiPort == 0)
		this->m_bNamedSocket = true;

	/* check that we try to send to an unpriviledged port */
	if (uiPort < 1024 && uiPort) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Only unpriviledged ports are "
					   "allowed for JTAG Ethernet.\n");
		return false;
	}

	/* release socket if it is already created */
	if (this->m_pEthSocket)
		delete this->m_pEthSocket;

#ifndef _WIN32
	if (this->m_bUseSystemC) {
		/* load socket functions from eth_stim */
		if (!eth_socket_sc::loadEthSocket()) {
			jtag_logger::sendMessage(
				MSG_ERROR, "createSession: Failed to load SystemC "
						   "socket functions.\n");
			return false;
		}
	}
#endif

	/* create socket instance */
	try {
#ifndef _WIN32
		if (this->m_bUseSystemC && !this->m_bNamedSocket)
			this->m_pEthSocket = new eth_socket_sc();
		else
#endif
			this->m_pEthSocket = new eth_socket_host();
	} catch (std::bad_alloc&) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Failed to create Ethernet "
					   "socket instance.\n");
		return false;
	}

	int iResult;
	if (!this->m_bNamedSocket) {
		/* try to resolve address */
		iResult = this->m_pEthSocket->resolveAddress(szAddr, this->m_sockAddr.sin_addr);
		if (iResult) {
			if (this->m_bUseSystemC)
				jtag_logger::sendMessage(
					MSG_ERROR,
					"createSession: '%s' is not a valid "
					"IPv4 address.\n",
					szAddr);
			else {
				jtag_logger::sendMessage(
					MSG_ERROR,
					"createSession: Resolving address '%s' "
					"failed.\n",
					szAddr);
#ifdef _WIN32
				const char* szError = eth_socket_base::strerror(iResult);
#else
				const char* szError = ::gai_strerror(iResult);
#endif
				if (szError)
					jtag_logger::sendMessage(MSG_ERROR, "createSession: %s.\n", szError);
			}
			return false;
		}
	}

	if (this->m_bNamedSocket)
		jtag_logger::sendMessage(
			MSG_INFO,
			"createSession: Connecting to named socket '%s' "
			"...\n",
			szAddr);
	else
		jtag_logger::sendMessage(
			MSG_INFO,
			"createSession: Connecting to '%s:%d' "
			"(%d.%d.%d.%d) ...\n",
			szAddr, uiPort, this->m_sockAddr.sin_addr.s_addr & 0xFF,
			(this->m_sockAddr.sin_addr.s_addr >> 8) & 0xFF,
			(this->m_sockAddr.sin_addr.s_addr >> 16) & 0xFF,
			(this->m_sockAddr.sin_addr.s_addr >> 24) & 0xFF);

	/* open the socket */
	if (this->m_bNamedSocket)
		iResult = this->m_pEthSocket->open(AF_UNIX, SOCK_STREAM, 0);
	else
		iResult = this->m_pEthSocket->open(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (iResult < 0) {
		jtag_logger::sendMessage(MSG_ERROR, "createSession: Open socket failed.\n");
		const char* szError = eth_socket_base::strerror(this->m_pEthSocket->lasterror());
		if (szError)
			jtag_logger::sendMessage(MSG_ERROR, "createSession: %s.\n", szError);
		return false;
	}

	if (this->m_bNamedSocket) {
		/* connect socket */
		struct sockaddr_un sockAddr;

		::memset(&sockAddr, 0, sizeof(struct sockaddr_un));
		sockAddr.sun_family = AF_UNIX;
		::memcpy(sockAddr.sun_path, szAddr, UNIX_PATH_MAX);
		iResult =
			this->m_pEthSocket->connect((struct sockaddr*) &sockAddr, sizeof(struct sockaddr_un));
	} else {
		/* bind socket */
		struct sockaddr_in sockAddr;
		sockAddr.sin_addr = this->m_sockAddr.sin_addr;

		this->m_sockAddr.sin_family = AF_INET;
		this->m_sockAddr.sin_port = htons(0);
		this->m_sockAddr.sin_addr.s_addr = INADDR_ANY;
		iResult = this->m_pEthSocket->bind(
			(struct sockaddr*) &this->m_sockAddr, sizeof(struct sockaddr_in));

		// m_sockAddr reused for sending; setting values for sending
		this->m_sockAddr.sin_addr = sockAddr.sin_addr;
		this->m_sockAddr.sin_port = htons(uiPort);
	}

	if (iResult) {
		this->m_pEthSocket->close();
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: %s socket failed.\n",
			(this->m_bNamedSocket) ? "Connect" : "Bind");
		const char* szError = eth_socket_base::strerror(this->m_pEthSocket->lasterror());
		if (szError)
			jtag_logger::sendMessage(MSG_ERROR, "createSession: %s.\n", szError);
		return false;
	}

	/* mark libraries open */
	this->setSessionOpen(true);

	return true;
}

bool jtag_lib_v2::jtag_ethernet::enumCables(uint8_t& uiCablesFound, const bool /*bEnumOther*/)
{
	uiCablesFound = 0;

	/* check session */
	if (!this->isSessionOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "enumCables: No open session.\n");
		return false;
	}

	/* do nothing if a cable is already open in this session */
	if (this->isCableOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "enumCables: A cable is already open.\n");
		return false;
	}

	jtag_logger::sendMessage(MSG_INFO, "enumCables: Enumerating connected devices ...\n");

	/* request IDCODE of device */
	this->m_auiSendBuffer[0] = uint8_t(CMD_ETH_IDCODE >> 24);
	this->m_auiSendBuffer[1] = uint8_t(CMD_ETH_IDCODE >> 16);
	this->m_auiSendBuffer[2] = uint8_t(CMD_ETH_IDCODE >> 8);
	this->m_auiSendBuffer[3] = uint8_t(CMD_ETH_IDCODE >> 0);

	if (!this->sendData(4)) {
		jtag_logger::sendMessage(MSG_ERROR, "enumCables: Requesting IDCODE failed.\n");
		return false;
	}

	const uint16_t uiResult = this->receiveData(12);
	if (!uiResult) {
		jtag_logger::sendMessage(MSG_ERROR, "enumCables: No IDCODE received.\n");
		return false;
	}
	if (uiResult != 4 && uiResult != 12) {
		jtag_logger::sendMessage(MSG_ERROR, "enumCables: Received wrong IDCODE length.\n");
		return false;
	}

	const uint32_t* puiIdcode = reinterpret_cast<uint32_t*>(this->m_auiReceiveBuffer);
	const uint32_t uiIdcode = ntohl(*puiIdcode);

	if (uiIdcode != ETH_IDCODE) {
		jtag_logger::sendMessage(MSG_ERROR, "enumCables: Received wrong IDCODE.\n");
		jtag_logger::sendMessage(
			MSG_ERROR, "enumCables: Expected 0x%08X, but got 0x%08X.\n", ETH_IDCODE, uiIdcode);
		return false;
	}

	this->m_uiBufferDepth = DEFAULT_BUFFER_DEPTH;
	if (uiResult > 4) {
		uint32_t uiBufferDepth = ntohl(
			(this->m_auiReceiveBuffer[11]) | (this->m_auiReceiveBuffer[10] << 8) |
			(this->m_auiReceiveBuffer[9] << 16) | (this->m_auiReceiveBuffer[8] << 24));
		if (uiBufferDepth >= 0x3FFFFFFF)
			uiBufferDepth = DEFAULT_BUFFER_DEPTH;
		else {
			uiBufferDepth++;
			uiBufferDepth <<= 2;
		}
		if (uiBufferDepth > MAX_BUFFER_DEPTH)
			uiBufferDepth = MAX_BUFFER_DEPTH;
		else if (uiBufferDepth < MIN_BUFFER_DEPTH) {
			jtag_logger::sendMessage(
				MSG_ERROR,
				"enumCables: Hardware buffer is too "
				"small (%d).\n",
				uiBufferDepth + 1);
			return false;
		}
		this->m_uiBufferDepth = uiBufferDepth;

		this->m_uiMajorVersion =
			ntohs((this->m_auiReceiveBuffer[4] << 8) | (this->m_auiReceiveBuffer[5]));
		this->m_uiMinorVersion =
			ntohs((this->m_auiReceiveBuffer[6] << 8) | (this->m_auiReceiveBuffer[7]));
	}

	jtag_logger::sendMessage(MSG_INFO, "enumCables: Found 1 cable.\n");

	cable_info* pCable;
	try {
		pCable = new cable_info();
	} catch (std::bad_alloc&) {
		return false;
	}

	/* ethernet cables are never locked */
	pCable->setLocked(false);

	this->addCableInfo(pCable);
	uiCablesFound++;

	/* signal that enumeration has completed */
	this->markEnumDone();

	return true;
}

bool jtag_lib_v2::jtag_ethernet::openCable(
	const uint8_t uiCableIndex, const uint32_t uiFrequency, const bool /*bForceUnlock*/)
{
	/* get a cable to open */
	cable_info* pCableInfo;
	if (!this->getCableToOpen(uiCableIndex, &pCableInfo, false))
		return false;

	/* store requested frequency */
	this->setJtagFrequency(uiFrequency);

	jtag_logger::sendMessage(
		MSG_INFO,
		"openCable: Using TCK frequency of "
		"%3.3lf MHz.\n",
		uiFrequency / 1000.);
	jtag_logger::sendMessage(
		MSG_INFO, "openCable: Buffer size: %d Bytes.\n", this->m_uiBufferDepth);

	if (this->m_uiMajorVersion || this->m_uiMinorVersion)
		jtag_logger::sendMessage(
			MSG_INFO, "openCable: Hardware version: v%d.%d.\n", this->m_uiMajorVersion,
			this->m_uiMinorVersion);

	/* mark cable as open */
	this->setCableOpen(true);

	/* set unknown jtag state */
	this->m_jtagState.setJtagState(UNKNOWN_JTAG_STATE);

	return true;
}

bool jtag_lib_v2::jtag_ethernet::closeCable()
{
	if (this->isCableOpen()) {
		/* Send pending data */
		this->flushCable();

		/* close socket */
		if (this->m_pEthSocket) {
			this->m_pEthSocket->close();
		}

		/* mark cable as closed */
		this->setCableOpen(false);

		/* set unknown jtag state */
		this->m_jtagState.setJtagState(UNKNOWN_JTAG_STATE);
	}

	return true;
}

bool jtag_lib_v2::jtag_ethernet::pulsePin(
	const enum jtag_lib_v2::eJtagPin ePin, const uint16_t uiCount)
{
	/* if no pulse, skip */
	if (!uiCount)
		return true;

	for (uint32_t i = 0; i < uiCount; ++i) {
		/* set pin */
		if (!this->setPin(ePin, true))
			return false;

		/* no wait required due to ethernet latency */

		/* release pin */
		if (!this->setPin(ePin, false))
			return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_ethernet::setPin(const enum jtag_lib_v2::eJtagPin ePin, const bool bValue)
{
	/* Flush current write packets */
	if (this->m_uiCurrTxSize)
		this->flushCable();

	uint8_t uiData = 0;
	uint8_t uiValid = 0;
	switch (ePin) {
		case JTAG_PIN_TDI:
			uiValid = 1;
			uiData = (bValue) ? 1 : 0;
			break;
		case JTAG_PIN_TMS:
			uiValid = 2;
			uiData = (bValue) ? 2 : 0;
			break;
		case JTAG_PIN_TCK:
			uiValid = 4;
			uiData = (bValue) ? 4 : 0;
			break;
		default:
			jtag_logger::sendMessage(MSG_ERROR, "setPin: Requested pin can't be set.\n");
			return false;
	}

	/* create ethernet packet */
	this->m_auiSendBuffer[0] = uint8_t(CMD_JTAG);
	this->m_auiSendBuffer[1] = uint8_t(CMD_SINGLE);
	this->m_auiSendBuffer[2] = uint8_t(0);
	this->m_auiSendBuffer[3] = (uiValid << 4) | uiData;

	/* send ethernet packet */
	if (!this->sendData(4))
		return false;

	/* receive ethernet answer and check result */
	uint16_t uiResult = this->receiveData(4);
	if (!uiResult)
		return false;
	if (uiResult != 4) {
		jtag_logger::sendMessage(MSG_ERROR, "setPin: Received wrong length.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_ethernet::getPin(const enum jtag_lib_v2::eJtagPin ePin, bool& bValue)
{
	/* Flush current write packets */
	if (this->m_uiCurrTxSize)
		this->flushCable();

	if (ePin != JTAG_PIN_TDI || ePin != JTAG_PIN_TMS || ePin != JTAG_PIN_TCK ||
		ePin != JTAG_PIN_TDO) {
		jtag_logger::sendMessage(
			MSG_ERROR, "getPin: Cannot retrieve value of requested "
					   "pin.\n");
		return false;
	}

	/* create ethernet packet */
	this->m_auiSendBuffer[0] = uint8_t(CMD_JTAG);
	this->m_auiSendBuffer[1] = uint8_t(CMD_SINGLE);
	this->m_auiSendBuffer[2] = uint8_t(0);
	this->m_auiSendBuffer[3] = uint8_t(0);

	/* send ethernet packet */
	if (!this->sendData(4))
		return false;

	/* receive ethernet answer and check result */
	uint16_t uiResult = this->receiveData(4);
	if (!uiResult)
		return false;
	if (uiResult != 4) {
		jtag_logger::sendMessage(MSG_ERROR, "getPin: Received wrong length.\n");
		return false;
	}

	/* decode result */
	uint8_t uiData = this->m_auiReceiveBuffer[3];
	bValue = false;
	switch (ePin) {
		case JTAG_PIN_TDI:
			bValue = uiData & 0x1;
			break;
		case JTAG_PIN_TMS:
			bValue = uiData & 0x2;
			break;
		case JTAG_PIN_TCK:
			bValue = uiData & 0x4;
			break;
		case JTAG_PIN_TDO:
			bValue = uiData & 0x8;
			break;
	}

	return true;
}

bool jtag_lib_v2::jtag_ethernet::shiftDeviceDR(
	const uint8_t /*uiDevice*/,
	uint8_t* /*puiTDIBuffer*/,
	uint8_t* /*puiTDOBuffer*/,
	uint32_t /*uiBitCount*/,
	const enum eScanMode /*eMode*/)
{
	/* unsupported, must be handled by higher level API */
	return false;
}

bool jtag_lib_v2::jtag_ethernet::shiftDeviceIR(
	const uint8_t /*uiDevice*/, uint8_t* /*puiTDIBuffer*/, uint32_t /*uiBitCount*/)
{
	/* unsupported, must be handled by higher level API */
	return false;
}

bool jtag_lib_v2::jtag_ethernet::shiftChainIR(uint8_t* puiTDIBuffer, uint32_t uiBitCount)
{
	/* check buffer */
	if (uiBitCount && !puiTDIBuffer) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftChainIR: Need TDI buffer to perform "
					   "requested operation.\n");
		return false;
	}

	/* move to CAPTURE-IR */
	if (!this->navigate(CAPTURE_IR, 0))
		return false;

	/* for zero shift bits go directly to RUN-TEST-IDLE */
	if (!uiBitCount)
		return this->navigate(RUN_TEST_IDLE, 0);

	/* move to SHIFT-IR */
	if (!this->navigate(SHIFT_IR, 0))
		return false;

	/* shift out TDI bits */
	uint8_t uiTDIPattern = 0;
	uint8_t uiData = 0;
	uint8_t uiTms = 0;
	uint8_t uiTdi = 0;

	for (uint32_t uiBits = 0; uiBits < uiBitCount; ++uiBits) {
		/* Flush, if buffer is full */
		if (this->m_uiCurrTxSize == this->m_uiBufferDepth)
			this->flushCable();

		/* get TDI pattern for next 8 bits */
		if (!(uiBits & 0x7)) {
			uiTDIPattern = *puiTDIBuffer;
			puiTDIBuffer++;
		}

		/* move to EXIT1-IR on last shift */
		if (uiBits == (uiBitCount - 1))
			uiTms = 0x02;
		/* set TDI bit */
		uiTdi = (uiTDIPattern & 0x1) ? 0x01 : 0x00;

		/* pulse TCK */
		if (!this->m_bNibbleHigh) {
			uiData = uiTms | uiTdi;
			uiData |= (uiTms | uiTdi | 0x4) << 4;
		} else {
			this->m_auiSendBuffer[this->m_uiCurrTxSize] |= (uiTms | uiTdi) << 4;
			uiData = uiTms | uiTdi | 0x04;
		}

		uiTDIPattern >>= 1;

		this->m_auiSendBuffer[this->m_uiCurrTxSize] = uiData;
		this->m_uiCurrTxSize++;
	}

	this->m_jtagState.setJtagState(EXIT1_IR);

	/* move to RUN-TEST-IDLE */
	return this->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_ethernet::shiftChainDR(
	uint8_t* puiTDIBuffer, uint8_t* puiTDOBuffer, uint32_t uiBitCount, const enum eScanMode eMode)
{
	/* check buffers */
	if (eMode != SCAN_MODE_WRITE && !puiTDOBuffer && uiBitCount) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftChainDR: Need TDO buffer to perform "
					   "requested operation.\n");
		return false;
	}

	if (eMode != SCAN_MODE_READ && !puiTDIBuffer && uiBitCount) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftChainDR: Need TDI buffer to perform "
					   "requested operation.\n");
		return false;
	}

	/* move to CAPTURE-DR */
	if (!this->navigate(CAPTURE_DR, 0))
		return false;

	/* for zero shift bits go directly to RUN-TEST-IDLE */
	if (!uiBitCount)
		return this->navigate(RUN_TEST_IDLE, 0);

	/* move to SHIFT-DR */
	if (!this->navigate(SHIFT_DR, 0))
		return false;

	/* flush ethernet buffer if we are going to read bits */
	uint16_t uiBufferDepth;
	if (eMode != SCAN_MODE_WRITE) {
		if (!this->flushCable()) {
			jtag_logger::sendMessage(
				MSG_ERROR, "shiftChainDR: Failed to send data "
						   "before read operation.\n");
			return false;
		}
		this->m_puiTDOBuffer = puiTDOBuffer;
		this->m_bRxData = true;
		uiBufferDepth = this->m_uiBufferDepth - this->m_uiCurrTxSize;
	} else {
		this->m_bRxData = false;
		uiBufferDepth = this->m_uiBufferDepth;
	}

	/* shift out TDI bits and capture TDO bits */
	uint8_t uiTDIPattern = 0;
	uint8_t uiData = 0;
	uint8_t uiTms = 0;
	uint8_t uiTdi = 0;
	uint8_t uiTdo = 0;
	uint16_t uiBitsToRead = 0;
	for (uint32_t uiBits = 0; uiBits < uiBitCount; ++uiBits) {
		/* Flush, if buffer is full */
		if (this->m_uiCurrTxSize == uiBufferDepth) {
			if (this->m_bRxData) {
				if (!this->readTDO(uiBitsToRead, false))
					return false;
				uiBitsToRead = 0;
			} else if (!this->flushCable()) {
				jtag_logger::sendMessage(MSG_ERROR, "shiftChainDR: Failed to send data.\n");
				return false;
			}
		}

		/* get TDI pattern for next 8 bits */
		if (!(uiBits & 0x7) && eMode != SCAN_MODE_READ) {
			uiTDIPattern = *puiTDIBuffer;
			puiTDIBuffer++;
		}
		/* move to EXIT1-DR on last shift */
		if (uiBits == (uiBitCount - 1))
			uiTms = 0x02;

		if (eMode != SCAN_MODE_READ) {
			/* set TDI bit */
			uiTdi = (uiTDIPattern & 0x1) ? 0x01 : 0x00;
			uiTDIPattern >>= 1;
		}

		/* pulse TCK to 0 (negedge) */
		if (!this->m_bNibbleHigh)
			uiData = uiTms | uiTdi;
		else
			this->m_auiSendBuffer[this->m_uiCurrTxSize] |= (uiTms | uiTdi) << 4;

		/* get TDO if read is enabled */
		if (eMode != SCAN_MODE_WRITE)
			uiTdo = 0x08;

		/* pulse TCK to 1 (posedge) */
		if (!this->m_bNibbleHigh)
			uiData |= ((uiTms | uiTdi | uiTdo | 0x4) << 4);
		else
			uiData = uiTms | uiTdi | uiTdo | 0x04;

		this->m_auiSendBuffer[this->m_uiCurrTxSize] = uiData;
		this->m_uiCurrTxSize++;
		uiBitsToRead++;
	}

	/* get last TDO values */
	if (this->m_bRxData) {
		if (!this->readTDO(uiBitsToRead))
			return false;
	}

	this->m_jtagState.setJtagState(EXIT1_DR);

	/* move to RUN-TEST-IDLE */
	return this->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_ethernet::flushCable()
{
	if (this->m_uiCurrTxSize <= 4) {
		// no transmit data
		// first 4 bytes are just bulk/single access header
		return true;
	}

	/* calculate number of nibbles */
	uint16_t uiNibblesSend = 2 * (this->m_uiCurrTxSize - 4);
	uiNibblesSend -= (this->m_bNibbleHigh) ? 1 : 0;

	m_auiSendBuffer[2] = uint8_t(uiNibblesSend >> 8);
	m_auiSendBuffer[3] = uint8_t(uiNibblesSend & 0xFF);

	/* send the ethernet frame */
	uint16_t uiNumBytes = this->m_uiCurrTxSize;
	if (uiNumBytes & 0x3)
		uiNumBytes += (4 - (uiNumBytes & 0x3));
	if (!this->sendData(uiNumBytes))
		return false;

	/* no rx data requested, get empty tx answer packet */
	if (!this->m_bRxData) {
		/* receive answer ethernet frame */
		uint16_t uiResult = this->receiveData(4);

		/* check result */
		if (!uiResult)
			return false;
		if (uiResult != 4) {
			jtag_logger::sendMessage(MSG_ERROR, "flushCable: Received wrong length.\n");
			return false;
		}
	}

	/* initialize for next transfer */
	this->m_bNibbleHigh = false;
	this->m_auiSendBuffer[0] = uint8_t(CMD_JTAG);
	this->m_auiSendBuffer[1] = uint8_t(CMD_BULK);
	this->m_auiSendBuffer[2] = 0;
	this->m_auiSendBuffer[3] = 0;
	this->m_uiCurrTxSize = 4;

	return true;
}

bool jtag_lib_v2::jtag_ethernet::navigate(const enum eJtagState eState, const uint16_t uiClocks)
{
	/* move to requested state */
	uint16_t uiTmsPattern;
	uint8_t uiTmsBits;
	uint8_t uiData;

	if (!this->m_jtagState.getTmsPatternForState(eState, uiTmsPattern, uiTmsBits)) {
		jtag_logger::sendMessage(MSG_ERROR, "navigate: Unknown JTAG state requested.\n");
		return false;
	}

	/* initialize send buffer if it is empty */
	if (!m_uiCurrTxSize) {
		this->m_auiSendBuffer[0] = uint8_t(CMD_JTAG);
		this->m_auiSendBuffer[1] = uint8_t(CMD_BULK);
		this->m_auiSendBuffer[2] = 0;
		this->m_auiSendBuffer[3] = 0;
		this->m_uiCurrTxSize = 4;
		this->m_bNibbleHigh = false;
	}

	/* copy TMS bits to send buffer */
	for (uint8_t uiBits = 0; uiBits < uiTmsBits; ++uiBits) {
		if (!this->m_bNibbleHigh) {
			uiData = (uiTmsPattern & 0x1) ? 0x02 : 0x00;
			uiData |= (uiTmsPattern & 0x1) ? 0x60 : 0x40;
		} else {
			this->m_auiSendBuffer[this->m_uiCurrTxSize] |= (uiTmsPattern & 0x1) ? 0x20 : 0x00;
			uiData = (uiTmsPattern & 0x1) ? 0x06 : 0x04;
		}

		/* Flush, if buffer is full */
		if (this->m_uiCurrTxSize == this->m_uiBufferDepth)
			this->flushCable();

		this->m_auiSendBuffer[this->m_uiCurrTxSize] = uiData;
		this->m_uiCurrTxSize++;
		uiTmsPattern >>= 1;
	}

	/* perform additional clocks in final state */
	this->m_jtagState.getTmsPatternForState(eState, uiTmsPattern, uiTmsBits);

	if (!uiClocks || uiTmsBits > 1)
		return true;

	/* create clock cycles and keep TMS bit */
	for (uint8_t uiClockCnt = 0; uiClockCnt < uiClocks; ++uiClockCnt) {
		if (!this->m_bNibbleHigh) {
			uiData = (uiTmsPattern & 0x1) ? 0x2 : 0x0;
			uiData |= (uiTmsPattern & 0x1) ? 0x60 : 0x40;
		} else {
			this->m_auiSendBuffer[this->m_uiCurrTxSize] |= (uiTmsPattern & 0x1) ? 0x20 : 0x00;
			uiData = (uiTmsPattern & 0x1) ? 0x06 : 0x04;
		}

		this->m_auiSendBuffer[this->m_uiCurrTxSize] = uiData;
		this->m_uiCurrTxSize++;
	}

	return true;
}

bool jtag_lib_v2::jtag_ethernet::setJtagFrequency(const uint32_t uiFrequency)
{
	/* Flush current write packets */
	if (this->m_uiCurrTxSize != 0)
		this->flushCable();

	/* limit TCK frequency to 50 MHz */
	uint32_t uiFreq = uiFrequency;
	if (uiFreq > 50000)
		uiFreq = 50000;

	/* default to 1MHz if frequency is 0 */
	if (!uiFreq)
		uiFreq = 1000;

	// ticks per TCK periode
	const uint32_t uiDivsPerClock = 2;
	// inner frequency after division
	const uint32_t uiInnerFreq = uiDivsPerClock * uiFrequency;
	// reference frequency
	const uint32_t uiRefFreqKhz = 125 * 1000;
	// nominator
	const uint32_t uiNom = uiRefFreqKhz - uiInnerFreq;
	// divider
	const uint32_t uiDiv = (uiNom < uiRefFreqKhz) ? uiNom / uiInnerFreq : 0;

	/* set requested frequency */
	this->m_auiSendBuffer[0] = uint8_t(CMD_JTAG);
	this->m_auiSendBuffer[1] = uint8_t(CMD_SINGLE);
	this->m_auiSendBuffer[2] = uint8_t(uiDiv);
	this->m_auiSendBuffer[3] = uint8_t(0x80);

	/* send ethernet packet */
	if (!this->sendData(4))
		return false;

	/* receive ethernet answer and check result */
	uint16_t uiResult = this->receiveData(4);
	if (!uiResult)
		return false;
	if (uiResult != 4) {
		jtag_logger::sendMessage(MSG_ERROR, "setJtagFrequency: Received wrong length.\n");
		return false;
	}

	return true;
}

jtag_lib_v2::jtag_ethernet::~jtag_ethernet()
{
	/* close cable */
	this->closeCable();
}

bool jtag_lib_v2::jtag_ethernet::sendData(const uint16_t uiLength)
{
	if (!uiLength) {
		return true;
	}

	if (this->m_pEthSocket) {
		this->m_uiSendSize = uiLength;
		int iResult;
		if (this->m_bNamedSocket)
			iResult = this->m_pEthSocket->sendto(this->m_auiSendBuffer, uiLength, 0, 0);
		else
			iResult = this->m_pEthSocket->sendto(
				this->m_auiSendBuffer, uiLength, (struct sockaddr*) &this->m_sockAddr,
				sizeof(struct sockaddr_in));
		if (iResult < 0) {
			const int iError = this->m_pEthSocket->lasterror();
			const char* szError = eth_socket_base::strerror(iError);
			if (szError)
				jtag_logger::sendMessage(MSG_ERROR, "sendData: %s (%d).\n", szError, iError);
			return false;
		}
		if (!iResult) {
			jtag_logger::sendMessage(
				MSG_ERROR,
				"sendData: Nothing was transferred, but %d "
				"Bytes requested to sent.\n",
				uiLength);
			return false;
		}
		if (iResult != uiLength) {
			jtag_logger::sendMessage(
				MSG_ERROR,
				"sendData: Only %d Bytes were transferred, "
				"but %d requested.\n",
				iResult, uiLength);
			return false;
		}
	} else if (this->m_pARQStream) {
		sctrltp::packet curr_pck;
		curr_pck.pid = application_layer_packet_types::JTAGBULK;
		curr_pck.len = ((uiLength + 4) + 7) / 8; // 64bit words
		size_t padded_length = curr_pck.len * 8 - 4;

		// align to 64bit words: append zeros if necessary
		for (size_t nbyte = uiLength; nbyte < padded_length; ++nbyte) {
			this->m_auiSendBuffer[nbyte] = 0;
		}

		memcpy((uint8_t*) (curr_pck.pdu) + 4, this->m_auiSendBuffer, padded_length);

		*((uint32_t*) (curr_pck.pdu)) = (uiLength + 3) / 4; // 4 bytes per 32 bit word

		// reorder data in 32bit blocks, will be reversed in
		// hmf-fpga/units/jtag_udp_if/source/rtl/verilog/jtag_eth_if.v
		for (size_t nword = 1; nword < ((size_t)uiLength + 7) / 4; ++nword) {
			*((uint32_t*) (curr_pck.pdu) + nword) = ntohl(*((uint32_t*) (curr_pck.pdu) + nword));
		}

#ifdef NCSIM
		while (this->m_pARQStream->send_buffer_full()) {
			wait(10.0, SC_US);
		}
#endif

		if (!this->m_pARQStream->send(curr_pck)) {
			jtag_lib_v2::jtag_logger::sendMessage(
				jtag_lib_v2::MSG_ERROR, "sendData: ARQ.send() not successful.\n");
			return false;
		}
		return true;
	} else {
		throw std::runtime_error("sending data without open session");
	}
	return true;
}

uint16_t jtag_lib_v2::jtag_ethernet::receiveData(const uint16_t uiLength)
{
	if (this->m_pEthSocket) {
		uint8_t uiRetries = 0;
		int iResult = 0;
		do {
			iResult = this->m_pEthSocket->recvDataAvail();
			if (iResult < 0) {
				const int iError = this->m_pEthSocket->lasterror();
				const char* szError = eth_socket_base::strerror(iError);
				if (szError)
					jtag_logger::sendMessage(MSG_ERROR, "receiveData: %s (%d).\n", szError, iError);
			}
			uiRetries++;
		} while (!iResult && uiRetries < 3);

		if (!iResult) {
			jtag_logger::sendMessage(MSG_ERROR, "receiveData: Waiting for data timed out.\n");
			return 0;
		}

		if (this->m_bNamedSocket) {
			iResult = this->m_pEthSocket->recvfrom(this->m_auiReceiveBuffer, uiLength, 0, 0, 0);
			if (!iResult)
				jtag_logger::sendMessage(MSG_ERROR, "receiveData: Server closed connection.\n");
		} else {
			sockaddr_in sock_addr_rcv;
			int iSockLen = sizeof(struct sockaddr_in);
			iResult = this->m_pEthSocket->recvfrom(
				this->m_auiReceiveBuffer, uiLength, 0, (struct sockaddr*) &sock_addr_rcv,
				&iSockLen);
		}
		if (iResult < 0) {
			const int iError = this->m_pEthSocket->lasterror();
			const char* szError = eth_socket_base::strerror(iError);
			if (szError)
				jtag_logger::sendMessage(MSG_ERROR, "receiveData: %s (%d).\n", szError, iError);
			return 0;
		}
		return iResult;
	} else if (this->m_pARQStream) {
		// wait to receive
#ifdef NCSIM
		for (size_t i = 0; i < 10; i++) {
			if (m_pARQStream->received_packet_available()) {
				break;
			}
			wait(10, SC_US);
		}
		if (!m_pARQStream->received_packet_available()) {
			throw std::runtime_error("No response after simulating 100us");
		}
#else
		auto start_wait = std::chrono::steady_clock::now();
		unsigned int sleep_duration_in_us = TO_RES;
		std::chrono::duration<int, std::micro> timeout(ARQ_JTAG_TIMEOUT);
		while (!m_pARQStream->received_packet_available()) {
			if ((std::chrono::steady_clock::now() - start_wait) > timeout) {
				throw std::runtime_error(
				    "No JTAG response received after timeout of " +
				    std::to_string(timeout.count()) + " us: " + m_pARQStream->get_remote_ip());
			}


			if ((usleep(sleep_duration_in_us) != 0) && (errno != EINTR)) {
				throw std::runtime_error("usleep failed");
			}
			sleep_duration_in_us *= 2;
		}
#endif

		// receive packet
		sctrltp::packet curr_pck;
		m_pARQStream->receive(curr_pck);

		// re-order 32bit words in host byte order
		for (unsigned int nword = 1; nword < 2 * curr_pck.len; ++nword) {
			*((uint32_t*) (curr_pck.pdu) + nword) = ntohl(*((uint32_t*) (curr_pck.pdu) + nword));
		}

		// bits 32-48 contain number of TDO bits in response
		uint16_t response_bits = (curr_pck.pdu[0] >> 32) & 0xffff;

		// first 32bit contain length in 32bit words,
		// multiply by 4 to get number of bytes
		uint16_t actual_length = 4 * (*(uint32_t*) (curr_pck.pdu));

		if (curr_pck.pid == application_layer_packet_types::JTAGBULK) {
			memcpy(this->m_auiReceiveBuffer, (uint8_t*) (curr_pck.pdu) + 4, actual_length);
			// overwrite everything above response_bits + 4*8 with zeros, stop at byte actual_length
			for (size_t current_bit = 4 * 8 + response_bits; current_bit < actual_length * 8;
				 current_bit++) {
				size_t index = current_bit / 8;
				size_t pos = current_bit % 8;
				// clear current bit
				this->m_auiReceiveBuffer[index] &= ~(1 << pos);
			}
			return actual_length;
		} else {
			// at this point we expect to not receive HICANN config or spikes yet
			std::stringstream ss;
			ss << std::hex << curr_pck.pid;
			throw std::runtime_error(
			    "Received ARQ packet of non-JTAG type: " + ss.str() + ", " +
			    m_pARQStream->get_remote_ip());
		}
	} else {
		throw std::runtime_error("receiving data without open session");
	}
}

bool jtag_lib_v2::jtag_ethernet::readTDO(const uint16_t uiBitsToRead, const bool bFinal)
{
	/* nothing to do if no bits left to read */
	if (uiBitsToRead == 0)
		return true;

	/* force flushCable to get read data */
	if (!this->flushCable()) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"readTDO: Failed to send data "
			"for%sread operation.\n",
			(bFinal) ? " final " : " ");
		this->m_bRxData = false;
		return false;
	}

	if (bFinal)
		this->m_bRxData = false;

	/* calculate number of expected receive bytes */
	uint16_t uiNumBytes = uiBitsToRead / 8;
	if (uiBitsToRead & 0x7)
		uiNumBytes++;
	uint16_t uiNumBytesAligned = uiNumBytes;
	if (uiNumBytesAligned & 0x3)
		uiNumBytesAligned += (4 - (uiNumBytes & 0x3));

	/* receive ethernet frame */
	uint16_t uiResult = this->receiveData(4 + uiNumBytesAligned);
	if (!uiResult)
		return false;

	/* check number of bytes received */
	if (uiResult != (4 + uiNumBytesAligned)) {
		jtag_logger::sendMessage(MSG_ERROR, "readTDO: Received wrong length.\n");
		return false;
	}

	/* decode answer packet */
	uint16_t uiRxNum = this->m_auiReceiveBuffer[3];
	uiRxNum |= (this->m_auiReceiveBuffer[2] << 8);

	if (uiRxNum != uiBitsToRead) {
		jtag_logger::sendMessage(
			MSG_ERROR, "readTDO: Received wrong length "
					   "of TDO count.\n");
		return false;
	}

	/* copy bits to TDO buffer */
	for (uint16_t i = 0; i < uiNumBytes; ++i) {
		*(this->m_puiTDOBuffer) = m_auiReceiveBuffer[4 + i];
		this->m_puiTDOBuffer++;
	}

	return true;
}
