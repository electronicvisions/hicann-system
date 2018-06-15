#pragma once

extern "C" {
#include <sys/types.h> // pid_t
}

#include "FPGAConnectionId.h"


struct HMFRunLog {
	// we identify usage by the connection information
	HMFRunLog(FPGAConnectionId const &);
	~HMFRunLog();

	bool operator==(HMFRunLog const &) const;
	friend std::size_t hash_value(HMFRunLog const &);

#if __cplusplus >= 201103L
	HMFRunLog() = delete;
	HMFRunLog(HMFRunLog const &) = delete;
	HMFRunLog(HMFRunLog &&) = delete;
	HMFRunLog operator=(HMFRunLog const &) = delete;
	HMFRunLog& operator=(HMFRunLog &&) = delete;
#else
private:
	HMFRunLog(HMFRunLog const &);
	HMFRunLog operator=(HMFRunLog const &);
public:
#endif

private:
	static char const lockdir[]; // /var/lock/hicann
	static void gracefulShutdown(int signal);

	FPGAConnectionId const connid;
	std::string lockfilename;
};
