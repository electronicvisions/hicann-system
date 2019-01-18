//-----------------------------------------------------------------
//
// Copyright (c) 2011 Xilinx, Inc.  All rights reserved.
// Copyright (c) 2012 TU-Dresden  All rights reserved.
//
//                 XILINX CONFIDENTIAL PROPERTY
// This   document  contains  proprietary information  which   is
// protected by  copyright. All rights  are reserved. No  part of
// this  document may be photocopied, reproduced or translated to
// another  program  language  without  prior written  consent of
// XILINX Inc., San Jose, CA. 95124
//
//-----------------------------------------------------------------

//-----------------------------------------------------------------
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
// Filename          :   jtag_cse.cpp
// Project Name      :   JTAG Library V2
// Description       :   Xilinx CSE JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_cse.hpp"
#include "jtag_logger.hpp"
#include "jtag_stdint.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>

#define CSE_WAIT_MS (5000)

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#define MAX_MAJOR_VERSION (14)
#define MAX_MINOR_VERSION (07)
#define MIN_MAJOR_VERSION (12)

#define MAX_DIGILENT_SERIAL_LEN 64

#ifdef _WIN32
#define snprintf _snprintf
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#if __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

bool jtag_lib_v2::jtag_cse::m_bDisableOutput = false;

void CALLBACK
jtag_lib_v2::jtag_cse::writeMessage(CSE_HANDLE /*cseHandle*/, uint32_t uiMsgFlags, const char* szMsg)
{
	if (jtag_cse::m_bDisableOutput)
		return;

	enum jtag_lib_v2::eMsgType eType = jtag_lib_v2::MSG_NONE;
	switch (uiMsgFlags) {
		case CSE_MSG_ERROR:
			eType = jtag_lib_v2::MSG_ERROR;
			break;
		case CSE_MSG_WARNING:
			eType = jtag_lib_v2::MSG_WARNING;
			break;
		case CSE_MSG_INFO:
			eType = jtag_lib_v2::MSG_INFO;
			break;
		case CSE_MSG_DEBUG:
			eType = jtag_lib_v2::MSG_DEBUG;
	}

	jtag_lib_v2::jtag_logger::sendMessage(eType, szMsg);
}

jtag_lib_v2::jtag_cse::jtag_cse(const enum eXilinxTarget eJtagTarget)
	: m_bCableLocked(false), m_eJtagTarget(eJtagTarget)
{}

bool jtag_lib_v2::jtag_cse::checkCableLock(const char* szFunc)
{
	if (!this->m_bCableLocked) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"%s: A cable lock is required to "
			"perform the requested operation.\n",
			szFunc);
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_cse::createSession(const char* szHost, const uint16_t uiPort)
{
	const char* aszArgs[2];
	char szServer[HOST_NAME_MAX + 10];
	char szPort[12];

	if (!szHost) {
		if (!uiPort)
			return this->createSession((const char**) 0, 0);
		return false;
	}

	if (!uiPort) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Port 0 is not allowed "
					   "for remote connection.\n");
		return false;
	}

	if (snprintf(szServer, HOST_NAME_MAX + 10, "-server %s", szHost) > HOST_NAME_MAX + 10) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"createSession: Host name is too long. "
			"Maximum allowed length is %d.\n",
			HOST_NAME_MAX);
		return false;
	}
	sprintf(szPort, "-port %u", uiPort);

	aszArgs[0] = szServer;
	aszArgs[1] = szPort;

	return this->createSession((const char**) aszArgs, 2);
}

bool jtag_lib_v2::jtag_cse::createSession(const char** pszArgs, const int iArgc)
{
	/* check whether session is already open */
	if (this->isSessionOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "createSession: Session is already open.\n");
		return false;
	}

	/* load CSE Jtag library */
	if (!cse_wrapper::getInstance().loadCseJtagLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Failed to load Xilinx JTAG "
					   "access library.\n");
		return false;
	}

	/* create Jtag session */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagSessionCreate(
		jtag_cse::writeMessage, iArgc, pszArgs, &this->m_cseHandle);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "createSession: Failed to create Xilinx JTAG "
					   "session.\n");
		return false;
	}

	/* mark session as open */
	this->setSessionOpen(true);

	return true;
}

