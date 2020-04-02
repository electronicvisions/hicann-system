//-----------------------------------------------------------------
//
// Copyright (c) 2015 TU-Dresden  All rights reserved.
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
// Filename          :   jtag_access.hpp
// Project Name      :   JTAG Library V2
// Description       :   High-Level JTAG access implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_ACCESS_HPP__
#define __JTAG_ACCESS_HPP__

#include "jtag_if.hpp"
#include "jtag_state.hpp"
#include "jtag_stdint.h"
#include "jtag_targets.hpp"

#include <vector>

namespace jtag_lib_v2 {

#ifdef NO_XILINX_LIB
#undef NO_XILINX_USB
#undef NO_XILINX_LPT
#undef NO_XILINX_DIGILENT

#define NO_XILINX_USB
#define NO_XILINX_LPT
#define NO_XILINX_DIGILENT
#endif

#if defined(NO_XILINX_USB) && defined(NO_XILINX_LPT) && defined(NO_XILINX_DIGILENT)
#undef NO_XILINX_LIB
#define NO_XILINX_LIB
#endif

#ifdef _WIN32
#undef NO_SYSTEMC_LIB
#define NO_SYSTEMC_LIB
#ifdef _MSC_VER
#if _MS_VER < 1300
#undef NO_ETHERNET_LIB
#define NO_ETHERNET_LIB
#endif
#endif
#endif


#ifndef JTAG_LIB_V2

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

#endif

class jtag_access
{
private:
	typedef struct device_info
	{
		bool bValid;
		uint32_t uiIdcode;
		uint8_t uiIRLength;
		char* szDeviceName;
	} device_info_t;

	jtag_if* m_pJtagIf;
	jtag_targets m_jtagTargets;
	uint16_t m_uiDeviceCount;
	enum eJtagInterface m_eJtagInterface;
	device_info_t m_jtagDevices[256];

	bool getDeviceIRLength(const uint8_t, uint8_t&);
	bool checkCable(const bool bIfOnly = false);
	bool doTLRAutodetect();
	bool isChainValid(const char*);
	bool isDeviceValid(const char*, const uint8_t);
	bool getBypassBits(const uint8_t, uint16_t&, uint16_t&);

	jtag_access(const jtag_access&) {}

protected:
	template <typename T>
	void formatDataWrite(
		const T& uiData,
		const uint16_t uiLength,
		std::vector<uint8_t>& auiTDIData,
		const uint16_t uiPreShiftBits = 0,
		const uint16_t uiPostShiftBits = 0,
		const bool bBypass = false)
	{
		auiTDIData.clear();
		const uint8_t uiMaxDataBits = sizeof(T) << 3;
		uint16_t uiDataBits = (uiMaxDataBits < uiLength) ? uiMaxDataBits : uiLength;
		T uiIntData = uiData;
		if (uiDataBits < uiMaxDataBits)
			uiIntData &= ((static_cast<T>(1) << uiDataBits) - 1);
		const uint32_t uiBits = uiLength + uiPreShiftBits + uiPostShiftBits;
		uint16_t uiPreBits = uiPreShiftBits;
		uint16_t uiLastBits = (uiLength + uiPreShiftBits) & 0x7;
		uiDataBits = uiLength;
		for (uint16_t i = 0; i<(uiBits + 7)>> 3; ++i) {
			uint8_t uiByte = (bBypass) ? 0xFF : 0x00;
			if (uiPreBits > 8)
				uiPreBits -= 8;
			else if (uiPreBits) {
				uiByte = uiIntData & ((1 << (8 - uiPreBits)) - 1);
				uiByte <<= uiPreBits;
				if (bBypass)
					uiByte |= ((1 << uiPreBits) - 1);
				uiIntData >>= (8 - uiPreBits);
				if (uiDataBits > (8 - uiPreBits))
					uiDataBits -= (8 - uiPreBits);
				else
					uiDataBits = 0;
				uiPreBits = 0;
			} else if (uiDataBits) {
				uiByte = uiIntData & 0xFF;
				uiIntData >>= 8;
				if (uiDataBits > 8)
					uiDataBits -= 8;
				else
					uiDataBits = 0;
			}

			if (!uiDataBits && !uiPreBits && uiLastBits && bBypass) {
				uiByte |= (((1 << (8 - uiLastBits)) - 1) << uiLastBits);
				uiLastBits = 0;
			}

			auiTDIData.push_back(uiByte);
		}
	}

