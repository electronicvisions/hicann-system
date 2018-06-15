#include "HMFRunLog.h"

#include <sstream>
#include <fstream>

extern "C" {
#include <unistd.h>
}

#ifndef NCSIM
#include <csignal>
#include <cerrno>
extern "C" {
#include <syslog.h>
#include <lockfile.h>
#include <sys/stat.h>
}

#include "logger.h"

// from cerrno header
extern char * program_invocation_short_name;
#endif // !NCSIM

static pid_t pid() {
#ifdef NCSIM
	static pid_t pid = 1234;
#else
	static pid_t pid = getpid();
#endif // NCSIM
	return pid;
}

static pid_t ppid() {
#ifdef NCSIM
	static pid_t pid = 1234;
#else
	static pid_t pid = getppid();
#endif // NCSIM
	return pid;
}

// get pid of locked process
static pid_t read_lockpid(std::string & lockfilename) {
	std::ifstream lockfile(lockfilename.c_str());
	const pid_t error = 0;
	if (! lockfile.is_open())
		return error;
	pid_t lockpid;
	lockfile >> lockpid;
	lockfile.close();
	return lockpid;
}

static void log(std::string const & m) {
#ifndef NCSIM
	// ECM (2016-03-30): We use
	//  * LOG_NOWAIT (cf. SIGCHLD and blocking syslog behavior)
	//  * LOG_NDELAY, LOG_PERROR and LOG_PERROR cf. man page
	openlog(NULL, (LOG_NOWAIT|LOG_NDELAY|LOG_PID), LOG_USER);
	syslog(LOG_INFO, m.c_str());
	closelog();
#endif
}

static void start() {
	std::stringstream msg;
	msg << "HMF (pid " << pid() << ", name " << program_invocation_short_name << ") started";
	log(msg.str());
}

static void stop() {
	std::stringstream msg;
	msg << "HMF (pid " << pid() << ", name " << program_invocation_short_name << ") exiting";
	log(msg.str());
}

#ifndef NCSIM
struct sigaction new_action, old_actions[7];
#endif

// other stuff
char const HMFRunLog::lockdir[] = "/var/lock/hicann";

namespace {
static void _get_process_parent_id(const pid_t pid, pid_t * ppid) {
	char buffer[BUFSIZ];
	sprintf(buffer, "/proc/%d/stat", pid);
	FILE* fp = fopen(buffer, "r");
	if (fp) {
		size_t size = fread(buffer, sizeof (char), sizeof (buffer), fp);
		if (size > 0) {
			// See: http://man7.org/linux/man-pages/man5/proc.5.html section /proc/[pid]/stat
			strtok(buffer, " "); // (1) pid  %d
			strtok(NULL, " "); // (2) comm  %s
			strtok(NULL, " "); // (3) state  %c
			char * s_ppid = strtok(NULL, " "); // (4) ppid  %d
			*ppid = atoi(s_ppid);
		}
		fclose(fp);
	}
}
} // anonymous namespace

