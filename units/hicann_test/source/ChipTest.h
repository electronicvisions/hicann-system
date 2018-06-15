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
#ifndef CHIPTEST_HH
#define CHIPTEST_HH


class Serial;


class ChipTests {
 public :
  ChipTests();
  ChipTests( Serial * );
  virtual ~ChipTests();
  
  void WaferTest();
  virtual std::string ReadMapPosition();
  virtual std::string MoveChuckZup( const int* );
  virtual std::string MoveChuckZdown( const int* );
  virtual std::string MoveChuckSeparation();
  virtual std::string MoveChuckContact();
  virtual std::string MoveChuckAlign();

  std::string Command( const std::string & );
  // liefert die Postion des Chucks zurueck
  std::string ReadChuckPosition(void); 
  // Uebergabewerte von 1-255, liefert die position 
  // des neuen Dies zurueck
  char* BinStepDie( const int*  ); 
  char* int_to_string(const int * );

 private :
  Serial *serial;
  std::string port;
  //  char ser1[11];
  char respp[255];
};
#endif
