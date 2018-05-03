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
// Filename          :   jtag_if.cpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG base interface implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_if.hpp"
#include "jtag_logger.hpp"

static const char caszPinNames[5][8] = {"TCK", "TMS", "TDI", "TDO", "UNKNOWN"};

#include <string.h>

#include <new>

const char* jtag_lib_v2::jtag_if::getJtagPinName(const enum jtag_lib_v2::eJtagPin ePin)
{
	switch (ePin) {
		case JTAG_PIN_TCK:
			return caszPinNames[0];
			break;
		case JTAG_PIN_TMS:
			return caszPinNames[1];
			break;
		case JTAG_PIN_TDI:
			return caszPinNames[2];
			break;
		case JTAG_PIN_TDO:
			return caszPinNames[3];
			break;
	}
	return caszPinNames[4];
}

void jtag_lib_v2::jtag_if::freeCableList()
{
	for (std::vector<cable_info*>::iterator it = this->m_connectedCables.begin();
		 it != this->m_connectedCables.end(); ++it)
		delete *it;
	this->m_connectedCables.clear();
}

void jtag_lib_v2::jtag_if::markEnumDone()
{
	this->m_bEnumDone = true;
}

void jtag_lib_v2::jtag_if::addCableInfo(cable_info* pCableInfo)
{
	this->m_connectedCables.push_back(pCableInfo);
}

bool jtag_lib_v2::jtag_if::getCableInfo(const uint8_t uiCableIndex, cable_info& cableInfo)
{
	/* check cable index */
	if (!uiCableIndex || !this->m_connectedCables.size())
		return false;

	if (uiCableIndex > this->m_connectedCables.size())
		return false;

	/* copy requested element */
	try {
		cable_info cableInfoFromList(*this->m_connectedCables[uiCableIndex - 1]);
		cableInfo = cableInfoFromList;
	} catch (std::bad_alloc) {
		return false;
	}

	return true;
}

bool jtag_lib_v2::jtag_if::getCableToOpen(
	const uint8_t uiCableIndex, cable_info** ppCableInfo, const bool bForceUnlock)
{
	/* check session */
	if (!this->m_bSessionOpen) {
		jtag_logger::sendMessage(MSG_ERROR, "openCable: No open session.\n");
		return false;
	}

	/* do nothing if a cable is already open in this session */
	if (this->m_bCableOpen) {
		jtag_logger::sendMessage(MSG_ERROR, "openCable: A cable is already open.\n");
		return false;
	}

	/* enumerate devices if not done previously */
	uint8_t uiCablesFound;
	if (!this->m_bEnumDone) {
		if (!this->enumCables(uiCablesFound, false)) {
			jtag_logger::sendMessage(MSG_ERROR, "openCable: Failed to enumerate devices.\n");
			return false;
		}
	} else
		uiCablesFound = this->m_connectedCables.size();

	/* check cable index */
	if (!this->m_connectedCables.size()) {
		jtag_logger::sendMessage(MSG_ERROR, "openCable: No device to open found.\n");
		return false;
	}

	if (this->m_connectedCables.size() < uiCableIndex) {
		jtag_logger::sendMessage(MSG_ERROR, "openCable: No device at index %d.\n", uiCableIndex);
		return false;
	}

	/* select cable to connect to */
	if (!uiCableIndex) {
		bool bFoundCable = false;
		for (uint16_t i = 0; i < uiCablesFound; ++i) {
			*ppCableInfo = this->m_connectedCables[i];
			if ((*ppCableInfo)->isLocked() && !bForceUnlock)
				continue;
			bFoundCable = true;
			break;
		}
		if (!bFoundCable) {
			jtag_logger::sendMessage(
				MSG_ERROR, "openCable: No suitable cable for "
						   "connection found.\n");
			return false;
		}
	} else
		*ppCableInfo = this->m_connectedCables[uiCableIndex - 1];

	return true;
}

bool jtag_lib_v2::jtag_if::closeSession()
{
	this->closeCable();
	this->setSessionOpen(false);
	return true;
}

void jtag_lib_v2::jtag_if::setCableOpen(const bool bCableOpen)
{
	this->m_bCableOpen = bCableOpen;
}

void jtag_lib_v2::jtag_if::setSessionOpen(const bool bSessionOpen)
{
	this->m_bSessionOpen = bSessionOpen;
}

jtag_lib_v2::jtag_if::jtag_if() : m_bEnumDone(false), m_bSessionOpen(false), m_bCableOpen(false) {}

jtag_lib_v2::jtag_if::~jtag_if() {}

jtag_lib_v2::cable_info::cable_info()
	: m_szSerial(0), m_szPort(0), m_bLocked(false), m_szPlugin(0), m_uiNumPorts(0)
{}

jtag_lib_v2::cable_info::cable_info(cable_info& other)
	: m_szSerial(0), m_szPort(0), m_bLocked(false), m_szPlugin(0), m_uiNumPorts(0)
{
	if (!this->setSerial(other.getSerial()))
		throw std::bad_alloc();
	if (!this->setPlugin(other.getPlugin()))
		throw std::bad_alloc();
	if (!this->setPort(other.getPort()))
		throw std::bad_alloc();

	this->setLocked(other.isLocked());
	this->setPortNumber(other.getPortNumber());
}

jtag_lib_v2::cable_info::~cable_info()
{
	if (this->m_szSerial)
		delete[] this->m_szSerial;
	if (this->m_szPort)
		delete[] this->m_szPort;
	if (this->m_szPlugin)
		delete[] this->m_szPlugin;

	this->m_szSerial = 0;
	this->m_szPort = 0;
	this->m_szPlugin = 0;
}

bool jtag_lib_v2::cable_info::setSerial(const char* szSerial)
{
	if (!szSerial) {
		this->m_szSerial = 0;
		return true;
	}

	try {
		this->m_szSerial = new char[strlen(szSerial) + 1];
	} catch (std::bad_alloc) {
		this->m_szSerial = 0;
		return false;
	}

	memcpy(this->m_szSerial, szSerial, strlen(szSerial) + 1);

	return true;
}

bool jtag_lib_v2::cable_info::setPlugin(const char* szPlugin)
{
	if (!szPlugin) {
		this->m_szPlugin = 0;
		return true;
	}

	try {
		this->m_szPlugin = new char[strlen(szPlugin) + 1];
	} catch (std::bad_alloc) {
		this->m_szPlugin = 0;
		return false;
	}

	memcpy(this->m_szPlugin, szPlugin, strlen(szPlugin) + 1);

	return true;
}

bool jtag_lib_v2::cable_info::setPort(const char* szPort)
{
	if (!szPort) {
		this->m_szPort = 0;
		return true;
	}

	try {
		this->m_szPort = new char[strlen(szPort) + 1];
	} catch (std::bad_alloc) {
		this->m_szPort = 0;
		return false;
	}

	memcpy(this->m_szPort, szPort, strlen(szPort) + 1);

	return true;
}

void jtag_lib_v2::cable_info::setPortNumber(const uint8_t uiNumPorts)
{
	this->m_uiNumPorts = uiNumPorts;
}

void jtag_lib_v2::cable_info::setLocked(const bool bLocked)
{
	this->m_bLocked = bLocked;
}
