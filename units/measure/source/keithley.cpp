//implementatin of measuremt class for keithley multimeter
//
#include <stdio.h>
#include <nullstream.h>
#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <MeasKeithley.h>
#ifndef COMMON
#include <common.h>
#define COMMON
#endif
#include <debug.h>


using namespace std;

Debug dbg;

int main(){
	MeasKeithley keithley;
	keithley.SetDebugLevel(10);
	
	keithley.connect("/dev/usbtmc1");
	keithley.getVoltage();
	/*
	fstream keithleyout ("/dev/usbtmc1",fstream::out);

	if(!keithleyout){
		cout<<"keithley not connected"<<endl;
		return 0;
	}
	cout<<"keithley connected"<<endl;
	
	fstream keithleyin ("/dev/usbtmc1",fstream::in);

	keithleyout<<"MEASure:VOLTage:DC?"<<endl;
	//string buffer;
	
	char * buffer;
	buffer = new char[100];
	
	keithleyin.read(buffer, 100);
	cout<<"test";
	cout.write(buffer, 100);
	cout<<endl;
	double test = atof(buffer);
	cout<<test<<endl;
*//*	
	
		write(file,"*IDN?\n",6);
		actual=read(file,buffer,4000);
		buffer[actual]=0;
		printf("Response:\n%s\n",buffer);
		close(file);
	}*//*
	keithleyout.close();
	keithleyin.close();*/
	return 1;
}
	
