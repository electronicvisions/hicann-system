//-----------------------------------------------------------------
//
// Copyright (c) 2013 TU-Dresden  All rights reserved.
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
// Filename          :   jtag_targets.cpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG target handling
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_targets.hpp"
#include "jtag_logger.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

inline bool _isblank(const char c)
{
#ifdef _WIN32
	return (c == 0x20 || c == 0x09);
#else
	return isblank(c);
#endif
}

bool jtag_lib_v2::jtag_targets::targetInfoCmp(
	jtag_targets::target_info_t* p1, jtag_targets::target_info_t* p2)
{
	for (int i = 0; i < 32; ++i) {
		if (p1->szIdcode[i] != p2->szIdcode[i]) {
			if (p1->szIdcode[i] == '0')
				return true;
			if (p1->szIdcode[i] == '1' && p2->szIdcode[i] == 'x')
				return true;
			return false;
		}
	}
	return false;
}

jtag_lib_v2::jtag_targets::jtag_targets() {}

jtag_lib_v2::jtag_targets::~jtag_targets()
{
	for (std::vector<target_info_t*>::iterator it = this->m_jtagTargets.begin();
		 it != this->m_jtagTargets.end(); ++it)
		delete (*it);
	for (std::vector<target_info_t*>::iterator it = this->m_tempJtagTargets.begin();
		 it != this->m_tempJtagTargets.end(); ++it)
		delete (*it);
}

void jtag_lib_v2::jtag_targets::parserError(const char* szMessage)
{
	if (this->m_szFile)
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: %s: Line %d:%d: %s\n", this->m_szFile, this->m_uiLine,
			this->m_uiLinePos, szMessage);
	else
		jtag_logger::sendMessage(
			MSG_ERROR, "addDeviceData: Internal: %d: %s\n", this->m_uiLinePos, szMessage);
}

void jtag_lib_v2::jtag_targets::unexpectedCharError(const char cChar)
{
	if (::_isblank(cChar))
		jtag_targets::parserError("Unexpected blank character.");
	else if (cChar == '\n')
		jtag_targets::parserError("Unexpected new line.");
	else if (std::iscntrl(cChar))
		jtag_targets::parserError("Unexpected control character.");
	else if (std::isalnum(cChar)) {
		char szMessage[128];
		::snprintf(szMessage, 128, "Unexpected character '%c'.", cChar);
		jtag_targets::parserError(szMessage);
	} else
		jtag_targets::parserError("Unexpected character.");
}

