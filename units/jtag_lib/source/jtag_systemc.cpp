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
// Filename          :   jtag_systemc.cpp
// Project Name      :   JTAG Library V2
// Description       :   SystemC JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_systemc.hpp"
#include "jtag_stdint.h"
#include "jtag_logger.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

jtag_lib_v2::jtag_systemc::jtag_systemc() : m_pDevice(0),
                                            m_lfTckPeriod(1000),
                                            m_uiBitcount(0)
{
}

bool jtag_lib_v2::jtag_systemc::createSession(const char *, const uint16_t)
{
  /* mark libraries open */
  this->setSessionOpen(true);

  return true;
}

bool jtag_lib_v2::jtag_systemc::enumCables(uint8_t &uiCablesFound,
                                           const bool bEnumOther)
{
  uiCablesFound = 0;

  /* check session */
  if ( !this->isSessionOpen() )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "enumCables: No open session.\n");
      return false;
    }

  /* do nothing if a cable is already open in this session */
  if ( this->isCableOpen() )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "enumCables: A cable is already open.\n");
      return false;
    }

  jtag_logger::sendMessage(MSG_INFO,
                           "enumCables: Enumerating connected devices ...\n");

  /* get number of attached devices */
  uint32_t uiDeviceCount = jtag_systemc_connector::getInstance().getDeviceCount();
  if ( uiDeviceCount > 255 )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "enumCables: Only 255 SystemC JTAG devices are "
                               "supported, but found %d.\n", uiDeviceCount);
      return false;
    }

  jtag_systemc_connector::const_iterator iterator =
    jtag_systemc_connector::getInstance().iterateDevices();

  /* iterate over all attached cables */
  for ( uint32_t i = 0; i < uiDeviceCount; ++i )
    {

      cable_info *pCable;
      try
        {
          pCable = new cable_info();
        }
      catch ( std::bad_alloc )
        {
          return false;
        }

      if ( !pCable->setPort(iterator->first) )
        {
          delete pCable;
          return false;
        }

      pCable->setLocked(iterator->second->isDeviceOpen());
      pCable->setPortNumber(1);

      if ( i < uiDeviceCount - 1 )
        ++iterator;
      this->addCableInfo(pCable);
    }

  uiCablesFound = uiDeviceCount;

  if ( ! uiCablesFound )
    jtag_logger::sendMessage(MSG_WARNING,
                           "enumCables: No cables found.\n");
  else if ( uiCablesFound == 1 )
    jtag_logger::sendMessage(MSG_INFO,
                           "enumCables: Found 1 cable.\n");
  else
    jtag_logger::sendMessage(MSG_INFO,
                             "enumCables: Found %d cables.\n",
                             uiCablesFound);

  /* signal that enumaration has completed */
  this->markEnumDone();

  return true;
}

bool jtag_lib_v2::jtag_systemc::openCable(const uint8_t uiCableIndex,
                                          const uint32_t uiFrequency,
                                          const bool bForceUnlock)
{
  /* get a cable to open */
  cable_info *pCableInfo;
  if ( !this->getCableToOpen(uiCableIndex, &pCableInfo, false) )
    return false;

  /* get device name */
  const char *szDevice = pCableInfo->getPort();
  if ( !szDevice )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "openCable: Invalid device at index %d.",
                               uiCableIndex);
      return false;
    }

  jtag_logger::sendMessage(MSG_INFO, "openCable: Trying to open SystemC "
                           "device '%s' ...\n",
                           szDevice);

  /* store requested frequency */
  this->setJtagFrequency(uiFrequency);

  jtag_logger::sendMessage(MSG_INFO, "openCable: Using TCK frequency of "
                           "%3.3lf MHz.\n", uiFrequency / 1000.);

  /* get device from systemc connector */
  this->m_pDevice =
    jtag_systemc_connector::getInstance().getDevice(szDevice);
  if ( !this->m_pDevice )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "openCable: Failed to get requested device "
                               "'%s'.\n", szDevice);
      return false;
    }

  /* open the device */
  if ( !this->m_pDevice->openDevice() )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "openCable: Failed to open device "
                               "'%s'.\n", szDevice);
      this->m_pDevice = 0;
      return false;
    }

  jtag_logger::sendMessage(MSG_INFO, "openCable: Successfully opened requested "
                           "SystemC device.\n");

  /* mark cable as open */
  this->setCableOpen(true);

  /* set unknown jtag state */
  this->m_jtagState.setJtagState(UNKNOWN_JTAG_STATE);

  /* initialize signals */
  this->m_pDevice->setTDI(false);
  this->m_pDevice->setTMS(false);
  this->m_pDevice->setTCK(false);

  return true;
}

