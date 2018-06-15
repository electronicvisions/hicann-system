#!/usr/bin/python

__all__ = ['HMFLock']

# global stuff
lockdir = "/var/lock/hicann"
logfile_path = '/var/log/hmf.log'

L_PID = 16
L_PPID = 32

EXIT_SUCCESS = 0
EXIT_FAILURE = 1

DEBUG = False

from sys import stdout, stderr
from time import sleep, ctime, time
from os import path, listdir, getpid, getppid
import ctypes, errno
lib = ctypes.CDLL("liblockfile.so")


class HMFLock:
    """
    Manage HMF Wafer/FPGA Locks (wrapper class to low-level shit).
    """
    def __init__(self, ip, port, retry_count = 0, retry_every = 1):
        """
        @param ip           IP of the FPGA
        @param port         JTAG port
        @param retry_count  Max try count [default = 0]
        @param retry_every  Lock try frequency [default = 1s]
        """
        if not isinstance(ip, str):
            if hasattr(ip, 'to_string'):
                ip = ip.to_string()
            else:
                ip = str(ip)
        if not isinstance(port, str):
            port = str(port)
        self.ip = ip
        self.port = port
        self.retry_count = retry_count
        self.retry_every = retry_every

    def lock(self):
        return try_lock_and_retry(self.ip, self.port, self.retry_count, self.retry_every, L_PID)

    def unlock(self):
        return unlock(self.ip, self.port)

    def owner(self):
        return check_if_owner(self.ip, self.port)

    def print_status(self):
        command_status(self.ip, self.port)




### helper functions
def check_pid_exists(pid):
    import errno
    from os import kill
    try:
        kill(pid, 0)
    except OSError, e:
        return e.errno == errno.EPERM
    else:
        return True

def get_username_from_pid(pid):
    from pwd import getpwuid
    UID, EUID = 1, 2
    for ln in open('/proc/%d/status' % pid):
        if ln.startswith('Uid:'):
            uid = int(ln.split()[UID])
            pwuid = getpwuid(uid).pw_name
    return pwuid

def get_processname_from_pid(pid):
    pname = ''
    dirname = '/proc/%d' % pid
    filename = dirname + '/cmdline'
    with open(filename) as fd:
        cmdline = ' '.join(fd.readlines())
        pname = cmdline.replace('\0', ' ').rstrip()
    return pname, ctime(path.getctime(dirname))


def get_pid_from_fd(fd):
    return int(fd.readlines()[0].rstrip())

def get_pid_from_lockfile(lockfile):
    assert isinstance(lockfile, str)
    fd = open(lockfile, 'r')
    pid = get_pid_from_fd(fd)
    fd.close()
    return pid

def get_lockfile(ip, port):
    from os.path import join
    return join(lockdir, ip + '-' + port + '.lock')

def check_if_owner(ip, port):
    """return True if lockfile belongs to us (me or parent)"""
    pid = None
    valid = False
    lockfile = get_lockfile(ip, port)
    try:
        pid = get_pid_from_lockfile(lockfile)
        valid = lib.lockfile_check(lockfile, L_PID) == 0
    except IOError:
        pass # ignore non-existing lockfile
    if valid and (pid in [getpid(), getppid()]):
        return True
    return False


def modify_lock(ip, port, create, flags = L_PPID):
    from os import listdir, getpid, getppid
    from os.path import isfile
    lockfile = get_lockfile(ip, port)
    ret = -1
    if create: # check or create lockfile
        if check_if_owner(ip, port):
            return EXIT_SUCCESS
        ret = lib.lockfile_create(lockfile, 0, flags)
    else: # remove lockfile
        fd = None
        try:
            fd = open(lockfile, 'r')
        except IOError as e: # does not exist...
            if e.errno == errno.ENOENT:
                return EXIT_SUCCESS
            else:
                print "unknown error:", e.errno
                return EXIT_FAILURE
        ret = lib.lockfile_check(lockfile, flags)
        if ret == -1: # invalid lockfile!
            ret = lib.lockfile_remove(lockfile)
        else:
            pid = get_pid_from_fd(fd)
            if pid == getpid() or pid == getppid():
                ret = lib.lockfile_remove(lockfile)
                return EXIT_SUCCESS
            else:
                ret = -1
    if ret != 0:
        if DEBUG:
            what = create and "creation" or "removal"
            print "Lockfile %s of %s-%s failed" % (what, ip, port)
        return EXIT_FAILURE
    return EXIT_SUCCESS


