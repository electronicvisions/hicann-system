// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_sramtiming.cpp
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

class TmSramTiming : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_sramtiming"; return ss.str();}
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

	// REP_ADRW = 7 in hicann_base.vh --> bits 0..6
	// access to control bit --> bit 7 = 0x80
	// (I think this is one addr bit to much because only 64 repeaters exist but however ...)
	// the sram controller itself is instantiated in repeatercontrol.sv
	// with aw=8+1 and cntlr=5 

 	static const int REG_BASE        = 0x80;           // bit #8 set 
 	static const int REG_CTRL        = REG_BASE+0x20;  // bit #8+#5 set 
 	static const int REG_TRD         = REG_CTRL;     
 	static const int REG_TSUTWR      = REG_CTRL+1;   

	// RamTest interface

	inline void clear() {};
	inline void write(int addr, uint value) { ll->write_cmd(ocp_base + addr, value, 10); }
	inline void read(int addr) { ll->read_cmd(ocp_base + addr, 10); }
	inline bool rdata_(uint& value, int block) { cmd_pck_t p; int r=ll->get_data(&p, block); value=p.data; return (bool)r; }
	inline bool rdata(uint& value) { return rdata_(value, false); }
	inline bool rdatabl(uint& value) { return rdata_(value, true); }

	int read_timing(uint& trd, uint& tsu, uint& twr) {
		uint tsutwr; 
		read(REG_TRD); read(REG_TSUTWR);
		if (!rdatabl(trd)) return 0;
		if (!rdatabl(tsutwr)) return 0;
		tsu= (tsutwr>>4) & 0xf; twr=tsutwr & 0xf;
		return 1;
	}

	int write_timing(uint trd, uint tsu, uint twr) {
		write(REG_TRD, trd); 
		uint tsutwr = ((tsu & 0xf) << 4) +(twr & 0xf); 
		write(REG_TSUTWR, tsutwr);
		return 1;
	}

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

		string starttime;
		classname = ClassName();
		debug = true;

		// ----------------------------------------------------
		// test
		// ----------------------------------------------------

		// ramtest
		RamTestUint ramtest(this);
		ramtest.SetDebug(true);
		ramtest.compare.debug=true;
		dbg(0) << hex;
		
		// init repeater
		repeater=0;
		select();
		dbg(0) << "using repeater " << repeater << "='" << rname << "' to test sram controller timings" << endl;
		ramtest.Resize(MEM_BASE, mem_size, 1<<MEM_WIDTH);

		// test
		uint trd, tsu, twr;
		if (!read_timing(trd, tsu, twr)) return 0;
		dbg(0) << "reset-settings: TRD="<< dec<<trd<<", TSU="<<tsu<<", TWR="<<twr<<endl;
		dbg(0) << "doing ramtest..."<< endl;
		if (!ramtest.Test(mem_size, RamTestUint::incremental)) return 0;

		dbg(0) << "testing double timing..."<< endl;
		write_timing(trd*2, tsu*2, twr*2);
		if (!ramtest.Test(mem_size, RamTestUint::random)) return 0;

		dbg(0) << "testing triple timing..."<< endl;
		write_timing(trd*3, tsu*3, twr*3);
		if (!ramtest.Test(mem_size, RamTestUint::random)) return 0;

		dbg(0) << "testing quad timing..."<< endl;
		write_timing(trd*4, tsu*4, twr*4);
		if (!ramtest.Test(mem_size, RamTestUint::random)) return 0;
		
		dbg(0) << "testing slowest possible timing (255,15,15)..."<< endl;
		write_timing(255, 15, 15);
		if (!ramtest.Test(mem_size, RamTestUint::random)) return 0;

		dbg(0) << "testing near-fastest possible timing (1,1,1)...(allow errors)"<< endl;
		write_timing(1, 1, 1);
		ramtest.Test(mem_size, RamTestUint::random);

		dbg(0) << "testing fastest possible timing (1,0,0)... (allow errors)"<< endl;
		write_timing(1, 0, 0);
		ramtest.Test(mem_size, RamTestUint::random);

		dbg(0) << "ok" << endl;
		wait(5, SC_US); 

		return 1;
	};
};


class LteeTmSramTiming : public Listee<Testmode>{
public:
	LteeTmSramTiming() : Listee<Testmode>(string("tm_sramtiming"), string("test timing of sram controller")){};
	Testmode * create(){return new TmSramTiming();};
};

LteeTmSramTiming ListeeTmSramTiming;

