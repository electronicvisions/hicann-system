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
// Author            :   Stephan Hartmann
// E-Mail            :   hartmann@iee.et.tu-dresden.de
//
// Filename          :   jtag_digilent.cpp
// Project Name      :   JTAG Library V2
// Description       :   Digilent JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_digilent.hpp"
#include "jtag_logger.hpp"
#include "jtag_stdint.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable : 4800)
#endif
#endif

jtag_lib_v2::jtag_digilent::jtag_digilent()
	: m_cableHandle(0),
	  m_uiDelay(0),
	  m_bPinStateSupported(false),
	  m_bSpeedSupported(false),
	  m_uiBitcount(0)
{}

bool jtag_lib_v2::jtag_digilent::createSession(const char*, const uint16_t)
{
	/* load Digilent Manager library */
	if (!digilent_wrapper::getInstance().loadDigilentMgrLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Failed to load Digilent "
					   "Manager library.\n");
		return false;
	}

	/* load Digilent JTAG library */
	if (!digilent_wrapper::getInstance().loadDigilentJtagLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Failed to load Digilent "
					   "JTAG library.\n");
		return false;
	}

	/* mark libraries open */
	this->setSessionOpen(true);

	return true;
}

bool jtag_lib_v2::jtag_digilent::enumCables(uint8_t& uiCablesFound, const bool /*bEnumOther*/)
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

	BOOL bResult;
	int iDevicesFound;
	/* enumerate connected devices */
	bResult = digilent_wrapper::getInstance().DmgrEnumDevicesEx(
		&iDevicesFound, dtpUSB, dtpUSB, dinfoNone, 0);
	if (!bResult) {
		jtag_logger::sendMessage(
			MSG_ERROR, "enumCables: Failed to enumerate Digilent USB "
					   "cables.\n");
		return false;
	}

	/* iterate over devices */
	for (int i = 0; i < iDevicesFound; ++i) {
		DVC dvc;
		bResult = digilent_wrapper::getInstance().DmgrGetDvc(i, &dvc);
		if (!bResult)
			continue;

		/* store cable info */
		cable_info* pCable;
		try {
			pCable = new cable_info();
		} catch (std::bad_alloc) {
			digilent_wrapper::getInstance().DmgrFreeDvcEnum();
			return false;
		}

		if (!pCable->setPort(dvc.szConn)) {
			delete pCable;
			digilent_wrapper::getInstance().DmgrFreeDvcEnum();
			return false;
		}

		const char* szSerial = ::strrchr(dvc.szConn, '#');
		if (!szSerial || !(*szSerial)) {
			delete pCable;
			continue;
		}
		if (!pCable->setSerial(szSerial + 1)) {
			delete pCable;
			digilent_wrapper::getInstance().DmgrFreeDvcEnum();
			return false;
		}

		pCable->setLocked(false);
		pCable->setPortNumber(0);

		this->addCableInfo(pCable);
		uiCablesFound++;
	}

	if (!uiCablesFound)
		jtag_logger::sendMessage(MSG_WARNING, "enumCables: No cables found.\n");
	else if (uiCablesFound == 1)
		jtag_logger::sendMessage(MSG_INFO, "enumCables: Found 1 cable.\n");
	else
		jtag_logger::sendMessage(MSG_INFO, "enumCables: Found %d cables.\n", uiCablesFound);

	digilent_wrapper::getInstance().DmgrFreeDvcEnum();

	/* mark enumeration done */
	this->markEnumDone();

	return true;
}

