// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_neuronbuilder.cpp
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
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"

using namespace facets;

class TmNeuronBuilder : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_neuronbuilder"; return ss.str();}
public:

	HicannCtrl *hc; 
	LinkLayer *ll;

	// -----------------------------------------------------	
	// neuronbuilder constants
	// -----------------------------------------------------	

	static const int TAG          = 0;
	       const int BASE()   	  { return OcpNeuronBuilder.startaddr; }

	static const int MEM_BASE     = 0;
	static const int MEM_SIZE     = 10;//512; 
	static const int MEM_WIDTH    = 25;
	static const int MEM_TEST_NUM = MEM_SIZE*4;

	static const int REG_BASE     = 512;
	static const int REG0_WIDTH   = 17;
	static const int REG1_WIDTH   = 8;
	static const int VERSION      = 0x01;
	static const int REG_TEST_NUM = 10;


	// -----------------------------------------------------	
	// overloaded function from ramtestintf
	// -----------------------------------------------------	

	// access to neuronbuilder
	inline int  get_addr(int addr) { return BASE()+addr; }
	inline void clear() {};
	inline void write(int addr, uint value) { ll->write_cmd(get_addr(addr), value, 10); }
	inline void read(int addr) { ll->read_cmd(get_addr(addr), 10); }
//	inline bool rdata(int& value, bool block=false) { cmd_pck_t p; int r=ll->get_data(&p, block); value=p.data; return (bool)r; }
	inline bool rdata(uint& value) { cmd_pck_t p; int r=ll->get_data(&p, false); value=p.data; return (bool)r; }
	inline bool rdatabl(uint& value) { cmd_pck_t p; int r=ll->get_data(&p, true); value=p.data; if(r) return true; else return false; }


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	dbg(0) << "ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		ll = hc->tags[TAG];
		ll->SetDebugLevel(3); // no ARQ deep debugging


		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		string starttime;
		classname = ClassName();
		debug = true;
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		dbg(0) << hex;

		dbg(0) << "testing register 0" << endl;
		ramtest.Resize(REG_BASE, 1, 1<<REG0_WIDTH);  // base, size, width
		ramtest.Clear();
		write(REG_BASE, 0); // clear
		if (!ramtest.Test(REG_TEST_NUM, RamTestUint::random)) return 0; 		


		dbg(0) << "testing register 1" << endl;
		ramtest.Resize(REG_BASE+1, 1, 1<<REG1_WIDTH);  // base, size, width
		ramtest.Clear();
		write(REG_BASE+1, 0); // clear
		if (!ramtest.Test(REG_TEST_NUM, RamTestUint::random)) return 0; 		

		
		dbg(0) << "read version control register 2" << endl;
		uint version;
		read(REG_BASE+2);
		if (!rdatabl(version)) return 0; // read blocking
//		if (!rdata(version, true)) return 0; // read blocking
		version &= 0xff;
		dbg(0) << "read version number 0x" << hex << version << endl;
		if (version != VERSION) {
			dbg(0) << "failed, expected version to be 0x" << hex << VERSION << endl;
			return 0;
		}


		// testing memory
		ramtest.Resize(MEM_BASE, MEM_SIZE, 1<<MEM_WIDTH);  // base, size, width

		dbg(0) << "testing first sram address" << endl;
		uint sramdata;
		write(0, 0xdead);
		read(0);
		if (!rdatabl(sramdata)) return 0; // read blocking
//		if (!rdata(sramdata, true)) return 0; // read blocking
		if (sramdata!=0xdead) {
			dbg(0) << "failed, sram not working, expected 0xdead, read 0x" << hex << sramdata << endl;
			return 0;
		}

		dbg(3) << "starting first pass of full sram test, incrementing addr+data.. " << endl;
		starttime = sc_time_stamp().to_string();
		if (!ramtest.Test(MEM_SIZE, RamTestUint::incremental)) return 0; // use incremental since SRAM is not initialized(!)
		dbg(3) << "finished test, execution time from " << starttime << " to " << sc_time_stamp() << endl;
		// wait until last (write) commands in fifos executed
		wait(10, SC_US);


		dbg(3) << "starting second pass of full sram test, random access+data.. " << endl;
		starttime = sc_time_stamp().to_string();
		if (!ramtest.Test(MEM_TEST_NUM, RamTestUint::random)) return 0; // use incremental since SRAM is not initialized(!)
		dbg(3) << "finished test, execution time from " << starttime << " to " << sc_time_stamp() << endl;
		// wait until last (write) commands in fifos executed
		wait(10, SC_US);


		dbg(3) << "Ok, all tests passed!" << endl;
		return 1;
	};
};


class LteeTmNeuronBuilder : public Listee<Testmode>{
public:
	LteeTmNeuronBuilder() : Listee<Testmode>(string("tm_neuronbuilder"), string("test neuronbuilder and neuron srams")){};
	Testmode * create(){return new TmNeuronBuilder();};
};

LteeTmNeuronBuilder ListeeTmNeuronBuilder;

