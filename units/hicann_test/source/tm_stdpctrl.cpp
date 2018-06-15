// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_stdpctrl.cpp
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

//functions defined in getch.c
int getch(void);
int kbhit(void);

using namespace facets;

class TmStdpCtrl : public Testmode, public RamTestIntfUint {
protected:
	virtual string ClassName() {stringstream ss; ss << "tm_stdpctrl"; return ss.str();}
public:

	HicannCtrl *hc; 
	LinkLayer *ll;

	// -----------------------------------------------------	
	// Stdp controller constants
	// -----------------------------------------------------	

	int cntlr;
	inline int get_addr(int addr) { 	
		switch(cntlr) {
		case 0 : return OcpStdpCtrl_t.startaddr+addr;
		case 1 : return OcpStdpCtrl_b.startaddr+addr;
		}
	}

	static const int TAG = 1;

	static const int
		SDRV_BASE     = 0x0000, // syndriver
		REG_BASE      = 0x0400, // register + weight/decoder read
		SCFG_BASE     = 0x0500, // timing for sram = syndriver
		SYNW_BASE     = 0x0600; // weight/decoder write

	static const int 
		REG_CTRL      = REG_BASE,
		REG_CNFG      = REG_BASE+1,
		REG_LUT       = REG_BASE+2,
		REG_3         = REG_BASE+3,
		REG_SYND	  = REG_BASE+6,

		REG_WIDTH     = 32,
		REG_TEST_NUM  = 16;

	// Note organisation of Synapse Driver Rams:
	// 
	// * each of the two controllers (top/bottom) controlles 2 areas (left/right)
    // * each area (tl,tr,bl,br) has 3 banks of 128 entries of 8 bit each
	// * a single 16-bit sram access to a controller writes/reads only 8 bit: 
	//   upper 8 to left area, lower 8 to right area, depending on adresses (even / odd)
	// * the sram (per controller) for the ramtest is therefore layes out 8-bit with size 2 * 3 * 128    

 	// synapse driver memory
 	static const int SDRV_SIZE       = 4;//2*3*128;
	static const int SDRV_WIDTH      = 8;
	static const int SDRV_TEST_NUM   = SDRV_SIZE*4;

	// command documentation see also hicann_pkg.sv 
	static const int 
		CTRL_BIT_CMD        = 0,
		CTRL_BIT_ENCR       = 4,
		CTRL_BIT_CONTINUOUT = 5,
		CTRL_BIT_NEWCMD     = 6,
		CTRL_BIT_ADDR       = 8,
		CTRL_BIT_LASTADR    = 16,
		CTRL_BIT_DONE       = 24;
						
	// weight/decoder state machine
	typedef enum {
		DSC_IDLE  = 0, 	// no active command
		DSC_READ  = 1, 	// reads correlation pattern and weights
		DSC_PROC  = 2, 	// like READ, but performs one STDP cycle and updates weights
		DSC_WRITE = 3, 	// writes weight register to synapse row
		DSC_AUTO  = 4,	// starts auto stdp: PROC cycles from adr to lastadr
		DSC_WDEC  = 5,	// write weight register to synapse address decode
		DSC_RDEC  = 6	// read synapse address decode in weight register
	} 
	dsccmd_t;

	// weight/decoder access
	static const int
		ROW_NUM     = 4,//224,
		ROW_ENTRIES = 32;

	int next_new_cmd[2];   // store last value of new cmd bit for each controller

	queue<int> raddr;

	// RamTest interface to StdpCtrl

	inline void clear() {};
	inline void write(int addr, uint value) { 
		if ((addr>=SDRV_BASE) && (addr<SDRV_BASE+SDRV_SIZE)) { 
			if (!(addr & 1)) value = value <<8; }
		ll->write_cmd(get_addr(addr), value, 10); 
	}
	inline void read(int addr) { ll->read_cmd(get_addr(addr), 10); raddr.push(addr); }
	inline bool rdata(uint& value) { return rdata_(value, false); } 
	inline bool rdatabl(uint& value) { return rdata_(value, true); } 

	bool rdata_(uint& value, bool block) 
	{ 
		cmd_pck_t p; int r=ll->get_data(&p, block); 
		if (r) { 
			int addr=raddr.front(); raddr.pop(); 
			if ((addr>=SDRV_BASE) && (addr<SDRV_BASE+SDRV_SIZE)) { 
				if (addr & 1) value=(p.data) & 0xff; else value = (p.data >> 8 ) & 0xff; } 
			else value=p.data;
		} 
		return (bool)r; 
	}


