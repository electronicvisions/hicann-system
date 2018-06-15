// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_arqtest.cpp
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


using namespace facets;

class TmARQTest : public Testmode{

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
		 	cout << "tm_arqtest: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "tm_arqtest: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		uint hicannr = 0;
		HicannCtrl* hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];

		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		L1SwitchControl* lc = hc->getLC_tl();		
		LinkLayer<ci_payload,ARQPacketStage2> *ll = hc->tags[0];
//deprecated since loc is known to L1SwitchControl class!		switch_loc loc = SYNTL;   // select enumaration for tag id 0

		ll->SetDebugLevel(1);
		cout << "tm_arqtest: ARQ Test starting ...." << endl;

		// initialize JTAG read access:
		comm->Clear();

		cout << "tm_arqtest: Asserting ARQ reset via JTAG..." << endl;
    // Note: jtag commands defined in "jtag_emulator.h"
    jtag->HICANN_arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
    jtag->HICANN_arq_write_ctrl(0x0, 0x0);

		// set timeouts
		cout << "tm_arqtest: Setting timeout values via JTAG..." << endl;
		jtag->HICANN_arq_write_rx_timeout(40, 41);    // tag0, tag1
		jtag->HICANN_arq_write_tx_timeout(110, 111);  // tag0, tag1

/*
		// ----------------------------------------------------
		// testing loopback
		// ----------------------------------------------------

		cout << "tm_arqtest: enabling tag0 loopback via JTAG..." << endl;
		jtag->HICANN_arq_write_ctrl(ARQ_CTRL_LOOPBACK, 0x0);

		// test using packet transfers via JTAG backdoor
		ci_packet_t cip;

		cip.clear();
		cip.SeqValid = 1;
		cip.Write = 1;

		// sent 4 test packets via jtag
		int i;
		for (i=0; i<4;i++) {
			cip.Seq  = i;
			cip.Addr = i;
			cip.Data = 256*i;
			jtag->HICANN_submit_ocp_packet(cip.to_uint64());
		}

		// loopback is on so packets are reflected
		// sent ACK to stop ARQ in hicann repeating the packats
		cip.SeqValid = 1;
		cip.Ack = i-1;  // ack packets 0-i
		jtag->HICANN_submit_ocp_packet(cip.to_uint64());

		for(int i=0; i<10; i++)
		{	
			uint64 rdata;
			if (!jtag->HICANN_receive_ocp_packet<uint64>(rdata))
				cout << "tm_arqtest: received none" << endl;
			else {
				cip = rdata; // automatic cast
				cout << "tm_arqtest: received " << cip << endl; 
			}
		}		

		return true;
*/

		// start linklayer threads 
//		for(int i=0;i<tagid_num;i++){
		for(int i=0;i<1;i++){
			chip[FPGA_COUNT+DNC_COUNT+hicannr]->tags[i]->Start();
			cout << "tm_arqtest_mini: Starting LinkLayer for tag " << i << " on chip " << hicannr << endl;
		}

 		ci_payload p;
//		ci_payload* pp;

		// options: tag id, addr, data [, delay]
//		lc->write_cfg(0, get_data(0x1234), 10); sleep(13);
//		lc->write_cfg(1, get_data(0x5678), 10); sleep(13);
//		lc->write_cfg(2, get_data(0xdead), 10); sleep(13);
//		lc->write_cfg(3, get_data(0xface), 10); sleep(13);
//		sleep(10);
/*
		// read back write commands (directly from ARQ)
		cout << "tm_arqtest: reading back loopback data..." << endl;
	
		hc->tags[0]->Read(&pp, true); cout << "tm_arqtest: Read back " << p << endl;
		hc->tags[0]->Read(&pp, true); cout << "tm_arqtest: Read back " << p << endl;
		hc->tags[0]->Read(&pp, true); cout << "tm_arqtest: Read back " << p << endl;
		hc->tags[0]->Read(&pp, true); cout << "tm_arqtest: Read back " << p << endl;

		// Wait for completion of previous commands.
		// These are queued within linklayer and return to this thread, directly!
		sleep(10);

		cout << "tm_arqtest: disabling tag0 loopback via JTAG..." << endl;
        jtag->HICANN_arq_write_ctrl(0x0, 0x0);
*/
		// ----------------------------------------------------
		// testing write/read to slave
		// ----------------------------------------------------

		// options: tag id, addr, data [, delay]

		vector<bool> data;
//deprecated since loc is known to L1SwitchControl class!		switch_loc tloc;
		uint taddr;//,tcfg;


		for(int i = 0; i <10;i++){
//			lc->write_cfg(loc , i, get_data(i+4), 10); sleep(1);
			lc->write_cfg(i, get_data(i+4), 10); sleep(1);
			// options: tag id, addr [, delay]
//			lc->read_cfg(loc, i, 10); //  sleep(1);
			lc->read_cfg(i, 10); //  sleep(1);


	//		sleep(3);
	//		lc->read_cfg(loc, 3, 1000); sleep(1);

			// now read data can be taken from link layer
			// this has to be encoded into the user modules
			// for now, use direct linklayer access to tag0
	//		hc->tags[0]->Read(&pp, true); cout << "Read back " << *pp << endl; sleep(1);
	//		hc->tags[0]->Read(&pp, true); cout << "Read back " << *pp << endl; sleep(1);
	//		hc->tags[0]->Read(&pp, true); cout << "Read back " << *pp << endl; sleep(1);
	//		hc->tags[0]->Read(&pp, true); cout << "Read back " << *pp << endl; sleep(1);

				// options: & tag id (redundant), & addr (from command, redundant), & read data



//			lc->get_read_cfg(tloc, taddr, data);
			lc->get_read_cfg(taddr, data);
	//		sleep(1);
			cout <<"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"<<endl;
			cout << "TmArqTest: Read back data L1 Config for: ";
			uint td = 0;
			for(uint y=0;y<data.size();y++) td += (data[y] << y);
			cout << " ADDR " << taddr << ", DATA " << td << endl;
			cout <<"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"<<endl<<endl;
		}
/*
		lc->write_cfg(3, get_data(0x8), 10);
		sleep(1);
		lc->read_cfg(3, 1000);
		slee
		lc->get_read_cfg(taddr, data);
		sleep(1);x

		cout << "TmArqTest: Read back data L1 Config for: ";
		td = 0;
		for(uint y=0;y<data.size();y++) td += (data[y] << y);
		cout << "ADDR " << taddr << ", DATA " << td << endl;
*/
		cout << "TmArqTest finished." << endl;

		return true;
	};
};


class LteeTmARQTest : public Listee<Testmode>{
public:
	LteeTmARQTest() : Listee<Testmode>(string("tm_arqtest"),string("simple write/ead command to test ARQ link layer protocol")){};
	Testmode * create(){return new TmARQTest();};
};

LteeTmARQTest ListeeTmARQTest;

