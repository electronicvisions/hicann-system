// *********************************************************
//
// Serial.h
//
// By J. Blouw, MPI, Heidelberg 2005.
// (Johan.Blouw@mpi-hd.mpg.de)
// For the Beetle chip tests
// 
// Class (Serial) used for opening/closing/communication
// using a serial port. Partially adapted from
// Ralf Achenbach's code (com.cc, com.h).
//
// *********************************************************
#include <termios.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef SERIAL_HH
#define SERIAL_HH

class Serial {
public: 
  Serial(); // default constructor
  virtual ~Serial(); // destructor
  int Initialize();
  char * int_to_string( int );
  int StartComm();
  void CloseComm();
  void setSer( const std::string& );
  std::string getSer();
  int Send( const std::string& );
  int Receive( std::string & );
  Serial &operator << ( const std::string& );
  const Serial &operator >> ( std::string & );
  const int Select( const int );
private:
  int fd;
  int flags;
  std::string ser1;
  std::string resp;
  int mode;

  struct termios oldtio;
  fd_set read_fd, write_fd;
  struct timeval time_2_wait, *time;

};
#endif
