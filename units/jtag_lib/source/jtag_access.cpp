//-----------------------------------------------------------------
//
// Copyright (c) 2014 TU-Dresden  All rights reserved.
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
// Filename          :   jtag_access.cpp
// Project Name      :   JTAG Library V2
// Description       :   High-Level JTAG access implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_access.hpp"
#include "jtag_logger.hpp"

#ifndef NO_XILINX_LIB
#include "jtag_cse.hpp"
#endif

#ifndef NO_DIGILENT_LIB
#include "jtag_digilent.hpp"
#endif

#ifndef NO_SYSTEMC_LIB
#include "jtag_systemc.hpp"
#endif

#ifndef NO_ETHERNET_LIB
#include "jtag_ethernet.hpp"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MAX_DEVICE_DATA_SIZE 65536
#define MAX_JTAG_DEVICES 256
#define MAX_DEVICE_NAME_LEN 64

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define LIBJTAG_V2_MAJOR_VERSION 0
#define LIBJTAG_V2_MINOR_VERSION 1
#define LIBJTAG_V2_PRE_VERSION 0

#ifndef LIBJTAG_V2_GIT_COMMIT
#define LIBJTAG_V2_GIT_COMMIT "unknown"
#endif

#ifndef LIBJTAG_V2_GIT_BRANCH
#define LIBJTAG_V2_GIT_BRANCH "unknown"
#endif

#ifndef LIBJTAG_V2_GIT_IS_DIRTY
#define LIBJTAG_V2_GIT_IS_DIRTY 0
#endif

#ifndef LIBJTAG_V2_GIT_IS_UTD
#define LIBJTAG_V2_GIT_IS_UTD 0
#endif

jtag_lib_v2::jtag_access::jtag_access() : m_pJtagIf(0), m_eJtagInterface(XILINX_USB) {}

jtag_lib_v2::jtag_access::~jtag_access()
{
	if (this->m_pJtagIf)
		delete this->m_pJtagIf;
	this->m_pJtagIf = 0;
}

bool jtag_lib_v2::jtag_access::isDeviceValid(const char* szFunc, const uint8_t uiDevice)
{
	if (uiDevice >= this->m_uiDeviceCount) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"%s: Requested device %d "
			"does not exist in the chain.\n",
			szFunc, uiDevice);
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_access::isChainValid(const char* szFunc)
{
	if (!this->m_uiDeviceCount) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"%s: JTAG chain contains "
			"no devices.\n",
			szFunc);

		return false;
	}

	for (uint16_t i = 0; i < this->m_uiDeviceCount; ++i)
		if (!this->m_jtagDevices[i].bValid) {
			jtag_logger::sendMessage(
				MSG_ERROR,
				"%s: JTAG chain contains "
				"invalid devices.\n",
				szFunc);

			return false;
		}

	return true;
}

bool jtag_lib_v2::jtag_access::getBypassBits(
	const uint8_t uiDevice, uint16_t& uiPreShiftBits, uint16_t& uiPostShiftBits)
{
	uint32_t uiPostBits = 0;
	for (uint16_t i = 0; i < uiDevice; ++i)
		uiPostBits += this->m_jtagDevices[i].uiIRLength;

	uint32_t uiPreBits = 0;
	for (uint16_t j = uiDevice + 1; j < this->m_uiDeviceCount; ++j)
		uiPreBits += this->m_jtagDevices[j].uiIRLength;

	if (uiPreBits > 1023) {
		jtag_logger::sendMessage(
			MSG_ERROR, "getBypassBits: Instruction register length "
					   "before requested device is too long.\n");
		return false;
	}

	if (uiPostBits > 1023) {
		jtag_logger::sendMessage(
			MSG_ERROR, "getBypassBits: Instruction register length "
					   "after requested device is too long.\n");
		return false;
	}

	uiPreShiftBits = uiPreBits;
	uiPostShiftBits = uiPostBits;
	return true;
}

