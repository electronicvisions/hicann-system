// *********************************************************
//
// Implementation of the Serial class.
// By Johan Blouw, (johan.blouw@mpi-hd.mpg.de)
// MPI fuer Kernfysik, 2005, Heidelberg.
//
// Adapted from com.c by Ralf Achenbach.
//
// *********************************************************

/***************************************************************************
                          com.cpp  -  description
                             -------------------
    begin                : Thu Feb  6 16:24:31 CET 2003
    copyright            : (C) 2003 by Ralf Achenbach
    email                : ralf.achenbach@kip.uni-heidelberg.de
 **************************************************************************/

#include <stdlib.h>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}
#include <math.h>
#include <errno.h>
#include <string.h>
#include "Serial.h"

using namespace std;

// Function to read from a filedescriptor... taken from ttcp.c
int Nread( int fd, void *buf, int &count ) {
  register int cnt;
  cnt = read( fd, buf, count );
  int err = errno;
  if ( cnt < 0 ) {
    cerr << "Nread: Error " << err << "\n" 
	 << " : " << strerror(err) << "\n"
	 << "reading from fd = " << fd << "\n";
    return -1;
  }
  cerr << "Nread: buf = " << (char *)buf << endl;
  cerr << "       cnt = " << cnt << endl;
  return cnt;
}

Serial::Serial() :
  fd( -1 ),
  flags( O_RDWR | O_NOCTTY | O_NONBLOCK ),
  ser1( "/dev/ttyS0" ),
  resp( "nochnixdrinn" ),
  mode( ios::in | ios::out ) {
  time_2_wait.tv_sec = 10;
  time = &time_2_wait;
  cerr << "Serial constructor" << "\n";
  //  flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_SYNC;
  cerr << "ser1 = " << ser1 << "\n" 
       << "resp = " << resp << "\n"
       << "mode = " << mode << "\n"
       << "flags = " << flags << "\n";
};

Serial::~Serial() {
  int status = 0; //tcsetattr( fd, TCSANOW, &oldtio);
  int error = errno;
  if ( status == -1 ) {
    cerr << "Error setting terminal settings " 
	 << " on port " << ser1.c_str() << " "
	 << error << " : " 
	 << strerror( error ) << endl;
  }
  cerr << "Closing file..." << fd << endl;
  //  close( fd );
}; // default destructor


int Serial::Initialize()
{
  /*
  ser1 = "/dev/ttS0";
  resp = "nochnixdrinn";
  mode = ios::in;
  */
  //  flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
  //  flags = O_RDWR | O_NOCTTY | O_SYNC;
  return 0;
};

void Serial::setSer( const string &c )
{
  ser1 = c;
  cout << "Serial::setSer(): serial port = " << ser1 << endl;
};

string Serial::getSer( )
{
  return ser1;
}



char *Serial::int_to_string(int number)
{
  int digit,i;
  double dum1,dum2;
  dum1 = (double) number;
  dum2 = log10( dum1 );
  dum2 += 1.0;
  digit = (int) dum2;
  char *char_number;
  char_number=new char[digit];
  for (i=0;i<digit;i++)
    {
      char_number[i]=(int)((double)number/pow(10.0,(double)digit-i-1))+'0';
      number-=(int)((char_number[i]-'0')*pow(10.0,(double)digit-i-1));
    }
  char_number[digit]='\0';
  return char_number;
}

