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
// Filename          :   jtag_targets.hpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG Target handling
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_TARGETS_HPP__
#define __JTAG_TARGETS_HPP__

#include "jtag_stdint.h"

#include <cstddef>
#include <vector>

namespace jtag_lib_v2 {

#ifndef MAX_DEVICENAME_LEN
#define MAX_DEVICENAME_LEN 128
#endif

class jtag_targets
{
private:
	typedef struct target_info
	{
		char szIdcode[32];
		uint8_t uiIRLength;
		char szDeviceName[MAX_DEVICENAME_LEN];
	} target_info_t;

	std::vector<target_info_t*> m_jtagTargets;
	std::vector<target_info_t*> m_tempJtagTargets;
	const char* m_szFile;
	target_info_t m_targetInfo;
	uint32_t m_uiLine;
	uint32_t m_uiLinePos;
	int16_t m_uiNumber;
	uint8_t m_uiNumberPos;

	jtag_targets(const jtag_targets&);
	void unexpectedCharError(const char);
	void parserError(const char*);
	bool parseIdcode(const char, const bool, bool&);
	bool parseNumber(const char, bool&);
	bool parseDeviceName(const char, bool&);
	bool insertTarget();
	static bool targetInfoCmp(jtag_targets::target_info_t*, jtag_targets::target_info_t*);

public:
	jtag_targets();
	virtual ~jtag_targets();
	bool addDeviceData(const char*, char*, const size_t);
	bool getDeviceInfoForIdcode(const uint32_t uiIdcode, uint8_t&, char**);
};

} // namespace jtag_lib_v2

#endif // __JTAG_TARGETS_HPP__