bool jtag_lib_v2::jtag_cse::closeSession()
{
	if (this->isSessionOpen()) {
		/* close cables */
		if (this->isCableOpen())
			this->closeCable();

		/* destroy session if active */
		cse_wrapper::getInstance().jtagSessionDestroy(this->m_cseHandle);

		/* mark session as closed */
		this->setSessionOpen(false);
	}

	/* free Jtag library */
	if (!cse_wrapper::getInstance().freeCseJtagLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "closeSession: Failed to release Xilinx JTAG "
					   "access library.\n");
		return false;
	}

	/* free Fpga library */
	if (!cse_wrapper::getInstance().freeCseFpgaLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "closeSession: Failed to release Xilinx FPGA "
					   "access library.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::enumCables(uint8_t& uiCablesFound, const bool bEnumOther)
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

	/* free previous cable list */
	this->freeCableList();

	jtag_logger::sendMessage(MSG_INFO, "enumCables: Enumerating connected devices ...\n");

	enum eXilinxTarget eJtagTarget = this->m_eJtagTarget;
	for (int iTargets = XILINX_JTAG_USB; iTargets <= ((bEnumOther) ? XILINX_JTAG_DIGILENT : 0);
		 ++iTargets) {
		/* switch to next target if first is done */
		if (iTargets > XILINX_JTAG_USB) {
			eJtagTarget = static_cast<enum eXilinxTarget>(static_cast<int>(eJtagTarget) + 1);
			eJtagTarget = static_cast<enum eXilinxTarget>(eJtagTarget % (XILINX_JTAG_DIGILENT + 1));
		}

		/* set number of supported targets */
		const uint8_t uiMaxTargets =
			(eJtagTarget == XILINX_JTAG_LPT) ? 3 : ((eJtagTarget == XILINX_JTAG_DIGILENT) ? 1 : 10);

		/* select cable driver */
		const char* szCableDriver;
		if (eJtagTarget == XILINX_JTAG_LPT)
			szCableDriver = CSEJTAG_TARGET_PARALLEL;
		else if (eJtagTarget == XILINX_JTAG_DIGILENT)
			szCableDriver = CSEJTAG_TARGET_DIGILENT;
		else
			szCableDriver = CSEJTAG_TARGET_PLATFORMUSB;

		/* calculate default frequency */
		const enum eXilinxTarget eJtagTargetSave = this->m_eJtagTarget;
		this->m_eJtagTarget = eJtagTarget;
		char szFreq[20];
		sprintf(szFreq, "frequency=%d", this->getFrequency(0) * 1000);
		this->m_eJtagTarget = eJtagTargetSave;

		char* aszArgs[2];
		char szPort[17];
		aszArgs[0] = szPort;
		aszArgs[1] = szFreq;
		for (uint8_t i = 1; i <= uiMaxTargets; ++i) {
			/* set USB/parallel port */
			if (eJtagTarget == XILINX_JTAG_LPT)
				sprintf(szPort, "port=LPT%d", i);
			else if (eJtagTarget == XILINX_JTAG_DIGILENT)
				sprintf(szPort, "device=");
			else
				sprintf(szPort, "port=USB2%d", i);

			/* open the cable */
			CSE_RETURN cseStatus;
			jtag_cse::m_bDisableOutput = true;
			cseStatus = cse_wrapper::getInstance().jtagTargetOpen(
				this->m_cseHandle, szCableDriver, 0, 2, (const char**) aszArgs, 0);
			if (IS_CSE_FAILURE(cseStatus))
				continue;

			/* get lock status */
			enum CSEJTAG_TARGET_LOCK_STATUS lockStatus;
			if (eJtagTarget != XILINX_JTAG_DIGILENT) {
				cseStatus =
					cse_wrapper::getInstance().jtagTargetLock(this->m_cseHandle, 500, &lockStatus);
				if (IS_CSE_FAILURE(cseStatus)) {
					cse_wrapper::getInstance().jtagTargetClose(this->m_cseHandle);
					break;
				}
				if (lockStatus == CSEJTAG_LOCKED_ME) {
					cse_wrapper::getInstance().jtagTargetUnlock(this->m_cseHandle);
					lockStatus = CSEJTAG_UNLOCKED;
				}
			} else
				lockStatus = CSEJTAG_UNLOCKED;

			/* get ESN */
			char* szESN = 0;
			if (eJtagTarget != XILINX_JTAG_LPT) {
				const char* szKey;
				if (eJtagTarget == XILINX_JTAG_DIGILENT)
					szKey = "port_esn_list";
				else
					szKey = "electronic_sn";
				try {
					szESN = new char[1024];
					if (szESN) {
						unsigned int uiLen = 128;
						cseStatus = cse_wrapper::getInstance().jtagTargetGetOption(
							this->m_cseHandle, szKey, szESN, &uiLen);

						if (IS_CSE_FAILURE(cseStatus)) {
							delete[] szESN;
							szESN = 0;
						}
					}
				} catch (std::bad_alloc&) {
				}

				jtag_cse::m_bDisableOutput = false;
			}

			cse_wrapper::getInstance().jtagTargetClose(this->m_cseHandle);

			/* split serial numbers */
			uint8_t uiPorts = 1;
			char* aszSerials[256];
			aszSerials[0] = szESN;
			if (eJtagTarget == XILINX_JTAG_DIGILENT && szESN) {
				char* szPorts;
				for (szPorts = strchr(szESN, ','); szPorts; szPorts = strchr(szPorts + 1, ',')) {
					if (uiPorts == 255) {
						jtag_cse::m_bDisableOutput = false;
						jtag_logger::sendMessage(
							MSG_ERROR, "enumCables: More than "
									   "256 Digilent USB "
									   "cables found.\n");
						delete[] szESN;
						return false;

					} else {
						aszSerials[uiPorts] = szPorts + 1;
						uiPorts++;
					}
					szPorts[0] = '\0';
				}
			}

			/* record cable info in list */
			for (uint16_t j = 0; j < uiPorts; ++j) {
				cable_info* pCable = 0;
				try {
					pCable = new cable_info();
				} catch (std::bad_alloc&) {
					jtag_cse::m_bDisableOutput = false;
					return false;
				}

				if (aszSerials[j]) {
					char* szSerial = aszSerials[j];
					if (eJtagTarget == XILINX_JTAG_DIGILENT) {
						if (strlen(aszSerials[j]) > MAX_DIGILENT_SERIAL_LEN) {
							jtag_cse::m_bDisableOutput = false;
							jtag_logger::sendMessage(
								MSG_WARNING, "enumCables: Invalid "
											 "serial number for "
											 "Digilent USB cable.\n");
							jtag_cse::m_bDisableOutput = true;
							szSerial = 0;
						} else {
							char* szEqual = strchr(szSerial, '=');
							if (szEqual)
								szSerial = szEqual + 1;
							char szDigilentSerial[MAX_DIGILENT_SERIAL_LEN + 8];
							strcpy(szDigilentSerial, szPort);
							strcat(szDigilentSerial, szSerial);
							szSerial = szDigilentSerial;
						}
					}

					if (szSerial) {
						if (!pCable->setSerial(szSerial)) {
							delete[] szESN;
							delete pCable;
							jtag_cse::m_bDisableOutput = false;
							return false;
						}
					}
				}
				if (!pCable->setPlugin(szCableDriver)) {
					if (szESN)
						delete[] szESN;
					delete pCable;
					jtag_cse::m_bDisableOutput = false;
					return false;
				}
				if (!pCable->setPort(szPort)) {
					if (szESN)
						delete[] szESN;
					delete pCable;
					jtag_cse::m_bDisableOutput = false;
					return false;
				}
				pCable->setLocked(lockStatus != CSEJTAG_UNLOCKED);
				pCable->setPortNumber(1);

				this->addCableInfo(pCable);

				uiCablesFound++;
			}
			if (szESN)
				delete[] szESN;
		}
	}

	jtag_cse::m_bDisableOutput = false;

	if (!uiCablesFound)
		jtag_logger::sendMessage(MSG_WARNING, "enumCables: No cables found.\n");
	else if (uiCablesFound == 1)
		jtag_logger::sendMessage(MSG_INFO, "enumCables: Found 1 cable.\n");
	else
		jtag_logger::sendMessage(MSG_INFO, "enumCables: Found %d cables.\n", uiCablesFound);

	this->markEnumDone();

	return true;
}