int Serial::StartComm() {
  cout << "Serial::StartComm: entering...\n";
  struct termios settings;
  
  // ***************************
  // Dummy Ser1 Setting, works but ...
  // **************************
  struct termios dummytio;
  //  dummytio.c_iflag=0x500;
  //  dummytio.c_oflag=0x5;
  //  dummytio.c_cflag=0xcbd;
  //  dummytio.c_lflag=0x8a33;
  
  
  //  fstream fs( ser1.c_str(), ios::in );
  //  int fdis;
  //  fdis = (fs.rdbuf())->fd();
  //  fd = open( ser1.c_str(), flags, mode );
  fd = open( ser1.c_str(), 2|O_NONBLOCK);
  int error=errno;
  cout << "Opening serial port " << ser1 << "\n"
       << "Filedescriptor = " << fd << "\n"
       << "Flags = " << flags << "\n";
  if ( fd == -1 ) {
    cerr << "Error opening serial port " 
	 << error << " : " 
	 << strerror( error ) << endl;
    close( fd );
    return -1;
  }
  /*
    int status = tcgetattr( fd, &dummytio );
    error = errno;
    if ( status == -1 ) {
    cerr << "Error setting terminal settings " 
    << " on port " << ser1.c_str() << " "
    << error << " : " 
    << strerror( error ) << endl;
    }
  */
  //  close( fd );
  // ****************************/
  
  // c_cflag:
  // BAUD     = B9600;   //4 B50 .... B38400
  // DATABITS = CS8;     //2 CS5 CS6 CS7 CS8
  // STOPBITS = 0;       //1 0=1StopBit CSTOPB=2StopBit
  // READ     = CREAD;   //1 0          CREAD
  // PARITY   = 0;       //1 0=no       PARENB
  // EVENODD  = 0;       //1 0=even     PARODD=odd
  // HANGUP   = 0;       //1 0          HUPCL
  // LOCAL    = 0;       //1 0          CLOCAL
  // Alle Flags werden beschrieben unter:
  // http://www.linuxcentral.com/linux/man-pages/termios.2.html
  
  //  fd = open(ser1.c_str(), flags);
  //  error = errno;
  int status = tcgetattr(fd, &oldtio); // Alte Settings lesen
  error = errno;
  if ( status == -1 ) {
    cerr << "Error setting terminal settings " 
	 << " on port " << ser1.c_str() << " "
	 << error << " : " 
	 << strerror( error ) << endl;
    close( fd );
    return -1;
  }
  
  // printf ("%x\n",oldtio.c_cflag);
  // ******************************************
  cout << "Old termios settings...\n"; 
  cout << oldtio.c_iflag << "\n";
  cout << oldtio.c_oflag << "\n";
  cout << oldtio.c_cflag << "\n";
  cout << oldtio.c_lflag << endl;
  cout << "New termios settings..." << endl;
  // ******************************************/
  // *** Nur die Bits setzen die noetig sind ***
  settings.c_iflag =0;
  settings.c_oflag =0;
  settings.c_cflag =0;
  settings.c_lflag =0;

  /*  
      settings.c_iflag &= (ICRNL);
      //      settings.c_iflag |= ~(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK);
      //      settings.c_iflag |= ~(ISTRIP|INLCR|ICRNL|IXON|IXOFF|IUCLC);
      #ifdef LynxOS
      settings.c_iflag |= ~(IXANY);
      settings.c_cflag &= ~(CSTOPB|PARENB|PARODD);
      settings.c_lflag &= (IEXTEN|ECHOE|ECHOK);
      settings.c_lflag |= ~(TOSTOP);
      #endif
      #ifdef Linux
      //      settings.c_iflag |= ~(IXANY|IMAXBEL);
      //      settings.c_cflag &= ~(CRTSCTS|CSTOPB|PARENB|PARODD);
      //      settings.c_lflag &= (IEXTEN|ECHOE|ECHOK|ECHOCTL|ECHOKE);
      //      settings.c_lflag |= ~(TOSTOP|ECHOPRT);
      #endif
      settings.c_iflag |= (ONLCR);
      //      settings.c_oflag &= ~(OPOST|ONOCR|ONLRET|OFILL|OFDEL);
      settings.c_oflag &= (ONLCR|OPOST);
      settings.c_oflag &= ~(ONOCR|ONLRET|OFILL|OFDEL);
      settings.c_cflag |= (CS8|CREAD|CLOCAL|HUPCL);
      //      settings.c_lflag |= ~(ISIG|ICANON|ECHO|ECHONL|NOFLSH|XCASE);
      */
  
  settings.c_iflag |= (ICRNL);
  settings.c_oflag |= (OPOST|ONLCR);
  settings.c_cflag |= (CS8|CREAD);

  cfsetispeed (&settings, B9600);
  cfsetospeed (&settings, B9600);
  // ******************************************
  cout << settings.c_iflag << "\n";
  cout << settings.c_oflag << "\n";
  cout << settings.c_cflag << "\n";
  cout << settings.c_lflag << endl;
  // ******************************************/
  
  //  tcflush(fd, TCIOFLUSH);
  status = tcsetattr( fd, TCSANOW, &settings);
  //  
  error = errno;
  if ( status == -1 ) {
    cerr << "Error setting terminal settings " 
	 << " on port " << ser1.c_str() << " "
	 << error << " : " 
	 << strerror( error ) << endl;
    close( fd );
    return -1;
  }
  // send...
  //  string s = "test serial\n";
  //  write ( fd, s.c_str(), sizeof(s.c_str()) );
  //  error = errno;
  //  if ( error ) {
  //    cerr << "Error in writing to ser port " << ser1 <<"...\n" ;
  //   cerr << "error " << error << " " << strerror(error) << endl;
  //    close( fd );
  //    return -1;
  //  }
  //  status = tcsetattr( fd, TCSANOW, &oldtio);
  // close(fd);
  cout << "Serial::StartComm: Filedescriptor fd = " << fd << endl;
  tcflush(fd, TCIOFLUSH);
  return 0;
}


