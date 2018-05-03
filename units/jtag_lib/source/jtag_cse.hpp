//-----------------------------------------------------------------
//
// Copyright (c) 2011 Xilinx, Inc.  All rights reserved.
// Copyright (c) 2012 TU-Dresden  All rights reserved.
//
//                 XILINX CONFIDENTIAL PROPERTY
// This   document  contains  proprietary information  which   is
// protected by  copyright. All rights  are reserved. No  part of
// this  document may be photocopied, reproduced or translated to
// another  program  language  without  prior written  consent of
// XILINX Inc., San Jose, CA. 95124
//
//-----------------------------------------------------------------

//-----------------------------------------------------------------
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
// Filename          :   jtag_cse.hpp
// Project Name      :   JTAG Library V2
// Description       :   Xilinx CSE JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_CSE_HPP__
#define __JTAG_CSE_HPP__

#include "CseFpgaAPI.h"
#include "CseJtagAPI.h"

#include "jtag_extern.h"
#include "jtag_if.hpp"
#include "jtag_state.hpp"
#include "jtag_stdint.h"

#include <vector>

namespace jtag_lib_v2 {

enum eXilinxTarget
{
	XILINX_JTAG_USB = 0,
	XILINX_JTAG_LPT,
	XILINX_JTAG_DIGILENT
};

class jtag_cse : public jtag_if
{
private:
	CSE_HANDLE m_cseHandle;
	bool m_bCableLocked;
	enum eXilinxTarget m_eJtagTarget;
	static bool m_bDisableOutput;

	jtag_cse(const jtag_cse&);
	bool createSession(const char**, const int);
	uint32_t binaryStringToInt(char*);
	bool checkCableLock(const char*);
	static void CALLBACK writeMessage(CSE_HANDLE, uint32_t, const char*);
	uint32_t getFrequency(const uint32_t);
	inline bool getCseScanMode(const enum eScanMode eMode, unsigned int& uiCseMode)
	{
		switch (eMode) {
			case SCAN_MODE_READ:
				uiCseMode = CSEJTAG_SHIFT_READ;
				break;
			case SCAN_MODE_WRITE:
				uiCseMode = CSEJTAG_SHIFT_WRITE;
				break;
			case SCAN_MODE_READ_WRITE:
				uiCseMode = CSEJTAG_SHIFT_READWRITE;
				break;
			default:
				return false;
		}
		return true;
	}

	inline bool getCsePin(const enum eJtagPin ePin, enum CSEJTAG_PIN& eCsePin)
	{
		switch (ePin) {
			case JTAG_PIN_TCK:
				eCsePin = CSEJTAG_TCK;
				break;
			case JTAG_PIN_TDI:
				eCsePin = CSEJTAG_TDI;
				break;
			case JTAG_PIN_TMS:
				eCsePin = CSEJTAG_TMS;
				break;
			case JTAG_PIN_TDO:
				eCsePin = CSEJTAG_TDO;
				break;
			default:
				return false;
		}
		return true;
	}

public:
	inline virtual bool createSession() { return this->createSession(0, 0); }

	jtag_cse(const enum eXilinxTarget eJtagTarget = XILINX_JTAG_USB);
	~jtag_cse();
	virtual bool createSession(const char*, const uint16_t);
	virtual bool closeSession();
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
	virtual bool setJtagFrequency(const uint32_t);
	virtual bool flushCable();

