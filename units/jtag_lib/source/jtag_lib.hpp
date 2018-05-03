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
// Filename          :   jtag_lib_v2.hpp
// Project Name      :   JTAG Library V2
// Description       :   High-Level JTAG library implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_LIB_V2_HPP__
#define __JTAG_LIB_V2_HPP__

#include <vector>
#include "jtag_stdint.h"

namespace jtag_lib_v2 {

#define MAX_DEVICENAME_LEN 128

enum eJtagInterface
{
	XILINX_USB,
	XILINX_LPT,
	XILINX_DIGILENT,
	JTAG_DIGILENT,
	JTAG_ETHERNET,
	JTAG_SYSTEMC,
	JTAG_ETHERNET_SYSTEMC
};

class jtag_access;

class jtag_lib
{
public:
	jtag_lib();
	~jtag_lib();
	bool initJtag(const enum eJtagInterface);
	bool createSession(const char*, const uint16_t);
	bool openCable(const uint8_t, const uint32_t, const bool);
	bool closeCable();
	bool flushCable();
	bool enumCables(uint8_t&, const bool bEnumOther = true);
	bool autodetectChain();
	bool resetJtag();
	bool setJtagFrequency(const uint32_t);
	bool setDeviceCount(const uint8_t);
	bool getDeviceCount(uint8_t&);
	bool setDeviceInfo(const uint8_t, const uint8_t, uint32_t);
	bool getDeviceInfo(const uint8_t, uint32_t&, char*);
	bool addDeviceData(const char*);
	bool addDeviceData(const char*, const uint32_t, const uint8_t);
	bool configureDevice(const uint8_t, uint8_t*, const uint32_t);
	bool isSysMonSupported(const uint8_t, bool&);
	bool getSysMonRegister(const uint8_t, const uint8_t, uint32_t&);

	bool setJtagInstr(const uint8_t, const uint8_t);
	bool setJtagInstr(const uint8_t, const uint16_t);
	bool setJtagInstr(const uint8_t, const uint32_t);
	bool setJtagInstr(const uint8_t, const uint64_t);

	bool setJtagData(const uint8_t, const uint8_t, const uint8_t, const bool bFlush = false);
	bool setJtagData(const uint8_t, const uint16_t, const uint8_t, const bool bFlush = false);
	bool setJtagData(const uint8_t, const uint32_t, const uint8_t, const bool bFlush = false);
	bool setJtagData(const uint8_t, const uint64_t, const uint8_t, const bool bFlush = false);
	bool setJtagData(
		const uint8_t, const std::vector<bool>&, const uint8_t, const bool bFlush = false);

	bool getJtagData(const uint8_t, uint8_t&, const uint8_t);
	bool getJtagData(const uint8_t, uint16_t&, const uint8_t);
	bool getJtagData(const uint8_t, uint32_t&, const uint8_t);
	bool getJtagData(const uint8_t, uint64_t&, const uint8_t);
	bool getJtagData(const uint8_t, std::vector<bool>&, const uint16_t);

	bool setGetJtagData(const uint8_t, const uint8_t, uint8_t&, const uint8_t);
	bool setGetJtagData(const uint8_t, const uint16_t, uint16_t&, const uint8_t);
	bool setGetJtagData(const uint8_t, const uint32_t, uint32_t&, const uint8_t);
	bool setGetJtagData(const uint8_t, const uint64_t, uint64_t&, const uint8_t);
	bool setGetJtagData(
		const uint8_t, std::vector<bool> const&, std::vector<bool>&, const uint16_t);


	inline bool setJtagInstr(const uint8_t uiDevice, const int uiInstr)
	{
		return this->setJtagInstr(uiDevice, uint32_t(uiInstr));
	}

	inline bool setJtagData(
		const uint8_t uiDevice, const int uiData, const uint8_t uiLength, const bool bFlush = false)
	{
		return this->setJtagData(uiDevice, uint32_t(uiData), uiLength, bFlush);
	}

	inline bool createSession() { return this->createSession(0, 0); }

	inline bool openCable(const uint8_t uiIndex) { return this->openCable(uiIndex, 0, false); }

	inline bool openCable() { return this->openCable(0, 0, false); }

	inline bool openCable(bool bForceUnlock) { return this->openCable(0, 0, bForceUnlock); }

	inline bool setJtagInstr(const uint8_t uiInstr) { return this->setJtagInstr(0, uiInstr); }
	inline bool setJtagInstr(const uint16_t uiInstr) { return this->setJtagInstr(0, uiInstr); }
	inline bool setJtagInstr(const uint32_t uiInstr) { return this->setJtagInstr(0, uiInstr); }
	inline bool setJtagInstr(const uint64_t uiInstr) { return this->setJtagInstr(0, uiInstr); }

	inline bool setJtagData(const uint8_t uiData, const uint8_t uiLength)
	{
		return this->setJtagData(0, uiData, uiLength);
	}
	inline bool setJtagData(const uint16_t uiData, const uint8_t uiLength)
	{
		return this->setJtagData(0, uiData, uiLength);
	}
	inline bool setJtagData(const uint32_t uiData, const uint8_t uiLength)
	{
		return this->setJtagData(0, uiData, uiLength);
	}
	inline bool setJtagData(const uint64_t uiData, const uint8_t uiLength)
	{
		return this->setJtagData(0, uiData, uiLength);
	}

	inline bool getJtagData(uint8_t& uiData, const uint8_t uiLength)
	{
		return this->getJtagData(0, uiData, uiLength);
	}
	inline bool getJtagData(uint16_t& uiData, const uint8_t uiLength)
	{
		return this->getJtagData(0, uiData, uiLength);
	}
	inline bool getJtagData(uint32_t& uiData, const uint8_t uiLength)
	{
		return this->getJtagData(0, uiData, uiLength);
	}
	inline bool getJtagData(uint64_t& uiData, const uint8_t uiLength)
	{
		return this->getJtagData(0, uiData, uiLength);
	}

private:
	jtag_access* m_pJtagAccess;
};

} // namespace jtag_lib_v2

#endif // #ifdef __JTAG_LIB_V2_HPP__
