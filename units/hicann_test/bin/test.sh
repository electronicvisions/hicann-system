#/bin/sh

if [ $# -ne 2 ]; then
	echo "Usage: `basename $0` IP PORT"
	exit 1
fi

FPGA_IP=$1
FPGA_PORT=$2


#### Include the library
. `dirname $0`/sh2ju.sh

#### Clean old reports
juLogClean

myResetCmd() {
	[ ! $PING_OK -eq 0 ] && return 1
	echo "u\rx" | ./tests2 -kill -bje2f 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_capo_net || true
	sleep 1 # maybe some race condition with fpga block reset?
	echo "r\rx" | ./tests2 -kill -bje 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_capo_net || true
	echo "1\r2\r7\rx\r" | ./tests2 -kill -bje 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tmak_iboardv2 || true
	echo "0\n" | ./tests2 -kill -bje 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

mySwitchRAMTest() {
	[ ! $RESET_OK -eq 0 ] && return 1
	echo "s\n" | ./tests2 -kill -bje 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

myHicannBegAndRepeaterTest() {
	[ ! $SWITCHRAM_OK -eq 0 ] && return 1
	echo "r\n" | ./tests2 -kill -bje 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

mySinglePulseLoopbackTest() {
	[ ! $RESET_OK -eq 0 ] && return 1
	echo "l\n" | ./tests2 -kill -bje 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

myFpgaBegTest() {
	[ ! $SWITCHRAM_OK -eq 0 ] && return 1
	echo "m\n" | ./tests2 -kill -bje2f 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

myFpgaFifoTest() {
	[ ! $SWITCHRAM_OK -eq 0 ] && return 1
	echo "n\n" | ./tests2 -kill -bje2f 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

myFpgaDncFpgaLoopbackTest() {
	[ ! $SWITCHRAM_OK -eq 0 ] && return 1
	echo "k\n" | ./tests2 -kill -bje2f 1 0 -ip $FPGA_IP -jp $FPGA_PORT -m tm_hardwaretest
}

mySynapticInputTest() {
	[ ! $SWITCHRAM_OK -eq 0 ] && return 1
	# ECM: still failing due to wrong calib values in xml file
	echo "i\n" | ./tests2 -kill -bje2f 1 0 -ip $FPGA_IP -jp $FPGA_PORT -c ../conf/akononov/FGparam_concatenation_test.xml -m tm_hardwaretest
}

juLog -name=pingTest    ping -n -w10 -c5 $FPGA_IP
PING_OK=$?

juLog -name=resetTest myResetCmd
RESET_OK=$?

juLog -name=switchRAMTest mySwitchRAMTest
SWITCHRAM_OK=$?

juLog -name=hicannBegAndRepeaterTest myHicannBegAndRepeaterTest
HICANN_OK=$?

juLog -name=singlePulseLoopbackTest mySinglePulseLoopbackTest
SINGLEPULSE_OK=$?

juLog -name=fpgaBegTest myFpgaBegTest
FPGABEG_OK=$?

juLog -name=fpgaFifoTest myFpgaFifoTest
FPGAFIFO_OK=$?

juLog -name=fpgaDncFpgaLoopbackTest myFpgaDncFpgaLoopbackTest
FPGADNCFPGA_OK=$?

juLog -name=synapticInputTest mySynapticInputTest
SYNAPTICINPUT_OK=$?

# calculate global result
echo -n "Global test result: "
if [[ $PING_OK && $RESET_OK && $SWITCHRAM_OK && $HICANN_OK && $SINGLEPULSE_OK && $FPGABEG_OK && $FPGAFIFO_OK && $FPGADNCFPGA_OK ]]; then
	# neglect synaptic input test...
	echo "GOOD"
	exit 0
else
	echo "BAD"
	exit 1
fi