void jtag_lib_v2::jtag_access::formatDataWrite(
	std::vector<bool> const& abData,
	const uint16_t uiLength,
	std::vector<uint8_t>& auiTDIData,
	const uint16_t uiPreShiftBits,
	const uint16_t uiPostShiftBits)
{
	auiTDIData.clear();
	const uint16_t uiMaxDataBits = abData.size();
	uint16_t uiDataBits = (uiMaxDataBits < uiLength) ? uiMaxDataBits : uiLength;
	const uint32_t uiBits = uiLength + uiPreShiftBits + uiPostShiftBits;
	uint16_t uiPreBits = uiPreShiftBits;
	uiDataBits = uiLength;
	uint16_t uiPos = 0;
	for (uint16_t i = 0; i<(uiBits + 7)>> 3; ++i) {
		uint8_t uiByte = 0;
		if (uiPreBits > 8)
			uiPreBits -= 8;
		else if (uiPreBits) {
			const uint8_t uiLastBits = 8 - uiPreBits;
			const uint8_t uiCopyBits = (uiDataBits > uiLastBits) ? uiLastBits : uiDataBits;
			for (uint8_t j = 0; j < uiCopyBits; ++j)
				uiByte |= ((abData[uiPos + j]) ? 1 << (j + uiPreBits) : 0);
			uiDataBits -= uiCopyBits;
			uiPos += uiCopyBits;
			uiPreBits = 0;
		} else if (uiDataBits) {
			const uint8_t uiCopyBits = (uiDataBits > 8) ? 8 : uiDataBits;
			for (uint8_t j = 0; j < uiCopyBits; ++j)
				uiByte |= ((abData[uiPos + j]) ? 1 << j : 0);
			uiPos += uiCopyBits;
			uiDataBits -= uiCopyBits;
		}

		auiTDIData.push_back(uiByte);
	}
}

void jtag_lib_v2::jtag_access::formatDataRead(
	uint8_t* puiTDOData,
	const uint16_t uiLength,
	std::vector<bool>& auiTDOData,
	const uint8_t uiPreShiftBits)
{
	auiTDOData.clear();
	if (!uiLength)
		return;

	const uint16_t uiDataBits = uiLength + uiPreShiftBits;
	uint8_t uiPreBits = uiPreShiftBits;
	for (uint16_t i = 0; i<(uiDataBits + 7)>> 3; ++i) {
		if (uiPreBits >= 8)
			uiPreBits -= 8;
		else {
			const uint8_t uiBitsLeft = (uiDataBits - 8 * i);
			uint8_t uiBits = (uiBitsLeft > 8) ? 8 : uiBitsLeft;
			uint8_t uiByte = puiTDOData[i] >> uiPreBits;

			uiBits -= uiPreBits;
			for (uint8_t j = 0; j < uiBits; ++j) {
				auiTDOData.push_back(uiByte & 0x1);
				uiByte >>= 1;
			}
			uiPreBits = 0;
		}
	}
}

