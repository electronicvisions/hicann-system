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
// Filename          :   jtag_systemc.hpp
// Project Name      :   JTAG Library V2
// Description       :   SystemC JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_SYSTEMC_HPP__
#define __JTAG_SYSTEMC_HPP__

#include "jtag_stdint.h"
#include "jtag_if.hpp"
#include "jtag_state.hpp"
#include "jtag_systemc_if.hpp"

#include <vector>

namespace jtag_lib_v2
{

  class jtag_systemc: public jtag_if
  {
  private:
    jtag_systemc_if *m_pDevice;
    double           m_lfTckPeriod;
    jtag_state       m_jtagState;
    uint8_t          m_auiTDIBuffer[128];
    uint8_t          m_auiTMSBuffer[128];
    uint8_t          m_auiTDOBuffer[128];
    uint16_t         m_uiBitcount;

    jtag_systemc(const jtag_systemc&);

    bool flushCable(const bool);
    bool setPin(const enum eJtagPin, const bool, const bool);
    bool pulsePin(const eJtagPin, const uint16_t, const bool);

  public:

    inline virtual bool createSession()
    {
      return this->createSession(0, 0);
    }

    jtag_systemc();
    virtual ~jtag_systemc();
    virtual bool createSession(const char *, const uint16_t);
    virtual bool enumCables(uint8_t &, const bool);
    virtual bool openCable(const uint8_t, const uint32_t, const bool);
    virtual bool closeCable();
    virtual bool setPin(const enum eJtagPin, const bool);
    virtual bool getPin(const enum eJtagPin, bool &);
    virtual bool pulsePin(const eJtagPin, const uint16_t);
    virtual bool shiftDeviceIR(const uint8_t, uint8_t *, uint32_t);
    virtual bool shiftChainIR(uint8_t *, uint32_t);
    virtual bool shiftDeviceDR(const uint8_t, uint8_t *, uint8_t *, uint32_t,
                               const enum eScanMode);
    virtual bool shiftChainDR(uint8_t *, uint8_t *,
                              uint32_t, const enum eScanMode);
    virtual bool navigate(const enum eJtagState, const uint16_t);
    virtual bool setJtagFrequency(const uint32_t);
    virtual bool flushCable();

    virtual inline bool isCableOpen()
    {
      if ( !this->m_pDevice )
        return false;
      return this->jtag_if::isCableOpen();
    }

  };

} // namespace jtag_lib_v2

#endif // __JTAG_SYSTEMC_HPP__