bool jtag_lib_v2::jtag_cse::openCable(
	const uint8_t uiCableIndex, const uint32_t uiFrequency, const bool bForceUnlock)
{
	/* get a cable to open */
	cable_info* pCableInfo;
	if (!this->getCableToOpen(uiCableIndex, &pCableInfo, bForceUnlock))
		return false;

	/* update target */
	if (!strcmp(pCableInfo->getPlugin(), CSEJTAG_TARGET_PARALLEL))
		this->m_eJtagTarget = XILINX_JTAG_LPT;
	else if (!strcmp(pCableInfo->getPlugin(), CSEJTAG_TARGET_DIGILENT))
		this->m_eJtagTarget = XILINX_JTAG_DIGILENT;
	else
		this->m_eJtagTarget = XILINX_JTAG_USB;

	/* select frequency */
	uint32_t uiFreq = this->getFrequency(uiFrequency);

	/* use lowest jtag frequency if nothing matches */
	if (!uiFreq)
		uiFreq = this->getFrequency(0);

	/* convert frequency to string */
	char szFreq[20];
	sprintf(szFreq, "frequency=%d", uiFreq * 1000);

	const char* aszArgs[2];
	unsigned int uiArgCount = 2;
	aszArgs[0] = pCableInfo->getPort();
	aszArgs[1] = szFreq;

	if (this->m_eJtagTarget == XILINX_JTAG_DIGILENT) {
		if (!uiCableIndex) {
			aszArgs[0] = szFreq;
			uiArgCount = 1;
		} else
			aszArgs[0] = pCableInfo->getSerial();
	}

	/* select cable driver */
	const char* szCableDriver;
	if (this->m_eJtagTarget == XILINX_JTAG_LPT)
		szCableDriver = CSEJTAG_TARGET_PARALLEL;
	else if (this->m_eJtagTarget == XILINX_JTAG_DIGILENT)
		szCableDriver = CSEJTAG_TARGET_DIGILENT;
	else
		szCableDriver = CSEJTAG_TARGET_PLATFORMUSB;

	/* open the cable */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagTargetOpen(
		this->m_cseHandle, szCableDriver, 0, uiArgCount, (const char**) aszArgs, 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "openCable: Failed to open %s cable.\n",
			(this->m_eJtagTarget == XILINX_JTAG_LPT)
				? "parallel"
				: (this->m_eJtagTarget == XILINX_JTAG_DIGILENT) ? "Digilent" : "Platform USB");
		return false;
	}

	/* mark cable as open */
	this->setCableOpen(true);

	/* lock cable */
	enum CSEJTAG_TARGET_LOCK_STATUS cseCableLock;
	cseStatus =
		cse_wrapper::getInstance().jtagTargetLock(this->m_cseHandle, CSE_WAIT_MS, &cseCableLock);
	if (IS_CSE_FAILURE(cseStatus) || cseCableLock != CSEJTAG_LOCKED_ME) {
		jtag_logger::sendMessage(MSG_ERROR, "openCable: Failed to obtain cable lock.\n");
		this->closeCable();
		return false;
	}

	/* mark cable as locked */
	this->m_bCableLocked = true;

	return true;
}

bool jtag_lib_v2::jtag_cse::closeCable()
{
	if (this->isCableOpen()) {
		if (this->m_bCableLocked) {
			/* unlock cable */
			CSE_RETURN cseStatus;
			cseStatus = cse_wrapper::getInstance().jtagTargetUnlock(this->m_cseHandle);
			if (IS_CSE_FAILURE(cseStatus)) {
				jtag_logger::sendMessage(MSG_ERROR, "closeCable: Failed to unlock cable.\n");
				return false;
			}
			jtag_logger::sendMessage(MSG_INFO, "closeCable: Successfully unlocked cable.\n");
			/* mark cable as unlocked */
			this->m_bCableLocked = false;
		}

		/* close target */
		CSE_RETURN cseStatus;
		cseStatus = cse_wrapper::getInstance().jtagTargetClose(this->m_cseHandle);
		if (IS_CSE_FAILURE(cseStatus)) {
			jtag_logger::sendMessage(MSG_ERROR, "closeCable: Failed to close cable.\n");
			return false;
		}
		jtag_logger::sendMessage(MSG_INFO, "closeCable: Successfully closed cable.\n");
	}

	/* mark cable as closed */
	this->setCableOpen(false);

	return true;
}

bool jtag_lib_v2::jtag_cse::pulsePin(const enum jtag_lib_v2::eJtagPin ePin, const uint16_t uiCount)
{
	/* if no pulse, skip */
	if (!uiCount)
		return true;

	/* cable must be locked */
	if (!this->checkCableLock("pulsePin"))
		return false;

	/* cannot pulse TDO */
	if (ePin == JTAG_PIN_TDO) {
		jtag_logger::sendMessage(MSG_ERROR, "pulsePin: Cannot pulse TDO pin.\n");
		return false;
	}

	/* convert into CSE Jtag API pin */
	enum CSEJTAG_PIN eCsePin;
	if (!this->getCsePin(ePin, eCsePin)) {
		jtag_logger::sendMessage(MSG_ERROR, "pulsePin: Unknown JTAG pin to pulse.\n");
		return false;
	}

	/* pulse the pin */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagTargetPulsePin(this->m_cseHandle, eCsePin, uiCount);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "pulsePin: Failed to pulse %s pin.\n", this->getJtagPinName(ePin));
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_cse::setPin(const enum jtag_lib_v2::eJtagPin ePin, const bool bValue)
{
	/* cable must be locked */
	if (!this->checkCableLock("setPin"))
		return false;

	/* cannot set TCK or TDO pins */
	if (ePin == JTAG_PIN_TCK || ePin == JTAG_PIN_TDO) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setPin: Cannot set %s pin.\n", this->getJtagPinName(ePin));
		return false;
	}

	/* convert into CSE Jtag API pin */
	enum CSEJTAG_PIN eCsePin;
	if (!this->getCsePin(ePin, eCsePin)) {
		jtag_logger::sendMessage(MSG_ERROR, "setPin: Unknown JTAG pin to set.\n");
		return false;
	}

	/* set the pin */
	CSE_RETURN cseStatus;
	cseStatus =
		cse_wrapper::getInstance().jtagTargetSetPin(this->m_cseHandle, eCsePin, (bValue) ? 1 : 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setPin: Failed to pulse %s pin.\n", getJtagPinName(ePin));
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_cse::getPin(const enum jtag_lib_v2::eJtagPin ePin, bool& bValue)
{
	/* cable must be locked */
	if (!this->checkCableLock("getPin"))
		return false;

	/* convert into CSE Jtag API pin */
	enum CSEJTAG_PIN eCsePin;
	if (!this->getCsePin(ePin, eCsePin)) {
		jtag_logger::sendMessage(MSG_ERROR, "getPin: Unknown JTAG pin to get.\n");
		return false;
	}

	/* get the pin */
	CSE_RETURN cseStatus;
	unsigned int uiValue;
	cseStatus = cse_wrapper::getInstance().jtagTargetGetPin(this->m_cseHandle, eCsePin, &uiValue);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "getPin: Failed to get %s pin.\n", getJtagPinName(ePin));
		return false;
	}

	bValue = (uiValue) ? true : false;

	return true;
}

