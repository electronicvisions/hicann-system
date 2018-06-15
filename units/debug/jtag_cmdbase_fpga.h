
#ifndef __JTAG_COMMANDS_TESTFPGATOP__
#define __JTAG_COMMANDS_TESTFPGATOP__


#include<vector>

#define CMD_READ_ID                0x00
#define CMD_EXP_CTRL               0x0e
#define CMD_SET_DEBUG_ADDR         0x12
#define CMD_GP_DEBUG_READ          0x13

#define CMD_WIDTH               5


#ifndef JTAG_CMDBASE
#include "fpga_jtag_cmdbase.h"
#else
#include JTAG_CMDBASE
#endif

template<typename JTAG>
class jtag_cmdbase_fpga : public jtag_cmdbase<JTAG>
{
  public:
  
  template <typename T>void ReadID(T& rdata)
  {
    this->setJtagInstr(this->getFpgaUserInstr());
    this->setJtagData(CMD_READ_ID, CMD_WIDTH);
    this->getJtagData(rdata, 32);
  }


  // control timestamp/experiments: setting systime_trigger alternately starts/stops the systime counter, playback_enable starts the playback of pulses
  void SetExperimentCtrl(bool playback_enable, bool systime_trigger = false, bool loopback_enable = false)
  {
    unsigned int wdata = 0;
	if (systime_trigger) wdata |= 1;
	if (playback_enable) wdata |= 2;
	if (loopback_enable) wdata |= 4;
	
	printf("SetExperimentCtrl: wdata: %x\n",wdata);

    this->setJtagInstr(this->getFpgaUserInstr());
    this->setJtagData(CMD_EXP_CTRL, CMD_WIDTH);
    this->setJtagData(wdata, 3);
  }

  void SetDebugAddress(unsigned int addr)
  {
    this->setJtagInstr(this->getFpgaUserInstr());
    this->setJtagData(CMD_SET_DEBUG_ADDR, CMD_WIDTH);
    this->setJtagData(addr, 8);
  }

  template <typename T> void GetDebugOutput(T& rdata)
  {
    this->setJtagInstr(this->getFpgaUserInstr());
    this->setJtagData(CMD_GP_DEBUG_READ, CMD_WIDTH);
    this->getJtagData(rdata, 32);
  }


};

#endif