bool jtag_lib_v2::jtag_digilent::openCable(
	const uint8_t uiCableIndex, const uint32_t uiFrequency, const bool /*bForceUnlock*/)
{
	/* get a cable to open */
	cable_info* pCableInfo;
	if (!this->getCableToOpen(uiCableIndex, &pCableInfo, false))
		return false;

	jtag_logger::sendMessage(
		MSG_INFO,
		"openCable: Opening Digilent "
		"cable 'SN:%s' ...\n",
		pCableInfo->getSerial());

	/* open the cable */
	BOOL bResult;
	bResult = digilent_wrapper::getInstance().DmgrOpen(
		&this->m_cableHandle, const_cast<char*>(pCableInfo->getPort()));
	if (!bResult) {
		jtag_logger::sendMessage(
			MSG_ERROR, "openCable: Could not open 'SN:%s'.\n", pCableInfo->getSerial());
		return false;
	}

	/* get number of ports */
	INT32 iNumPorts;
	bResult = digilent_wrapper::getInstance().DjtgGetPortCount(this->m_cableHandle, &iNumPorts);
	if (!bResult) {
		jtag_logger::sendMessage(
			MSG_ERROR, "openCable: Could not retrieve number of ports "
					   "for selected device.\n");
		digilent_wrapper::getInstance().DmgrClose(this->m_cableHandle);
		return false;
	}

	if (iNumPorts < 1) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"openCable: Obtained invalid number of ports "
			"(%d).\n",
			iNumPorts);
		digilent_wrapper::getInstance().DmgrClose(this->m_cableHandle);
		return false;
	}

	if (iNumPorts != 1)
		jtag_logger::sendMessage(
			MSG_WARNING,
			"openCable: Currently only one port per device "
			"is supported, but found %d ports.\n",
			iNumPorts);

	/* get port capabilities */
	DWORD dwPortProperties;
	bResult = digilent_wrapper::getInstance().DjtgGetPortProperties(
		this->m_cableHandle, pCableInfo->getPortNumber(), &dwPortProperties);
	if (!bResult) {
		jtag_logger::sendMessage(
			MSG_ERROR, "openCable: Failed to retrieve port "
					   "properties.\n");
		digilent_wrapper::getInstance().DmgrClose(this->m_cableHandle);
		return false;
	}

	/* check whether TCK speed setting is supported */
	this->m_bSpeedSupported = dwPortProperties & dprpJtgSetSpeed;
	if (!this->m_bSpeedSupported) {
		jtag_logger::sendMessage(
			MSG_INFO, "openCable: Selected device does not support "
					  "TCK speed setting.\n");
		jtag_logger::sendMessage(
			MSG_INFO, "openCable: This function is emulated in "
					  "software.\n");
	} else
		jtag_logger::sendMessage(
			MSG_INFO, "openCable: Selected device does support "
					  "TCK speed setting.\n");

	/* check whether pin state setting is supported */
	this->m_bPinStateSupported = dwPortProperties & dprpJtgSetPinState;
	if (!this->m_bPinStateSupported)
		jtag_logger::sendMessage(
			MSG_INFO, "openCable: Selected device does not support "
					  "pin state setting.\n");
	else
		jtag_logger::sendMessage(
			MSG_INFO, "openCable: Selected device does support "
					  "pin state setting.\n");

	/* check that pins can be set if TCK speed setting is not supported */
	if (!this->m_bPinStateSupported && !this->m_bSpeedSupported) {
		jtag_logger::sendMessage(
			MSG_ERROR, "openCable: Cable does neither support TCK "
					   "speed setting nor pin state change.\n");
		digilent_wrapper::getInstance().DmgrClose(this->m_cableHandle);
		return false;
	}

	/* enable port */
	bResult = digilent_wrapper::getInstance().DjtgEnableEx(
		this->m_cableHandle, pCableInfo->getPortNumber());
	if (!bResult) {
		jtag_logger::sendMessage(MSG_ERROR, "openCable: Could not enable port of device.");
		return false;
	}

	/* mark cable as open */
	this->setCableOpen(true);

	/* clear number of bits */
	this->m_uiBitcount = 0;

	/* set unknown jtag state */
	this->m_jtagState.setJtagState(UNKNOWN_JTAG_STATE);

	/* set TCK frequency */
	if (!this->setJtagFrequency(uiFrequency))
		return false;

	return true;
}

bool jtag_lib_v2::jtag_digilent::closeCable()
{
	if (this->isCableOpen()) {
		/* flush remaining bits in TX buffer */
		this->flushCable();

		BOOL bResult;
		/* disable the port */
		bResult = digilent_wrapper::getInstance().DjtgDisable(this->m_cableHandle);
		if (!bResult) {
			jtag_logger::sendMessage(MSG_ERROR, "closeCable: Failed to disable cable port.\n");
			return false;
		}
		jtag_logger::sendMessage(MSG_INFO, "closeCable: Successfully disabled port.\n");

		/* close the cable */
		bResult = digilent_wrapper::getInstance().DmgrClose(this->m_cableHandle);
		if (!bResult) {
			jtag_logger::sendMessage(MSG_ERROR, "closeCable: Failed to close cable.\n");
			return false;
		}
		jtag_logger::sendMessage(MSG_INFO, "closeCable: Successfully closed cable.\n");
	}

	/* mark cable as closed */
	this->setCableOpen(false);

	return false;
}

