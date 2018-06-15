# Makefile to compile the Stage 2 test software "tests2" which uses the Stage 2 HAL.
#
# created 20.06.08, Andreas Gruebl, KIP
#
# adapted to new repository structure 24.09.09, Andreas Gruebl
#


#################################################################
# project directories:

SYMAP2IC_PATH ?= $(ICPRO_DIR)/../symap2ic


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
# measuring stuff
MEASURE = $(ICPRO_DIR)/units/measure
# configuration class(xmlinput)
STAGE2_CONF = $(ICPRO_DIR)/units/stage2_conf
# the softy logger class
LOGGERDIR = $(SYMAP2IC_PATH)/src/logging/
# directory containing testmode sources
TMSRC   = $(ICPRO_DIR)/units/hicann_test/source

# the Stage 2 test software executable
PROG		= tests2

#################################################################
#basic sources necessary to build testenvironment for HICANN chip, requires only ANSI C++ libs
BASICSRCS	= s2_types.cpp s2comm.cpp s2c_tcpip.cpp s2c_jtagphys.cpp s2c_jtagphys_fpga.cpp \
	          s2c_jtagphys_full.cpp s2c_jtagphys_2fpga.cpp \
		  ctrlmod.cpp synapse_control.cpp l1switch_control.cpp spl1_control.cpp \
		  fg_control.cpp s2ctrl.cpp hicann_ctrl.cpp neuronbuilder_control.cpp \
		  neuron_control.cpp fpga_control.cpp dnc_control.cpp
BASICSRCSD     := $(foreach SRC,$(BASICSRCS),$(S2HAL)/source/$(SRC))

#testmodes requiring only ANSI C++
BASICTM 	= tmag_fpga_i2c.cpp tmag_p_plltest_multi.cpp tmag_switchramtest.cpp tmjs_repeater.cpp tmjs_stdpctrl.cpp \
		tmjs_neuronmem.cpp tm_fgzero.cpp  tm_singleneuron.cpp tm_fgkeithley.cpp tm_fgrowinc.cpp tmsm_neuronout.cpp tmsm_fg.cpp\
		tmsm_neuronsynin.cpp tm_spl1.cpp tmag_hcinit.cpp tm_fgkeithleymean.cpp tm_l2init.cpp tm_iboardv2.cpp
#		tm_arqtest.cpp tm_arqtest_mini.cpp tmag_p_plltest.cpp tmag_trivramtest.cpp \
#                  tmsm_ramtest.cpp tm_switchmx.cpp tm_fgctrl.cpp tm_singleneuron.cpp \
#		  tm_fgkeithley.cpp tm_fgzero.cpp tm_fgrowinc.cpp tm_fgkeithleyrow.cpp \
#		  tm_neuronram.cpp tm_outamp.cpp tmag_fpga_i2c.cpp tm_currentsource.cpp 
#		  tm_fgcalib.cpp tm_fgkeithleymean.cpp
#BASICTM 	= tmag_fpga_i2c.cpp tmag_arqtest_multi.cpp tmsm_ramtest.cpp tmag_p_plltest.cpp \
#                  tmag_p_plltest_multi.cpp
BASICTMD       := $(foreach TM,$(BASICTM),$(TMSRC)/$(TM))

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
MEASURE_SRC = $(MEASURE)/source/MeasKeithley.cpp

# Stage2_conf sources
STAGE2_CONF_SEARCH_PATH :=-I $(STAGE2_CONF)/source
STAGE2_CONF_SRC := $(STAGE2_CONF)/source/stage2_conf.cpp


#################################################################
# collect sources...
SRCS = $(TMSRC)/$(PROG).cpp $(BASICSRCSD) $(ARQ_SOURCES) $(BASICTMD) $(MEASURE_SRC) $(STAGE2_CONF_SRC) $(LOGGERDIR)logger.cpp

LINKFLAGS = -lm -lstdc++ -pthread -lrt -lboost_thread-mt -lxerces-c -ldl

#LINKFLAGS = -lm -lstdc++ -pthread -lrt -lgomp

OBJS 		= $(SRCS:.cpp=.o)

files := $(foreach dir,$(dirs),$(wildcard $(dir)/*))

#################################################################
CCACHE   = $(shell test -x /usr/bin/ccache && echo "ccache")
CXX	 = g++

MAKE = make -C

CFLAGS  = -DLINUX -DBINARY -Wall -ggdb -pipe#-Werror

INCPATH = -I. -I/usr/include -I $(ICPRO_DIR)/global_src/systemc \
          $(S2HAL_SEARCH_PATH) \
	  $(ARQ_SEARCH_PATH) \
	  $(HICANN_SEARCH_PATH) \
	  $(MEASURE_SEARCH_PATH) \
	  $(STAGE2_CONF_SEARCH_PATH) \
	  $(JTAG_SEARCH_PATH) \
	  -I $(LOGGERDIR)

# select jtag lib to link to
ifeq (64, $(findstring 64, $(shell uname -m)))
	JTL = $(JTAGLIB)/bin/jtag_lib_64.a
else
	JTL = $(JTAGLIB)/bin/jtag_lib_32.a
endif

LIBS    = -lpthread -lrt -lusb $(JTL)

all: $(PROG) 	


# linking
$(PROG): $(OBJS) ../source/getch.o
	$(CXX) $(INCPATH) $(CFLAGS) $(OBJS) $(TMSRC)/getch.o $(LIBS) -o $@ $(LINKFLAGS)

#make getch.o
getch.o:%.o:../source/getch.c
	$(CCACHE) $(CXX) $(CFLAGS) $(INCPATH)  -c $< -o $@

# cpp object compilation
$(OBJS):%.o:%.cpp
	$(CCACHE) $(CXX) $(CFLAGS) $(INCPATH) -c $< -o $@


clean:
	rm -f $(OBJS)
	rm -f getch.o
	rm -f Makefile.deps
	rm -f $(PROG)

Makefile.deps: $(SRCS)
	$(CXX) $(CFLAGS) $(INCPATH) -M $(SRCS) > Makefile.deps

-include Makefile.deps