bool jtag_lib_v2::jtag_systemc::closeCable()
{
  if ( this->isCableOpen() )
    {
      if ( !this->flushCable() )
        return false;

      this->m_pDevice->closeDevice();
    }

  /* mark cable as closed */
  this->setCableOpen(false);

  /* set unknown jtag state */
  this->m_jtagState.setJtagState(UNKNOWN_JTAG_STATE);

  return true;
}

bool jtag_lib_v2::jtag_systemc::pulsePin(const enum jtag_lib_v2::eJtagPin ePin,
                                         const uint16_t uiCount)
{
  return this->pulsePin(ePin, uiCount, true);
}

bool jtag_lib_v2::jtag_systemc::pulsePin(const enum jtag_lib_v2::eJtagPin ePin,
                                         const uint16_t uiCount,
                                         const bool bFlush)
{
  /* flush TX buffer bits */
  if ( this->m_uiBitcount && bFlush )
    {
      if ( !this->flushCable() )
        return false;
    }

  /* if no pulse, skip */
  if ( !uiCount )
    return true;

  for ( uint32_t i = 0; i < uiCount; ++i )
    {
      /* wait half period */
      this->m_pDevice->waitNS(this->m_lfTckPeriod / 2);
      /* set pin */
      if ( !this->setPin(ePin, true, false) )
        return false;
      /* wait half period */
      this->m_pDevice->waitNS(this->m_lfTckPeriod / 2);
      /* release pin */
      if ( !this->setPin(ePin, false, false) )
        return false;
    }

  return true;
}

bool jtag_lib_v2::jtag_systemc::setPin(const enum jtag_lib_v2::eJtagPin ePin,
                                       const bool bValue)
{
  return this->setPin(ePin, bValue, true);
}

bool jtag_lib_v2::jtag_systemc::setPin(const enum jtag_lib_v2::eJtagPin ePin,
                                       const bool bValue, const bool bFlush)
{
  /* flush TX buffer bits */
  if ( this->m_uiBitcount && bFlush )
    {
      if ( !this->flushCable() )
        return false;
    }

  /* set/clear requested pin */
  switch ( ePin )
    {
    case JTAG_PIN_TDI :
      this->m_pDevice->setTDI(bValue); break;
    case JTAG_PIN_TMS :
      this->m_pDevice->setTMS(bValue); break;
    case JTAG_PIN_TCK :
      this->m_pDevice->setTCK(bValue); break;
    default :
      jtag_logger::sendMessage(MSG_ERROR,
                               "setPin: Requested pin is impossible to set.\n");
      return false;
    }
  return true;
}

bool jtag_lib_v2::jtag_systemc::getPin(const enum jtag_lib_v2::eJtagPin ePin,
                                       bool &bValue)
{
  switch ( ePin )
    {
    case JTAG_PIN_TDO :
      bValue = this->m_pDevice->getTDO(); break;
    default :
      jtag_logger::sendMessage(MSG_ERROR,
                               "getPin: Cannot retrieve value of requested "
                               "pin.\n");
      return false;
    }

  return true;
}

