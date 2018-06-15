//implementatin of measuremt class for keithley multimeter
//
#include <cstdio>
#include <nullstream.h>
#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <stage2_conf.h>
#ifndef COMMON
#include "common.h"
#define COMMON
#endif
#include "logger.h"


using namespace std;



int main(){
	Logger& log = Logger::instance(Logger::INFO, "");
	log(Logger::INFO) << "Testbench for stage2_conf started ...";	
	stage2_conf* conf=new stage2_conf();
	conf->readFile("hicann_cfg.xml");
	conf->getHicann(0);	
	std::vector<int> line = conf->getFgLine(FGTL,0);
	for( uint i = 0; i < 10; i += 1)
	{
		log(Logger::INFO)<<"Value at position "<<i<<" is:"<<line[i];
	}
	
	return 1;
}
	
