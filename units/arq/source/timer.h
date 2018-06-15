/*
	Stefan Philipp

	timer.h

	Simple timer abstract class
	usable from systemc if calling tick() function periodically.
	Override this class with threads, system timers etc if required

	Created: Feb 2009, Stefan Philipp
*/


#ifndef _TIMER_H_
#define _TIMER_H_

extern "C" {
#include <unistd.h>
//#include <pthread.h>
}
#include <iostream>
#include <string>

#include "nullstream.h"

class Timer
{
	private:
		std::string name;
		bool enabled;
		int  value;
		bool timeout;
		int  max;
		void SetTimeout();
		int debug_level;
		std::ostream & dbg(int l) { if (debug_level>=l) return std::cout<<name<<": "; else return NULLOS; }
	public:
		Timer(std::string n, int m) : name(n), enabled(false), timeout(false), max(m) { debug_level=0; };
		void SetDebugLevel(int l) { debug_level=l; }
		void SetTimeoutVal(int t);
		void Start();
		void Continue();
		void Stop();
		bool Timeout() { return timeout; };
		void Ack();
		void Tick();
};

#endif