bool jtag_lib_v2::jtag_systemc::shiftDeviceDR(const uint8_t uiDevice,
                                              uint8_t *puiTDIBuffer,
                                              uint8_t *puiTDOBuffer,
                                              uint32_t uiBitCount,
                                              const enum eScanMode eMode)
{
  /* unsupported, must be handled by higher level API */
  return false;
}

bool jtag_lib_v2::jtag_systemc::shiftDeviceIR(const uint8_t uiDevice,
                                              uint8_t *puiTDIBuffer,
                                              uint32_t uiBitCount)
{
  /* unsupported, must be handled by higher level API */
  return false;
}

bool jtag_lib_v2::jtag_systemc::shiftChainIR(uint8_t *puiTDIBuffer,
                                             uint32_t uiBitCount)
{
  /* check buffer */
  if ( uiBitCount && !puiTDIBuffer )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "shiftChainIR: Need TDI buffer to perform "
                               "requested operation.\n");
      return false;
    }

  /* move to CAPTURE-IR */
  if ( !this->navigate(CAPTURE_IR, 0) )
    return false;

  /* for zero shift bits go directly to RUN-TEST-IDLE */
  if ( !uiBitCount )
    return this->navigate(RUN_TEST_IDLE, 0);

  /* move to SHIFT-IR */
  if ( !this->navigate(SHIFT_IR, 0) )
    return false;

  /* shift out TDI bits */
  uint8_t uiTDIPattern = 0;
  uint8_t uiBit        = this->m_uiBitcount & 0x7;
  uint8_t uiByte       = this->m_uiBitcount >> 3;
  for ( uint32_t uiBits = 0; uiBits < uiBitCount; ++uiBits )
    {
      /* get TDI pattern for next 8 bits */
      if ( ! ( uiBits & 0x7 ) )
        {
          uiTDIPattern = *puiTDIBuffer;
          puiTDIBuffer++;
        }
      /* move to EXIT1-IR on last shift */
      if ( uiBits == ( uiBitCount - 1 ) )
        this->m_auiTMSBuffer[uiByte] |= ( 1 << uiBit );
      /* set TDI bit */
      this->m_auiTDIBuffer[uiByte] |= ( ( uiTDIPattern & 0x1 ) << uiBit );

      uiBit++;
      this->m_uiBitcount++;
      if ( uiBit == 8 )
        {
          uiBit = 0;
          uiByte++;
          if ( this->m_uiBitcount == 1024 )
            {
              if ( !this->flushCable() )
                return false;
              uiByte = 0;
            }
          this->m_auiTMSBuffer[uiByte] = 0;
          this->m_auiTDIBuffer[uiByte] = 0;
        }
      uiTDIPattern >>= 1;
    }

  this->m_jtagState.setJtagState(EXIT1_DR);

  /* move to RUN-TEST-IDLE */
  return this->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_systemc::shiftChainDR(uint8_t *puiTDIBuffer,
                                             uint8_t *puiTDOBuffer,
                                             uint32_t uiBitCount,
                                             const enum eScanMode eMode)
{
  /* check buffers */
  if ( eMode != SCAN_MODE_WRITE && !puiTDOBuffer && uiBitCount )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "shiftChainDR: Need TDO buffer to perform "
                               "requested operation.\n");
      return false;
    }

  if ( eMode != SCAN_MODE_READ && !puiTDIBuffer && uiBitCount )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "shiftChainDR: Need TDI buffer to perform "
                               "requested operation.\n");
      return false;
    }

  /* move to CAPTURE-DR */
  if ( !this->navigate(CAPTURE_DR, 0) )
    return false;

  /* for zero shift bits go directly to RUN-TEST-IDLE */
  if ( !uiBitCount )
    return this->navigate(RUN_TEST_IDLE, 0);

  /* move to SHIFT-DR */
  if ( !this->navigate(SHIFT_DR, 0) )
    return false;

  /* flush TX buffer in case of read */
  if ( eMode != SCAN_MODE_WRITE )
    {
      if ( !this->flushCable() )
        return false;
    }

  /* shift out TDI bits and capture TDO bits */
  uint8_t  uiTDIPattern = 0;
  uint8_t  uiBit        = this->m_uiBitcount & 0x7;
  uint8_t  uiByte       = this->m_uiBitcount >> 3;
  uint32_t uiTDOPos     = 0;
  for ( uint32_t uiBits = 0; uiBits < uiBitCount; ++uiBits )
    {
      /* get TDI pattern for next 8 bits */
      if ( ! ( uiBits & 0x7 ) && eMode != SCAN_MODE_READ )
        {
          uiTDIPattern = *puiTDIBuffer;
          puiTDIBuffer++;
        }
      /* move to EXIT1-DR on last shift */
      if ( uiBits == ( uiBitCount - 1 ) )
        this->m_auiTMSBuffer[uiByte] |= ( 1 << uiBit );

      if ( eMode != SCAN_MODE_READ )
        {
          /* set TDI bit */
          this->m_auiTDIBuffer[uiByte] |= ( ( uiTDIPattern & 0x1 ) << uiBit );
          uiTDIPattern >>= 1;
        }

      uiBit++;
      this->m_uiBitcount++;
      if ( uiBit == 8 )
        {
          uiBit = 0;
          uiByte++;
          if ( this->m_uiBitcount == 1024 )
            {
              if ( !this->flushCable(eMode != SCAN_MODE_WRITE) )
                return false;
              uiByte = 0;

              /* copy TDO bits */
              if ( eMode != SCAN_MODE_WRITE )
                ::memcpy(&(puiTDOBuffer[uiTDOPos]), this->m_auiTDOBuffer, 128);
              uiTDOPos += 128;
            }
          this->m_auiTMSBuffer[uiByte] = 0;
          this->m_auiTDIBuffer[uiByte] = 0;
        }
    }

  /* shift out remaining bits */
  if ( this->m_uiBitcount && eMode != SCAN_MODE_WRITE )
    {
      const uint8_t uiBytes = ( this->m_uiBitcount + 7 ) >> 3;
      if ( !this->flushCable(true) )
        return false;
      /* copy remaining TDO bits */
      ::memcpy(&(puiTDOBuffer[uiTDOPos]), this->m_auiTDOBuffer, uiBytes);
    }

  this->m_jtagState.setJtagState(EXIT1_DR);

  /* move to RUN-TEST-IDLE */
  return this->navigate(RUN_TEST_IDLE, 0);
}