	// access to weights and decoder memory
	// 
	// * for each controller 0..1 controllers:
	// * for each row 0..223: 
	// * 1024 bits for each weights and decoders (2+2 bits per synapse)
	// * enter row addr in cmd field  
	// * write/read 1024 bit row at once via appropriate W/D R/W command
	// * access values via 1024 bit registers, different for read and write

	int write_row(int cmd, int addr, vector<uint>row) {
		int i; uint value;
		for(i=0;i<ROW_ENTRIES;i++) write(SYNW_BASE+i, row[i]); // write 1024 bits
		write(REG_CTRL, cmd | next_new_cmd[cntlr] | ((addr & 0xff) << CTRL_BIT_ADDR));
		next_new_cmd[cntlr] = next_new_cmd[cntlr] ^ (1 << CTRL_BIT_NEWCMD); 
		do { read(REG_CTRL); if (!rdatabl(value)) return 0; } while (!(value & (1<<CTRL_BIT_DONE)));
		return 1;
	}

	int write_weights(int addr, vector<uint>row) { return write_row(DSC_WRITE, addr, row); }
	int write_decoder(int addr, vector<uint>row) { return write_row(DSC_WDEC, addr, row); }

	int read_row(int cmd, int addr, vector<uint>& row) {
		int i; uint value;
		write(REG_CTRL, cmd | next_new_cmd[cntlr] | ((addr & 0xff) << CTRL_BIT_ADDR));
		next_new_cmd[cntlr] = next_new_cmd[cntlr] ^ (1 << CTRL_BIT_NEWCMD); 
		do { read(REG_CTRL); if (!rdatabl(value)) return 0; } while (!(value & (1<<CTRL_BIT_DONE)));
		for (i=0;i<ROW_ENTRIES;i++) read(REG_SYND+i);
		for (i=0;i<ROW_ENTRIES;i++) { if (!rdatabl(value)) return 0; row[i]=value; }
		return 1;
	}

	int read_weights(int addr, vector<uint>& row) { return read_row(DSC_READ, addr, row); }
	int read_decoder(int addr, vector<uint>& row) { return read_row(DSC_RDEC, addr, row); }


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
		
		// compare tests also with tb_stdpctrl.sv
		//cntlr=0;
		//write(REG_CNFG, 0xdeadface);

		uint data,data2;
/*		// SIMPLE LUT TEST
		write(REG_LUT, 0x12345678);
		read(REG_LUT);
		rdatabl(data);
		dbg(0) << cout << "read back lut2=0x" << data << endl;
		write(REG_3, 0xdeadface);
		read(REG_3);
		rdatabl(data);
		dbg(0) << cout << "read back reg3=0x" << data << endl;
		return(1);
*/

		// control reg 
		for (cntlr=0; cntlr<2; cntlr++) {
			dbg(0) << "testing controller " << cntlr << " control register" << endl;
			read(REG_CTRL);	if(!rdatabl(data))return 0;
			dbg(0) << "control resister value = 0x" << hex << data << endl;

			dbg(0) << "toggle syn_encr" << endl;
			wait(1, SC_US);
			write(REG_CTRL, data ^ (1<<CTRL_BIT_ENCR));
			read(REG_CTRL);	if(!rdatabl(data2))return 0;
			if(data != (data2 ^ (1<<CTRL_BIT_ENCR))) { dbg(0) << "could not toggle" <<endl; return 0; }
		}

/*
		typedef struct packed {
		logic [1:0] dllresetb;       11
		logic [3:0] gen;               1111  
		logic [3:0] predel,	                1111 .... .... ....   
					endel;                  .... 1111 .... ....  
		logic [3:0][0:pat_num-1] pattern;   .... .... 1111 1111
		} configreg_t;
*/		
	
		// config reg 
		for (cntlr=0; cntlr<2; cntlr++)
		{
			dbg(0) << "testing controller " << cntlr << " config register" << endl;
			dbg(0) << "testing dllreset,enable" << endl;
			write(REG_CNFG, 0x00a200);  // reset, disable
			wait(1, SC_US);
			write(REG_CNFG, 0x3fb300);  // enable
			dbg(0) << "change access timing predel=10,endel=2" << endl;
			wait(1, SC_US);
			write(REG_CNFG, 0x3fa200);  // enable

		}