void Serial::CloseComm()
{
  /*
    cout << oldtio.c_iflag << "\n";
    cout << oldtio.c_oflag << "\n";
    cout << oldtio.c_cflag << "\n";
    cout << oldtio.c_lflag << endl;
  */
  //    fd = open(ser1.c_str(), flags);
  // Alte Settings zurueckschreiben!
  //  tcsetattr (fd , TCSANOW, &oldtio); 
  close( fd );
  return;
};

int Serial::Send( const string &s ) {
  tcflush(fd, TCIOFLUSH);
  //  cerr << "Serial::Send: sending " << s << endl;
  if ( fd < 0 ) {
    cerr << "Error opening serial port... " << ser1.c_str() << endl;
    return -1;
  }
  time->tv_sec = 1;
  time->tv_usec = 0;
  FD_ZERO( &write_fd );
  FD_SET( fd, &write_fd );
  int n_ready = 0;
  n_ready = select( 32, 0, &write_fd, 0, (struct timeval *)NULL );
  if ( n_ready <= 0 ) cerr << "Error in Serial::Send" << endl;
  if ( FD_ISSET( fd, &write_fd ) ) {
    write( fd, s.c_str(), strlen(s.c_str()) );
    int error = errno;
//    if ( error ) {
    usleep(100000);
		if ( 0 ) {
      cerr << "Error in writing to serial port;\n"
	   << "Was trying fd = " << fd << "\n"
	   << "error " << error << ": " << strerror(error) << endl;
      //      close( fd );
      return 0;
    }
  }
  tcflush( fd, TCOFLUSH);
  return 0;
}

Serial & Serial::operator << ( const string& s ) {
  if ( this->Send( s ) < 0 ) {
    cerr << "Could not write to serial port..." << endl;
  }
  return *this;
}

const Serial & Serial::operator >> ( string& s ) {
  if ( this->Receive( s )  < 0 ) {
    cerr << "Could not read from serial port." << endl;
  }
  return *this;
}

const int Serial::Select( const int sec) {
  time->tv_sec = sec;
  time->tv_usec = 0;
  FD_ZERO( &read_fd );
  FD_SET( fd, &read_fd );
  int n_ready = 0;
  if ( sec < 0 ) {
    n_ready = select( 32, &read_fd, 0, 0, (struct timeval *) NULL);
    if ( n_ready < 0 ) {
      tcflush( fd, TCIFLUSH);
      cerr << "Error from select, n_ready = " << n_ready << endl;
      return -1;
    }
  } else {
    n_ready = select( 32, &read_fd, 0, 0, time );
    if ( n_ready < 0 ) {
      tcflush( fd, TCIFLUSH);
      cerr << "Error from select, n_ready = " << n_ready << endl;
      return -1;
    }
  }
  return n_ready;
}


int Serial::Receive( string &s ) {
  int buflen = 1024;
  register int cnt;
  char dummy[1024];
  strcpy(dummy,"");
  while ( this->Select( 1 ) > 0 ) {
    if ( FD_ISSET( fd, &read_fd ) ) {
      char *buf;
      buf = (char *) malloc(buflen);
      while ( (cnt = read( fd, buf, buflen ) ) > 0 ) {
	strncat( dummy, buf, cnt );
      }
      free( buf );
    } else { 
      cerr << "Nothing to report" << endl;
    }
  }
  s = dummy;
  tcflush(fd, TCIOFLUSH);
  return 0;
}




/*
char* Serial::BinStepDie(int color)
{
  char command[255]="BinStepDie ";
  char* colorS;
  FILE *commr;
  FILE *commw;
  
  commw = fopen (ser1,"w");
  commr = fopen (ser1,"r");
  
  colorS= int_to_string(color);
  strcat(command,colorS);
  strcat(command,"\n");
  
  fputs(command,commw);
  fclose (commw);         
  
  fgets (resp, 255, commr);
  fclose (commr);
  
  respp=resp;
  return respp;
  // Die Antwort des Probers gibt die neue Position des Chucks an bezogen auf die Wafermap:
  // "0:{x} {y} 0 0"
  // Wenn der Wafer abgearbeitet ist bleibt der Chuck stehen und die Antwort des Probes lautet:
  // "703:End of wafer"
  // Wenn die Wafermap nicht aktiviert ist:
  // "713:WaferMap paused or not running"
}

*/