bool jtag_lib_v2::jtag_digilent::pulsePin(
	const enum jtag_lib_v2::eJtagPin ePin, const uint16_t uiCount)
{
	/* cable must be open and we cannot pulse TDO */
	if (!this->isCableOpen() || ePin == JTAG_PIN_TDO)
		return false;

	/* if no pulse, skip */
	if (!uiCount)
		return true;

	return false;
}

bool jtag_lib_v2::jtag_digilent::setPin(const enum jtag_lib_v2::eJtagPin ePin, const bool bValue)
{
	/* check whether pin state change is supported */
	if (!this->m_bPinStateSupported) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setPin: Set pin state is not supported "
					   "by this device.\n");
		return false;
	}

	/* we cannot set TDO pin */
	if (ePin == JTAG_PIN_TDO) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setPin: Cannot set %s pin.\n", this->getJtagPinName(ePin));
		return false;
	}

	/* check for a valid pin */
	if (ePin != JTAG_PIN_TDI && ePin != JTAG_PIN_TMS && ePin != JTAG_PIN_TCK) {
		jtag_logger::sendMessage(MSG_ERROR, "setPin: Unknown Jtag pin to set.\n");
		return false;
	}


	/* flush TX buffer bits */
	if (this->m_uiBitcount) {
		if (!this->flushCable())
			return false;
	}

	/* get actual pin values */
	BOOL bTms, bTdi, bTdo, bTck;
	BOOL bResult;
	bResult = digilent_wrapper::getInstance().DjtgGetTmsTdiTdoTck(
		this->m_cableHandle, &bTms, &bTdi, &bTdo, &bTck);

	if (!bResult) {
		jtag_logger::sendMessage(MSG_ERROR, "setPin: Failed to get actual pin state.\n");
		return false;
	}

	/* change pin state as requested */
	if (ePin == JTAG_PIN_TCK)
		bTck = bValue;
	else if (ePin == JTAG_PIN_TMS)
		bTms = bValue;
	else if (ePin == JTAG_PIN_TDI)
		bTdi = bValue;

	/* update pin state */
	bResult =
		digilent_wrapper::getInstance().DjtgSetTmsTdiTck(this->m_cableHandle, bTms, bTdi, bTck);
	if (!bResult) {
		jtag_logger::sendMessage(MSG_ERROR, "setPin: Failed to update pin state.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_digilent::getPin(const enum jtag_lib_v2::eJtagPin ePin, bool& bValue)
{
	/* check for a valid pin */
	if (ePin != JTAG_PIN_TDI && ePin != JTAG_PIN_TMS && ePin != JTAG_PIN_TCK &&
		ePin != JTAG_PIN_TDO) {
		jtag_logger::sendMessage(MSG_ERROR, "getPin: Unknown Jtag pin to read.\n");
		return false;
	}

	/* get actual pin values */
	BOOL bTms, bTdi, bTdo, bTck;
	BOOL bResult;
	bResult = digilent_wrapper::getInstance().DjtgGetTmsTdiTdoTck(
		this->m_cableHandle, &bTms, &bTdi, &bTdo, &bTck);

	if (!bResult) {
		jtag_logger::sendMessage(MSG_ERROR, "getPin: Failed to get actual pin state.\n");
		return false;
	}

	/* return requested pin state */
	if (ePin == JTAG_PIN_TCK)
		bValue = bTck;
	else if (ePin == JTAG_PIN_TMS)
		bValue = bTms;
	else if (ePin == JTAG_PIN_TDI)
		bValue = bTdi;
	else if (ePin == JTAG_PIN_TDO)
		bValue = bTdo;

	return true;
}

bool jtag_lib_v2::jtag_digilent::shiftDeviceDR(
	const uint8_t /*uiDevice*/,
	uint8_t* /*puiTDIBuffer*/,
	uint8_t* /*puiTDOBuffer*/,
	uint32_t /*uiBitCount*/,
	const enum eScanMode /*eMode*/)
{
	/* unsupported, must be handled by higher level API */
	return false;
}

bool jtag_lib_v2::jtag_digilent::shiftDeviceIR(
	const uint8_t /*uiDevice*/, uint8_t* /*puiTDIBuffer*/, uint32_t /*uiBitCount*/)
{
	/* unsupported, must be handled by higher level API */
	return false;
}

bool jtag_lib_v2::jtag_digilent::shiftChainIR(uint8_t* puiTDIBuffer, uint32_t uiBitCount)
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
	uint8_t uiTms = 0;
	uint8_t uiBit = (this->m_uiBitcount & 0x3) << 1;
	uint8_t uiByte = this->m_uiBitcount >> 2;
	for (uint32_t uiBits = 0; uiBits < uiBitCount; ++uiBits) {
		/* get TDI pattern for next 8 bits */
		if (!(uiBits & 0x7)) {
			uiTDIPattern = *puiTDIBuffer;
			puiTDIBuffer++;
		}
		/* move to EXIT1-IR on last shift */
		if (uiBits == (uiBitCount - 1))
			uiTms = 2;
		/* copy bits to TX buffer */
		const uint8_t uiData = (uiTDIPattern & 0x1) | uiTms;
		this->m_auiTxBuffer[uiByte] |= uiData << uiBit;
		this->m_uiBitcount++;
		uiBit += 2;
		if (uiBit == 8) {
			uiBit = 0;
			uiByte++;
			if (this->m_uiBitcount == 1024) {
				if (!this->flushCable())
					return false;
				uiByte = 0;
			}
			this->m_auiTxBuffer[uiByte] = 0;
		}

		uiTDIPattern >>= 1;
	}

	this->m_jtagState.setJtagState(EXIT1_IR);

	/* move to RUN-TEST-IDLE */
	return this->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_digilent::shiftChainDR(
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

	/* flush TX buffer in case of read */
	if (eMode != SCAN_MODE_WRITE) {
		if (!this->flushCable())
			return false;
	}

	/* shift out TDI bits and capture TDO bits */
	uint8_t uiTDIPattern = 0;
	uint32_t uiTDOPos = 0;
	uint8_t uiTms = 0;
	uint8_t uiTdi = 0;
	uint8_t uiBit = (this->m_uiBitcount & 0x3) << 1;
	uint8_t uiByte = this->m_uiBitcount >> 2;
	for (uint32_t uiBits = 0; uiBits < uiBitCount; ++uiBits) {
		/* get TDI pattern for next 8 bits */
		if (!(uiBits & 0x7) && eMode != SCAN_MODE_READ) {
			uiTDIPattern = *puiTDIBuffer;
			puiTDIBuffer++;
		}
		/* move to EXIT1-DR on last shift */
		if (uiBits == (uiBitCount - 1))
			uiTms = 2;

		if (eMode != SCAN_MODE_READ) {
			/* set TDI bit */
			uiTdi = uiTDIPattern & 0x1;
			uiTDIPattern >>= 1;
		}

		/* copy bits to TX buffer */
		const uint8_t uiData = uiTdi | uiTms;
		this->m_auiTxBuffer[uiByte] |= uiData << uiBit;
		this->m_uiBitcount++;
		uiBit += 2;
		if (uiBit == 8) {
			uiBit = 0;
			uiByte++;
			if (this->m_uiBitcount == 1024) {
				if (!this->flushCable(eMode != SCAN_MODE_WRITE))
					return false;
				uiByte = 0;

				/* copy TDO bits */
				if (eMode != SCAN_MODE_WRITE)
					::memcpy(&(puiTDOBuffer[uiTDOPos]), this->m_auiRxBuffer, 128);
				uiTDOPos += 128;
			}
			this->m_auiTxBuffer[uiByte] = 0;
		}
	}

	/* shift out remaining bits */
	if (this->m_uiBitcount && eMode != SCAN_MODE_WRITE) {
		const uint8_t uiBytes = (this->m_uiBitcount + 7) >> 3;
		if (!this->flushCable(true))
			return false;
		/* copy remaining TDO bits */
		if (eMode != SCAN_MODE_WRITE)
			::memcpy(&(puiTDOBuffer[uiTDOPos]), this->m_auiRxBuffer, uiBytes);
	}

	this->m_jtagState.setJtagState(EXIT1_DR);

	/* move to RUN-TEST-IDLE */
	return this->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_digilent::flushCable()
{
	return this->flushCable(false);
}

bool jtag_lib_v2::jtag_digilent::flushCable(const bool bReadTDO)
{
	/* open cable is required */
	if (!this->isCableOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "flushCable: No Jtag cable open.\n");
		return false;
	}

	/* do nothing if no bits are in TX buffer */
	if (!this->m_uiBitcount)
		return true;

	/* shift out TX buffer and optionally fill RX buffer */
	uint8_t* puiRxBuffer = 0;
	if (bReadTDO)
		puiRxBuffer = this->m_auiRxBuffer;
	BOOL bResult;
	bResult = digilent_wrapper::getInstance().DjtgPutTmsTdiBits(
		this->m_cableHandle, this->m_auiTxBuffer, puiRxBuffer, this->m_uiBitcount, 0);
	if (!bResult) {
		jtag_logger::sendMessage(MSG_ERROR, "flushCable: Failed to shift out TX buffer.\n");
		return false;
	}

	/* initialize first byte */
	this->m_auiTxBuffer[0] = 0;

	/* clear bitcount */
	this->m_uiBitcount = 0;

	return true;
}

bool jtag_lib_v2::jtag_digilent::navigate(const enum eJtagState eState, const uint16_t uiClocks)
{
	/* move to requested state */
	uint16_t uiTmsPattern;
	uint8_t uiTmsBits;

	if (!this->m_jtagState.getTmsPatternForState(eState, uiTmsPattern, uiTmsBits)) {
		jtag_logger::sendMessage(MSG_ERROR, "navigate: Unknown JTAG state requested.\n");
		return false;
	}

	/* copy TMS bits to TX buffer */
	uint8_t uiBit = ((this->m_uiBitcount & 0x3) << 1) + 1;
	uint8_t uiByte = this->m_uiBitcount >> 2;
	for (uint8_t uiBits = 0; uiBits < uiTmsBits; ++uiBits) {
		this->m_auiTxBuffer[uiByte] |= (uiTmsPattern & 0x1) << uiBit;
		this->m_uiBitcount++;
		uiBit += 2;
		if (uiBit > 8) {
			uiBit = 1;
			uiByte++;
			if (this->m_uiBitcount == 1024) {
				if (!this->flushCable())
					return false;
				uiByte = 0;
			}
			this->m_auiTxBuffer[uiByte] = 0;
		}
		uiTmsPattern >>= 1;
	}

	/* perform additional clocks in final state */
	this->m_jtagState.getTmsPatternForState(eState, uiTmsPattern, uiTmsBits);

	if (!uiClocks || uiTmsBits > 1)
		return true;

	/* copy last TMS bit for requested clock cycles to TX buffer */
	for (uint8_t uiClockCnt = 0; uiClockCnt < uiClocks; ++uiClockCnt) {
		this->m_auiTxBuffer[uiByte] |= (uiTmsPattern & 0x1) << uiBit;
		this->m_uiBitcount++;
		uiBit += 2;
		if (uiBit > 8) {
			uiBit = 1;
			uiByte++;
			if (this->m_uiBitcount == 1024) {
				if (!this->flushCable())
					return false;
				uiByte = 0;
			}
			this->m_auiTxBuffer[uiByte] = 0;
		}
	}

	return true;
}

bool jtag_lib_v2::jtag_digilent::setJtagFrequency(const uint32_t uiFrequency)
{
	/* we need an open cable */
	if (!this->isCableOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "setJtagFrequency: No Jtag cable open.\n");
		return false;
	}

	if (!this->m_bSpeedSupported)

	{
		/* frequency value is just a delay value in us */
		if (uiFrequency > 500 || !uiFrequency)
			this->m_uiDelay = 0;
		else
			this->m_uiDelay = (500 + uiFrequency - 1) / uiFrequency;

		return true;
	}

	/* limit frequency to 100MHz */
	uint32_t uiFreq = uiFrequency;
	if (uiFreq > 1000000)
		uiFreq = 1000000;

	/* set the frequency */
	BOOL bResult;
	bResult = digilent_wrapper::getInstance().DjtgSetSpeed(
		this->m_cableHandle, uiFreq * 1000, reinterpret_cast<DWORD*>(&uiFreq));
	if (!bResult) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setJtagFrequency: Failed to set "
					   "Jtag TCK frequency.\n");
		return false;
	}

	/* create readable frequency */
	uint32_t uiFreqF = 0;
	char szUnit[4] = "Hz";
	if (uiFreq > 1000) {
		uiFreqF = uiFreq % 1000;
		uiFreq = uiFreq / 1000;
		strcpy(szUnit, "kHz");
	}
	if (uiFreq > 1000) {
		uiFreqF = uiFreq % 1000;
		uiFreq = uiFreq / 1000;
		strcpy(szUnit, "MHz");
	}

	jtag_logger::sendMessage(
		MSG_INFO,
		"setJtagFrequency: TCK frequency set "
		"to %d.%03d %s.\n",
		uiFreq, uiFreqF, szUnit);

	return true;
}