bool jtag_lib_v2::jtag_cse::shiftDeviceDR(
	const uint8_t uiDevice,
	uint8_t* puiTDIBuffer,
	uint8_t* puiTDOBuffer,
	uint32_t uiBitCount,
	const enum eScanMode eMode)
{
	/* cable lock is required */
	if (!this->checkCableLock("shiftDeviceDR"))
		return false;

	/* convert our shift mode definition to CSE API shift mode */
	unsigned int uiCseMode;
	if (!this->getCseScanMode(eMode, uiCseMode)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftDeviceDR: Unknown shift mode during "
					   "device DR shift.\n");
		return false;
	}

	/* shift device data */
	CSE_RETURN cseStatus;
	uint8_t* puiBuffer = (eMode == SCAN_MODE_WRITE) ? 0 : puiTDOBuffer;
	cseStatus = cse_wrapper::getInstance().jtagTapShiftDeviceDR(
		this->m_cseHandle, uiDevice, uiCseMode, CSEJTAG_RUN_TEST_IDLE, 0, uiBitCount, puiTDIBuffer,
		puiBuffer, 0, 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftDeviceDR: Failed to shift "
					   "device data.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::shiftDeviceIR(
	const uint8_t uiDevice, uint8_t* puiTDIBuffer, uint32_t uiBitCount)
{
	/* cable lock is required */
	if (!this->checkCableLock("shiftDeviceIR"))
		return false;

	/* shift device instruction */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagTapShiftDeviceIR(
		this->m_cseHandle, uiDevice, CSEJTAG_SHIFT_WRITE, CSEJTAG_RUN_TEST_IDLE, 0, uiBitCount,
		puiTDIBuffer, 0, 0, 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftDeviceIR: Failed to shift "
					   "device instruction.\n");
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_cse::shiftChainIR(uint8_t* puiTDIBuffer, uint32_t uiBitCount)
{
	/* cable lock is required */
	if (!this->checkCableLock("shiftChainIR"))
		return false;

	/* shift chain instruction */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagTapShiftChainIR(
		this->m_cseHandle, CSEJTAG_SHIFT_WRITE, CSEJTAG_RUN_TEST_IDLE, 0, uiBitCount, puiTDIBuffer,
		0, 0, 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftChainIR: Failed to shift "
					   "chain instruction.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::shiftChainDR(
	uint8_t* puiTDIBuffer, uint8_t* puiTDOBuffer, uint32_t uiBitCount, const enum eScanMode eMode)
{
	/* cable lock is required */
	if (!this->checkCableLock("shiftChainDR"))
		return false;

	/* convert our shift mode definition to CSE API shift mode */
	unsigned int uiCseMode;
	if (!this->getCseScanMode(eMode, uiCseMode)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftChainDR: Unknown shift mode "
					   "during chain DR shift.\n");
		return false;
	}

	/* shift chain data */
	CSE_RETURN cseStatus;
	uint8_t* puiBuffer = (eMode == SCAN_MODE_WRITE) ? 0 : puiTDOBuffer;
	cseStatus = cse_wrapper::getInstance().jtagTapShiftChainDR(
		this->m_cseHandle, uiCseMode, CSEJTAG_RUN_TEST_IDLE, 0, uiBitCount, puiTDIBuffer, puiBuffer,
		0, 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "shiftChainDR: Failed to shift "
					   "chain data.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::flushCable()
{
	/* parallel cable is unbuffered */
	if (this->m_eJtagTarget == XILINX_JTAG_LPT)
		return true;

	/* need cable lock to flush */
	if (!this->checkCableLock("flushCable"))
		return false;

	/* perform flush */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagTargetFlush(this->m_cseHandle);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(MSG_ERROR, "flushCable: Failed to flush JTAG target.\n");
		return false;
	}

	return true;
}

uint32_t jtag_lib_v2::jtag_cse::getFrequency(const uint32_t uiFrequency)
{
	static const uint32_t uiFrequenciesLPT[] = {5000, 2500, 1250, 625, 200};
	static const uint32_t uiFrequenciesUSB[] = {12000, 6000, 3000, 1500, 750};
	static const uint32_t uiFrequenciesDigilent[] = {30000, 15000, 10000, 7500, 4000, 6000,
													 3000,  2000,  1000,  500,  250,  125};

	/* select closest frequency */
	const uint32_t* uiFrequencies;
	uint8_t uiFrequencyCount;
	if (this->m_eJtagTarget == XILINX_JTAG_LPT) {
		uiFrequencies = uiFrequenciesLPT;
		uiFrequencyCount = sizeof(uiFrequenciesLPT) / sizeof(uint32_t);
	} else if (this->m_eJtagTarget == XILINX_JTAG_DIGILENT) {
		uiFrequencies = uiFrequenciesDigilent;
		uiFrequencyCount = sizeof(uiFrequenciesDigilent) / sizeof(uint32_t);
	} else {
		uiFrequencies = uiFrequenciesUSB;
		uiFrequencyCount = sizeof(uiFrequenciesUSB) / sizeof(uint32_t);
	}

	/* return lowest frequency */
	if (!uiFrequency)
		return uiFrequencies[uiFrequencyCount - 1];

	int i = 0;
	while (uiFrequencies[i] > uiFrequency) {
		if (i == uiFrequencyCount)
			return 0;
		++i;
	}

	return uiFrequencies[i];
}

bool jtag_lib_v2::jtag_cse::setJtagFrequency(const uint32_t uiFrequency)
{
	/* select closest frequency */
	const uint32_t uiFreq = this->getFrequency(uiFrequency);
	if (!uiFreq) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setJtagFrequency: Requested TCK frequency is "
					   "not supported.\n");
		return false;
	}

	/* convert frequency to string */
	char szFrequency[9];
	sprintf(szFrequency, "%d", uiFreq * 1000);

	if (uiFreq < 1000)
		jtag_logger::sendMessage(
			MSG_INFO,
			"setJtagFrequency: Setting cable speed to "
			"%d KHz.\n",
			uiFreq);
	else if (uiFreq % 1000)
		jtag_logger::sendMessage(
			MSG_INFO,
			"setJtagFrequency: Setting cable speed to "
			"%.2f MHz.\n",
			uiFreq / 1000.);
	else
		jtag_logger::sendMessage(
			MSG_INFO,
			"setJtagFrequency: Setting cable speed to "
			"%d MHz.\n",
			uiFreq / 1000);

	/* set new frequency */
	CSE_RETURN cseStatus;
	cseStatus =
		cse_wrapper::getInstance().jtagTargetSetOption(this->m_cseHandle, "DEVICE", szFrequency);

	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setJtagFrequency: Failed to set JTAG TCK "
					   "frequency.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::getDeviceIRLength(const uint8_t uiDevice, uint8_t& uiLength)
{
	/* cable lock is required */
	if (!this->checkCableLock("getDeviceIRLength"))
		return false;

	/* retrieve IR length */
	CSE_RETURN cseStatus;
	unsigned int uiIRLength = 100;
	cseStatus =
		cse_wrapper::getInstance().jtagTapGetIRLength(this->m_cseHandle, uiDevice, &uiIRLength);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getDeviceIRLength: Failed to get IR length "
			"for device %0d.\n",
			uiDevice);
		return false;
	}

	if (uiIRLength > 255 || uiIRLength < 2) {
		jtag_logger::sendMessage(
			MSG_ERROR, "Invalid IR length for device %d (%d).\n", uiDevice, uiIRLength);
		return false;
	}

	uiLength = uiIRLength;

	return true;
}