	template <typename T>
	void formatDataRead(
		uint8_t* puiTDOData, const uint16_t uiLength, T& uiData, const uint8_t uiPreShiftBits = 0)
	{
		const uint8_t uiMaxDataBits = sizeof(T) << 3;
		uint16_t uiDataBits = (uiMaxDataBits < uiLength) ? uiMaxDataBits : uiLength;
		uint8_t uiBytes = (uiDataBits + 7) >> 3;
		const uint8_t uiPreBytes = uiPreShiftBits >> 3;
		uiData = 0;
		const uint8_t uiPreBits = uiPreShiftBits & 0x7;
		if (((uiPreShiftBits + uiDataBits + 7) >> 3) > uiBytes)
			uiBytes++;
		uint8_t uiLastBits = 0;
		if (uiBytes > 1)
			uiLastBits = (uiDataBits + uiPreShiftBits - ((uiBytes - 1) << 3));

		for (uint16_t i = 0; i < uiBytes; ++i) {
			if (uiLastBits) {
				uiData |= (puiTDOData[uiBytes - 1 + uiPreBytes] & ((1 << uiLastBits) - 1));
				uiLastBits = 0;
			} else {
				const uint8_t uiIndex = uiBytes - i - 1 + uiPreBytes;
				if (i == (uiBytes - 1) && uiPreBits) {
					const uint8_t uiBits = 8 - uiPreBits;
					uiData <<= uiBits;
					uiData |= ((puiTDOData[uiIndex] >> uiPreBits) & ((1 << uiBits) - 1));
				} else {
					uiData <<= 8;
					uiData |= puiTDOData[uiIndex];
				}
			}
		}
	}

	inline bool isXilinxJtag()
	{
		return (
			this->m_eJtagInterface == XILINX_USB || this->m_eJtagInterface == XILINX_LPT ||
			this->m_eJtagInterface == XILINX_DIGILENT);
	}

	void formatDataWrite(
		std::vector<bool> const&,
		const uint16_t,
		std::vector<uint8_t>&,
		const uint16_t uiPreShiftBits = 0,
		const uint16_t uiPostShiftBits = 0);
	void formatDataRead(
		uint8_t*, const uint16_t, std::vector<bool>&, const uint8_t uiPreShiftBits = 0);

public:
	jtag_access();
	virtual ~jtag_access();
	bool initJtag(const enum eJtagInterface);
	bool createSession(const char*, const uint16_t);
	bool createSession(std::shared_ptr<sctrltp::ARQStream<sctrltp::ParametersFcpBss1>>);
	bool closeSession();
	bool enumCables(uint8_t&, const bool bEnumOther = true);
	bool getCableInfo(const uint8_t, cable_info&);
	bool openCable(const uint8_t, const uint32_t, const bool);
	bool closeCable();
	bool flushCable();
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
	bool navigateState(const enum eJtagState eState, const uint16_t uiClocks = 1);

	template <typename T>
	bool setJtagInstr(const uint8_t uiDevice, const T& uiInstr)
	{
		if (!this->checkCable())
			return false;

		if (!this->isXilinxJtag()) {
			if (!this->isDeviceValid("setJtagInstr", uiDevice))
				return false;
			if (!this->isChainValid("setJtagInstr"))
				return false;
		}

		uint8_t uiLength;
		if (!this->getDeviceIRLength(uiDevice, uiLength))
			return false;

		std::vector<uint8_t> auiData;
		if (this->isXilinxJtag()) {
			this->formatDataWrite(uiInstr, uiLength, auiData);
			return this->m_pJtagIf->shiftDeviceIR(uiDevice, &auiData[0], uiLength);
		}

		uint16_t uiPreShiftBits;
		uint16_t uiPostShiftBits;
		if (!this->getBypassBits(uiDevice, uiPreShiftBits, uiPostShiftBits))
			return false;

		this->formatDataWrite(uiInstr, uiLength, auiData, uiPreShiftBits, uiPostShiftBits, true);
		const uint32_t uiBits = uiPreShiftBits + uiPostShiftBits + uiLength;
		return this->m_pJtagIf->shiftChainIR(&auiData[0], uiBits);
	}