jtag_lib_v2::jtag_digilent::~jtag_digilent()
{
	/* close cable */
	this->closeCable();
}

HMODULE jtag_lib_v2::digilent_wrapper::loadDigilentLib(const TCHAR* szLibName)
{
	/* load the library */
	HMODULE hModule = ::loadLibrary(szLibName);
	if (!hModule) {
		jtag_logger::sendMessage(
			MSG_DEBUG,
			"loadDigilentLib: Failed to open "
			"Digilent library '%s'.\n",
			szLibName);
		jtag_logger::sendMessage(MSG_DEBUG, "%s\n", ::getErrorMessage());
		return 0;
	}
	return hModule;
}

bool jtag_lib_v2::digilent_wrapper::loadDigilentJtagLib()
{
	/* load Digilent Jtag library */
	if (this->m_hDjtgModule)
		return true;

#ifdef _WIN32
	this->m_hDjtgModule = this->loadDigilentLib(TEXT("djtg.") DLL_EXT);
#else
	this->m_hDjtgModule = this->loadDigilentLib("libdjtg." DLL_EXT ".2");
#endif

	if (!this->m_hDjtgModule)
		return false;

	/* load Jtag functions */
	this->DjtgGetVersion =
		(DjtgGetVersion_t)::getProcFunction(this->m_hDjtgModule, "DjtgGetVersion");
	this->DjtgGetPortCount =
		(DjtgGetPortCount_t)::getProcFunction(this->m_hDjtgModule, "DjtgGetPortCount");
	this->DjtgGetPortProperties =
		(DjtgGetPortProperties_t)::getProcFunction(this->m_hDjtgModule, "DjtgGetPortProperties");
	this->DjtgEnableEx = (DjtgEnableEx_t)::getProcFunction(this->m_hDjtgModule, "DjtgEnableEx");
	this->DjtgDisable = (DjtgDisable_t)::getProcFunction(this->m_hDjtgModule, "DjtgDisable");
	this->DjtgSetSpeed = (DjtgSetSpeed_t)::getProcFunction(this->m_hDjtgModule, "DjtgSetSpeed");
	this->DjtgSetTmsTdiTck =
		(DjtgSetTmsTdiTck_t)::getProcFunction(this->m_hDjtgModule, "DjtgSetTmsTdiTck");
	this->DjtgGetTmsTdiTdoTck =
		(DjtgGetTmsTdiTdoTck_t)::getProcFunction(this->m_hDjtgModule, "DjtgGetTmsTdiTdoTck");
	this->DjtgPutTmsTdiBits =
		(DjtgPutTmsTdiBits_t)::getProcFunction(this->m_hDjtgModule, "DjtgPutTmsTdiBits");

	const bool bValid = this->DjtgGetVersion && this->DjtgEnableEx && this->DjtgDisable &&
						this->DjtgSetSpeed && this->DjtgSetTmsTdiTck && this->DjtgGetTmsTdiTdoTck &&
						this->DjtgPutTmsTdiBits && this->DjtgGetPortCount &&
						this->DjtgGetPortProperties;
	if (!bValid) {
		::freeLibrary(this->m_hDjtgModule);
		this->m_hDjtgModule = 0;
		return false;
	}

	char szVersion[cchVersionMax];
	::memset(szVersion, 0, sizeof(char) * cchVersionMax);
	if (this->DjtgGetVersion(szVersion)) {
		if (szVersion[0]) {
			jtag_logger::sendMessage(MSG_INFO, "Digilent JTAG API: %s\n", szVersion);
		}
	}

	return true;
}

