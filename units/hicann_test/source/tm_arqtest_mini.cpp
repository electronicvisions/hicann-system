// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_arqtest_mini.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   12/2008
// Last Change     :   
// by              :   
//------------------------------------------------------------------------


//example file for a custom testmode implementation

#include "common.h"

//#ifdef NCSIM
//#include "systemc.h"
//#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

// jtag
#include "common.h"
//#include "verification_control.h"
//#include "s2c_jtag.h"

//only if needed
#include "ctrlmod.h"
#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class


// JTAG commands

#define READID          	0x0
#define LVDS_PADS_EN     	0x2
#define LINK_CTRL 		0x3
#define LAYER1_MODE		0x4
#define SYSTEM_ENABLE		0x5
#define BIAS_CTRL			0x6
#define SET_IBIAS 		0x7
#define START_LINK   		0x8
#define STOP_LINK   		0x9
#define STOP_TIME_COUNT  	0xa
#define READ_SYSTIME 	 	0xb
#define UNUSED28	 		0xc
#define SET_2XPLS  		0xd
#define UNUSED30		  	0xe
#define UNUSED4			0xf
#define PLL2G_CTRL	 	0x10
#define SET_TX_DATA		0x11
#define GET_RX_DATA		0x12
#define UNUSED33	 		0x13
#define UNUSED34	 		0x14
#define UNUSED7	  		0x15
#define UNUSED8	  		0x16
#define SET_TEST_CTRL    	0x17
#define START_CFG_PKG   	0x18
#define START_PULSE_PKG 	0x19
#define READ_STATUS     	0x1a
#define SET_RESET	  		0x1b
#define REL_RESET	  		0x1c
#define UNUSED32			0x1d
#define SAMPLE_PRELOAD	0x1e
#define INTEST			0x1f
#define EXTEST			0x20
#define SET_DELAY_RX_DATA	0x21
#define SET_DELAY_RX_CLK	0x22
#define UNUSED9       	0x23
#define UNUSED35			0x24
#define UNUSED36			0x25
#define UNUSED29			0x26
#define READ_CRC_COUNT    0x27
#define RESET_CRC_COUNT   0x28
#define CMD_PLL_FAR_CTRL		0x29


using namespace facets;

class TmARQTestMini : public Testmode{

public:

	vector<bool> get_data(int data)
	{
		vector<bool> d;
		d.clear();
		for(int i=0;i<16;i++) d.push_back(((data >> i) & 1) == 1);
		return d;
	}

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "tm_arqtest_mini: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "tm_arqtest_mini: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		uint hicannr = 0;
		HicannCtrl* hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];

		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		L1SwitchControl* lc = hc->getLC_tl();		
		LinkLayer<ci_payload,ARQPacketStage2> *ll = hc->tags[0];
//deprecated since loc is known to L1SwitchControl class!		switch_loc loc = SYNTL;   // select enumaration for tag id 0

		ll->SetDebugLevel(5);
		cout << "tm_arqtest_mini: ARQ Test starting ...." << endl;
/*
		cout << "reset test logic" << endl;
		jtag->reset_jtag();
		
		usleep(10000);
		
		uint id;
		jtag->read_jtag_id(id);
		cout << "read jtag id: 0x" << hex << id << endl;

		uint test=0x12345678;
		uint veri=0x0f0f0f0f;
		
		jtag->bypass(test, 32, veri);
		cout << "read jtag bypass: 0x" << hex << veri << endl;

		// set test_Ctrl reg to 0001 to enable rx_data readout
		jtag->set_jtag_instr(SET_TEST_CTRL);
		int cmd=0x1;
		jtag->set_jtag_data<int>(cmd,4);
		usleep(10000);


		// disable dnc interface
		jtag->set_jtag_instr(SYSTEM_ENABLE);
		cmd=0x0;
		jtag->set_jtag_data<int>(cmd,1);
		usleep(10000);

*/
		// set timeouts
		cout << "tm_arqtest_mini: Setting timeout values via JTAG..." << endl;
		jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
		jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1

		cout << "tm_arqtest_mini: Asserting ARQ reset via JTAG..." << endl;
		// Note: jtag commands defined in "jtag_cmdbase_arq.h"
		jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
		usleep(100000);
		jtag->HICANN_arq_write_ctrl(0x0, 0x0);


