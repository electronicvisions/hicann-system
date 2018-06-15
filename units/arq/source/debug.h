/*
	Stefan Philipp, KIP, UNI Heidelberg

	debug.h
	
	implements basic debug functions with level and 
	classname output.
	
	USAGE
	
	derive other objects from this
*/	


#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <string>
#include <iostream>

#include "nullstream.h"

class Debug
{
	protected:
		int debug_level;
		std::string _ClassName;
		virtual std::string ClassName() { return _ClassName; };
		virtual std::ostream & dbg() { return std::cout << _ClassName << ": "; }
	virtual std::ostream & dbg(int level) { if (this->debug_level>=level) return dbg(); else return NULLOS; }
	public:
		Debug() : debug_level(0), _ClassName("Debug") {};
		Debug(std::string cn) : debug_level(0), _ClassName(cn) {};
		virtual ~Debug() {};
		virtual void SetDebugLevel(int l) { debug_level=l; };
};

#endif
