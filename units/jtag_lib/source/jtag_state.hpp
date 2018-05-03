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
// Filename          :   jtag_state.cpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG state machine implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_STATE_HPP__
#define __JTAG_STATE_HPP__

#include "jtag_stdint.h"

namespace jtag_lib_v2 {

enum eJtagState
{
	TEST_LOGIC_RESET = 0,
	RUN_TEST_IDLE,
	SELECT_DR_SCAN,
	CAPTURE_DR,
	SHIFT_DR,
	EXIT1_DR,
	EXIT2_DR,
	PAUSE_DR,
	UPDATE_DR,
	SELECT_IR_SCAN,
	CAPTURE_IR,
	SHIFT_IR,
	EXIT1_IR,
	EXIT2_IR,
	PAUSE_IR,
	UPDATE_IR,
	UNKNOWN_JTAG_STATE
};

class jtag_state
{
private:
	typedef struct
	{
		uint8_t uiLength;
		uint8_t uiPattern;
	} tms_pattern_t;

	const static tms_pattern_t m_cTMSPattern[16][16];

	enum eJtagState m_eCurrentState;

public:
	jtag_state();

	bool getTmsPatternForState(const enum eJtagState, uint16_t&, uint8_t&);
	bool setJtagState(const enum eJtagState);
	inline enum eJtagState getJtagState() { return this->m_eCurrentState; }
};

} // namespace jtag_lib_v2

#endif // #ifdef __JTAG_STATE_HPP__