bool jtag_lib_v2::jtag_targets::parseIdcode(const char cChar, const bool bStart, bool& bEnd)
{
	bEnd = false;
	if (bStart) {
		if (cChar == '0' || cChar == '1' || cChar == 'x' || cChar == 'X')
			this->m_targetInfo.szIdcode[0] = (cChar == 'X') ? 'x' : cChar;
		else {
			this->unexpectedCharError(cChar);
			this->parserError("'_' is not allowed for first "
							  "IDCODE bit.");
			return false;
		}
		this->m_uiNumberPos = 1;
		return true;
	}

	if (cChar == '0' || cChar == '1' || cChar == 'x' || cChar == 'X') {
		if (this->m_uiNumberPos == 32) {
			this->unexpectedCharError(cChar);
			this->parserError("IDCODE is limited to 32-Bit.");
			return false;
		}
		this->m_targetInfo.szIdcode[this->m_uiNumberPos] = (cChar == 'X') ? 'x' : cChar;
		this->m_uiNumberPos++;
	} else if (::_isblank(cChar)) {
		if (this->m_uiNumberPos != 32) {
			this->unexpectedCharError(cChar);
			this->parserError("Unexpected character while "
							  "parsing IDCODE.");
			return false;
		}
	} else if (cChar == ',') {
		if (this->m_uiNumberPos != 32) {
			this->unexpectedCharError(cChar);
			this->parserError("Unexpected character while "
							  "parsing IDCODE.");
			return false;
		}
		bEnd = true;
	} else if (cChar == '_') {
		if (this->m_uiNumberPos == 32) {
			this->unexpectedCharError(cChar);
			this->parserError("'_' is not allowed after last "
							  "IDCODE bit.");
			return false;
		}
	} else {
		this->unexpectedCharError(cChar);
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_targets::parseNumber(const char cChar, bool& bEnd)
{
	bEnd = false;
	if (::_isblank(cChar)) {
		if (this->m_uiNumber >= 0)
			this->m_uiNumberPos = 255;
	} else if (std::isdigit(cChar)) {
		if (this->m_uiNumberPos == 255) {
			this->unexpectedCharError(cChar);
			this->parserError("Blank characters are not allowed between digits.");
			return false;
		}
		if (this->m_uiNumber < 0)
			this->m_uiNumber = 0;
		this->m_uiNumber *= 10;
		this->m_uiNumber += (cChar - '0');
		this->m_uiNumberPos++;
		if (this->m_uiNumber > 255) {
			this->parserError("Number is out of range.");
			return false;
		}
	} else if (cChar == ',') {
		if (!this->m_uiNumberPos) {
			this->unexpectedCharError(cChar);
			this->parserError("Need at least one digit for number.");
			return false;
		}
		bEnd = true;
	} else {
		jtag_targets::unexpectedCharError(cChar);
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_targets::parseDeviceName(const char cChar, bool& bEnd)
{
	bEnd = false;
	if (::_isblank(cChar)) {
		if (this->m_uiNumber >= 0)
			this->m_uiNumberPos = 255;
	} else if (cChar > 44 && cChar < 127) {
		this->m_uiNumber = 0;
		if (this->m_uiNumberPos == 127) {
			this->parserError("Device name is limited to 127 characters.");
			return false;
		}
		if (this->m_uiNumberPos == 255) {
			this->parserError("Blank characters are not allowed in device name.");
			return false;
		}
		this->m_targetInfo.szDeviceName[this->m_uiNumberPos] = cChar;
		this->m_uiNumberPos++;
	} else if (cChar == '\n' || cChar == '#') {
		if (!this->m_uiNumberPos) {
			this->unexpectedCharError(cChar);
			this->parserError("Need at least one character for device name.");
			return false;
		}
		this->m_targetInfo.szDeviceName[this->m_uiNumberPos] = '\0';
		bEnd = true;
	} else {
		jtag_targets::unexpectedCharError(cChar);
		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_targets::insertTarget()
{
	jtag_targets::target_info_t* pTargetInfo = 0;
	try {
		pTargetInfo = new jtag_targets::target_info_t;
		::memcpy(pTargetInfo, &this->m_targetInfo, sizeof(target_info_t));
		this->m_tempJtagTargets.push_back(pTargetInfo);
	} catch (std::bad_alloc&) {
		jtag_logger::sendMessage(
			MSG_ERROR, "Not enough memory store "
					   "target infos.\n");
		if (pTargetInfo)
			delete pTargetInfo;

		return false;
	}
	return true;
}

bool jtag_lib_v2::jtag_targets::addDeviceData(
	const char* szFile, char* puiBuffer, const size_t uiSize)
{
	enum eParserState
	{
		SOL,
		COMMENT,
		IDCODE,
		IRLENGTH,
		DEVICETYPE,
		DEVICESUBTYPE,
		DEVICENAME
	};
	enum eParserState eState = SOL;
	size_t uiPos = 0;

	this->m_uiLine = 1;
	this->m_uiLinePos = 1;
	this->m_szFile = szFile;

	while (uiPos < uiSize) {
		const char cChar = puiBuffer[uiPos];
		switch (eState) {
			case SOL: {
				this->m_targetInfo.uiIRLength = 0;
				if (cChar == '\n') {
					this->m_uiLine++;
					this->m_uiLinePos = 1;
				} else if (!::_isblank(cChar)) {
					bool bEnd;
					if (cChar == '#')
						eState = COMMENT;
					else if (!this->parseIdcode(cChar, true, bEnd))
						return false;
					else
						eState = IDCODE;
				}
			} break;
			case COMMENT: {
				if (cChar == '\n') {
					this->m_uiLine++;
					this->m_uiLinePos = 1;
					eState = SOL;
				}
			} break;
			case IDCODE: {
				bool bEnd;
				if (!this->parseIdcode(cChar, false, bEnd))
					return false;
				if (bEnd) {
					this->m_uiNumber = -1;
					this->m_uiNumberPos = 0;
					eState = IRLENGTH;
				}
			} break;
			case IRLENGTH: {
				bool bEnd;
				if (!this->parseNumber(cChar, bEnd))
					return false;
				if (bEnd) {
					this->m_uiNumberPos = 0;
					this->m_targetInfo.uiIRLength = static_cast<uint8_t>(this->m_uiNumber);
					this->m_uiNumber = -1;
					eState = DEVICETYPE;
				}
			} break;
			case DEVICETYPE: {
				bool bEnd;
				if (!this->parseNumber(cChar, bEnd))
					return false;
				if (bEnd) {
					this->m_uiNumberPos = 0;
					this->m_uiNumber = -1;
					eState = DEVICESUBTYPE;
				}
			} break;
			case DEVICESUBTYPE: {
				bool bEnd;
				if (!this->parseNumber(cChar, bEnd))
					return false;
				if (bEnd) {
					this->m_uiNumberPos = 0;
					this->m_uiNumber = -1;
					eState = DEVICENAME;
				}
			} break;
			case DEVICENAME: {
				bool bEnd;
				if (!this->parseDeviceName(cChar, bEnd))
					return false;
				if (bEnd) {
					if (cChar == '#')
						eState = COMMENT;
					else {
						this->m_uiLine++;
						this->m_uiLinePos = 1;
						eState = SOL;
						if (!this->insertTarget())
							return false;
					}
				}
			} break;
		}
		uiPos++;
		this->m_uiLinePos++;
	}

	if ((eState != SOL && eState != COMMENT && eState != DEVICENAME) ||
		(eState == DEVICENAME && !this->m_uiNumberPos)) {
		if (szFile) {
			jtag_logger::sendMessage(MSG_ERROR, "%s: Unexpected end of file.\n", szFile);
		} else
			jtag_logger::sendMessage(MSG_ERROR, "Unexpected end of input.\n");
	}

	if (eState == COMMENT || eState == DEVICENAME) {
		if (szFile)
			jtag_logger::sendMessage(MSG_WARNING, "%s: No newline at end of file.\n", szFile);
		else
			jtag_logger::sendMessage(MSG_WARNING, "No newline at end of input.\n");
	}

	try {
		this->m_jtagTargets.reserve(this->m_jtagTargets.size() + this->m_tempJtagTargets.size());
	} catch (std::bad_alloc&) {
		jtag_logger::sendMessage(
			MSG_ERROR, "Not enough memory store "
					   "target infos.\n");
		return false;
	} catch (std::length_error&) {
		jtag_logger::sendMessage(MSG_ERROR, "Maximum number of targets reached.\n");
		return false;
	}

	this->m_jtagTargets.insert(
		this->m_jtagTargets.end(), this->m_tempJtagTargets.begin(), this->m_tempJtagTargets.end());
	this->m_tempJtagTargets.clear();

	std::sort(this->m_jtagTargets.begin(), this->m_jtagTargets.end(), targetInfoCmp);

	return true;
}

bool jtag_lib_v2::jtag_targets::getDeviceInfoForIdcode(
	const uint32_t uiIdcode, uint8_t& uiIRLength, char** pszDeviceName)
{
	uint32_t uiIdcode_t = uiIdcode;
	char szIdcode[32];
	for (uint8_t i = 0; i < 32; ++i) {
		szIdcode[31 - i] = (uiIdcode_t & 0x1) ? '1' : '0';
		uiIdcode_t >>= 1;
	}

	for (std::vector<target_info_t*>::iterator it = this->m_jtagTargets.begin();
		 it != this->m_jtagTargets.end(); ++it) {
		bool bFound = true;
		for (uint8_t i = 0; i < 32; ++i) {
			if ((*it)->szIdcode[i] != szIdcode[i] && (*it)->szIdcode[i] != 'x') {
				bFound = false;
				break;
			}
		}
		if (bFound) {
			uiIRLength = (*it)->uiIRLength;
			if (pszDeviceName)
				*pszDeviceName = (*it)->szDeviceName;
			return true;
		}
	}

	return false;
}
