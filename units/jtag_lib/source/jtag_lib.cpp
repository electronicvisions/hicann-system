//-----------------------------------------------------------------
//
// Copyright (c) 2012 TU-Dresden  All rights reserved.
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
// Filename          :   jtag_lib_v2.cpp
// Project Name      :   JTAG Library V2
// Description       :   High-Level JTAG library implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_lib.hpp"

#define JTAG_LIB_V2
#include "jtag_access.hpp"

jtag_lib_v2::jtag_lib::jtag_lib()
{
	this->m_pJtagAccess = new jtag_access();
}

jtag_lib_v2::jtag_lib::~jtag_lib()
{
	delete this->m_pJtagAccess;
}

bool jtag_lib_v2::jtag_lib::initJtag(const enum jtag_lib_v2::eJtagInterface eInterface)
{
	return this->m_pJtagAccess->initJtag(eInterface);
}

bool jtag_lib_v2::jtag_lib::createSession(const char* szHost, const uint16_t uiPort)
{
	return this->m_pJtagAccess->createSession(szHost, uiPort);
}

bool jtag_lib_v2::jtag_lib::openCable(
	const uint8_t uiIndex, const uint32_t uiFrequency, const bool bForceUnlock)
{
	return this->m_pJtagAccess->openCable(uiIndex, uiFrequency, bForceUnlock);
}

bool jtag_lib_v2::jtag_lib::closeCable()
{
	return this->m_pJtagAccess->closeCable();
}

bool jtag_lib_v2::jtag_lib::flushCable()
{
	return this->m_pJtagAccess->flushCable();
}

bool jtag_lib_v2::jtag_lib::enumCables(uint8_t& uiCables, const bool bEnumOther)
{
	return this->m_pJtagAccess->enumCables(uiCables, bEnumOther);
}

bool jtag_lib_v2::jtag_lib::autodetectChain()
{
	return this->m_pJtagAccess->autodetectChain();
}

bool jtag_lib_v2::jtag_lib::resetJtag()
{
	return this->m_pJtagAccess->resetJtag();
}

bool jtag_lib_v2::jtag_lib::setJtagFrequency(const uint32_t uiFrequency)
{
	return this->m_pJtagAccess->setJtagFrequency(uiFrequency);
}

bool jtag_lib_v2::jtag_lib::setDeviceCount(const uint8_t uiDevices)
{
	return this->m_pJtagAccess->setDeviceCount(uiDevices);
}

bool jtag_lib_v2::jtag_lib::getDeviceCount(uint8_t& uiDevices)
{
	return this->m_pJtagAccess->getDeviceCount(uiDevices);
}

bool jtag_lib_v2::jtag_lib::setDeviceInfo(
	const uint8_t uiDevice, const uint8_t uiIRLength, uint32_t uiIdCode)
{
	return this->m_pJtagAccess->setDeviceInfo(uiDevice, uiIRLength, uiIdCode);
}

bool jtag_lib_v2::jtag_lib::getDeviceInfo(
	const uint8_t uiDevice, uint32_t& uiIdcode, char* szDeviceName)
{
	return this->m_pJtagAccess->getDeviceInfo(uiDevice, uiIdcode, szDeviceName);
}

bool jtag_lib_v2::jtag_lib::addDeviceData(const char* szFile)
{
	return this->m_pJtagAccess->addDeviceData(szFile);
}

bool jtag_lib_v2::jtag_lib::addDeviceData(
	const char* szName, const uint32_t uiIdcode, const uint8_t uiIRLength)
{
	return this->m_pJtagAccess->addDeviceData(szName, uiIdcode, uiIRLength);
}

bool jtag_lib_v2::jtag_lib::configureDevice(
	const uint8_t uiDevice, uint8_t* puiData, const uint32_t uiLength)
{
	return this->m_pJtagAccess->configureDevice(uiDevice, puiData, uiLength);
}

bool jtag_lib_v2::jtag_lib::isSysMonSupported(const uint8_t uiDevice, bool& bSupported)
{
	return this->m_pJtagAccess->isSysMonSupported(uiDevice, bSupported);
}

bool jtag_lib_v2::jtag_lib::getSysMonRegister(
	const uint8_t uiDevice, const uint8_t uiRegister, uint32_t& uiData)
{
	return this->m_pJtagAccess->getSysMonRegister(uiDevice, uiRegister, uiData);
}

bool jtag_lib_v2::jtag_lib::setJtagInstr(const uint8_t uiDevice, const uint8_t uiInstr)
{
	return this->m_pJtagAccess->setJtagInstr(uiDevice, uiInstr);
}

bool jtag_lib_v2::jtag_lib::setJtagInstr(const uint8_t uiDevice, const uint16_t uiInstr)
{
	return this->m_pJtagAccess->setJtagInstr(uiDevice, uiInstr);
}