def try_lock_and_retry(ip, port, retry_count, retry_every, flags = L_PPID):
    # try and retry
    while True:
        ret=modify_lock(ip, port, True, flags)
        if (ret == EXIT_SUCCESS):
            if DEBUG:
                print "Hardware lock acquired: %s %s" % (ip, port)
            return ret                  # SUCCESS

        if (retry_count != None):
            retry_count -= 1
            if retry_count < 0:
                if DEBUG:
                    print "Hardware lock could not be acquired: %s %s" % (ip, port)
                break

        stdout.flush()
        sleep(retry_every)         # SLEEP
    return EXIT_FAILURE

def unlock(ip, port):
    # try and retry
    ret=modify_lock(ip, port, False)
    if (ret == EXIT_SUCCESS):
        if DEBUG:
            print "Hardware lock released: %s %s" % (ip, port)
        return ret
    else:
        if DEBUG:
            print "Could not relase lock: %s %s" % (ip, port)
        return ret


### command handlers
def command_status(*args):
    if len(args) > 2:
        raise Exception("Too many arguments: [ip] [port] (both are optional parameters)")
    import ctypes
    lib = ctypes.CDLL("liblockfile.so")
    from os import listdir
    from os.path import isfile, join
    files = [ join(lockdir,f) for f in listdir(lockdir) if isfile(join(lockdir,f)) ]
    files = [ f for f in files if '-'.join(args) in f ] # filter by argument :)
    stats = [ lib.lockfile_check(f, L_PID) for f in files]
    ret = EXIT_SUCCESS
    if 0 in stats:
        for (s,f) in zip(stats, files):
            if s != 0: continue
            try:
                pid = get_pid_from_lockfile(f)
            except IndexError:
                # ignore empty files...
                continue
            owner = get_username_from_pid(pid)
            pname, pctime = get_processname_from_pid(pid)
            print "%s locked by %s (pid %s, \"%s\", %s)" % (f, owner, pid, pname, pctime)
            ret = EXIT_FAILURE
    return ret


def command_log(*args):
    if len(args) > 2:
        raise Exception("Too many arguments: [ip] [time period in s] (optional parameter)")
    ip = ''
    max_ago = float('inf')
    if len(args) >= 1:
        ip      = args[0]
    if len(args) == 2:
        max_ago = float(args[1])

    from pyparsing import Word, alphas, Suppress, Combine, nums, string, Optional, Regex, ParseException
    # define line in (sys)log
    month = Word(string.uppercase, string.lowercase, exact=3)
    integer = Word(nums)
    serverDateTime = Combine(month + " " + integer + " " + integer + ":" + integer + ":" + integer)
    hostname = Word(alphas + nums + "_" + "-")
    daemon = Word(alphas + nums + "/" + "-" + "_") + Optional(Suppress("[") + integer + Suppress("]")) + Suppress(":")
    message = Regex(".*")
    bnf = serverDateTime + hostname + daemon + message

    from collections import deque
    import re, time
    last_access = {}
    tail_n = 100
    for line in deque(open(logfile_path), tail_n):
        try:
            fields = bnf.parseString(line)
        except ParseException:
            continue
        else:
            m = re.search('requests (\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3})', fields[-1])
            if m:
                #print fields[0], m.group(1)
                cur = time.localtime()
                # guess year...
                st = time.strptime(fields[0] + " %s" % cur.tm_year, "%b %d %H:%M:%S %Y")
                if st > cur: # ok, re-guess
                    st = time.strptime(fields[0] + " %s" % (cur.tm_year - 1), "%b %d %H:%M:%S %Y")
                if (st > cur):
                    raise Exception("HMF logfile seems too old!?!")
                last_access[m.group(1)] = st

    ips = [ key for key in last_access.keys() if ip in key ]
    access_in_period = [ ((time.mktime(cur)-time.mktime(t)) <= max_ago) for r,t in last_access.items() ]
    if ips and any(access_in_period):
        print "Previous accesses:"
        for (resource, timestamp), state in zip(last_access.items(), access_in_period):
            if not state: continue
            if not resource in ips: continue
            print "\t%s was accessed on %s (%.1fs ago)" % (resource, time.asctime(timestamp), time.mktime(cur) - time.mktime(timestamp))
        return EXIT_FAILURE
    return EXIT_SUCCESS


