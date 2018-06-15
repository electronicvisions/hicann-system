// *********************************************************
// 
// Chiptest.h
// By J. Blouw, MPI fuer Kernfysik, 2005
// Adapted from R. Achenbach
// (ralf.achenbach@kip.uni-heidelberg.de)
// 21.09.2004
//
// Class (ChipTests) used in the FrameWork
// for constructing commands for the probestation
// Note: the only method used is the "Command" method.
// it takes a std::string, containing the probestation command.
//
// For instance:
//      ostringstream command;
//      command << "BinMapDie " << colour << " " << col << " " << row << " \n";
//      std::string result = chip_test->Command( command.str() );
// 
// *********************************************************
#include <iostream>
#include <fstream>
// *********************
// Local includes
// *********************
//#include <Socket.h>
#include "Serial.h"
#ifndef POWERSUP_HH
#define POWERSUP_HH


class Serial;


class PowerSupply {
 public :
  PowerSupply();
  PowerSupply( Serial * );
  virtual ~PowerSupply();
  
  virtual std::string Clear();
	virtual std::string PowerOn();
  virtual std::string PowerOff();
  virtual std::string GetState();
  virtual std::string ClearProt();
  virtual std::string GetCurrProt(uint channel);
  virtual std::string SetCurrProt(uint channel, bool onoff);
  virtual std::string GetCurrent(uint channel);
  virtual std::string SetCurrent(uint channel, double c);
  virtual std::string GetVoltage(uint channel);
  virtual std::string SetVoltage(uint channel, double v);

  std::string Command( const std::string & );
  // liefert die Postion des Chucks zurueck

 private :
  Serial *serial;
  std::string port;
  //  char ser1[11];
  char respp[255];
};
#endif
