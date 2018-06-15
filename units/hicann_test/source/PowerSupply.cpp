// *********************************************************
//
// Implementation of the Chiptest class.
// 
//
// *********************************************************
extern "C" {
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
}
#include <sstream>
#include <iomanip>
#include <errno.h>
#include "PowerSupply.h"
#include "Serial.h"

using namespace std;

PowerSupply::PowerSupply( Serial *s ) : 
  serial(s) {
  port = serial->getSer();
};



PowerSupply::~PowerSupply(){}; // standard destructor

//Functions!

string PowerSupply::Clear()
{

  string s = "*CLS\n";

  *serial << s;
  
  string response;

  *serial >> response;

  return response;  
}

string PowerSupply::PowerOn()
{

  string s = "OUTP:STAT 1\n";

  *serial << s;
  
  string response;

  *serial >> response;

  return response;  
}

string PowerSupply::PowerOff()
{
  string s = "OUTP:STAT 0\n";

  *serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::GetCurrent(uint channel)
{
	stringstream ss;
	ss << ":CHAN" << channel << ":MEAS:CURR?\n";
  
	string s = ss.str();

	*serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::SetCurrent(uint channel, double c)
{
	stringstream ss;
	ss << ":CHAN" << channel << ":CURR " << setw(3) << c << "\n";
  
	string s = ss.str();

	*serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::GetVoltage(uint channel)
{
	stringstream ss;
	ss << ":CHAN" << channel << ":MEAS:VOLT?\n";
  
	string s = ss.str();

	*serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::SetVoltage(uint channel, double c)
{
	stringstream ss;
	ss << ":CHAN" << channel << ":VOLT " << setw(3) << c << "\n";
  
	string s = ss.str();

	*serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::ClearProt()
{
	string s = "OUTP:PROT:CLE\n";;

  *serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::GetCurrProt(uint channel)
{
	stringstream ss;
	ss << ":CHAN" << channel << ":PROT:CURR?\n";
  
	string s = ss.str();

  *serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::SetCurrProt(uint channel, bool onoff)
{
	stringstream ss;
	ss << ":CHAN" << channel << ":PROT:CURR " << onoff << "\n";
  
	string s = ss.str();

  *serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::GetState()
{
	string s = "OUTP:STAT?\n";

  *serial << s;
  
  string response;

  *serial >> response;

  return response;
}

string PowerSupply::Command( const string &s ) {
  *serial << s;
  if ( serial->Select( -1 ) >= 0 ) {
//cout << "tst1" << endl; usleep(100000);
    string response = "";
    *serial >> response;
    return response;
  }
  return "Error in PowerSupply::Command\n";
}

