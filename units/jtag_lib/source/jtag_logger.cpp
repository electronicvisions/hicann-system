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
// Filename          :   jtag_logger.cpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG library logging implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_logger.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LIB_NAME "jtag_lib_v2"
#define MAX_MSG_LENGTH (255)

#ifdef _WIN32
#define vsnprintf _vsnprintf
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

void defaultMessageHandler(const enum jtag_lib_v2::eMsgType eType, const char* szMsg)
{
	if (eType == jtag_lib_v2::MSG_DEBUG && !jtag_lib_v2::jtag_logger::isDebugEnabled())
		return;
	if (szMsg) {
		const char* szType = jtag_lib_v2::jtag_logger::getMessageType(eType);
		if (eType == jtag_lib_v2::MSG_NONE)
			fprintf(stdout, "%s", szMsg);
		else
			fprintf(stdout, LIB_NAME "::%s: %s", szType, szMsg);
	}
}

char* jtag_lib_v2::jtag_logger::getMessageType(const enum jtag_lib_v2::eMsgType eType)
{
	switch (eType) {
		case jtag_lib_v2::MSG_ERROR:
			return (char*) "Error";
		case jtag_lib_v2::MSG_WARNING:
			return (char*) "Warning";
		case jtag_lib_v2::MSG_INFO:
			return (char*) "Info";
		case jtag_lib_v2::MSG_DEBUG:
			return (char*) "Debug";
		default:
			return (char*) "";
	}
}

jtag_lib_v2::MESSAGE_HANDLER* jtag_lib_v2::jtag_logger::pMsgHandler = defaultMessageHandler;
bool jtag_lib_v2::jtag_logger::bEnableDebug = false;

void jtag_lib_v2::jtag_logger::sendMessage(
	const enum jtag_lib_v2::eMsgType eType, const char* szFormat, ...)
{
	char szMsg[MAX_MSG_LENGTH];
	va_list ap;
	bool bTruncated = false;
	va_start(ap, szFormat);
	if (vsnprintf(szMsg, MAX_MSG_LENGTH, szFormat, ap) == MAX_MSG_LENGTH)
		bTruncated = true;
	va_end(ap);
	jtag_lib_v2::jtag_logger::pMsgHandler(eType, szMsg);
	if (bTruncated) {
		sprintf(szMsg, "Message was truncated to %d characters.\n", MAX_MSG_LENGTH - 1);
		jtag_logger::pMsgHandler(MSG_DEBUG, szMsg);
	}
}

void jtag_lib_v2::jtag_logger::setMessageHandler(MESSAGE_HANDLER* pHandler)
{
	if (!pHandler)
		jtag_lib_v2::jtag_logger::pMsgHandler = defaultMessageHandler;
	else
		jtag_lib_v2::jtag_logger::pMsgHandler = pHandler;
}
