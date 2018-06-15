#!/usr/bin/python

import socket
import sys

try:
	value = int(sys.argv[1])
except:
	sys.stderr.write("\nMust give SYS_START value!\n\n")
	sys.exit(1)

HOST = "192.168.2.1"
PORT = 1800

if value == 0:
	DATA = "\x55\x00\x00\x00"
else:
	DATA = "\x55\x00\x00\x55"

try:
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
except socket.error, msg:
	sys.stderr.write("[ERROR-SOCK] %s\n" % msg[1])
	sys.exit(1)

bsent = sock.sendto(DATA,(HOST,PORT))

sock.close()

sys.exit(bsent)