bool jtag_lib_v2::jtag_access::checkCable(const bool bIfOnly)
{
	if (!this->m_pJtagIf) {
		jtag_logger::sendMessage(MSG_ERROR, "No JTAG interface open.\n");
		return false;
	}
	if (bIfOnly)
		return true;
	if (!this->m_pJtagIf->isCableOpen()) {
		jtag_logger::sendMessage(MSG_ERROR, "No JTAG cable open.\n");
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_access::setJtagData(
	const uint8_t uiDevice,
	std::vector<bool> const& abData,
	const uint16_t uiLength,
	const bool bFlush)
{
	if (!this->checkCable())
		return false;

	std::vector<uint8_t> auiData;
	if (!this->isXilinxJtag()) {
		if (!this->isDeviceValid("setJtagData", uiDevice))
			return false;
		if (!this->isChainValid("setJtagData"))
			return false;

		this->formatDataWrite(abData, uiLength, auiData, 0, uiDevice);
		const uint32_t uiDataLength = uiLength + uiDevice;
		if (!this->m_pJtagIf->shiftChainDR(&auiData[0], 0, uiDataLength, SCAN_MODE_WRITE))
			return false;
	} else {
		this->formatDataWrite(abData, uiLength, auiData);
		if (!this->m_pJtagIf->shiftDeviceDR(
				uiDevice, &auiData[0], (uint8_t*) (0), uiLength, SCAN_MODE_WRITE))
			return false;
	}

	if (bFlush)
		return this->flushCable();
	return true;
}

bool jtag_lib_v2::jtag_access::getJtagData(
	const uint8_t uiDevice, std::vector<bool>& abData, const uint16_t uiLength)
{
	if (!this->checkCable())
		return false;

	uint8_t* puiTDIData = 0;
	uint8_t* puiTDOData = 0;

	if (!this->isXilinxJtag()) {
		if (!this->isDeviceValid("getJtagData", uiDevice))
			return false;
		if (!this->isChainValid("getJtagData"))
			return false;

		const uint8_t uiPreShiftBits = this->m_uiDeviceCount - uiDevice - 1;
		const uint32_t uiDataLength = uiLength + uiPreShiftBits;

		try {
			puiTDOData = new uint8_t[(uiDataLength + 7) >> 3];
		} catch (std::bad_alloc) {
			return false;
		}

		if (!this->m_pJtagIf->shiftChainDR(0, puiTDOData, uiDataLength, SCAN_MODE_READ))
			return false;
		this->formatDataRead(puiTDOData, uiLength, abData, uiPreShiftBits);
	} else {
		try {
#ifdef READ_TDI_ZERO_SHIFT
			puiTDIData = new uint8_t[(uiLength + 7) >> 3];
#endif
			puiTDOData = new uint8_t[(uiLength + 7) >> 3];
		} catch (std::bad_alloc) {
			if (puiTDIData)
				delete[] puiTDIData;
			return false;
		}

		const bool bResult = this->m_pJtagIf->shiftDeviceDR(
			uiDevice, puiTDIData, puiTDOData, uiLength, SCAN_MODE_READ);
		if (bResult)
			this->formatDataRead(puiTDOData, uiLength, abData);

		if (puiTDIData)
			delete[] puiTDIData;
		delete[] puiTDOData;

		return bResult;
	}

	return true;
}

bool jtag_lib_v2::jtag_access::setGetJtagData(
	const uint8_t uiDevice,
	std::vector<bool> const& abDataIn,
	std::vector<bool>& abDataOut,
	const uint16_t uiLength)
{
	if (!this->checkCable())
		return false;

	std::vector<uint8_t> auiDataIn;
	uint8_t* puiDataOut = 0;
	bool bResult = false;

	if (!this->isXilinxJtag()) {
		if (!this->isDeviceValid("setGetJtagData", uiDevice))
			return false;
		if (!this->isChainValid("setGetJtagData"))
			return false;

		const uint16_t uiPreShiftBits = this->m_uiDeviceCount - uiDevice - 1;
		this->formatDataWrite(abDataIn, uiLength, auiDataIn, uiPreShiftBits, uiDevice);
		const uint32_t uiDataLength = uiLength + uiPreShiftBits + uiDevice;

		try {
			puiDataOut = new uint8_t[(uiDataLength + 7) >> 3];
		} catch (std::bad_alloc) {
			return false;
		}

		bResult = this->m_pJtagIf->shiftChainDR(
			&auiDataIn[0], puiDataOut, uiDataLength, SCAN_MODE_READ_WRITE);
		if (bResult)
			this->formatDataRead(puiDataOut, uiLength, abDataOut, uiPreShiftBits);
	} else {
		this->formatDataWrite(abDataIn, uiLength, auiDataIn);

		try {
			puiDataOut = new uint8_t[(uiLength + 7) >> 3];
		} catch (std::bad_alloc) {
			return false;
		}

		bResult = this->m_pJtagIf->shiftDeviceDR(
			uiDevice, &auiDataIn[0], puiDataOut, uiLength, SCAN_MODE_READ_WRITE);
		if (bResult)
			this->formatDataRead(puiDataOut, uiLength, abDataOut);
	}

	if (puiDataOut)
		delete[] puiDataOut;

	return bResult;
}

bool jtag_lib_v2::jtag_access::initJtag(const enum jtag_lib_v2::eJtagInterface eInterface)
{
	if (this->m_pJtagIf) {
		delete this->m_pJtagIf;
		this->m_pJtagIf = 0;
	} else {
		jtag_logger::sendMessage(
			MSG_INFO, "libjtag_v2-v%d.%d%s-git-%s-%s-%s%s\n", LIBJTAG_V2_MAJOR_VERSION,
			LIBJTAG_V2_MINOR_VERSION, (LIBJTAG_V2_PRE_VERSION) ? LIBJTAG_V2_PRE_VERSION : "",
			LIBJTAG_V2_GIT_BRANCH, LIBJTAG_V2_GIT_COMMIT,
			(LIBJTAG_V2_GIT_IS_DIRTY) ? "dirty" : "clean", (LIBJTAG_V2_GIT_IS_UTD) ? "-utd" : "");
	}

	switch (eInterface) {
#ifndef NO_XILINX_USB
		case XILINX_USB:
			this->m_pJtagIf = new jtag_cse(XILINX_JTAG_USB);
			break;
#endif
#ifndef NO_XILINX_LPT
		case XILINX_LPT:
			this->m_pJtagIf = new jtag_cse(XILINX_JTAG_LPT);
			break;
#endif
#ifndef NO_XILINX_DIGILENT
		case XILINX_DIGILENT:
			this->m_pJtagIf = new jtag_cse(XILINX_JTAG_DIGILENT);
			break;
#endif
#ifndef NO_DIGILENT_LIB
		case JTAG_DIGILENT:
			this->m_pJtagIf = new jtag_digilent();
			break;
#endif
#ifndef NO_SYSTEMC_LIB
		case JTAG_SYSTEMC:
			this->m_pJtagIf = new jtag_systemc();
			break;
#endif
#ifndef NO_ETHERNET_LIB
		case JTAG_ETHERNET:
			this->m_pJtagIf = new jtag_ethernet(false);
			break;
#ifndef NO_SYSTEMC_LIB
		case JTAG_ETHERNET_SYSTEMC:
			this->m_pJtagIf = new jtag_ethernet(true);
			break;
#endif
#endif
		default:
			return false;
	}

	if (!this->m_pJtagIf) {
		jtag_logger::sendMessage(
			MSG_ERROR, "initJtag: Could not open requested "
					   "JTAG interface.\n");
		return false;
	}

	this->m_eJtagInterface = eInterface;

	return true;
}

bool jtag_lib_v2::jtag_access::createSession(const char* szHost, const uint16_t uiPort)
{
	if (!this->checkCable(true))
		return false;

	return this->m_pJtagIf->createSession(szHost, uiPort);
}

bool jtag_lib_v2::jtag_access::closeSession()
{
	if (!this->checkCable(true))
		return false;

	return this->m_pJtagIf->closeSession();
}

bool jtag_lib_v2::jtag_access::enumCables(uint8_t& uiCablesFound, const bool bEnumOther)
{
	if (!this->checkCable(true))
		return false;

	return this->m_pJtagIf->enumCables(uiCablesFound, bEnumOther);
}

bool jtag_lib_v2::jtag_access::getCableInfo(const uint8_t uiCableIndex, cable_info& cableInfo)
{
	if (!this->checkCable(true))
		return false;

	if (this->m_eJtagInterface == JTAG_ETHERNET)
		return false;

	return this->m_pJtagIf->getCableInfo(uiCableIndex, cableInfo);
}

bool jtag_lib_v2::jtag_access::openCable(
	const uint8_t uiIndex, const uint32_t uiFrequency, const bool bForceUnlock)
{
	if (!this->checkCable(true))
		return false;

	return this->m_pJtagIf->openCable(uiIndex, uiFrequency, bForceUnlock);
}

bool jtag_lib_v2::jtag_access::closeCable()
{
	if (!this->checkCable(true))
		return false;

	return this->m_pJtagIf->closeCable();
}

bool jtag_lib_v2::jtag_access::flushCable()
{
	if (!this->checkCable())
		return false;

	return this->m_pJtagIf->flushCable();
}

bool jtag_lib_v2::jtag_access::setJtagFrequency(const uint32_t uiFrequency)
{
	if (!this->checkCable())
		return false;

	return this->m_pJtagIf->setJtagFrequency(uiFrequency);
}

bool jtag_lib_v2::jtag_access::doTLRAutodetect()
{
	uint8_t auiTDIBuffer[4 * MAX_JTAG_DEVICES];
	std::fill_n(auiTDIBuffer, 4 * MAX_JTAG_DEVICES, 0xFF);
	uint8_t auiTDOBuffer[4 * MAX_JTAG_DEVICES];
	uint16_t uiLength = 0;
	uint32_t uiLastIdcode;
	do {
		if (!this->resetJtag())
			return false;

		if (!this->m_pJtagIf->shiftChainDR(
				auiTDIBuffer, auiTDOBuffer, (uiLength + 1) << 5, SCAN_MODE_READ_WRITE))
			return false;

		uiLastIdcode = (auiTDOBuffer[4 * uiLength + 0] << 24) |
					   (auiTDOBuffer[4 * uiLength + 1] << 16) |
					   (auiTDOBuffer[4 * uiLength + 2] << 8) | auiTDOBuffer[4 * uiLength + 3];

		uiLength++;

	} while (uiLength <= MAX_JTAG_DEVICES && uiLastIdcode != 0xFFFFFFFF);

	if (uiLength > MAX_JTAG_DEVICES && uiLastIdcode != 0xFFFFFFFF) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"autodetectChain: More than "
			"%d devices found in JTAG chain.\n",
			MAX_JTAG_DEVICES);
		this->setDeviceCount(0);
		return false;
	}

	if (!uiLength) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"autodetectChain: No "
			"device found in JTAG chain.\n",
			MAX_JTAG_DEVICES);
		this->setDeviceCount(0);
		return false;
	}

	this->setDeviceCount(uiLength - 1);

	for (uint16_t i = 0; i < (uiLength - 1); ++i) {
		uint32_t uiIdcode;
		this->formatDataRead(&(auiTDOBuffer[4 * i]), 32, uiIdcode);

		const uint8_t uiPos = uiLength - 2 - i;

		uint8_t uiIRLength;
		char* szDeviceName;
		if (this->m_jtagTargets.getDeviceInfoForIdcode(uiIdcode, uiIRLength, &szDeviceName)) {
			this->setDeviceInfo(uiPos, uiIRLength, uiIdcode);
			this->m_jtagDevices[uiPos].szDeviceName = szDeviceName;
		} else {
			jtag_logger::sendMessage(
				MSG_WARNING,
				"autodetectChain: Unknown JTAG "
				"device at position %d with IDCODE "
				"0x%08X.\n",
				uiPos + 1, uiIdcode);
			this->m_jtagDevices[uiPos].bValid = false;
		}
	}

	return true;
}