bool jtag_lib_v2::jtag_cse::interrogateChain()
{
	/* cable lock is required */
	if (!this->checkCableLock("interrogateChain"))
		return false;

	jtag_logger::sendMessage(MSG_INFO, "interrogateChain: Autodetecting chain content.\n");

	/* try to detect chain with Test-Logic-Reset shift */
	CSE_RETURN cseStatus;
	cseStatus =
		cse_wrapper::getInstance().jtagTapAutodetectChain(this->m_cseHandle, CSEJTAG_SCAN_TLRSHIFT);
	if (IS_CSE_SUCCESS(cseStatus))
		return true;

	/* try to detect chain with Walking-Ones shift */
	cseStatus = cse_wrapper::getInstance().jtagTapAutodetectChain(
		this->m_cseHandle, CSEJTAG_SCAN_WALKING_ONES);
	if (IS_CSE_SUCCESS(cseStatus))
		return true;

	jtag_logger::sendMessage(
		MSG_ERROR, "interrogateChain: Failed to autodetect "
				   "JTAG chain.\n");

	return false;
}

bool jtag_lib_v2::jtag_cse::setDeviceCount(const uint8_t uiDevices)
{
	/* cable lock is required */
	if (!this->checkCableLock("setDeviceCount"))
		return false;

	/* chain must have at least one device */
	if (!uiDevices) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setDeviceCount: Chain must have at "
					   "least one device.\n");
		return false;
	}

	/* set actual device count */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagTapSetDeviceCount(this->m_cseHandle, uiDevices);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"setDeviceCount: Could not set device "
			"count to %d.\n",
			uiDevices);
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::getDeviceCount(uint8_t& uiDevices)
{
	/* cable lock is required */
	if (!this->checkCableLock("getDeviceCount"))
		return false;

	/* get actual device count */
	CSE_RETURN cseStatus;
	unsigned int uiCount;
	cseStatus = cse_wrapper::getInstance().jtagTapGetDeviceCount(this->m_cseHandle, &uiCount);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(MSG_ERROR, "getDeviceCount: Could not get device count.\n");
		return false;
	}

	if (!uiCount || uiCount > 256) {
		jtag_logger::sendMessage(
			MSG_ERROR, "getDeviceCount: Invalid number of "
					   "devices in chain.\n");
		return false;
	}

	uiDevices = uiCount;

	return true;
}