bool jtag_lib_v2::digilent_wrapper::loadDigilentMgrLib()
{
	/* load Digilent Device Manager library */
	if (this->m_hDmgrModule)
		return true;

#ifdef _WIN32
	this->m_hDmgrModule = this->loadDigilentLib(TEXT("dmgr.") DLL_EXT);
#else
	this->m_hDmgrModule = this->loadDigilentLib("libdmgr." DLL_EXT ".2");
#endif

	if (!this->m_hDmgrModule)
		return false;

	/* load management functions */
	this->DmgrGetVersion =
		(DmgrGetVersion_t)::getProcFunction(this->m_hDmgrModule, "DmgrGetVersion");
	this->DmgrSzFromErc = (DmgrSzFromErc_t)::getProcFunction(this->m_hDmgrModule, "DmgrSzFromErc");
	this->DmgrOpen = (DmgrOpen_t)::getProcFunction(this->m_hDmgrModule, "DmgrOpen");
	this->DmgrClose = (DmgrClose_t)::getProcFunction(this->m_hDmgrModule, "DmgrClose");
	this->DmgrEnumDevicesEx =
		(DmgrEnumDevicesEx_t)::getProcFunction(this->m_hDmgrModule, "DmgrEnumDevicesEx");
	this->DmgrGetDvc = (DmgrGetDvc_t)::getProcFunction(this->m_hDmgrModule, "DmgrGetDvc");
	this->DmgrFreeDvcEnum =
		(DmgrFreeDvcEnum_t)::getProcFunction(this->m_hDmgrModule, "DmgrFreeDvcEnum");

	const bool bValid = this->DmgrGetVersion && this->DmgrSzFromErc && this->DmgrOpen &&
						this->DmgrClose && this->DmgrEnumDevicesEx && this->DmgrGetDvc &&
						this->DmgrFreeDvcEnum;

	if (!bValid) {
		::freeLibrary(this->m_hDmgrModule);
		this->m_hDmgrModule = 0;
		return false;
	}

	char szVersion[cchVersionMax];
	if (this->DmgrGetVersion(szVersion))
		jtag_logger::sendMessage(MSG_INFO, "Digilent Manager API: %s\n", szVersion);

	return true;
}