	virtual bool getDeviceIRLength(const uint8_t, uint8_t&);
	virtual bool interrogateChain();
	virtual bool setDeviceCount(const uint8_t);
	virtual bool getDeviceCount(uint8_t&);
	virtual bool setDeviceInfo(const uint8_t, const uint8_t, uint32_t);
	virtual bool navigate(const enum eJtagState, const uint16_t);
	virtual bool getDeviceInfo(const uint8_t, uint32_t&, char*);
	virtual bool addDeviceData(const char*, const char*, const uint32_t);
	virtual bool configureDevice(const uint8_t, uint8_t*, const uint32_t);
	virtual bool isSysMonSupported(const uint8_t, bool&);
	virtual bool getSysMonRegister(const uint8_t, const uint8_t, uint32_t&);
};

class cse_wrapper
{
private:
	typedef CSE_RETURN (*jtagSessionCreate_t)(CSE_MSG_RTR, unsigned int, const char**, CSE_HANDLE*);
	typedef CSE_RETURN (*jtagSessionSendMessage_t)(CSE_HANDLE, unsigned int, const char*);
	typedef CSE_RETURN (*jtagSessionDestroy_t)(CSE_HANDLE);
	typedef CSE_RETURN (*jtagTargetUnlock_t)(CSE_HANDLE);
	typedef CSE_RETURN (*jtagTargetLock_t)(CSE_HANDLE, int, enum CSEJTAG_TARGET_LOCK_STATUS*);
	typedef CSE_RETURN (*jtagTargetCleanLocks_t)(CSE_HANDLE);
	typedef CSE_RETURN (*jtagTargetClose_t)(CSE_HANDLE);
	typedef CSE_RETURN (*jtagTargetOpen_t)(
		CSE_HANDLE,
		const char*,
		CSE_PROGRESS_CB,
		unsigned int,
		const char**,
		struct CSEJTAG_TARGET_INFO*);
	typedef CSE_RETURN (*jtagTargetFlush_t)(CSE_HANDLE);
	typedef CSE_RETURN (*jtagTargetSetPin_t)(CSE_HANDLE, CSEJTAG_PIN, unsigned int);
	typedef CSE_RETURN (*jtagTargetGetPin_t)(CSE_HANDLE, CSEJTAG_PIN, unsigned int*);
	typedef CSE_RETURN (*jtagTargetPulsePin_t)(CSE_HANDLE, CSEJTAG_PIN, unsigned int);
	typedef CSE_RETURN (*jtagTargetSetOption_t)(CSE_HANDLE, const char*, const char*);
	typedef CSE_RETURN (*jtagTargetGetOption_t)(CSE_HANDLE, const char*, char*, unsigned int*);
	typedef CSE_RETURN (*jtagTargetGetLockStatus_t)(CSE_HANDLE, enum CSEJTAG_TARGET_LOCK_STATUS*);
	typedef CSE_RETURN (*jtagTapAutodetectChain_t)(CSE_HANDLE, unsigned int);
	typedef CSE_RETURN (*jtagTapShiftDeviceIR_t)(
		CSE_HANDLE,
		unsigned int,
		unsigned int,
		enum CSEJTAG_STATE,
		CSE_PROGRESS_CB,
		unsigned int,
		const unsigned char*,
		unsigned char*,
		const unsigned char*,
		const unsigned char*);
	typedef CSE_RETURN (*jtagTapShiftDeviceDR_t)(
		CSE_HANDLE,
		unsigned int,
		unsigned int,
		enum CSEJTAG_STATE,
		CSE_PROGRESS_CB,
		unsigned int,
		const unsigned char*,
		unsigned char*,
		const unsigned char*,
		const unsigned char*);
	typedef CSE_RETURN (*jtagTapShiftChainIR_t)(
		CSE_HANDLE,
		unsigned int,
		enum CSEJTAG_STATE,
		CSE_PROGRESS_CB,
		unsigned int,
		const unsigned char*,
		unsigned char*,
		const unsigned char*,
		const unsigned char*);
	typedef CSE_RETURN (*jtagTapShiftChainDR_t)(
		CSE_HANDLE,
		unsigned int,
		enum CSEJTAG_STATE,
		CSE_PROGRESS_CB,
		unsigned int,
		const unsigned char*,
		unsigned char*,
		const unsigned char*,
		const unsigned char*);
	typedef CSE_RETURN (*jtagTapGetIRLength_t)(CSE_HANDLE, unsigned int, unsigned int*);
	typedef CSE_RETURN (*jtagTapNavigate_t)(
		CSE_HANDLE, enum CSEJTAG_STATE, unsigned int, unsigned int);
	typedef CSE_RETURN (*jtagTapSetDeviceCount_t)(CSE_HANDLE, unsigned int);
	typedef CSE_RETURN (*jtagTapGetDeviceCount_t)(CSE_HANDLE, unsigned int*);
	typedef CSE_RETURN (*jtagTapSetDeviceIdcode_t)(CSE_HANDLE, unsigned int, char*);
	typedef CSE_RETURN (*jtagTapSetIRLength_t)(CSE_HANDLE, unsigned int, unsigned int);
	typedef CSE_RETURN (*jtagTapGetDeviceIdcode_t)(CSE_HANDLE, unsigned int, char*, unsigned int);
	typedef CSE_RETURN (*jtagDbGetDeviceNameForIdcode_t)(
		CSE_HANDLE, const char*, char*, unsigned int);
	typedef CSE_RETURN (*jtagDbAddDeviceData_t)(CSE_HANDLE, const char*, const char*, unsigned int);
	typedef CSE_RETURN (*fpgaConfigureDevice_t)(
		CSE_HANDLE,
		unsigned int,
		const char*,
		unsigned int,
		const char*,
		unsigned char*,
		unsigned int,
		enum CSE_CONFIG_STATUS*,
		CSE_PROGRESS_CB);
	typedef CSE_RETURN (*fpgaConfigureDevice12_t)(
		CSE_HANDLE,
		unsigned int,
		const char*,
		unsigned int,
		unsigned char*,
		unsigned int,
		enum CSE_CONFIG_STATUS*,
		CSE_PROGRESS_CB);
	typedef CSE_RETURN (*fpgaIsSysMonSupported_t)(CSE_HANDLE, const char*, unsigned int*);
	typedef CSE_RETURN (*fpgaGetSysMonReg_t)(CSE_HANDLE, unsigned int, unsigned int, unsigned int*);

