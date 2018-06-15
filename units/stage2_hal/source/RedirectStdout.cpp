#include "RedirectStdout.h"

extern "C" {
#include <unistd.h>
}

RedirectStdout::RedirectStdout(int targetfd) :
	pos(),
	fd(-1)
{
	fflush(stdout);
	fgetpos(stdout, &pos);
	fd = dup(fileno(stdout));
	dup2(targetfd, fileno(stdout));
	close(targetfd);
}

RedirectStdout::~RedirectStdout() {
	fflush(stdout);
	dup2(fd, fileno(stdout));
	close(fd);
	clearerr(stdout);
	fsetpos(stdout, &pos);
}