HMFRunLog::HMFRunLog(FPGAConnectionId const & cid) :
	connid(cid)
{
#ifndef NCSIM
	new_action.sa_handler = HMFRunLog::gracefulShutdown;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	struct sigaction tmp_action;

	// save signal handler for:
	// SIGILL, SIGABRT, SIGBUS, SIGFPE, SIGSEGV, SIGINT, SIGTERM
#define ADDSIGNALHANDLER(IDX, SIGNAL) \
	sigaction (SIGNAL, NULL, &tmp_action); \
	/* dont register twice */ \
	if (tmp_action.sa_handler != new_action.sa_handler) \
		sigaction (SIGNAL,  &new_action, &old_actions[IDX]);
	ADDSIGNALHANDLER(0, SIGILL);
	ADDSIGNALHANDLER(1, SIGABRT);
	ADDSIGNALHANDLER(2, SIGBUS);
	ADDSIGNALHANDLER(3, SIGFPE);
	ADDSIGNALHANDLER(4, SIGSEGV);
	ADDSIGNALHANDLER(5, SIGINT);
	ADDSIGNALHANDLER(6, SIGTERM);
#undef ADDSIGNALHANDLER

	start();

	std::stringstream msg;
	msg << "requests " << connid.get_fpga_ip();
	log(msg.str());

	// TODO: after testing, remove MPIMODE thing here
#ifndef MPIMODE
	// check for and create lockfile
	int status = mkdir(lockdir, 0777); // world (un-)lockable hardware
	if (! ((status == 0) || (errno == EEXIST))) {
		std::stringstream e;
		e << lockdir << " does not exist/not accessible (needed for lock files)";
		throw std::runtime_error(e.str());
	}
	if (status == 0)
		chmod(lockdir, 0777);

	{
		std::stringstream ss;
		ss << lockdir << "/" /* << program_invocation_short_name << "-" */
		   << connid.get_fpga_ip().to_string() << "-"
		   << connid.get_jtag_port() << ".lock"; // ip + port == unique...
		lockfilename = ss.str();
	}

	// lock existing? do we own the lock already? check it!
	bool own_lock = false;
	if (lockfile_check(lockfilename.c_str(), L_PID) != -1) {
		std::stringstream emsg;
		emsg << "Hardware locked! (Valid) lockfile found: " << lockfilename;

		pid_t lockpid = read_lockpid(lockfilename);
		if (lockpid > 0) {
			emsg << "(pid: " << lockpid;

			// proc file => get cmdline
			std::stringstream procfilename;
			procfilename << "/proc/" << lockpid << "/comm";
			std::ifstream procfile(procfilename.str().c_str());
			if (procfile.is_open()) {
				std::string name;
				procfile >> name;
				procfile.close();
				emsg << "(" << name << ")";
			}
			pid_t lock_parent = 0;
			_get_process_parent_id(lockpid, &lock_parent);
			// lock owner might be parent, too!
			own_lock = (pid() == lockpid) || (ppid() == lockpid) || (ppid() == lock_parent);
			emsg << ")";
		}

		if (!own_lock) // it's our own lockfile (multiple connections!)
			throw std::runtime_error(emsg.str());
	}
	if (!own_lock && lockfile_create(lockfilename.c_str(), 0, L_PID) != L_SUCCESS)
		throw std::runtime_error("Could not create lockfile (needed for HW access)!");
#endif // MPIMODE
#endif
}

HMFRunLog::~HMFRunLog() {
#ifndef NCSIM
	std::stringstream msg;
	msg << "relinquishes " << connid.get_fpga_ip(); // ECM: already destroyed connid?
	log(msg.str());
	stop();
	// remove if really "own" lockfile (not inherited from parent)
	if (pid() == read_lockpid(lockfilename))
		lockfile_remove(lockfilename.c_str()); // ok to remove non-existing file
#endif
}

bool HMFRunLog::operator==(HMFRunLog const & b) const {
	return (connid == b.connid);
}

#include <boost/functional/hash.hpp>
std::size_t hash_value(HMFRunLog const & h) {
	std::size_t s = 0;
	boost::hash_combine(s, h.connid.get_fpga_ip().to_ulong());
	boost::hash_combine(s, h.connid.get_jtag_port());
	return s;
}

void HMFRunLog::gracefulShutdown(int s) {
#ifndef NCSIM
	// ECM (2016-03-30): We must not call syslog() (used by stop()) from a
	// signal handler.
	// stop();

	// restore old signal handler
#define RESTORESIGNAL(IDX, SIGNAL) \
sigaction (SIGNAL,  &old_actions[IDX], NULL);
	RESTORESIGNAL(0, SIGILL);
	RESTORESIGNAL(1, SIGABRT);
	RESTORESIGNAL(2, SIGBUS);
	RESTORESIGNAL(3, SIGFPE);
	RESTORESIGNAL(4, SIGSEGV);
	RESTORESIGNAL(5, SIGINT);
	RESTORESIGNAL(6, SIGTERM);
#undef RESTORESIGNAL

	raise(s);
#endif
}
