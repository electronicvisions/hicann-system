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
// Filename          :   jtag_if.hpp
// Project Name      :   JTAG Library V2
// Description       :   JTAG base interface implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_IF_HPP__
#define __JTAG_IF_HPP__

#include "jtag_state.hpp"
#include "jtag_stdint.h"

#include <memory>
#include <vector>

#ifdef NCSIM
#include "ARQStreamWrap.h"
#else
#include "sctrltp/ARQStream.h"
#endif // NCSIM

namespace jtag_lib_v2 {

enum eJtagPin
{
	JTAG_PIN_TCK,
	JTAG_PIN_TDI,
	JTAG_PIN_TMS,
	JTAG_PIN_TDO
};

enum eScanMode
{
	SCAN_MODE_READ,
	SCAN_MODE_WRITE,
	SCAN_MODE_READ_WRITE
};

class cable_info
{
private:
	char* m_szSerial;
	char* m_szPort;
	bool m_bLocked;
	char* m_szPlugin;
	uint8_t m_uiNumPorts;

public:
	cable_info();
	cable_info(cable_info&);
	~cable_info();
#if __cplusplus >= 201103L
	cable_info& operator=(cable_info const&) = default;
#endif
	bool setSerial(const char*);
	bool setPort(const char*);
	void setLocked(const bool);
	bool setPlugin(const char*);
	void setPortNumber(const uint8_t);

	inline char* getSerial() { return this->m_szSerial; }
	inline char* getPort() { return this->m_szPort; }
	inline bool isLocked() { return this->m_bLocked; }
	inline char* getPlugin() { return this->m_szPlugin; }
	inline uint8_t getPortNumber() { return this->m_uiNumPorts; }
};

class jtag_if
{
private:
	std::vector<cable_info*> m_connectedCables;
	bool m_bEnumDone;
	bool m_bSessionOpen;
	bool m_bCableOpen;

protected:
	const char* getJtagPinName(const enum eJtagPin);
	void freeCableList();
	void addCableInfo(cable_info*);
	void markEnumDone();
	void setSessionOpen(const bool);
	void setCableOpen(const bool);
	bool getCableToOpen(const uint8_t, cable_info**, const bool);
	inline bool isSessionOpen() { return this->m_bSessionOpen; }

public:
	virtual bool getCableInfo(const uint8_t, cable_info&);
	virtual bool createSession(const char*, const uint16_t) = 0;
	virtual bool createSession(std::shared_ptr<sctrltp::ARQStream>);
	virtual bool closeSession();
	virtual bool enumCables(uint8_t&, const bool) = 0;
	virtual bool openCable(const uint8_t, const uint32_t, const bool) = 0;
	virtual bool closeCable() = 0;
	virtual bool setPin(const enum eJtagPin, const bool) = 0;
	virtual bool getPin(const enum eJtagPin, bool&) = 0;
	virtual bool pulsePin(const enum eJtagPin, const uint16_t) = 0;
	virtual bool shiftDeviceIR(const uint8_t, uint8_t*, uint32_t) = 0;
	virtual bool shiftChainIR(uint8_t*, uint32_t) = 0;
	virtual bool shiftDeviceDR(
		const uint8_t, uint8_t*, uint8_t*, uint32_t, const enum eScanMode) = 0;
	virtual bool shiftChainDR(uint8_t*, uint8_t*, uint32_t, const enum eScanMode) = 0;
	virtual bool navigate(const enum eJtagState, const uint16_t) = 0;
	virtual bool flushCable() = 0;
	virtual bool setJtagFrequency(const uint32_t) = 0;

	virtual inline bool isCableOpen() { return this->m_bCableOpen; }
	inline bool getPin(bool& bValue) { return this->getPin(JTAG_PIN_TDO, bValue); }
	inline bool pulsePin(const enum eJtagPin ePin) { return this->pulsePin(ePin, 1); }
	jtag_if();
	virtual ~jtag_if();
};

} // namespace jtag_lib_v2

#endif // #ifndef __JTAG_IF_HPP__