bool jtag_lib_v2::jtag_access::autodetectChain()
{
	if (!this->checkCable())
		return false;

#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag())
		return (static_cast<jtag_cse*>(this->m_pJtagIf))->interrogateChain();
#endif

	jtag_logger::sendMessage(
		MSG_INFO, "autodetectChain: Autodetecting "
				  "chain content ...\n");

	if (!this->doTLRAutodetect())
		return false;

	return true;
}

bool jtag_lib_v2::jtag_access::getDeviceIRLength(const uint8_t uiDevice, uint8_t& uiIRLength)
{
	if (!this->checkCable())
		return false;

#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag()) {
		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->getDeviceIRLength(uiDevice, uiIRLength);
	}
#endif

	if (!this->isDeviceValid("getDeviceIRLength", uiDevice))
		return false;

	if (!this->m_jtagDevices[uiDevice].bValid) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getDeviceIRLength: Instruction register "
			"length of requested device %d "
			"is unknown.\n",
			uiDevice);
		return false;
	}

	uiIRLength = this->m_jtagDevices[uiDevice].uiIRLength;

	return true;
}

bool jtag_lib_v2::jtag_access::resetJtag()
{
	if (!this->checkCable())
		return false;

	if (!this->m_pJtagIf->navigate(TEST_LOGIC_RESET, 5))
		return false;

	return this->m_pJtagIf->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_access::navigateState(const enum eJtagState eState, const uint16_t uiClocks)
{
	if (!this->checkCable())
		return false;

	return this->m_pJtagIf->navigate(eState, uiClocks);
}

bool jtag_lib_v2::jtag_access::setDeviceCount(const uint8_t uiDevices)
{
	if (!this->checkCable())
		return false;

#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag()) {
		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->setDeviceCount(uiDevices);
	}
#endif

	this->m_uiDeviceCount = uiDevices;
	memset(this->m_jtagDevices, 0x00, sizeof(device_info_t) * 256);

	return true;
}

