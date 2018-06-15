# Makefile for test of measurement class
# 01.02.20010, Sebastian Millner KIP
#
# compare hicann-test Makefile
#
#
MEASURE = $(ICPRO_DIR)/units/measure
# root of HICANN repository
HICANN  = $(ICPRO_DIR)/resources/s_hicann1
# system simulation root
SYSIM   = $(ICPRO_DIR)/resources/s_systemsim
# jtag interface control sources
JTAGLIB = $(ICPRO_DIR)/units/jtag_lib
# dnc interface control sources
DNCLIB  = $(ICPRO_DIR)/units/dnc_lib
# arq protocol
ARQ     = $(ICPRO_DIR)/units/arq
# stage2 hardware abstraction layer
S2HAL   = $(ICPRO_DIR)/units/stage2_hal

MEASSRCS = keithley.cpp MeasKeithley.cpp


PROG = keithley

LINKFLAGS = -lm -lstdc++ -pthread -lrt

OBJS =$(SRCS:.cpp=.o)

CCACHE = $(shell test -x /usr/bin/ccache && echo "ccache")

CXX    = g++

CFLAGS = -DLINUX -DBINARY -Wall -ggdb -Werror
# arq protocol
ARQ_SEARCH_PATH := -I $(ARQ)/source
ARQ_SOURCES = $(ARQ)/source/timer.cpp \
	      $(ARQ)/source/nullstream.cpp

# hardware includes
HICANN_SEARCH_PATH := -I $(HICANN)/units/hicann_top/genheader

# JTAG include path
JTAG_SEARCH_PATH := -I $(JTAGLIB)/source

# Stage 2 sources include path
S2HAL_SEARCH_PATH := -I $(S2HAL)/source

# Measure sources
MEASURE_SEARCH_PATH := -I $(MEASURE)/source

INCPATH = -I. -I/usr/include -I $(ICPRO_DIR)/global_src/systemc \
          $(S2HAL_SEARCH_PATH) \
	  $(ARQ_SEARCH_PATH) \
	  $(HICANN_SEARCH_PATH) \
	  $(MEASURE_SEARCH_PATH) \
	  $(JTAG_SEARCH_PATH)

SRCS = $(foreach SRC,$(MEASSRCS),$(MEASURE)/source/$(SRC)) $(ARQ_SOURCES)


#########################################################

all: $(PROG)

#linking
$(PROG):$(OBJS)
	$(CXX) $(INCPATH) $(CFLAGS) $(OBJS) $(LIBS) -o $@ $(LINKFLAGS)

#objects
$(OBJS):%.o:%.cpp
	$(CCACHE) $(CXX) $(CFLAGS) $(INCPATH) -c $< -o $@

clean:
	rm -f $(OBJS)
	rm -f $(PROG) 
