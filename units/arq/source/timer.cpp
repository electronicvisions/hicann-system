/*
	Stefan Philipp

	timer.cpp

	Simple timer abstract class
	usable from systemc if calling tick() function periodically.
	Override this class with threads, system timers etc if required

	Created: Feb 2009, Stefan Philipp
*/

#include <unistd.h>
#include <iostream>
#include <string>

#include "timer.h"

using namespace std;

void Timer::SetTimeoutVal(int t)
{
	if (value>t) value=t;
	max=t;
}

void Timer::Start()
{
	dbg(5) << "started" << endl;
/*
	pthread_mutex_lock(&timer_mutex);
	sendtimer->start(SEND_TIMEOUT_US, TimerBase::TM_SINGLE_SHOT);
	pthread_mutex_unlock(&timer_mutex);
*/
	value = max;
	enabled = true;
	// timeout = false;	// hold timeout if not already processed
}

void Timer::Continue()
{
	if (!enabled) Start();
}

void Timer::Stop()
{
	dbg(5) << "stopped" << endl;
/*
	pthread_mutex_unlock(&timer_mutex);
	sendtimer->stop();
	pthread_mutex_lock(&timer_mutex);
*/
	timeout = false;
	enabled = false;
}

void Timer::SetTimeout()
{
	dbg(5) << "TIMEOUT" << endl;
	// wakeup send thread and continue
//	pthread_mutex_lock(&send_mutex);
//	pthread_cond_signal(&send_cond);
//	pthread_mutex_unlock(&send_mutex);
	timeout = true;
}

void Timer::Ack()
{
	timeout = false;
}

void Timer::Tick()
{
	// dbg(5) << "Tick" << endl;
	if (enabled)
	{
		if (!value) {
			Stop();
			SetTimeout();
		}
		else value--;
	}
}
