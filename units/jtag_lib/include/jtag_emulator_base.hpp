#ifndef __JTAG_EMULATOR_BASE_H__
#define __JTAG_EMULATOR_BASE_H__

#include "jtag_systemc_if.hpp"
#include "jtag_logger.hpp"

#include <systemc.h>

namespace jtag_lib_v2
{

  class jtag_emulator_base : public sc_module, public jtag_systemc_if
  {
  public:
    sc_in<sc_logic>  tdo;
    sc_out<sc_logic> tck;
    sc_out<sc_logic> tms;
    sc_out<sc_logic> tdi;

    jtag_emulator_base(const char *szDevice) : tdo("tdo"),
                                               tck("tck"),
                                               tms("tms"),
                                               tdi("tdi")
    {
      if ( !this->registerDevice(szDevice) )
        {
          jtag_logger::sendMessage(MSG_ERROR,
                                   "Failed to register SystemC "
                                   "device '%s'.\n", szDevice);
          sc_stop();
        }
    }

    virtual ~jtag_emulator_base() {}

  private:

    virtual bool registerDevice(const char *szDevice)
    {
      return jtag_lib_v2::jtag_systemc_connector::getInstance().registerDevice(szDevice,
                                                                               this);
    }

    virtual void setTDI(const bool bValue)
    {
      tdi.write( ( bValue ) ? SC_LOGIC_1 : SC_LOGIC_0 );
    }

    virtual void setTMS(const bool bValue)
    {
      tms.write( ( bValue ) ? SC_LOGIC_1 : SC_LOGIC_0 );
    }

    virtual void setTCK(const bool bValue)
    {
      tck.write( ( bValue ) ? SC_LOGIC_1 : SC_LOGIC_0 );
    }

    virtual bool getTDO()
    {
      return ( tdo.read() == SC_LOGIC_1 );
    }

    virtual void waitNS(const double lfTime)
    {
      wait(lfTime, SC_NS);
    }

  };

}

#endif // ifndef __JTAG_EMULATOR_BASE_H__
