#include "HandleCommFreezeError.h"

#include <cassert>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" {
#include <unistd.h>
#include <setjmp.h>
#include <sys/wait.h>
}

#include "RedirectStdout.h"


// signal handling stuff
static sigjmp_buf returnFromSignalHandler;
static struct sigaction old_handler;
void sig_handler(int signal, siginfo_t*, void*) {
	if (signal == SIGUSR1) {
		sigaction(SIGUSR1, &old_handler, NULL);
		siglongjmp(returnFromSignalHandler, 42);
	}
}

// mark errors (not in-class, we need access during error handling)
static bool recvfromError;


HandleCommFreezeError::HandleCommFreezeError() :
		redirector(NULL)
{
	recvfromError = false;
	assert(pipe(mypipe) == 0);

	// fork or die
	child = fork();
	assert(child >= 0);

	if (child == 0) {
		child  = getpid();
		parent = getppid();
	} else {
		parent = getpid();
	}

	if (isParent()) {
		redirector = new RedirectStdout(mypipe[1]);
		memset(returnFromSignalHandler, 0, sizeof(sigjmp_buf));
		int val = sigsetjmp(returnFromSignalHandler, true);
		if (!val) { // normal invocation
			struct sigaction handler;
			memset(&handler, 0, sizeof(handler));
			handler.sa_sigaction = sig_handler;
			handler.sa_flags = SA_SIGINFO;
			sigaction(SIGUSR1, &handler, &old_handler);
		}
		else { // longjumped
			recvfromError = true;
		}
	} else {
		// CHILD checks for timeout/error and kills parent in case of fuckup
		if (close(mypipe[1]) != 0) {
			assert(false && "couldn't close pipe");
		}

		bool connection_failure = false;

		char buffer[1024];
		while(true) {
			memset(buffer, 0, sizeof(buffer));
			int ret = read(mypipe[0], buffer, sizeof(buffer)-1);
			if (ret <= 0) {
				// 0 if no writer... process already dead?
				break;
			}

			std::string strbuffer(buffer);
			if (strbuffer.find("connection initialized") == std::string::npos) {
				std::cout << buffer << std::flush; // log output
			}

			// check for error message
			std::string tmp(buffer);
			if (tmp.find("recvfrom") != std::string::npos) {
				connection_failure = true;
				break;
			}
		}

		// if parent alive and other error conditions active... then signal it!
		if (connection_failure) {
			if (kill(parent, 0) == 0) {
				kill(parent, SIGUSR1); // signal to parent
				std::cout << "signal to parent" << std::endl;
			}
		}
		exit(EXIT_SUCCESS);
	}
}


HandleCommFreezeError::~HandleCommFreezeError() {
	assert(isParent());

	close(mypipe[0]); // no error checking, don't care here

	if (kill(child, 0) == 0) {
		usleep(100000); // 0.1s
		kill(child, SIGTERM);
		std::cout << "signal to child" << std::endl;
	}
	int status;
	// wait for killing child to exit
	waitpid(child, &status, 0);

	// parent has redirected stdout/err stuff
	if (redirector)
	delete redirector;

	assert(0 == recvfromError);
}


bool HandleCommFreezeError::isParent() {
	if (getpid() != parent) return false;
	return true;
}
