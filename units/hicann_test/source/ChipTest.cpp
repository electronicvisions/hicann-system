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
#include <errno.h>
#include "ChipTest.h"
#include "Serial.h"

using namespace std;

ChipTests::ChipTests( Serial *s ) : 
  serial(s) {
  port = serial->getSer();
};



ChipTests::~ChipTests(){}; // standard destructor

void ChipTests::WaferTest()
{
  string ProbResp = "0";
  string ProbResp4Sep = "nix";
  bool Waferflag = true;
  
  // StartComm();
  ProbResp = ReadMapPosition();
  //  qDebug("ProbResp:  %s",ProbResp); //undefined function
  do
    {
      int z = 25;
      ProbResp4Sep = MoveChuckContact();
      sleep(2);
      ProbResp4Sep = MoveChuckZdown( &z );
      sleep(1);
      ProbResp4Sep = MoveChuckContact();
      sleep(2);
      ProbResp4Sep = MoveChuckZdown( &z );
      sleep(1);
      ProbResp4Sep = MoveChuckContact();	
      sleep(2);
      
      // /////////// START DIE CHECK ////////////////////////
      // add your ASIC test Here 
      // /////////// END DIE CHECK //////////////////////////
      
      
      sleep(8);
      ProbResp4Sep = MoveChuckSeparation();	
      sleep(1);
      int wafernumber;
      ProbResp = BinStepDie( &wafernumber );
      cout << ProbResp << endl;
      
      
    }
  while( ProbResp == "703:End of wafer"
	 && ProbResp == "713:WaferMap paused or not running"
	 && ProbResp == "705:'StepFirstDie' has not been issued" 
	 && ProbResp == "705:'StepFirstDie' has not been issued BinStepDie 1 \n" );  //Stoppe Messung nach letztem Die
  
  // CloseComm();
  
  Waferflag=false;
}


//Functions!

string ChipTests::ReadMapPosition()
{

  string s = "ReadMapPosition \n";

  *serial << s << "\n";
  
  string response;

  *serial >> response;

  return response;  
}


string ChipTests::MoveChuckSeparation()
{
  string s = "MoveChuckSeparation\n";

  *serial << s << "\n";
  
  string response;

  *serial >> response;

  return response;
}

string ChipTests::MoveChuckContact()
{
  string s ="MoveChuckContact\n";
  
  *serial << s << "\n";
  
  string response;
  *serial >> response;

  return response;
  
}

string ChipTests::MoveChuckAlign()
{
  string s ="MoveChuckAlign\n";
  
  *serial << s << "\n";
  
  string response;
  *serial >> response;

  return response;
  
}
string ChipTests::MoveChuckZup(const int *Z)
{

  /*
  string s;
  s = "MoveChuckZ "+ *Z;
  s += " R";
  // + *Z + " R";

  //char comand[255]="MoveChuckZ 50 R\n";
  char command[255]="";
  //  const char *c;
  for ( int i=0; i < s.length(); i++ )
    {
      command[i]=*(s+i);//c[i]
    }
  
  FILE *commr;
  FILE *commw;
  
  commw = fopen (ser1.c_str(),"w");
  commr = fopen (ser1.c_str(),"r");
  
  fputs(command,commw);
  fclose (commw);
  
  fgets (respp, 255, commr);
  fclose (commr);
  
  return respp;
  */
  // rewrite this into c++ language

  string s;
  s = "MoveChuckZ "+ *Z;
  s += " R";
  
  *serial << s << "\n";
  
  string response;
  *serial >> response;

  return response;
}

string ChipTests::MoveChuckZdown(const int *Z)
{
  string s;
  s = "MoveChuckZ ";
  s += *Z;
  s += " R";

  *serial << s << "\n";
  
  string response;
  *serial >> response;

  return response;
}
/*void ChipTests::MoveChuckZ25down()
  {
  char comand[255]="MoveChuckZ -25 R\n";
  FILE*commw;
  commw=fopen(ser1.c_str(),"w");
  fputs(comand,commw);
  
  
  }
*/

char* ChipTests::BinStepDie(const int *color)
{
  //char resp[255]="nochnixdrinn";
  char command[255]="BinStepDie ";
  char* colorS, *respp;
  colorS = int_to_string( color );
  strcat( command, colorS );
  strcat( command, "\n" );
  //qDebug("Send: %s\n",comand);
  
  *serial << command;

  //fflush(commw);
  //  m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  //  m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  
  //        qDebug("Command sent com closed\n");
  /*
    for(int i=0;i<1000;i++)
    {
    printf("feof  %d\n",feof(commr));
    }
  */
  
  // fgets ( respp,255,commr);
  //  m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  //  m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  
  //      qDebug("resp %s\n",resp);
  //respp=resp;
  //printf("Error: in  %d out %d\n",ferror(commw),ferror(commr));
  //  qDebug("respp in BinStepDie ->  %s\n",respp);
  //  fclose (commr);
  //m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  //  fclose(commw);
  //  m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  //  m_mcmt->m_app->flush();
  //  m_mcmt->m_app->sendPostedEvents();
  
  
  
  return respp;
  // Die Antwort des Probers gibt die neue Position des Chucks an bezogen auf die Wafermap:
  // "0:{x} {y} 0 0"
  // Wenn der Wafer abgearbeitet ist bleibt der Chuck stehen und die Antwort des Probes lautet:
  // "703:End of wafer"
  // Wenn die Wafermap nicht aktiviert ist:
  // "713:WaferMap paused or not running"
}

char *ChipTests::int_to_string(const int *number)
{
  int digit=(int)log10((float)*number)+1,i;
  char *char_number;
  char_number=new char[digit];
  for (i=0;i<digit;i++)
    {
      char_number[i]=(int)(((double) *number)/pow(10.0,(double)digit-i-1))+'0';
      number-=(int)((char_number[i]-'0')*pow(10.0,(double)digit-i-1));
    }
  char_number[digit]='\0';
  return char_number;
}




string ChipTests::ReadChuckPosition()
{            
  return this->Command("ReadChuckPosition \n");
}

string ChipTests::Command( const string &s ) {
  *serial << s;
  if ( serial->Select( -1 ) >= 0 ) {
    string response = "";
    *serial >> response;
    return response;
  }
  return "Error in ChipTests::Command\n";
}
