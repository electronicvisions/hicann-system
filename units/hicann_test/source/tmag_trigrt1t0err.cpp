//------------------------------------------------------------------------
// Testmode to trigger erroneous generation of ill-formatted tag0 packets when sending
// one tag1 read packet.
//------------------------------------------------------------------------

#include "common.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

//only if needed
#include "synapse_control.h"

using namespace facets;
using namespace std;

class TmTrigRT1T0Err : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmag_trigrt1t0err"; return ss.str();}
public:

	HicannCtrl *hc; 
	SynapseControl *sc;

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {

		bool result = true;

		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

		uint hicannr = 0;
		// use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		sc = &hc->getSC_top();
		
		// ----------------------------------------------------
		// reset test logic and FPGA
		// ----------------------------------------------------
		FPGAControl *fpga = (FPGAControl*) chip[0];
		fpga->reset();

		jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(0x1);
		
		uint64_t jtagid=0xf;
		jtag->read_id(jtagid,jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << jtagid << endl;

		// set arbiter delay to determine wether we can trigger DNC bug...
		jtag->SetARQTimings(0x4, 0x0c8, 0x032);
		
		// reset fpga arq instances to load new timing values
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, true); // set reset
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, false); // unset reset


		// ----------------------------------------------------
		// Initialize high-speed comm for ram write via FPGA-ARQ
		// ----------------------------------------------------
	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr(), false, true) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}
	 	dbg(0) << "Init() ok" << endl;


		// ----------------------------------------------------
		// write to switch ram using FPGA ARQ
		// ----------------------------------------------------

		dbg(0)<<"sending one read to tag1..."<<endl;

		// test read on tag 1
		sc->read_cmd(0x4001, 0);
		comm->Flush();
		
#ifdef NCSIM
		wait(500,SC_US);

		dbg(0)<<"...read didn't break simulation... :-)"<<endl;
#else
		usleep(500);

		dbg(0)<<"...returning without further action"<<endl;
#endif

		return result;
	}
};


class LteeTmTrigRT1T0Err : public Listee<Testmode>{
public:
	LteeTmTrigRT1T0Err() : Listee<Testmode>(string("tmag_trigrt1t0err"), string("triggers erroneous generation of ill tag0 reads when sending tag1 read")){};
	Testmode * create(){return new TmTrigRT1T0Err();};
};

LteeTmTrigRT1T0Err ListeeTmTrigRT1T0Err;

