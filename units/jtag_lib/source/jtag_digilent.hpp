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
// Filename          :   jtag_digilent.hpp
// Project Name      :   JTAG Library V2
// Description       :   Digilent JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_DIGILENT_HPP__
#define __JTAG_DIGILENT_HPP__

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#endif

#include "djtg.h"
#include "dmgr.h"
#include "dpcdecl.h"

#include "jtag_extern.h"
#include "jtag_if.hpp"
#include "jtag_state.hpp"
#include "jtag_stdint.h"

#include <vector>

namespace jtag_lib_v2 {

class jtag_digilent : public jtag_if
{
private:
	HIF m_cableHandle;
	uint32_t m_uiDelay;
	bool m_bPinStateSupported;
	bool m_bSpeedSupported;
	uint8_t m_auiTxBuffer[256];
	uint8_t m_auiRxBuffer[256];
	uint16_t m_uiBitcount;
	jtag_state m_jtagState;

	jtag_digilent(const jtag_digilent&);
	bool flushCable(const bool);

public:
	inline virtual bool createSession() { return this->createSession(0, 0); }

	jtag_digilent();
	virtual ~jtag_digilent();
	virtual bool createSession(const char*, const uint16_t);
	virtual bool enumCables(uint8_t&, const bool);
	virtual bool openCable(const uint8_t, const uint32_t, const bool);
	virtual bool closeCable();
	virtual bool setPin(const enum eJtagPin, const bool);
	virtual bool getPin(const enum eJtagPin, bool&);
	virtual bool pulsePin(const eJtagPin, const uint16_t);
	virtual bool shiftDeviceIR(const uint8_t, uint8_t*, uint32_t);
	virtual bool shiftChainIR(uint8_t*, uint32_t);
	virtual bool shiftDeviceDR(const uint8_t, uint8_t*, uint8_t*, uint32_t, const enum eScanMode);
	virtual bool shiftChainDR(uint8_t*, uint8_t*, uint32_t, const enum eScanMode);
	virtual bool navigate(const enum eJtagState, const uint16_t);
	virtual bool setJtagFrequency(const uint32_t);
	virtual bool flushCable();
};

class digilent_wrapper
{
private:
	typedef BOOL (*DmgrGetVersion_t)(char*);
	typedef BOOL (*DmgrSzFromErc_t)(ERC, char*, char*);
	typedef BOOL (*DmgrOpen_t)(HIF*, char*);
	typedef BOOL (*DmgrClose_t)(HIF);
	typedef BOOL (*DmgrEnumDevicesEx_t)(int*, DTP, DTP, DINFO, void*);
	typedef BOOL (*DmgrGetDvc_t)(int, DVC*);
	typedef BOOL (*DmgrFreeDvcEnum_t)();

	typedef BOOL (*DjtgGetVersion_t)(char*);
	typedef BOOL (*DjtgGetPortCount_t)(HIF, INT32*);
	typedef BOOL (*DjtgGetPortProperties_t)(HIF, INT32, DWORD*);
	typedef BOOL (*DjtgEnableEx_t)(HIF, INT32);
	typedef BOOL (*DjtgDisable_t)(HIF);
	typedef BOOL (*DjtgSetSpeed_t)(HIF, DWORD, DWORD*);
	typedef BOOL (*DjtgSetTmsTdiTck_t)(HIF, BOOL, BOOL, BOOL);
	typedef BOOL (*DjtgGetTmsTdiTdoTck_t)(HIF, BOOL*, BOOL*, BOOL*, BOOL*);
	typedef BOOL (*DjtgPutTmsTdiBits_t)(HIF, BYTE*, BYTE*, DWORD, BOOL);

	HMODULE m_hDmgrModule;
	HMODULE m_hDjtgModule;

	HMODULE loadDigilentLib(const TCHAR*);
	digilent_wrapper();

public:
	~digilent_wrapper();

	static digilent_wrapper& getInstance();

	DmgrGetVersion_t DmgrGetVersion;
	DmgrSzFromErc_t DmgrSzFromErc;
	DmgrOpen_t DmgrOpen;
	DmgrClose_t DmgrClose;
	DmgrEnumDevicesEx_t DmgrEnumDevicesEx;
	DmgrGetDvc_t DmgrGetDvc;
	DmgrFreeDvcEnum_t DmgrFreeDvcEnum;

	DjtgGetVersion_t DjtgGetVersion;
	DjtgGetPortCount_t DjtgGetPortCount;
	DjtgGetPortProperties_t DjtgGetPortProperties;
	DjtgEnableEx_t DjtgEnableEx;
	DjtgDisable_t DjtgDisable;
	DjtgSetSpeed_t DjtgSetSpeed;
	DjtgSetTmsTdiTck_t DjtgSetTmsTdiTck;
	DjtgGetTmsTdiTdoTck_t DjtgGetTmsTdiTdoTck;
	DjtgPutTmsTdiBits_t DjtgPutTmsTdiBits;

	bool loadDigilentJtagLib();
	bool loadDigilentMgrLib();
};

} // namespace jtag_lib_v2

#endif // __JTAG_DIGILENT_HPP__
