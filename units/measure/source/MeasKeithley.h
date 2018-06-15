/*
 * Class to do measurements with Keithley Multimeter. 
 *
 * @author: Sebstian Millner, KIP
 * @date: 01.02.2010
 * @email: sebastian.millner@kip.uni-heidelberg.de
 *
 */

#include "measure.h"
#include <fstream>
#include <iostream>


class MeasKeithley: public Measure
{
	private:
		virtual std::string ClassName() { return "MeasureKeithly"; };
		std::fstream in;
		std::fstream out;
   		char* buffer;
	
	public:
		MeasKeithley();
		~MeasKeithley();
		bool connect(const char * filename);
		
		float getVoltage();
		bool isConnected();
};