/*
		// ----------------------------------------------------
		// testing loopback
		// ----------------------------------------------------

		usleep(100000);
		cout << "tm_arqtest_mini: enabling tag0 loopback via JTAG..." << endl;
        jtag->HICANN_arq_write_ctrl(ARQ_CTRL_LOOPBACK, 0x0);


		// test using packet transfers via JTAG backdoor
		ci_packet_t cip;

		cip.clear();
		cip.SeqValid = 1;
		cip.Write = 1;


		// read packet counters
		unsigned short rdata0=0xaaaa, rdata1=0x5555;
		jtag->HICANN_arq_read_tx_packet_num(rdata0, rdata1);
		cout << "tx_packets: rd0 = 0x" << hex << rdata0 << " | rd1 = 0x" << rdata1 << endl;
		
		jtag->HICANN_arq_read_rx_packet_num(rdata0, rdata1);
		cout << "rx_packets: rd0 = 0x" << hex << rdata0 << " | rd1 = 0x" << rdata1 << endl;


		// sent 4 test packets via jtag
		int i;
		uint64 sop;
		for (i=0; i<4;i++) {
			cip.Seq  = i;
			cip.Addr = i;
			cip.Data = 256*(i+1);
			sop = cip.to_uint64();
			jtag->HICANN_submit_ocp_packet(sop);
			cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;
		}

		// loopback is on so packets are reflected
		// sent ACK to stop ARQ in hicann repeating the packats
		cip.SeqValid = 1;
		cip.Ack = i-1;  // ack packets 0-i
		sop = cip.to_uint64();
		jtag->HICANN_submit_ocp_packet(cip.to_uint64());
		cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;

		for(int i=0; i<1; i++)
		{	
			uint64 rdata;
			if (!jtag->HICANN_receive_ocp_packet<uint64>(rdata))
				cout << "tm_arqtest_mini: received none" << endl;
			else {
				cip = rdata; // automatic cast
				cout << "tm_arqtest_mini: received " << cip << endl; 
			}
		}		

		// read packet counters
		rdata0=0xaaaa; rdata1=0x5555;
		jtag->HICANN_arq_read_tx_packet_num(rdata0, rdata1);
		cout << "tx_packets: rd0 = 0x" << hex << rdata0 << " | rd1 = 0x" << rdata1 << endl;
		
		jtag->arq_read_rx_packet_num(rdata0, rdata1);
		cout << "rx_packets: rd0 = 0x" << hex << rdata0 << " | rd1 = 0x" << rdata1 << endl;

		return true;
*/

		// start linklayer threads
		for(int i=0;i<tagid_num;i++){
			chip[FPGA_COUNT+DNC_COUNT+hicannr]->tags[i]->Start();
			cout << "tm_arqtest_mini: Starting LinkLayer for tag " << i << " on chip " << hicannr << endl;
		}

/*
		// options: tag id, addr, data [, delay]
		lc->write_cfg(0, get_data(0x1234), 10);

		// read back write commands (directly from ARQ)
		cout << "tm_arqtest_mini: reading back loopback data..." << endl;
	
		// ok now the communication object is assumed to be "s2c_jtag"
		// and the ARQ polling thread is enabled
		usleep(1000);
	
 		ci_payload p;
		ci_payload* pp;

		hc->tags[0]->Read(&pp, true); cout << "tm_arqtest_mini: Read back 0x" << hex << p << endl;

		// disable loopback and repeat above test
		jtag->HICANN_arq_write_ctrl(0x0, 0x0);
*/
//		lc->write_cfg(loc , 0, get_data(0x1234), 10);
		lc->write_cfg(0, get_data(0x1234), 10);
//		lc->read_cfg(0, 10);
//		hc->tags[0]->Read(&pp, true); cout << "tm_arqtest: Read back 0x" << hex << p << endl;
sleep(10);
		cout << "TmArqTest finished." << endl;
		return true;
	};
};


class LteeTmARQTestMini : public Listee<Testmode>{
public:
	LteeTmARQTestMini() : Listee<Testmode>(string("tm_arqtest_mini"),string("simple write/ead command to test ARQ link layer protocol")){};
	Testmode * create(){return new TmARQTestMini();};
};

LteeTmARQTestMini ListeeTmARQTestMini;

