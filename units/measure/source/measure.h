/*
 * abstract class for measurements. First child ist keithleymm - a clatt to get data from keythleymultimeter 2100
 *
 * @author: Sebstian Millner, KIP
 * @date: 01.02.2010
 * @email: sebastian.millner@kip.uni-heidelberg.de
 *
 */
#include "debug.h"

class Measure: public Debug 
{
protected:
	virtual std::string ClassName() { return "Measure"; };
public:
	virtual float getVoltage()=0;
	virtual bool isConnected()=0;
};