bool jtag_lib_v2::jtag_cse::setDeviceInfo(
	const uint8_t uiDevice, const uint8_t uiIRLength, uint32_t uiIdcode)
{
	/* cable lock is required */
	if (!this->checkCableLock("setDeviceInfo"))
		return false;

	/* IR register length must be at least 2 bits */
	if (uiIRLength < 2) {
		jtag_logger::sendMessage(
			MSG_ERROR, "setDeviceInfo JTAG instruction "
					   "register must be at least 2 Bits wide.\n");
		return false;
	}

	/* set IR length for device */
	CSE_RETURN cseStatus;
	cseStatus =
		cse_wrapper::getInstance().jtagTapSetIRLength(this->m_cseHandle, uiDevice, uiIRLength);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"setDeviceInfo: Could not set IR length for "
			"device %d to %d.\n",
			uiDevice, uiIRLength);
		return false;
	}

	/* set IDCODE for device */
	char szIdcode[33];
	for (int i = 31; i >= 0; --i) {
		szIdcode[i] = (uiIdcode & 0x1) ? '1' : '0';
		uiIdcode >>= 1;
	}
	szIdcode[32] = '\0';

	cseStatus =
		cse_wrapper::getInstance().jtagTapSetDeviceIdcode(this->m_cseHandle, uiDevice, szIdcode);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"setDeviceInfo: Could not set IDCODE 0x%08X "
			"for device %d.\n",
			uiIdcode, uiDevice);
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::navigate(const enum eJtagState eState, const uint16_t uiClocks)
{
	/* cable lock is required */
	if (!this->checkCableLock("navigate"))
		return false;

	/* convert out state definition to CSE API equivalent */
	enum CSEJTAG_STATE cseState;
	switch (eState) {
		case TEST_LOGIC_RESET:
			cseState = CSEJTAG_TEST_LOGIC_RESET;
			break;
		case RUN_TEST_IDLE:
			cseState = CSEJTAG_RUN_TEST_IDLE;
			break;
		case SELECT_DR_SCAN:
			cseState = CSEJTAG_SELECT_DR_SCAN;
			break;
		case CAPTURE_DR:
			cseState = CSEJTAG_CAPTURE_DR;
			break;
		case SHIFT_DR:
			cseState = CSEJTAG_SHIFT_DR;
			break;
		case EXIT1_DR:
			cseState = CSEJTAG_EXIT1_DR;
			break;
		case EXIT2_DR:
			cseState = CSEJTAG_EXIT2_DR;
			break;
		case PAUSE_DR:
			cseState = CSEJTAG_PAUSE_DR;
			break;
		case UPDATE_DR:
			cseState = CSEJTAG_UPDATE_DR;
			break;
		case SELECT_IR_SCAN:
			cseState = CSEJTAG_SELECT_IR_SCAN;
			break;
		case CAPTURE_IR:
			cseState = CSEJTAG_CAPTURE_IR;
			break;
		case SHIFT_IR:
			cseState = CSEJTAG_SHIFT_IR;
			break;
		case EXIT1_IR:
			cseState = CSEJTAG_EXIT1_IR;
			break;
		case EXIT2_IR:
			cseState = CSEJTAG_EXIT2_IR;
			break;
		case PAUSE_IR:
			cseState = CSEJTAG_PAUSE_IR;
			break;
		case UPDATE_IR:
			cseState = CSEJTAG_UPDATE_IR;
			break;
		default: {
			jtag_logger::sendMessage(MSG_ERROR, "navigate: Unknown JTAG state.\n");
			return false;
		}
	}

	/* move to new state */
	CSE_RETURN cseStatus;
	cseStatus =
		cse_wrapper::getInstance().jtagTapNavigate(this->m_cseHandle, cseState, uiClocks, 0);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(MSG_ERROR, "navigate: State transition failed.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::getDeviceInfo(
	const uint8_t uiDevice, uint32_t& uiIdcode, char* szDeviceName)
{
	/* cable lock is required */
	if (!this->checkCableLock("getDeviceInfo"))
		return false;

	/* retrieve idcode for device */
	CSE_RETURN cseStatus;
	char szIdcode[CSE_IDCODEBUF_LEN];
	cseStatus = cse_wrapper::getInstance().jtagTapGetDeviceIdcode(
		this->m_cseHandle, uiDevice, szIdcode, CSE_IDCODEBUF_LEN);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getDeviceInfo: Failed to get IDCODE for "
			"device %d.\n",
			uiDevice);
		return false;
	}

	/* convert IDCODE from binary to integer */
	int i = 0;
	uiIdcode = 0;
	while (szIdcode[i] && i < 32) {
		uiIdcode <<= 1;
		uiIdcode |= (szIdcode[i++] == '1') ? 1 : 0;
	}

	if (i < 32 || szIdcode[i] != '\0') {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getDeviceInfo: IDCODE for device %d is "
			"invalid.\n",
			uiDevice);
		return false;
	}

	if (!szDeviceName)
		return true;

	/* retrieve device name for idcode */
	cseStatus = cse_wrapper::getInstance().jtagDbGetDeviceNameForIdcode(
		this->m_cseHandle, szIdcode, szDeviceName, CSEJTAG_MAX_DEVICENAME_LEN);

	if (IS_CSE_FAILURE(cseStatus)) {
		szDeviceName[0] = '\0';
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getDeviceInfo: Failed to find device name "
			"for IDCODE 0x%08X for device %d.\n",
			uiIdcode, uiDevice);
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::addDeviceData(
	const char* szFileName, const char* pBuffer, const uint32_t uiBufferLen)
{
	/* check session */
	if (!this->isSessionOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "addDeviceData: No open session.\n");
		return false;
	}

	/* add device data */
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().jtagDbAddDeviceData(
		this->m_cseHandle, szFileName, pBuffer, uiBufferLen);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: Failed to add additional "
					   "device data.\n");
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_cse::configureDevice(
	const uint8_t uiDevice, uint8_t* puiData, const uint32_t uiLength)
{
	/* cable lock is required */
	if (!this->checkCableLock("configureDevice"))
		return false;

	/* load CSE Fpga library */
	if (!cse_wrapper::getInstance().loadCseFpgaLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "configureDevice: Failed to load Xilinx FPGA "
					   "JTAG access library.\n");
		return false;
	}

	CSE_RETURN cseStatus;
	enum CSE_CONFIG_STATUS uiStatus;
	cseStatus = cse_wrapper::getInstance().fpgaConfigureDevice(
		this->m_cseHandle, uiDevice, "bit", puiData, uiLength, &uiStatus, 0);

	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(MSG_NONE, "\n");
		jtag_logger::sendMessage(
			MSG_ERROR,
			"configureDevice: Failed to configure "
			"device at %d.\n",
			uiDevice);
		return false;
	}

	const enum CSE_CONFIG_STATUS uiExpectedStatus = (enum CSE_CONFIG_STATUS)(
		CSE_INTERNAL_DONE_HIGH_STATUS | CSE_BITSTREAM_READ_ENABLED | CSE_BITSTREAM_WRITE_ENABLED);
	if (uiExpectedStatus ^ uiStatus) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"confiureDevice: Unexpected status after "
			"configuration of device at %d.\n",
			uiDevice);
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_cse::isSysMonSupported(const uint8_t uiDevice, bool& bSupported)
{
	/* get IDCODE of device */
	CSE_RETURN cseStatus;
	char szIdcode[CSE_IDCODEBUF_LEN];
	bSupported = false;
	cseStatus = cse_wrapper::getInstance().jtagTapGetDeviceIdcode(
		this->m_cseHandle, uiDevice, szIdcode, CSE_IDCODEBUF_LEN);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"isSysMonSupported: Failed to get IDCODE "
			"for device %d.\n",
			uiDevice);
		return false;
	}

	/* load CSE Fpga library */
	if (!cse_wrapper::getInstance().loadCseFpgaLib()) {
		jtag_logger::sendMessage(
			MSG_ERROR, "isSysMonSupported: Failed to load Xilinx FPGA "
					   "JTAG access library.\n");
		return false;
	}

	/* retrieve System Monitor availability */
	unsigned int uiSupported;
	cseStatus =
		cse_wrapper::getInstance().fpgaIsSysMonSupported(this->m_cseHandle, szIdcode, &uiSupported);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"isSysMonSupported: Failed to get System "
			"monitor availability for device %d.\n",
			uiDevice);
		return false;
	}

	bSupported = (uiSupported != 0);

	return true;
}

bool jtag_lib_v2::jtag_cse::getSysMonRegister(
	const uint8_t uiDevice, const uint8_t uiAddress, uint32_t& uiData)
{
	uiData = 0;
	CSE_RETURN cseStatus;
	cseStatus = cse_wrapper::getInstance().fpgaGetSysMonReg(
		this->m_cseHandle, uiDevice, uiAddress, &uiData);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getSysMonRegister: Failed to get "
			"System Monitor register from device at %d.\n",
			uiDevice);
		return false;
	}
	return true;
}

jtag_lib_v2::jtag_cse::~jtag_cse()
{
	/* close cable */
	this->closeCable();

	/* destroy session if active */
	if (this->isSessionOpen())
		cse_wrapper::getInstance().jtagSessionDestroy(this->m_cseHandle);

	this->freeCableList();
}

HMODULE jtag_lib_v2::cse_wrapper::loadCseLib(const TCHAR* szLibName)
{
	/* load the library */
	HMODULE hModule = ::loadLibrary(szLibName);
	if (!hModule) {
		jtag_logger::sendMessage(
			MSG_DEBUG, "loadCseLib: Failed to open CSE library '%s'.\n", szLibName);
		jtag_logger::sendMessage(MSG_DEBUG, "%s\n", ::getErrorMessage());
		return 0;
	}
	return hModule;
}

bool jtag_lib_v2::cse_wrapper::loadCseFpgaLib()
{
	/* load CSE Fpga library */
	if (this->m_hFpgaModule)
		return true;

	this->m_hFpgaModule = this->loadCseLib(TEXT("libCseFpga.") DLL_EXT);
	if (!this->m_hFpgaModule)
		return false;

	/* load CSE Fpga functions */
	this->fpgaConfigureDeviceProc =
		::getProcFunction(this->m_hFpgaModule, "CseFpga_configureDevice");
	this->fpgaIsSysMonSupported = (fpgaIsSysMonSupported_t)::getProcFunction(
		this->m_hFpgaModule, "CseFpga_isSysMonSupported");
	this->fpgaGetSysMonReg =
		(fpgaGetSysMonReg_t)::getProcFunction(this->m_hFpgaModule, "CseFpga_getSysMonReg");

	const bool bValid =
		this->fpgaConfigureDeviceProc && this->fpgaIsSysMonSupported && this->fpgaGetSysMonReg;

	if (!bValid) {
		::freeLibrary(this->m_hFpgaModule);
		this->m_hFpgaModule = 0;
		return false;
	}

	return true;
}

