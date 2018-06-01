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
// Filename          :   jtag_systemc_if.cpp
// Project Name      :   JTAG Library V2
// Description       :   SystemC JTAG implementation
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#include "jtag_systemc_if.hpp"
#include "jtag_logger.hpp"

#include <string.h>

jtag_lib_v2::jtag_systemc_if::jtag_systemc_if() : m_bDeviceOpen(false)
{
}

void jtag_lib_v2::jtag_systemc_if::closeDevice()
{
  this->m_bDeviceOpen = false;
}

bool jtag_lib_v2::jtag_systemc_if::openDevice()
{
  if ( this->m_bDeviceOpen )
    return false;
  this->m_bDeviceOpen = true;
  return true;
}

bool jtag_lib_v2::jtag_systemc_if::isDeviceOpen() const
{
  return this->m_bDeviceOpen;
}

jtag_lib_v2::jtag_systemc_if::~jtag_systemc_if()
{
}

jtag_lib_v2::jtag_systemc_connector::jtag_systemc_connector()
{
}

jtag_lib_v2::jtag_systemc_connector&
jtag_lib_v2::jtag_systemc_connector::getInstance()
{
  static jtag_lib_v2::jtag_systemc_connector myInstance;
  return myInstance;
}

bool jtag_lib_v2::jtag_systemc_connector::registerDevice(const char * szDevice,
                                                         jtag_systemc_if *pDevice)
{
  jtag_systemc_connector::const_iterator iterator;
  for ( iterator = this->iterateDevices(); iterator != this->m_DeviceMap.cend();
        ++iterator )
    {
      if ( !::strcmp(szDevice, iterator->first) )
        return false;
    }

  this->m_DeviceMap.insert(jtag_systemc_connector::value_type(szDevice,
                                                              pDevice));

  return true;
}

jtag_lib_v2::jtag_systemc_if *
jtag_lib_v2::jtag_systemc_connector::getDevice(const char *szDevice)
{
  jtag_systemc_connector::const_iterator iterator;
  for ( iterator = this->iterateDevices(); iterator != this->m_DeviceMap.cend();
        ++iterator )
    {
      if ( !::strcmp(szDevice, iterator->first) )
        return iterator->second;
    }
  return 0;
}

jtag_lib_v2::jtag_systemc_connector::const_iterator
jtag_lib_v2::jtag_systemc_connector::iterateDevices()
{
  return this->m_DeviceMap.cbegin();
}

uint32_t jtag_lib_v2::jtag_systemc_connector::getDeviceCount()
{
  return this->m_DeviceMap.size();
}
