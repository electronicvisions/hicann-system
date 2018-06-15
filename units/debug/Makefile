

JTAG_PATH     ?= ../jtag_lib
LIBS          ?= $(JTAG_PATH)/bin/jtag_lib.$(shell uname -m).a -ldl
INCLUDES      ?= -I$(JTAG_PATH)/source -I.


all:
	g++ $(INCLUDES) monitor_jtag.cpp $(LIBS) -o monitor_jtag

