// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_neuronctrl.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   3/2009
//------------------------------------------------------------------------

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
#include "neuronbuilder_control.h"
#include "neuron_control.h"
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"

using namespace std;
using namespace facets;

class TmNeuronCtrl : public Testmode{ // , public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_neuronctrl"; return ss.str();}
public:


	HicannCtrl *hc; 

	LinkLayer<ci_payload,ARQPacketStage2> *ll;

	FGControl *fc[4];//array of floatinggate controlers
	NeuronBuilderControl *nbc;
	NeuronControl *nc;
	// -----------------------------------------------------	
	// constants
	// -----------------------------------------------------	

	static const int TAG          = 0;
	static const int NB_BASE()    { return OcpNeuronBuilder.startaddr; }
	       const int NC_BASE()    { return OcpNeuronCtrl.startaddr; }

	// neuron builder
	static const int 
		nb_sram			= 0,
		nb_cfg			= 0x0200;

	// neuron builder
	static const int MEM_BASE     = 0;
	static const int MEM_SIZE     = 10;//512; 
	static const int MEM_WIDTH    = 25;
	static const int MEM_TEST_NUM = MEM_SIZE*4;

	// neuron builder
	static const int REG_BASE     = 512;
	static const int REG0_WIDTH   = 17;
	static const int REG1_WIDTH   = 8;
	static const int VERSION      = 0x01;
	static const int REG_TEST_NUM = 10;

	// neuron control regs
	static const int
		enreg			=0,
		ensel			=1,
		enslow			=2,
		endncloopb		=3,
		enrandomreset_n	=4,
		enphase			=5,
		enseed			=6,
  		enperiod		=7;


	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to NeuronCtrl
//	inline int  get_addr(int addr) { return BASE()+addr; }
//	inline void clear() {};

//	inline void write(int addr, uint value) { ll->write_cmd(get_addr(addr), value, 10); }
	inline void write_nbc(int addr, uint value) { nbc->write_cmd(addr, value, 10); }
	inline void write_nc(int addr, uint value) { nc->write_cmd(addr, value, 10); }

//	inline void read(int addr) { ll->read_cmd(get_addr(addr), 10); }
	inline void read_nbc(int addr) { nbc->read_cmd(addr, 10); }
	inline void read_nc(int addr) { nc->read_cmd(addr, 10); }

	inline bool rdata_nc_(uint& value) { ci_addr_t address; int r=nc->get_data(address, value); return (bool)r; }
	inline bool rdata_nbc_(uint& value) { ci_addr_t address; int r=nbc->get_data(address, value); return (bool)r; }
	inline bool rdatabl_nc(uint& value) { return rdata_nc_(value); }
	inline bool rdatabl_nbc(uint& value) { return rdata_nbc_(value); }



	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {



		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::ERROR)<<"ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];



	 	 log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}


		nbc = &hc -> getNBC();
		nc = &hc -> getNC();
		ll->SetDebugLevel(3); // no ARQ deep debugging


		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

//		classname = ClassName();
//		debug = true;
//		RamTestUint ramtest(this);
//		ramtest.SetDebug(true);
		dbg(0) << hex;

		uint data;

		// release drvresetb
		write_nbc(nb_cfg, 0x4000);
		
		// enable all pathways through neuron control without merging and background
		write_nc(enreg,				0x0000); // 32'b00000000_00000000 , 32'hx};
		write_nc(ensel,				0x5100); // 32'b01010001_00000000 , 32'hx};
		write_nc(enslow,  			0x59a9); // 32'b01011001_10101001 , 32'hx};
		write_nc(endncloopb,  		0x0000); // 32'b00000000_00000000 , 32'hx};
		write_nc(enrandomreset_n,	0x0000); // 32'b00000000_00000000 , 32'hx};
		write_nc(enphase,  			0x0000); // 32'b00000000_00000000 , 32'hx};
		write_nc(enseed,  			0);      // 32'd0 , 32'hx};
		write_nc(enperiod,  		2);      // 32'd2 , 32'hx};
		write_nc(enperiod+1,  		2);      // 32'd2 , 32'hx};

		// now add background	for in0 and in1
		write_nc(enreg,				0x03);  // 32'b00000000_00000011 , 			32'hx};
		write_nc(enrandomreset_n,	0x03);  // 32'b00000000_00000011 ,       32'hx};
		write_nc(enslow,	0x03);  // 32'b00000000_00000011 ,       32'hx};
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);

		// now merge in0 and in1 and background	
		write_nc(enreg,				0x0103); // 32'b00000001_00000011 , 			32'hx};
		write_nc(enslow,  			0x59a8); //	32'b01011001_10101000 ,       32'hx};
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);

		// reduce high rates
		write_nc(enperiod,  		60000);  // 32'd60000 ,       32'hx};
		write_nc(enperiod+1,  		10);     // 32'd10 ,       32'hx};

		// random background on bg0
		write_nc(enperiod,  		60000);  // 32'd60000 ,       32'hx};
		write_nc(enrandomreset_n,	0x0103); // 32'b00000001_00000011 ,       32'hx};
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);
		read_nc(ensel); rdatabl_nc(data);

		//wait(10, SC_US);
		dbg(3) << "Ok, all tests passed!" << endl;
		return 1;
	};
};


class LteeTmNeuronCtrl : public Listee<Testmode>{
public:
	LteeTmNeuronCtrl() : Listee<Testmode>(string("tm_neuronctrl"), string("test NeuronCtrl, generate events, forward to repeater, insert background")){};
	Testmode * create(){return new TmNeuronCtrl();};
};

LteeTmNeuronCtrl ListeeTmNeuronCtrl;

