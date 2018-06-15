// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_repeater.cpp
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
#include <queue>

using namespace facets;

class TmRepeater : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_repeater"; return ss.str();}
public:

	HicannCtrl *hc; 
	LinkLayer *ll;

	// -----------------------------------------------------	
	// OCP constants
	// -----------------------------------------------------	

	int repeater;
	string rname;
	int ocp_base;
	int mem_size;

	void select() { 	
		switch(repeater) {
		case 0 : rname="TopLeft";    ocp_base = OcpRep_tl.startaddr; mem_size=64; break;
		case 1 : rname="TopRight";   ocp_base = OcpRep_tr.startaddr; mem_size=64; break;
		case 2 : rname="BottomLeft"; ocp_base = OcpRep_bl.startaddr; mem_size=64; break;
		case 3 : rname="BottomRight";ocp_base = OcpRep_br.startaddr; mem_size=64; break;
		case 4 : rname="HorizLeft";  ocp_base = OcpRep_l.startaddr; mem_size=32; break;
		default: rname="HorizRight"; ocp_base = OcpRep_r.startaddr; mem_size=32; break;
		}
	}

	static const int TAG             = 0;

 	static const int MEM_BASE        = 0;
// 	static const int MEM_SIZE        = 10; depends, see select()
	static const int MEM_WIDTH       = 8; // bit
	static const int MEM_TEST_NUM    = 2; // times size

 	static const int REG_BASE        = 0x40;  // bit #6 set 

	// RamTest interface

	inline void clear() {};
	inline void write(int addr, uint value) { ll->write_cmd(ocp_base + addr, value, 10); }
	inline void read(int addr) { ll->read_cmd(ocp_base + addr, 10); }
	inline bool rdata_(uint& value, int block) { cmd_pck_t p; int r=ll->get_data(&p, block); value=p.data; return (bool)r; }
	inline bool rdata(uint& value) { return rdata_(value, false); }
	inline bool rdatabl(uint& value) { return rdata_(value, true); }


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
		ramtest.compare.debug=true;
		
	
		// register tests for both 2 controllers
		dbg(0) << "initializing repeater srams with incremental data" << endl;
		for(repeater=0;repeater<6;repeater++) {
			select();
			dbg(0) << "testing repeater " << repeater << "='" << rname << "'" << endl;
			ramtest.Resize(MEM_BASE, mem_size, 1<<MEM_WIDTH);
			if (!ramtest.Test(mem_size, RamTestUint::incremental)) return 0;
		} 		

		dbg(0) << "testing repeater srams with random data" << endl;
		for(repeater=0;repeater<6;repeater++) {
			select();
			dbg(0) << "testing repeater " << repeater << "='" << rname << "'" << endl;
			ramtest.Resize(MEM_BASE, mem_size, 1<<MEM_WIDTH);
			ramtest.Init(mem_size, RamTestUint::incremental); // init compare data
			if (!ramtest.Test(mem_size * MEM_TEST_NUM, RamTestUint::random)) return 0;
		} 		
		
		dbg(0) << "ok" << endl;
		wait(5, SC_US); 

		return 1;
	};
};


class LteeTmRepeater : public Listee<Testmode>{
public:
	LteeTmRepeater() : Listee<Testmode>(string("tm_repeater"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmRepeater();};
};

LteeTmRepeater ListeeTmRepeater;

