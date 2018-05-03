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
// Filename          :   jtag_logger.hpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG library logging implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_LOGGER_HPP__
#define __JTAG_LOGGER_HPP__

namespace jtag_lib_v2 {
enum eMsgType
{
	MSG_ERROR,
	MSG_WARNING,
	MSG_INFO,
	MSG_DEBUG,
	MSG_NONE
};

typedef void MESSAGE_HANDLER(const enum eMsgType, const char*);

class jtag_logger
{
private:
	static MESSAGE_HANDLER* pMsgHandler;
	static bool bEnableDebug;
	jtag_logger() {}

public:
	static char* getMessageType(const enum jtag_lib_v2::eMsgType);
	static void sendMessage(const enum eMsgType, const char*, ...);
	static void setMessageHandler(MESSAGE_HANDLER*);
	static inline bool isDebugEnabled() { return jtag_logger::bEnableDebug; }
	static inline void enableDebug() { jtag_logger::bEnableDebug = true; }
	static inline void disableDebug() { jtag_logger::bEnableDebug = false; }
};
}

#endif // __JTAG_LOGGER_HPP__