bool jtag_lib_v2::jtag_systemc::flushCable()
{
  return this->flushCable(false);
}

bool jtag_lib_v2::jtag_systemc::flushCable(const bool bReadTDO)
{
  /* open cable is required */
  if ( !this->isCableOpen() )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "flushCable: No Jtag cable open.\n");
      return false;
    }

  /* do nothing if no bits are in TX buffer */
  if ( !this->m_uiBitcount )
    return true;

  /* shift out TX buffer and optionally fill RX buffer */
  uint8_t uiByte = 1;
  uint8_t uiBit = 0;
  uint8_t uiTMSPattern = this->m_auiTMSBuffer[0];
  uint8_t uiTDIPattern = this->m_auiTDIBuffer[0];
  uint8_t uiTDOPattern = 0;
  for ( uint32_t i = 0; i < this->m_uiBitcount; ++i )
    {
      /* set TDI and TMS */
      this->setPin(JTAG_PIN_TMS, uiTMSPattern & 0x1, false);
      this->setPin(JTAG_PIN_TDI, uiTDIPattern & 0x1, false);
      uiBit++;
      uiTMSPattern >>= 1;
      uiTDIPattern >>= 1;

      /* pulse TCK */
      this->pulsePin(JTAG_PIN_TCK, 1, false);

      /* get TDO if read is enabled */
      if ( bReadTDO )
        {
          bool bTdo;
          this->getPin(JTAG_PIN_TDO, bTdo);
          uiTDOPattern |= ( ( bTdo ) ? 1 << ( uiBit - 1 ) : 0 );
        }

      /* goto to next byte if all bits are shifted out */
      if ( uiBit == 8 )
        {
          uiTMSPattern = this->m_auiTMSBuffer[uiByte];
          uiTDIPattern = this->m_auiTDIBuffer[uiByte];
          if ( bReadTDO )
            {
              this->m_auiTDOBuffer[uiByte - 1] = uiTDOPattern;
              uiTDOPattern = 0;
            }
          uiByte++;
          uiBit = 0;
        }
    }

  /* push remaining bits to TDO buffer */
  if ( uiBit )
    this->m_auiTDOBuffer[uiByte - 1] = uiTDOPattern;

  /* initialize first byte */
  this->m_auiTMSBuffer[0] = 0;
  this->m_auiTDIBuffer[0] = 0;

  /* clear bitcount */
  this->m_uiBitcount = 0;

  return true;
}