	template <typename T>
	bool setJtagInstrAll(T const* puiInstr)
	{
		if (!this->checkCable())
			return false;

		if (!this->isXilinxJtag()) {
			if (!this->isChainValid("setJtagInstrAll"))
				return false;
		}

		std::vector<uint8_t> auiData;

		auiData.clear();

		uint8_t uiDevCnt;
		if (!this->getDeviceCount(uiDevCnt))
			return false;

		uint32_t uiBits = 0;
		uint8_t uiByte = 0;
		uint8_t uiFree = 8;
		for (int16_t i = uiDevCnt - 1; i >= 0; --i) {
			uint8_t uiLength;
			if (!this->getDeviceIRLength(i, uiLength))
				return false;

			uiBits += uiLength;
			T uiInstr = puiInstr[i];
			do {
				uiByte |= ((static_cast<uint8_t>(uiInstr) & ((1 << uiFree) - 1)) << (8 - uiFree));
				if (uiFree > uiLength) {
					uiFree -= uiLength;
					uiLength = 0;
				} else if (uiFree == uiLength) {
					auiData.push_back(uiByte);
					uiFree = 8;
					uiByte = 0;
					uiLength = 0;
				} else {
					auiData.push_back(uiByte);
					uiByte = 0;
					uiLength -= uiFree;
					uiInstr >>= uiFree;
					uiFree = 8;
				}
			} while (uiLength);
		}

		if (uiFree != 8)
			auiData.push_back(uiByte);

		return this->m_pJtagIf->shiftChainIR(&auiData[0], uiBits);
	}

	template <typename T>
	bool setJtagData(
		const uint8_t uiDevice, const T& uiData, const uint16_t uiLength, const bool bFlush = false)
	{
		if (!this->checkCable())
			return false;

		std::vector<uint8_t> auiData;
		if (!this->isXilinxJtag()) {
			if (!this->isDeviceValid("setJtagData", uiDevice))
				return false;
			if (!this->isChainValid("setJtagData"))
				return false;

			this->formatDataWrite(uiData, uiLength, auiData, 0, uiDevice);
			const uint32_t uiDataLength = uiLength + uiDevice;
			if (!this->m_pJtagIf->shiftChainDR(&auiData[0], 0, uiDataLength, SCAN_MODE_WRITE))
				return false;
		} else {
			this->formatDataWrite(uiData, uiLength, auiData);
			if (!this->m_pJtagIf->shiftDeviceDR(
					uiDevice, &auiData[0], 0, uiLength, SCAN_MODE_WRITE))
				return false;
		}

		if (bFlush)
			return this->flushCable();

		return true;
	}

	bool setJtagData(const uint8_t, std::vector<bool> const&, const uint16_t, const bool);

	inline bool setJtagData(
		const uint8_t uiDevice, std::vector<bool> const& abData, const uint16_t uiLength)
	{
		return this->setJtagData(uiDevice, abData, uiLength, false);
	}


	template <typename T>
	bool setJtagDataAll(T const* puiData, const uint8_t* puiLength, const bool bFlush = false)
	{
		if (!this->checkCable())
			return false;

		if (!this->isXilinxJtag()) {
			if (!this->isChainValid("setJtagDataAll"))
				return false;
		}

		std::vector<uint8_t> auiData;

		auiData.clear();

		uint8_t uiDevCnt;
		if (!this->getDeviceCount(uiDevCnt))
			return false;

		uint32_t uiBits = 0;
		uint8_t uiByte = 0;
		uint8_t uiFree = 8;
		for (int16_t i = uiDevCnt - 1; i >= 0; --i) {
			uint8_t uiLength = puiLength[i];

			uiBits += uiLength;
			T uiData = puiData[i];
			do {
				uiByte |= ((static_cast<uint8_t>(uiData) & ((1 << uiFree) - 1)) << (8 - uiFree));
				if (uiFree > uiLength) {
					uiFree -= uiLength;
					uiLength = 0;
				} else if (uiFree == uiLength) {
					auiData.push_back(uiByte);
					uiFree = 8;
					uiByte = 0;
					uiLength = 0;
				} else {
					auiData.push_back(uiByte);
					uiByte = 0;
					uiLength -= uiFree;
					uiData >>= uiFree;
					uiFree = 8;
				}
			} while (uiLength);
		}

		if (uiFree != 8)
			auiData.push_back(uiByte);

		if (!this->m_pJtagIf->shiftChainDR(&auiData[0], 0, uiBits, SCAN_MODE_WRITE))
			return false;

		if (bFlush)
			return this->flushCable();

		return true;
	}