CSE_RETURN jtag_lib_v2::cse_wrapper::fpgaConfigureDevice(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	const char* format,
	unsigned char* fileData,
	unsigned int fileDataByteLen,
	enum CSE_CONFIG_STATUS* configStatus,
	CSE_PROGRESS_CB progressFunc)
{
	if (this->m_uiMajorVersion < 13) {
		const fpgaConfigureDevice12_t fpgaConfigureDevice =
			fpgaConfigureDevice12_t(this->fpgaConfigureDeviceProc);

		return fpgaConfigureDevice(
			handle, deviceIndex, format, 0x000C, fileData, fileDataByteLen, configStatus,
			progressFunc);
	}

	const fpgaConfigureDevice_t fpgaConfigureDevice =
		fpgaConfigureDevice_t(this->fpgaConfigureDeviceProc);
	return fpgaConfigureDevice(
		handle, deviceIndex, format, 0, 0, fileData, fileDataByteLen, configStatus, progressFunc);
}

bool jtag_lib_v2::cse_wrapper::checkAPIVersion()
{
	typedef CSE_RETURN (*jtagSessionGetAPIVersion_t)(unsigned int*, char*, unsigned int);
	jtagSessionGetAPIVersion_t jtagSessionGetAPIVersion;

	/* load function locally */
	jtagSessionGetAPIVersion = (jtagSessionGetAPIVersion_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_session_getAPIVersion");
	if (!jtagSessionGetAPIVersion)
		return false;

	/* get API version */
	unsigned int uiAPIVersion;
	char szVersion[CSEJTAG_VERSION_LEN];
	CSE_RETURN cseStatus = jtagSessionGetAPIVersion(&uiAPIVersion, szVersion, CSEJTAG_VERSION_LEN);
	if (IS_CSE_FAILURE(cseStatus)) {
		jtag_logger::sendMessage(MSG_ERROR, "checkAPIVersion: Failed to get API version.\n");
		return false;
	}

	/* print API version */
	jtag_logger::sendMessage(MSG_INFO, "API: %s\n", szVersion);

	/* extract major version */
	uint32_t uiMajorVersion = (((uiAPIVersion >> 16) & 0xF) * 10) + ((uiAPIVersion >> 12) & 0xF);
	/* extract minor version */
	uint32_t uiMinorVersion = (uiAPIVersion >> 8) & 0xF;

	/* check upper limit API version */
	if (uiMajorVersion > MAX_MAJOR_VERSION ||
		(uiMajorVersion == MAX_MAJOR_VERSION && uiMinorVersion > MAX_MINOR_VERSION)) {
		bool bIgnoreVersion = false;
		if (::getenv("XILINX_API_VERSION_TEST"))
			bIgnoreVersion = true;
		jtag_logger::sendMessage(
			(bIgnoreVersion) ? MSG_WARNING : MSG_ERROR,
			"checkAPIVersion: Xilinx installation is "
			"too new and not yet supported. Please "
			"downgrade to %d.%d.\n",
			MAX_MAJOR_VERSION, MAX_MINOR_VERSION);
		if (!bIgnoreVersion)
			return false;
	}

	/* check lower limit API version */
	if (uiMajorVersion < MIN_MAJOR_VERSION) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"checkAPIVersion: Xilinx installation is "
			"too old and no longer supported. Please "
			"upgrade to at least %d.1.\n",
			MIN_MAJOR_VERSION);
		return false;
	}

	this->m_uiMajorVersion = uiMajorVersion;

	return true;
}