bool jtag_lib_v2::jtag_lib::setJtagInstr(const uint8_t uiDevice, const uint32_t uiInstr)
{
	return this->m_pJtagAccess->setJtagInstr(uiDevice, uiInstr);
}

bool jtag_lib_v2::jtag_lib::setJtagInstr(const uint8_t uiDevice, const uint64_t uiInstr)
{
	return this->m_pJtagAccess->setJtagInstr(uiDevice, uiInstr);
}

bool jtag_lib_v2::jtag_lib::setJtagData(
	const uint8_t uiDevice, const uint8_t uiData, const uint8_t uiLength, const bool bFlush)
{
	return this->m_pJtagAccess->setJtagData(uiDevice, uiData, uiLength, bFlush);
}

bool jtag_lib_v2::jtag_lib::setJtagData(
	const uint8_t uiDevice, const uint16_t uiData, const uint8_t uiLength, const bool bFlush)
{
	return this->m_pJtagAccess->setJtagData(uiDevice, uiData, uiLength, bFlush);
}

bool jtag_lib_v2::jtag_lib::setJtagData(
	const uint8_t uiDevice, const uint32_t uiData, const uint8_t uiLength, const bool bFlush)
{
	return this->m_pJtagAccess->setJtagData(uiDevice, uiData, uiLength, bFlush);
}

bool jtag_lib_v2::jtag_lib::setJtagData(
	const uint8_t uiDevice, const uint64_t uiData, const uint8_t uiLength, const bool bFlush)
{
	return this->m_pJtagAccess->setJtagData(uiDevice, uiData, uiLength, bFlush);
}

bool jtag_lib_v2::jtag_lib::setJtagData(
	const uint8_t uiDevice,
	const std::vector<bool>& uiData,
	const uint8_t uiLength,
	const bool bFlush)
{
	return this->m_pJtagAccess->setJtagData(uiDevice, uiData, uiLength, bFlush);
}

bool jtag_lib_v2::jtag_lib::getJtagData(
	const uint8_t uiDevice, uint8_t& uiData, const uint8_t uiLength)
{
	return this->m_pJtagAccess->getJtagData(uiDevice, uiData, uiLength);
}

bool jtag_lib_v2::jtag_lib::getJtagData(
	const uint8_t uiDevice, uint16_t& uiData, const uint8_t uiLength)
{
	return this->m_pJtagAccess->getJtagData(uiDevice, uiData, uiLength);
}

bool jtag_lib_v2::jtag_lib::getJtagData(
	const uint8_t uiDevice, uint32_t& uiData, const uint8_t uiLength)
{
	return this->m_pJtagAccess->getJtagData(uiDevice, uiData, uiLength);
}

bool jtag_lib_v2::jtag_lib::getJtagData(
	const uint8_t uiDevice, uint64_t& uiData, const uint8_t uiLength)
{
	return this->m_pJtagAccess->getJtagData(uiDevice, uiData, uiLength);
}

bool jtag_lib_v2::jtag_lib::getJtagData(
	const uint8_t uiDevice, std::vector<bool>& uiData, const uint16_t uiLength)
{
	return this->m_pJtagAccess->getJtagData(uiDevice, uiData, uiLength);
}

bool jtag_lib_v2::jtag_lib::setGetJtagData(
	const uint8_t uiDevice, const uint8_t uiDataIn, uint8_t& uiDataOut, const uint8_t uiLength)
{
	return this->m_pJtagAccess->setGetJtagData(uiDevice, uiDataIn, uiDataOut, uiLength);
}

bool jtag_lib_v2::jtag_lib::setGetJtagData(
	const uint8_t uiDevice, const uint16_t uiDataIn, uint16_t& uiDataOut, const uint8_t uiLength)
{
	return this->m_pJtagAccess->setGetJtagData(uiDevice, uiDataIn, uiDataOut, uiLength);
}

bool jtag_lib_v2::jtag_lib::setGetJtagData(
	const uint8_t uiDevice, const uint32_t uiDataIn, uint32_t& uiDataOut, const uint8_t uiLength)
{
	return this->m_pJtagAccess->setGetJtagData(uiDevice, uiDataIn, uiDataOut, uiLength);
}

bool jtag_lib_v2::jtag_lib::setGetJtagData(
	const uint8_t uiDevice, const uint64_t uiDataIn, uint64_t& uiDataOut, const uint8_t uiLength)
{
	return this->m_pJtagAccess->setGetJtagData(uiDevice, uiDataIn, uiDataOut, uiLength);
}

bool jtag_lib_v2::jtag_lib::setGetJtagData(
	const uint8_t uiDevice,
	std::vector<bool> const& uiDataIn,
	std::vector<bool>& uiDataOut,
	const uint16_t uiLength)
{
	return this->m_pJtagAccess->setGetJtagData(uiDevice, uiDataIn, uiDataOut, uiLength);
}