bool jtag_lib_v2::jtag_access::getDeviceCount(uint8_t& uiDevices)
{
	if (!this->checkCable())
		return false;

#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag()) {
		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->getDeviceCount(uiDevices);
	}
#endif

	uiDevices = static_cast<uint8_t>(this->m_uiDeviceCount);

	return true;
}

bool jtag_lib_v2::jtag_access::setDeviceInfo(
	const uint8_t uiDevice, const uint8_t uiIRLength, uint32_t uiIdcode)
{
	if (!this->checkCable())
		return false;

#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag()) {
		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->setDeviceInfo(uiDevice, uiIRLength, uiIdcode);
	}
#endif

	if (!this->isDeviceValid("setDeviceInfo", uiDevice))
		return false;

	this->m_jtagDevices[uiDevice].uiIRLength = uiIRLength;
	this->m_jtagDevices[uiDevice].uiIdcode = uiIdcode;
	this->m_jtagDevices[uiDevice].bValid = true;

	return true;
}

bool jtag_lib_v2::jtag_access::getDeviceInfo(
	const uint8_t uiDevice, uint32_t& uiIdcode, char* szDeviceName)
{
	if (!this->checkCable())
		return false;

#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag()) {
		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->getDeviceInfo(uiDevice, uiIdcode, szDeviceName);
	}