bool jtag_lib_v2::cse_wrapper::loadCseJtagLib()
{
	/* load CSE Jtag library */
	if (this->m_hJtagModule)
		return true;

	this->m_hJtagModule = this->loadCseLib(TEXT("libCseJtag.") DLL_EXT);
	if (!this->m_hJtagModule)
		return false;

	/* check that the API version matches */
	if (!this->checkAPIVersion())
		return false;

	/* load CSE Jtag functions */
	this->jtagSessionCreate =
		(jtagSessionCreate_t)::getProcFunction(this->m_hJtagModule, "CseJtag_session_create");
	this->jtagSessionDestroy =
		(jtagSessionDestroy_t)::getProcFunction(this->m_hJtagModule, "CseJtag_session_destroy");
	this->jtagSessionSendMessage = (jtagSessionSendMessage_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_session_sendMessage");
	this->jtagTargetOpen =
		(jtagTargetOpen_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_open");
	this->jtagTargetClose =
		(jtagTargetClose_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_close");
	this->jtagTargetLock =
		(jtagTargetLock_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_lock");
	this->jtagTargetUnlock =
		(jtagTargetUnlock_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_unlock");
	this->jtagTargetCleanLocks =
		(jtagTargetCleanLocks_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_cleanLocks");
	this->jtagTargetFlush =
		(jtagTargetFlush_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_flush");
	this->jtagTargetSetPin =
		(jtagTargetSetPin_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_setPin");
	this->jtagTargetGetPin =
		(jtagTargetGetPin_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_getPin");
	this->jtagTargetPulsePin =
		(jtagTargetPulsePin_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_pulsePin");
	this->jtagTargetSetOption =
		(jtagTargetSetOption_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_setOption");
	this->jtagTargetGetOption =
		(jtagTargetGetOption_t)::getProcFunction(this->m_hJtagModule, "CseJtag_target_getOption");
	this->jtagTargetGetLockStatus = (jtagTargetGetLockStatus_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_target_setOption");
	this->jtagTapAutodetectChain = (jtagTapAutodetectChain_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_tap_autodetectChain");
	this->jtagTapShiftDeviceIR =
		(jtagTapShiftDeviceIR_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_shiftDeviceIR");
	this->jtagTapShiftDeviceDR =
		(jtagTapShiftDeviceDR_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_shiftDeviceDR");
	this->jtagTapShiftChainIR =
		(jtagTapShiftChainIR_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_shiftChainIR");
	this->jtagTapShiftChainDR =
		(jtagTapShiftChainDR_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_shiftChainDR");
	this->jtagTapGetIRLength =
		(jtagTapGetIRLength_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_getIRLength");
	this->jtagTapNavigate =
		(jtagTapNavigate_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_navigate");
	this->jtagTapSetDeviceCount = (jtagTapSetDeviceCount_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_tap_setDeviceCount");
	this->jtagTapGetDeviceCount = (jtagTapGetDeviceCount_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_tap_getDeviceCount");
	this->jtagTapSetDeviceIdcode = (jtagTapSetDeviceIdcode_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_tap_setDeviceIdcode");
	this->jtagTapSetIRLength =
		(jtagTapSetIRLength_t)::getProcFunction(this->m_hJtagModule, "CseJtag_tap_setIRLength");
	this->jtagTapGetDeviceIdcode = (jtagTapGetDeviceIdcode_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_tap_getDeviceIdcode");
	this->jtagDbGetDeviceNameForIdcode = (jtagDbGetDeviceNameForIdcode_t)::getProcFunction(
		this->m_hJtagModule, "CseJtag_db_getDeviceNameForIdcode");
	this->jtagDbAddDeviceData =
		(jtagDbAddDeviceData_t)::getProcFunction(this->m_hJtagModule, "CseJtag_db_addDeviceData");

	const bool bValid =
		this->jtagSessionCreate && this->jtagSessionSendMessage && this->jtagSessionDestroy &&
		this->jtagTargetUnlock && this->jtagTargetLock && this->jtagTargetCleanLocks &&
		this->jtagTargetClose && this->jtagTargetOpen && this->jtagTargetFlush &&
		this->jtagTargetSetPin && this->jtagTargetGetPin && this->jtagTargetPulsePin &&
		this->jtagTargetSetOption && this->jtagTargetGetOption && this->jtagTargetGetLockStatus &&
		this->jtagTapAutodetectChain && this->jtagTapShiftDeviceIR && this->jtagTapShiftDeviceDR &&
		this->jtagTapShiftChainIR && this->jtagTapShiftChainDR && this->jtagTapGetIRLength &&
		this->jtagTapNavigate && this->jtagTapSetDeviceCount && this->jtagTapSetDeviceIdcode &&
		this->jtagTapSetIRLength && this->jtagTapGetDeviceIdcode && this->jtagTapGetDeviceCount &&
		this->jtagDbGetDeviceNameForIdcode && this->jtagDbAddDeviceData;

	if (!bValid) {
		::freeLibrary(this->m_hJtagModule);
		this->m_hJtagModule = 0;
		return false;
	}

	return true;
}

jtag_lib_v2::cse_wrapper::cse_wrapper()
	: m_hJtagModule(0),
	  m_hFpgaModule(0),
	  m_uiMajorVersion(0),
	  fpgaConfigureDeviceProc(0),
	  fpgaIsSysMonSupported(0),
	  fpgaGetSysMonReg(0),
	  jtagSessionCreate(0),
	  jtagSessionSendMessage(0),
	  jtagSessionDestroy(0),
	  jtagTargetUnlock(0),
	  jtagTargetLock(0),
	  jtagTargetCleanLocks(0),
	  jtagTargetClose(0),
	  jtagTargetOpen(0),
	  jtagTargetFlush(0),
	  jtagTargetSetPin(0),
	  jtagTargetGetPin(0),
	  jtagTargetPulsePin(0),
	  jtagTargetSetOption(0),
	  jtagTargetGetOption(0),
	  jtagTargetGetLockStatus(0),
	  jtagTapAutodetectChain(0),
	  jtagTapShiftDeviceIR(0),
	  jtagTapShiftDeviceDR(0),
	  jtagTapShiftChainIR(0),
	  jtagTapShiftChainDR(0),
	  jtagTapGetIRLength(0),
	  jtagTapNavigate(0),
	  jtagTapSetDeviceCount(0),
	  jtagTapGetDeviceCount(0),
	  jtagTapSetDeviceIdcode(0),
	  jtagTapSetIRLength(0),
	  jtagTapGetDeviceIdcode(0),
	  jtagDbGetDeviceNameForIdcode(0),
	  jtagDbAddDeviceData(0)
{}

bool jtag_lib_v2::cse_wrapper::freeCseFpgaLib()
{
	/* check whether CSE Fpga library is currently loaded */
	if (!this->m_hFpgaModule)
		return true;

	/* free CSE Fpga library */
	const int iResult = ::freeLibrary(this->m_hFpgaModule);
	this->m_hFpgaModule = 0;

	return (iResult == 0);
}

bool jtag_lib_v2::cse_wrapper::freeCseJtagLib()
{
	/* check whether CSE Jtag library is currently loaded */
	if (!this->m_hJtagModule)
		return true;

	/* free CSE Jtag library */
	const int iResult = ::freeLibrary(this->m_hJtagModule);
	this->m_hJtagModule = 0;

	return (iResult == 0);
}

jtag_lib_v2::cse_wrapper& jtag_lib_v2::cse_wrapper::getInstance()
{
	static cse_wrapper myCseWrapper;
	return myCseWrapper;
}

jtag_lib_v2::cse_wrapper::~cse_wrapper()
{
	if (this->m_hJtagModule)
		::freeLibrary(this->m_hJtagModule);

	if (this->m_hFpgaModule)
		::freeLibrary(this->m_hFpgaModule);

	this->jtagSessionCreate = 0;
	this->jtagSessionSendMessage = 0;
	this->jtagSessionDestroy = 0;
	this->jtagTargetUnlock = 0;
	this->jtagTargetLock = 0;
	this->jtagTargetCleanLocks = 0;
	this->jtagTargetClose = 0;
	this->jtagTargetOpen = 0;
	this->jtagTargetFlush = 0;
	this->jtagTargetSetPin = 0;
	this->jtagTargetGetPin = 0;
	this->jtagTargetPulsePin = 0;
	this->jtagTargetSetOption = 0;
	this->jtagTargetGetOption = 0;
	this->jtagTargetGetLockStatus = 0;
	this->jtagTapAutodetectChain = 0;
	this->jtagTapShiftDeviceIR = 0;
	this->jtagTapShiftDeviceDR = 0;
	this->jtagTapShiftChainIR = 0;
	this->jtagTapShiftChainDR = 0;
	this->jtagTapGetIRLength = 0;
	this->jtagTapNavigate = 0;
	this->jtagTapSetDeviceCount = 0;
	this->jtagTapGetDeviceCount = 0;
	this->jtagTapSetDeviceIdcode = 0;
	this->jtagTapSetIRLength = 0;
	this->jtagTapGetDeviceIdcode = 0;
	this->jtagDbGetDeviceNameForIdcode = 0;
	this->jtagDbAddDeviceData = 0;
	this->fpgaConfigureDeviceProc = 0;

	this->m_hJtagModule = 0;
	this->m_hFpgaModule = 0;
}

#if __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
