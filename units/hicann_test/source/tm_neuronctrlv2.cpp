// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_neuronctrlv2.cpp
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
#include "testmode.h" 
#include "spl1_control.h"     //spl1 control class

using namespace facets;

class TmNeuronCtrlV2 : public Testmode{ // , public RamTestIntfUint {
protected:
	virtual std::string ClassName() {std::stringstream ss; ss << "tm_neuronctrlv2"; return ss.str();}
public:


	HicannCtrl *hc; 
  SPL1Control *spc;

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
  	enperiod		=7,
// new V2
		enslowenext=15, //new v2 merger settings
		enbgnum=16; //to 23 new v2 neuron number regs
		

	// access to NeuronCtrl
	inline void write_nbc(int addr, ci_data_t value) { nbc->write_cmd((ci_addr_t)addr, value, 10); }
	inline void write_nc(int addr, ci_data_t value) { nc->write_cmd((ci_addr_t)addr, value, 10); }

	inline void read_nbc(int addr) { nbc->read_cmd((ci_addr_t)addr, 10); }
	inline void read_nc(int addr) { nc->read_cmd((ci_addr_t)addr, 10); }

	inline bool rdata_nc_(ci_data_t& value) { ci_addr_t address; int r=nc->get_data(address, value); return (bool)r; }
	inline bool rdata_nbc_(ci_data_t& value) { ci_addr_t address; int r=nbc->get_data(address, value); return (bool)r; }
	inline bool rdata_nc(ci_data_t& value) { return rdata_nc_(value); }
	inline bool rdata_nbc(ci_data_t& value) { return rdata_nbc_(value); }

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {



		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
			log(Logger::ERROR)<<"ERROR: object 'chip' in testmode not set, abort" << std::endl;
			return 0;
		}
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];



	 	 log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
			log(Logger::ERROR) << "ERROR: Init failed, abort" << std::endl;
			return 0;
		}


    spc = &hc->getSPL1Control();
		nbc = &hc->getNBC();
		nc  = &hc->getNC();


		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		dbg(0) << std::hex;
  
	 	uint64_t systime = 0xffffffff;
   	uint neuron  = 0x83;

		ci_data_t data;

// initialize dncif
   	jtag->HICANN_set_test(0);
   	jtag->HICANN_set_layer1_mode(0x55);
	 	spc->write_cfg(0xffff);//activate all do lines
  	jtag->HICANN_set_test(0x2);
		jtag->HICANN_read_systime(systime);
			
 
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
		write_nc(enbgnum,  	0x0201);      
		write_nc(enbgnum+1,  	0x0403);      
		write_nc(enbgnum+2,  	0x0605);      
		write_nc(enbgnum+3,  	0x0807);      

		//switch to dnc input
		write_nc(endncloopb, 0xff00); //	16'b11111111_00000000 ,       16'hx};
		
		// send dnc packet
  	neuron = 0x23+(7<<6);
   	uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
				
   	jtag->HICANN_set_double_pulse(0);
   	jtag->HICANN_start_pulse_packet(jtag_sendpulse);
		
		// switch lower four back to neuron input
		write_nc(endncloopb, 0xf000); 		//16'b11110000_00000000 ,       16'hx};

		// now add background	for in0 and in1
		write_nc(enreg,				0x03);  // 32'b00000000_00000011 , 			32'hx};
		write_nc(enrandomreset_n,	0x03);  // 32'b00000000_00000011 ,       32'hx};
		write_nc(enslow,	0x03);  // 32'b00000000_00000011 ,       32'hx};
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);

		// now merge in0 and in1 and background	, as well as in6 and in7 with dnc
		write_nc(enreg,				0x0103); 	// 32'b00000001_00000011 , 			32'hx};
		write_nc(enslow,  			0x59a8); //	32'b01011001_10101000 ,       32'hx};
		write_nc(enslowenext, 	0xc0c0); //16'b11000000_11000000 ,       16'hx};
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);

		// reduce high rates
		write_nc(enperiod,  		60000);  // 32'd60000 ,       32'hx};
		write_nc(enperiod+1,  		10);     // 32'd10 ,       32'hx};

		// random background on bg0
		write_nc(enperiod,  		60000);  // 32'd60000 ,       32'hx};
		write_nc(enrandomreset_n,	0x0103); // 32'b00000001_00000011 ,       32'hx};
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);
		read_nc(ensel); rdata_nc(data);

		//wait(10, SC_US);
		dbg(3) << "Ok, all tests passed!" << std::endl;
		return 1;
	};
};


class LteeTmNeuronCtrlV2 : public Listee<Testmode>{
public:
	LteeTmNeuronCtrlV2() : Listee<Testmode>(std::string("tm_neuronctrlv2"), std::string("test NeuronCtrl V2, generate events, forward to repeater, insert background")){};
	Testmode * create(){return new TmNeuronCtrlV2();};
};

LteeTmNeuronCtrlV2 ListeeTmNeuronCtrlV2;