def command_lock(*args):
    if len(args) != 2:
        raise Exception("Too few arguments; please specify IP and port number")
    ip, port = args
    return modify_lock(ip, port, True)


def command_unlock(*args):
    if len(args) != 2:
        raise Exception("Too few arguments; please specify IP and port number")
    ip, port = args
    return modify_lock(ip, port, False)


def command_retrylock(*args):
    from argparse import ArgumentParser
    from math import sqrt

    default_repeat_every = 30

    # arg parse type tools
    def non_neg_int(value):
        ivalue = int(value)
        if ivalue < 0: raise ValueError()
        return ivalue

    def timevalue(value):
        try:
            try: ivalue = int(value)
            except:
                value, spec = value[:-1], value[-1]
                ivalue = int(value)
                specs = {
                        's' : 1,
                        'm' : 60,
                        'h' : 3600,
                        'd' : 24 * 3600
                }
                ivalue *= specs[spec]
        except:
            raise ValueError()
        return ivalue


    parser = ArgumentParser(
        prog        = "%s retrylock" % sys.argv[0],
        description = """
        The command "retrylock" tries to get a hardware lock like the "lock"
        command, but automatically retries on failure. The default behaviour
        will retry to get the lock every %s seconds. It will continue until a
        lock is successfully acquired. Default behaviour can be changed using
        the --retry-{every,count,for} options.

        If you use the retry-for option retries will happen every sqrt(time in
        seconds), unless you specify either retry-every or retry-count.
        Specifiying all three options will raise an error.

        Time values are generally specified in seconds. They can optionally be followed
        by one of {s,m,h,d}.
        """ % default_repeat_every,
        epilog="Author: Kai Husmann <kai.husmann@kip.uni-heidelberg.de> (KHS)"
    )
    parser.add_argument('ip', help = 'the IP of the hardware, required.')
    parser.add_argument('port', help = 'the port to lock, required.')
    parser.add_argument('--retry-every', '-s', help = 'sleep phase between each retry (in seconds, defaults to %ss' % default_repeat_every, default=None, type=timevalue)
    parser.add_argument('--retry-count', '-c', help = 'how often to retry (defaults to forever)', default=None, type=int) # neg value/0: don't retry.
    parser.add_argument('--retry-for', '-t', help = 'maximal retry duration until lock failure', default=None, type=timevalue)

    argx = parser.parse_args(args)

    # combine options
    if (argx.retry_for != None):
        if argx.retry_every != None:
            if argx.retry_count != None: raise "All three option together won't work"
            argx.retry_count = argx.retry_for / argx.retry_every
        elif argx.retry_count != None:
            argx.retry_every = argx.retry_for / argx.retry_count
        else:
            assert (argx.retry_count == None) and (argx.retry_every == None)
            argx.retry_every = sqrt(argx.retry_for)
            argx.retry_count = argx.retry_every
    if argx.retry_every == None: argx.retry_every = default_repeat_every
    print argx

    return try_lock_and_retry(argx.ip, argx.port, argx.retry_count, argx.retry_every)



builtin_commands = {
    'status'    : command_status,
    'log'       : command_log,
    'lock'      : command_lock,
    'unlock'    : command_unlock,
    'retrylock' : command_retrylock, # KHS: like lock but implementing automatic retries
}


def handle_options(args):
    # argparse sux
    usage = "Usage: %s [%s]*" % (sys.argv[0], ", ".join(builtin_commands.keys()))
    if len(args) <= 1:
        print usage
        exit(1)
    ret = []
    for arg in args[1:]:
        if arg in builtin_commands.keys():
            ret.append([builtin_commands[arg]])
        else:
            # some positional parameter
            if ret:
                ret[-1].append(arg)
            else:
                if arg in ['-h', '--help']:
                    print usage
                    exit(1)
                raise Exception("command \"%s\" unknown" % arg)
    return ret


if __name__ == "__main__":
    import sys
    cmds = handle_options(sys.argv)
    if len(cmds) > 1:
        raise Exception("Multiple command feature disabled as it is not safe considering dead locks. If you want it implemented mail KHS/ECM.")
    ret = EXIT_SUCCESS
    for cmd in cmds:
        if cmd[1:]:
            ret = cmd[0](*cmd[1:])
        else:
            ret = cmd[0]()
        if ret != EXIT_SUCCESS:
            break
    exit(ret)
