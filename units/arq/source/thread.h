//	Stefan Philipp
//	Electronic Vision(s) group, UNI Heidelberg
//
// 	thread.h
//
//	This file provides objects for a simple and save
//	use of parrallel execution and mutexes.
//
//	--> use same code outside for executables and testbench
//	
//	*	for executables it uses the pthread library
//	*	for simulation, compile with SYSTEMC define, such that 
//		the systemc functions of your simulator do the job.
//
//	july 2009, created, StPh
//	have fun!
// ---------------------------------------------------------


#ifndef _THREAD_H_
#define _THREAD_H_

#ifndef BINARY
#define SYSTEMC
#endif

#ifndef SYSTEMC
extern "C" {
#include <pthread.h>
}
#else
#include <systemc.h>
#endif

#include "debug.h"

// ---------------------------------------------------------
// mutexes
// ---------------------------------------------------------

#ifndef SYSTEMC
// use pthread mutexes
#define MUTEX_INST(mutex) pthread_mutex_t mutex;
#define MUTEX_INIT(pmutex) pthread_mutex_init(pmutex, NULL);
#define MUTEX_FUNC_BOOL(pmutex, func) pthread_mutex_lock(pmutex); bool _res = func; pthread_mutex_unlock(pmutex);
#define MUTEX_FUNC_INT(pmutex, func) pthread_mutex_lock(pmutex); int _res = func; pthread_mutex_unlock(pmutex);
#else
// no mutexes are used in testbench, systemc executes threads one after another
#define MUTEX_INST(mutex) 
#define MUTEX_INIT(pmutex) 
#define MUTEX_FUNC_BOOL(pmutex, func) bool _res = func;
#define MUTEX_FUNC_INT(pmutex, func) int _res = func;
#endif


// ---------------------------------------------------------
// sleeper class for wait/wakeup processes
// ---------------------------------------------------------

class Sleeper 
#ifdef SYSTEMC
: sc_module
#endif
{
#ifndef SYSTEMC
	pthread_mutex_t app_mutex;
	pthread_cond_t app_cond;
#else
	 sc_event wakeup;
#endif
	bool app_sleeping;
public:
	Sleeper() //: Debug("Sleeper")
	{
#ifndef SYSTEMC
		pthread_mutex_init(&app_mutex, NULL);
		pthread_cond_init(&app_cond, NULL);
#else
		//wakeup = false;
#endif
		app_sleeping = false;
	}

	void Sleep()
	{
		std::cout << "AppWait: go sleeping, waiting for ack..." << std::endl;
#ifndef SYSTEMC
		pthread_mutex_lock(&app_mutex);
#endif
		app_sleeping = true;

#ifndef SYSTEMC
		pthread_cond_wait(&app_cond, &app_mutex);
#else
		wait(wakeup);
		//wakeup = false;
#endif

		app_sleeping = false;
#ifndef SYSTEMC
		pthread_mutex_unlock(&app_mutex);
#endif
		std::cout << "AppWait: wakeup" << std::endl;
	}

	void Wakeup()
	{							
		if (app_sleeping)
		{
			std::cout << "waking up" << std::endl;
#ifndef SYSTEMC
			pthread_mutex_lock(&app_mutex);
			pthread_cond_signal(&app_cond);
			pthread_mutex_unlock(&app_mutex);
#else
			wakeup.notify();
			wait (app_sleeping == false);
#endif
		}					
	}
};


// ---------------------------------------------------------
// thread class
// ---------------------------------------------------------

template <class CallbackClass> class Thread : virtual public Debug
{
public:
	typedef void (CallbackClass::*pcallbackfunc)();
private:
    CallbackClass  *user;
    pcallbackfunc  callback;

#ifndef SYSTEMC
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
#endif
	bool term;
	bool halt;
	bool sleeping;

	static void* ThreadCallback(void *arg) 
	{
		((Thread*) arg)->Exec(); 
		return NULL; 
	}
	
public:	
	
	Thread(CallbackClass *cc, pcallbackfunc cb, std::string cn) : Debug(cn)
	{
	    user = cc;
		callback = cb;

		term = false;
		halt = true;
		sleeping = false;
#ifndef SYSTEMC
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&cond, NULL);
		pthread_create(&thread, NULL, ThreadCallback, (void*)this);
#endif
	}

	Thread(CallbackClass *cc, pcallbackfunc cb) : 
		Thread(cc, cb, "Thread") {};
	
	virtual ~Thread() { Terminate(); };
	
	void Halt()
	{
		if (!sleeping)
		{
			halt = true;
			while (!sleeping)
			{
#ifndef SYSTEMC
				pthread_mutex_lock(&mutex);
				pthread_mutex_unlock(&mutex);
#endif
			}
		}
	}
	
	void Start()
	{
		if (sleeping)
		{
			halt = false;
#ifndef SYSTEMC
			pthread_mutex_lock(&mutex);
			pthread_cond_signal(&cond);
			pthread_mutex_unlock(&mutex);
#endif
		}
	}
	
	void Terminate()
	{
		dbg(5) << "end thread" << std::endl;
		term = true;
		Start();
#ifndef SYSTEMC
		pthread_join(thread, NULL);
#endif
	}
	
				
	void Sleep()
	{
		dbg(5) << "go sleeping" << std::endl;
#ifndef SYSTEMC
		pthread_mutex_lock(&mutex);
		sleeping = true;
		pthread_cond_wait(&cond, &mutex);
		sleeping = false;
		pthread_mutex_unlock(&mutex);
#else
		sc_wait();
#endif
		dbg(5) << "waked up" << std::endl;
	}
	
	
	void Exec()
	{
		dbg(0) << "started" << std::endl;
		while(true)
		{
			if (term) break;
			if (halt) Sleep();
			if (term) break;
			if (callback) (user->*(callback))();
#ifndef SYSTEMC
#else
			wait(5, SC_NS);
#endif
		}
		dbg(0) << "terminated" << std::endl;
	}
	
};



#endif