#endif

	if (!this->isDeviceValid("getDeviceInfo", uiDevice))
		return false;
	if (!this->m_jtagDevices[uiDevice].bValid) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"getDeviceInfo: Device information of "
			"requested device %d are unknown.\n",
			uiDevice);
		return false;
	}

	uiIdcode = this->m_jtagDevices[uiDevice].uiIdcode;
	if (!this->m_jtagDevices[uiDevice].szDeviceName) {
		char* szDeviceName_t;
		uint8_t uiIRLength;
		if (this->m_jtagTargets.getDeviceInfoForIdcode(uiIdcode, uiIRLength, &szDeviceName_t))
			this->m_jtagDevices[uiDevice].szDeviceName = szDeviceName_t;
	}

	if (this->m_jtagDevices[uiDevice].szDeviceName && szDeviceName)
		strcpy(szDeviceName, this->m_jtagDevices[uiDevice].szDeviceName);

	return true;
}

bool jtag_lib_v2::jtag_access::addDeviceData(const char* szFile)
{
	if (!this->checkCable(true))
		return false;

	/* open file */
	FILE* pFile = ::fopen(szFile, "rb");
	if (!pFile) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"addDeviceData: Failed to open "
			"device data file '%s'.\n",
			szFile);
		return false;
	}

	jtag_logger::sendMessage(
		MSG_INFO,
		"addDeviceData: Reading additional "
		"device data from '%s'.\n",
		szFile);

	/* read file to buffer */
	char* pBuffer;
	try {
		pBuffer = new char[MAX_DEVICE_DATA_SIZE];
	} catch (std::bad_alloc) {
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: Not enough memory to "
					   "read device data file.\n");
		::fclose(pFile);
		return false;
	}

	size_t uiRead = ::fread(pBuffer, 1, MAX_DEVICE_DATA_SIZE, pFile);
#ifdef _WIN32
	int iError = ferror(pFile);
#else
	int iError = ::ferror(pFile);
#endif
	if (iError) {
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: Failed to read "
					   "device data file.\n");
		jtag_logger::sendMessage(MSG_ERROR, "%s\n", ::strerror(iError));
		delete[] pBuffer;
		::fclose(pFile);
		return false;
	}

#ifdef _WIN32
	if (!feof(pFile))
#else
	if (!::feof(pFile))
#endif
	{
		jtag_logger::sendMessage(
			MSG_ERROR,
			"addDeviceData: Device data "
			"file is too large. Maximum "
			"%d bytes allowed.\n",
			MAX_DEVICE_DATA_SIZE);
		delete[] pBuffer;
		::fclose(pFile);
		return false;
	}

	/* add device data */
	bool bResult = true;
	if (uiRead) {
#ifndef NO_XILINX_LIB
		if (this->isXilinxJtag()) {
			jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
			bResult = pJtagCse->addDeviceData(szFile, pBuffer, uiRead);
		} else
#endif
			bResult = this->m_jtagTargets.addDeviceData(szFile, pBuffer, uiRead);
	}

	delete[] pBuffer;
	::fclose(pFile);
	return bResult;
}