		// register tests for both 2 controllers
		for (cntlr=0; cntlr<2; cntlr++)
		{
			dbg(0) << "testing controller " << cntlr << " register '2=LUT'" << endl;
			ramtest.Resize(REG_LUT, 1, (llu)1<<31);  // 32 bit size
			ramtest.Clear();
			write(REG_LUT, 0);
			if (!ramtest.Test(REG_TEST_NUM, RamTestUint::random)) return 0; 		

			dbg(0) << "testing controller " << cntlr << " register '3'" << endl;
			ramtest.Resize(REG_3, 1, 1<< (llu)1<<31);
			ramtest.Clear();
			write(REG_3, 0);
			if (!ramtest.Test(REG_TEST_NUM, RamTestUint::random)) return 0; 		
		}

		// syndriver srams

		// first init syn driver rams doing incremental tests
		for (cntlr=0; cntlr<2; cntlr++)	{
			dbg(0) << "testing controller " << cntlr << " synapse driver sram (incremental)" << endl;
			ramtest.Resize(SDRV_BASE, SDRV_SIZE, (llu)1<<SDRV_WIDTH);
			if (!ramtest.Test(SDRV_SIZE, RamTestUint::incremental)) return 0; 		
		}

		// random access to syn driver rams ...
		for (cntlr=0; cntlr<2; cntlr++)
		{
			dbg(0) << "testing controller " << cntlr << " synapse driver sram (random access)" << endl;
			ramtest.Resize(SDRV_BASE, SDRV_SIZE, (llu)1<<SDRV_WIDTH);
			ramtest.Init(SDRV_SIZE, RamTestUint::incremental); // init compare mem with incremental values 
			if (!ramtest.Test(SDRV_TEST_NUM, RamTestUint::random)) return 0; 		
		}


		// test weight and decode write
		dbg(0) << "testing weight and decoder memory write/compare" << endl;
		uint value,i,row;
		vector<uint> weights(ROW_ENTRIES), decs(ROW_ENTRIES);

		// init of controller cmd toggle bit
		for (cntlr= 0; cntlr<2; cntlr++) 
			next_new_cmd[cntlr] = 0; 

		// write incremental
		for (cntlr= 0; cntlr<2; cntlr++)
		{
			// write all synapse weights
			dbg(0) << "writing controller " << cntlr << " weights (incremental)" << endl;
			for (row=0;row<ROW_NUM;row++) {		
				for(i=0;i<ROW_ENTRIES;i++) weights[i]= 0x10000000 * cntlr + ROW_ENTRIES*row+i;
				write_weights(row, weights);
			}
			// write all decoders
			dbg(0) << "writing controller " << cntlr << " decoders (incremental)" << endl;
			for (row=0;row<ROW_NUM;row++) {		
				for(i=0;i<ROW_ENTRIES;i++) decs[i]= 0x10000000 * cntlr + 0x00ab0000 + ROW_ENTRIES*row+i;
				write_decoder(row, decs);
			}
		}
	
		// read back and compare
		for (cntlr= 0; cntlr<2; cntlr++)
		{
			// compare synapse weights
			dbg(0) << "comparing controller " << cntlr << " weights" << endl;
			for (row=0;row<ROW_NUM;row++) {		
				read_weights(row, weights);
				for(i=0;i<ROW_ENTRIES;i++) if (weights[i]!= 0x10000000 * cntlr + ROW_ENTRIES*row+i) {
					dbg(0) << "weight compare error, read 0x" << weights[i]<<
						", expected "<<0x10000000 * cntlr + ROW_ENTRIES*row+i<<endl;
					return 0;
				}
			}
			// compare decoders
			dbg(0) << "comparing controller " << cntlr << " decoders" << endl;
			for (row=0;row<ROW_NUM;row++) {		
				read_decoder(row, decs);
				for(i=0;i<ROW_ENTRIES;i++) if (decs[i]!= 0x10000000 * cntlr + 0x00ab0000 + ROW_ENTRIES*row+i) {
					dbg(0) << "decoder compare error, read 0x" << decs[i]<<
						", expected "<<0x10000000 * cntlr + 0x00ab0000 + ROW_ENTRIES*row+i<<endl;
					return 0;
				}
			}
		}

		dbg(0) << "ok" << endl;
		wait(5, SC_US); 

		return 1;
	};
};


class LteeTmStdpCtrl : public Listee<Testmode>{
public:
	LteeTmStdpCtrl() : Listee<Testmode>(string("tm_stdpctrl"), string("test stdp stuff: synapse weights, decodr weights, driver, controller")){};
	Testmode * create(){return new TmStdpCtrl();};
};

LteeTmStdpCtrl ListeeTmStdpCtrl;

