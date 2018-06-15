#pragma once

extern "C" {
#include <sys/types.h>
}

struct HandleCommFreezeError {
	HandleCommFreezeError();
	~HandleCommFreezeError();

private:
	bool isParent();

	struct RedirectStdout * redirector;
	int mypipe[2];
	pid_t child;
	pid_t parent;
};
