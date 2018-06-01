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
// Filename          :   jtag_systemc_if.hpp
// Project Name      :   JTAG Library V2
// Description       :   SystemC JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_SYSTEMC_IF_HPP__
#define __JTAG_SYSTEMC_IF_HPP__

#include "jtag_stdint.h"

#include <map>

namespace jtag_lib_v2
{

  class jtag_systemc_if
  {
  private:
    bool m_bDeviceOpen;
  public:
    jtag_systemc_if();
    virtual bool openDevice();
    virtual void closeDevice();
    virtual void setTDI(const bool) = 0;
    virtual void setTMS(const bool) = 0;
    virtual void setTCK(const bool) = 0;
    virtual bool getTDO() = 0;
    virtual void waitNS(const double) = 0;
    virtual bool isDeviceOpen() const;
    virtual ~jtag_systemc_if();
  };

  class jtag_systemc_connector
  {
  public:
    typedef std::map<char const *, jtag_systemc_if *> jtag_systemc_map_t;
    typedef jtag_systemc_map_t::const_iterator const_iterator;
    typedef jtag_systemc_map_t::value_type value_type;

    static jtag_systemc_connector& getInstance();
    bool registerDevice(const char *, jtag_systemc_if *);
    jtag_systemc_if *getDevice(const char *);
    jtag_systemc_map_t::const_iterator iterateDevices();
    uint32_t getDeviceCount();

  private:
    jtag_systemc_map_t m_DeviceMap;

    jtag_systemc_connector();
    jtag_systemc_connector(const jtag_systemc_connector&);
  };

} // namespace jtag_lib_v2

#endif // __JTAG_SYSTEMC_IF_HPP__
