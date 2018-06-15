#include "MeasKeithley.h"
#ifndef COMMON
#include "common.h"
#define COMMON
#endif
#include "debug.h"
using namespace std;

extern Debug dbg;

MeasKeithley::MeasKeithley(){
	buffer = new char[100];
	_ClassName="MeasKeithley";

}

MeasKeithley::~MeasKeithley(){
	/*
	if(isConnected()){
		in.close();
		out.close();
	}*/
}


bool MeasKeithley::connect(const char * filename){
	in.open(filename);
	out.open(filename);
	return isConnected();	
		
}

bool MeasKeithley::isConnected(){
	if(out && in){
		dbg(10)<<"Keithley Multimeter is Connected"<<endl;
		return 1;
	} else { 
		dbg(0)<<"ERROR:Keithley Multimeter is not Connected"<<endl;
		return 0;
	}
}

float MeasKeithley::getVoltage(){
	out<<"MEASure:VOLTage:DC?"<<endl;
	in.read(buffer, 15);
	//in>>buffer;	
	dbg(10)<<buffer<<endl;
	//in.get(buffer, 100);
	//in.sync();
	return atof(buffer);
}