	HMODULE m_hJtagModule;
	HMODULE m_hFpgaModule;
	uint32_t m_uiMajorVersion;

	FARPROC fpgaConfigureDeviceProc;

	HMODULE loadCseLib(const TCHAR*);
	bool checkAPIVersion();
	cse_wrapper();

public:
	~cse_wrapper();

	static cse_wrapper& getInstance();

	CSE_RETURN fpgaConfigureDevice(
		CSE_HANDLE,
		unsigned int,
		const char*,
		unsigned char*,
		unsigned int,
		enum CSE_CONFIG_STATUS*,
		CSE_PROGRESS_CB);

	fpgaIsSysMonSupported_t fpgaIsSysMonSupported;
	fpgaGetSysMonReg_t fpgaGetSysMonReg;

	jtagSessionCreate_t jtagSessionCreate;
	jtagSessionSendMessage_t jtagSessionSendMessage;
	jtagSessionDestroy_t jtagSessionDestroy;
	jtagTargetUnlock_t jtagTargetUnlock;
	jtagTargetLock_t jtagTargetLock;
	jtagTargetCleanLocks_t jtagTargetCleanLocks;
	jtagTargetClose_t jtagTargetClose;
	jtagTargetOpen_t jtagTargetOpen;
	jtagTargetFlush_t jtagTargetFlush;
	jtagTargetSetPin_t jtagTargetSetPin;
	jtagTargetGetPin_t jtagTargetGetPin;
	jtagTargetPulsePin_t jtagTargetPulsePin;
	jtagTargetSetOption_t jtagTargetSetOption;
	jtagTargetGetOption_t jtagTargetGetOption;
	jtagTargetGetLockStatus_t jtagTargetGetLockStatus;
	jtagTapAutodetectChain_t jtagTapAutodetectChain;
	jtagTapShiftDeviceIR_t jtagTapShiftDeviceIR;
	jtagTapShiftDeviceDR_t jtagTapShiftDeviceDR;
	jtagTapShiftChainIR_t jtagTapShiftChainIR;
	jtagTapShiftChainDR_t jtagTapShiftChainDR;
	jtagTapGetIRLength_t jtagTapGetIRLength;
	jtagTapNavigate_t jtagTapNavigate;
	jtagTapSetDeviceCount_t jtagTapSetDeviceCount;
	jtagTapGetDeviceCount_t jtagTapGetDeviceCount;
	jtagTapSetDeviceIdcode_t jtagTapSetDeviceIdcode;
	jtagTapSetIRLength_t jtagTapSetIRLength;
	jtagTapGetDeviceIdcode_t jtagTapGetDeviceIdcode;
	jtagDbGetDeviceNameForIdcode_t jtagDbGetDeviceNameForIdcode;
	jtagDbAddDeviceData_t jtagDbAddDeviceData;

	bool loadCseJtagLib();
	bool loadCseFpgaLib();
	bool freeCseJtagLib();
	bool freeCseFpgaLib();
};

} // namespace jtag_lib_v2

#endif // __JTAG_CSE_HPP__