bool jtag_lib_v2::jtag_access::addDeviceData(
	const char* szDeviceName, const uint32_t uiIdcode, const uint8_t uiIRLength)
{
	if (!this->checkCable(true))
		return false;

	/* check parameters */
	if (!szDeviceName) {
		jtag_logger::sendMessage(MSG_ERROR, "addDeviceData: Device name is required.\n");
		return false;
	}
	const uint32_t uiDeviceNameLen = strlen(szDeviceName);
	if (uiDeviceNameLen > MAX_DEVICE_NAME_LEN) {
		jtag_logger::sendMessage(
			MSG_ERROR,
			"addDeviceData: Device name is limited to "
			"%d characters.\n",
			MAX_DEVICE_NAME_LEN);
		return false;
	}
	if (uiDeviceNameLen < 1) {
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: Device name needs at least "
					   "one characters.\n");
		return false;
	}
	if (uiIRLength < 2) {
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: Length of Jtag instruction "
					   "register has to be at least 2 Bits.\n");
		return false;
	}
	if (uiIdcode == 0xFFFFFFFF || uiIdcode == 0x00000000) {
		jtag_logger::sendMessage(MSG_ERROR, "addDeviceData: Invalid IDCODE 0x%08X.\n", uiIdcode);
		return false;
	}

	if (!(uiIdcode & 0x1)) {
		jtag_logger::sendMessage(
			MSG_WARNING, "addDeviceData: LSB of IDCODE should be 1 "
						 "for proper auto-detection of chain.\n");
	}

	/* convert IDCODE */
	char szBuffer[MAX_DEVICE_NAME_LEN + 42];
	uint32_t uiIdcode_t = uiIdcode;
	for (int i = 31; i >= 0; --i) {
		szBuffer[i] = (uiIdcode_t & 0x1) ? '1' : '0';
		uiIdcode_t >>= 1;
	}
	szBuffer[32] = ',';

	/* convert IR register length */
	uint8_t uiIRLen = uiIRLength;
	uint8_t uiDigit = (uiIRLen / 100);
	uiIRLen -= (100 * uiDigit);
	szBuffer[33] = '0' + uiDigit;
	uiDigit = (uiIRLen / 10);
	uiIRLen -= (10 * uiDigit);
	szBuffer[34] = '0' + uiDigit;
	szBuffer[35] = '0' + uiIRLen;
	szBuffer[36] = ',';

	/* add unused fields */
	szBuffer[37] = '0';
	szBuffer[38] = ',';
	szBuffer[39] = '0';
	szBuffer[40] = ',';

	/* add device name */
	memcpy(&(szBuffer[41]), szDeviceName, uiDeviceNameLen);

	/* add new line */
	szBuffer[41 + uiDeviceNameLen] = '\n';

	/* add device data */
	bool bResult = true;
	const uint16_t uiRead = 42 + uiDeviceNameLen;
#ifndef NO_XILINX_LIB
	if (this->isXilinxJtag()) {
		const char szFile[] = "(null)";
		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		bResult = pJtagCse->addDeviceData(szFile, szBuffer, uiRead);
	} else
#endif
		bResult = this->m_jtagTargets.addDeviceData(0, szBuffer, uiRead);

	return bResult;
}

bool jtag_lib_v2::jtag_access::configureDevice(
	const uint8_t uiDevice, uint8_t* puiData, const uint32_t uiLength)
{
#ifdef NO_XILINX_LIB
	return false;
#else
	if (this->isXilinxJtag()) {
		if (!this->checkCable())
			return false;

		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->configureDevice(uiDevice, puiData, uiLength);
	}
	return false;
#endif
}

bool jtag_lib_v2::jtag_access::isSysMonSupported(const uint8_t uiDevice, bool& bSupported)
{
#ifdef NO_XILINX_LIB
	return false;
#else
	if (this->isXilinxJtag()) {
		if (!this->checkCable())
			return false;

		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->isSysMonSupported(uiDevice, bSupported);
	}
	return false;
#endif
}

bool jtag_lib_v2::jtag_access::getSysMonRegister(
	const uint8_t uiDevice, const uint8_t uiAddress, uint32_t& uiData)
{
#ifdef NO_XILINX_LIB
	return false;
#else
	if (this->isXilinxJtag()) {
		if (!this->checkCable())
			return false;

		jtag_cse* pJtagCse = (static_cast<jtag_cse*>(this->m_pJtagIf));
		return pJtagCse->getSysMonRegister(uiDevice, uiAddress, uiData);
	}
	return false;
#endif
}