bool jtag_lib_v2::jtag_systemc::navigate(const enum eJtagState eState,
                                         const uint16_t uiClocks)
{
  /* move to requested state */
  uint16_t uiTmsPattern;
  uint8_t  uiTmsBits;

  if ( !this->m_jtagState.getTmsPatternForState(eState, uiTmsPattern,
                                                uiTmsBits) )
    {
      jtag_logger::sendMessage(MSG_ERROR,
                               "navigate: Unknown JTAG state requested.\n");
      return false;
    }

  /* copy TMS bits to TX buffer */
  uint8_t uiBit  = this->m_uiBitcount & 0x7;
  uint8_t uiByte = this->m_uiBitcount >> 3;
  for ( uint8_t uiBits = 0; uiBits < uiTmsBits; ++uiBits )
    {
      this->m_auiTMSBuffer[uiByte] |= ( uiTmsPattern & 0x1 ) << uiBit;
      this->m_uiBitcount++;
      uiBit++;
      if ( uiBit == 8 )
        {
          uiBit = 0;
          uiByte++;
          if ( this->m_uiBitcount == 1024 )
            {
              if ( !this->flushCable() )
                return false;
              uiByte = 0;
            }
          this->m_auiTMSBuffer[uiByte] = 0;
          this->m_auiTDIBuffer[uiByte] = 0;
        }
      uiTmsPattern >>= 1;
    }

  /* perform additional clocks in final state */
  this->m_jtagState.getTmsPatternForState(eState, uiTmsPattern,
                                          uiTmsBits);

  if ( !uiClocks || uiTmsBits > 1 )
    return true;

  /* copy last TMS bit for requested clock cycles to TX buffer */
  for ( uint8_t uiClockCnt = 0; uiClockCnt < uiClocks; ++uiClockCnt )
    {
      this->m_auiTMSBuffer[uiByte] |= ( uiTmsPattern & 0x1 ) << uiBit;
      this->m_uiBitcount++;
      uiBit++;
      if ( uiBit == 8 )
        {
          uiBit = 0;
          uiByte++;
          if ( this->m_uiBitcount == 1024 )
            {
              if ( !this->flushCable() )
                return false;
              uiByte = 0;
            }
          this->m_auiTMSBuffer[uiByte] = 0;
          this->m_auiTDIBuffer[uiByte] = 0;
        }
    }

  return true;
}

bool jtag_lib_v2::jtag_systemc::setJtagFrequency(const uint32_t uiFrequency)
{
  /* limit TCK frequency to 50 MHz */
  uint32_t uiFreq = uiFrequency;
  if ( uiFreq > 50000 )
    uiFreq = 50000;

  /* default to 10MHz if frequency is 0 */
  if ( !uiFreq )
    uiFreq = 10000;

  /* store requested frequency */
  this->m_lfTckPeriod = 1e6 / static_cast<double>(uiFreq);

  return true;
}

jtag_lib_v2::jtag_systemc::~jtag_systemc()
{
  /* close cable */
  this->closeCable();
}