	template <typename T>
	bool getJtagData(const uint8_t uiDevice, T& uiData, const uint16_t uiLength)
	{
		if (!this->checkCable())
			return false;

		if (!this->isXilinxJtag()) {
			if (!this->isDeviceValid("getJtagData", uiDevice))
				return false;
			if (!this->isChainValid("getJtagData"))
				return false;

			uint8_t auiData[64] = {0x0};
			const uint8_t uiPreShiftBits = this->m_uiDeviceCount - uiDevice - 1;
			const uint32_t uiDataLength = uiLength + uiPreShiftBits;
			if (!this->m_pJtagIf->shiftChainDR(
					0, static_cast<uint8_t*>(auiData), uiDataLength, SCAN_MODE_READ))
				return false;
			this->formatDataRead(auiData, uiLength, uiData, uiPreShiftBits);
		} else {
#ifdef READ_TDI_ZERO_SHIFT
			static uint8_t auiTDIData[32] = {0};
#else
			static uint8_t* auiTDIData = 0;
#endif

			uint8_t auiData[64];
			if (!this->m_pJtagIf->shiftDeviceDR(
					uiDevice, auiTDIData, static_cast<uint8_t*>(auiData), uiLength, SCAN_MODE_READ))
				return false;

			this->formatDataRead(auiData, uiLength, uiData);
		}

		return true;
	}

	bool getJtagData(const uint8_t, std::vector<bool>&, const uint16_t);

	template <typename T, typename U>
	bool setGetJtagData(
		const uint8_t uiDevice, const T uiDataIn, U& uiDataOut, const uint16_t uiLength)
	{
		if (!this->checkCable())
			return false;

		std::vector<uint8_t> auiDataIn;
		uint8_t auiDataOut[64];
		if (!this->isXilinxJtag()) {
			if (!this->isDeviceValid("setGetJtagData", uiDevice))
				return false;
			if (!this->isChainValid("setGetJtagData"))
				return false;

			const uint8_t uiPreShiftBits = this->m_uiDeviceCount - uiDevice - 1;
			this->formatDataWrite(uiDataIn, uiLength, auiDataIn, uiPreShiftBits, uiDevice);
			const uint32_t uiDataLength = uiLength + uiPreShiftBits + uiDevice;
			if (!this->m_pJtagIf->shiftChainDR(
					&auiDataIn[0], static_cast<uint8_t*>(auiDataOut), uiDataLength,
					SCAN_MODE_READ_WRITE))
				return false;

			this->formatDataRead(auiDataOut, uiLength, uiDataOut, uiPreShiftBits);
		} else {
			this->formatDataWrite(uiDataIn, uiLength, auiDataIn);
			if (!this->m_pJtagIf->shiftDeviceDR(
					uiDevice, &auiDataIn[0], static_cast<uint8_t*>(auiDataOut), uiLength,
					SCAN_MODE_READ_WRITE))
				return false;

			this->formatDataRead(auiDataOut, uiLength, uiDataOut);
		}
		return true;
	}

	bool setGetJtagData(
		const uint8_t, std::vector<bool> const&, std::vector<bool>&, const uint16_t);

	template <typename T>
	inline bool setJtagInstr(T uiInstr)
	{
		return this->setJtagInstr(0, uiInstr);
	}

	template <typename T>
	inline bool setJtagData(T uiData, const uint8_t uiLength)
	{
		return this->setJtagData(0, uiData, uiLength);
	}

	template <typename T>
	inline bool getJtagData(T& uiData, const uint8_t uiLength)
	{
		return this->getJtagData(0, uiData, uiLength);
	}

	inline bool createSession() { return this->createSession(0, 0); }

	inline bool openCable(const uint8_t uiIndex) { return this->openCable(uiIndex, 0, false); }

	inline bool openCable() { return this->openCable(0, 0, false); }

	inline bool openCable(bool bForceUnlock) { return this->openCable(0, 0, bForceUnlock); }
};

} // namespace jtag_lib_v2

#endif // #ifdef __JTAG_ACCESS_HPP__
