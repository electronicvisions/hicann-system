// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   plltestag.cpp
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

// testmode used for faraday pll testing of real HICANN chip.
class TmTrivRamTestAG : public Testmode{

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
		 	cout << "plltestag: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "plltestag: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

				cout << "plltest: Asserting ARQ reset via JTAG..." << endl;
				// Note: jtag commands defined in "jtag_cmdbase_arq.h"
				jtag->arq_write_ctrl(ARQ_CTRL_RESET, ARQ_CTRL_RESET);
				usleep(100000);
				jtag->arq_write_ctrl(0x0, 0x0);

				// set timeouts
				cout << "plltest: Setting timeout values via JTAG..." << endl;
				jtag->arq_write_rx_timeout(40, 41);    // tag0, tag1
				jtag->arq_write_tx_timeout(110, 111);  // tag0, tag1


				// test using packet transfers via JTAG backdoor
				ci_packet_t cip,ciprec;

				cip.clear();
				ciprec.clear();
				cip.TagID = 0;

				// sent 4 test packets via jtag
				uint64 sop;

				cip.Seq  = 0;
				cip.Ack  = -1;
				cip.SeqValid = 1;
				cip.Write = 1;
				cip.Addr = 0; // crossbar left
				cip.Data = 0x5;
				sop = cip.to_uint64();

				jtag->submit_ocp_packet(sop);
				cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;
				uint64 rdata;
				if (!jtag->receive_ocp_packet<uint64>(rdata))
					cout << "plltest: received none" << endl;
				else {
					ciprec = rdata; // automatic cast
					cout << "plltest: received " << ciprec << endl << "-----" << endl; 
				}

				cip.Seq  = 1;
				cip.Ack  = 0;
				cip.SeqValid = 1;
				cip.Write = 0;
				cip.Addr = 0; // crossbar left
				cip.Data = 0x0;
				sop = cip.to_uint64();

				jtag->submit_ocp_packet(sop);
				cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;
				if (!jtag->receive_ocp_packet<uint64>(rdata))
					cout << "plltest: received none" << endl;
				else {
					ciprec = rdata; // automatic cast
					cout << "plltest: received " << ciprec << endl << "-----" << endl; 
				}

				cip.Seq  = 1;
				cip.Ack  = 1;
				cip.SeqValid = 1;
				cip.Write = 0;
				cip.Addr = 0; // crossbar left
				cip.Data = 0x0;
				sop = cip.to_uint64();

				jtag->submit_ocp_packet(sop);
				cout << "submitted ocp packet: " << cip << " : 0x" << sop << endl;
				if (!jtag->receive_ocp_packet<uint64>(rdata))
					cout << "plltest: received none" << endl;
				else {
					ciprec = rdata; // automatic cast
					cout << "plltest: received " << ciprec << endl << "-----" << endl; 
				}


		return true;
	};
};


class LteeTrivRamTestAG : public Listee<Testmode>{
public:
	LteeTrivRamTestAG() : Listee<Testmode>(string("tmag_trivramtest"),string("perform one access to cbl without arq")){};
	Testmode * create(){return new TmTrivRamTestAG();};
};

LteeTrivRamTestAG ListeeTrivRamTestAG;