jtag_lib_v2::digilent_wrapper::digilent_wrapper()
	: m_hDmgrModule(0),
	  m_hDjtgModule(0),
	  DmgrGetVersion(0),
	  DmgrSzFromErc(0),
	  DmgrOpen(0),
	  DmgrClose(0),
	  DmgrEnumDevicesEx(0),
	  DmgrGetDvc(0),
	  DmgrFreeDvcEnum(0),
	  DjtgGetVersion(0),
	  DjtgGetPortCount(0),
	  DjtgGetPortProperties(0),
	  DjtgEnableEx(0),
	  DjtgDisable(0),
	  DjtgSetSpeed(0),
	  DjtgSetTmsTdiTck(0),
	  DjtgGetTmsTdiTdoTck(0),
	  DjtgPutTmsTdiBits(0)
{}

jtag_lib_v2::digilent_wrapper& jtag_lib_v2::digilent_wrapper::getInstance()
{
	static digilent_wrapper myDigilentWrapper;
	return myDigilentWrapper;
}

jtag_lib_v2::digilent_wrapper::~digilent_wrapper()
{
	if (this->m_hDjtgModule)
		::freeLibrary(this->m_hDjtgModule);

	if (this->m_hDmgrModule)
		::freeLibrary(this->m_hDmgrModule);

	this->DmgrGetVersion = 0;
	this->DmgrSzFromErc = 0;
	this->DmgrOpen = 0;
	this->DmgrClose = 0;
	this->DmgrEnumDevicesEx = 0;
	this->DmgrGetDvc = 0;
	this->DmgrFreeDvcEnum = 0;

	this->DjtgGetVersion = 0;
	this->DjtgGetPortCount = 0;
	this->DjtgGetPortProperties = 0;
	this->DjtgEnableEx = 0;
	this->DjtgDisable = 0;
	this->DjtgSetSpeed = 0;
	this->DjtgSetTmsTdiTck = 0;
	this->DjtgGetTmsTdiTdoTck = 0;
	this->DjtgPutTmsTdiBits = 0;

	this->m_hDjtgModule = 0;
	this->m_hDmgrModule = 0;
}
