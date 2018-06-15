#pragma once

#include <cstdio>

class RedirectStdout {
public:
	RedirectStdout(int targetfd);
	~RedirectStdout();

private:
	fpos_t pos;
	int fd;
};
