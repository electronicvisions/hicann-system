// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_NeuronMem.cpp
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
#include "l1switch_control.h" //layer 1 switch control class
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"
#include <queue>

//functions defined in getch.c
int getch(void);
int kbhit(void);

using namespace facets;
using namespace std;

//binary output in stream
class bino 
{
public:
	bino(uint v, uint s=32):val(v),size(s){};
	uint val,size;
	friend std::ostream & operator <<(std::ostream &os, class bino b)
	{
		for(int i=b.size-1 ; i>=0 ; i--)
			if( b.val & (1<<i) ) os<<"1"; else os<<"0";
		return os;
	}
};

//address,data tuple
class ad
{
public:
	uint address, data;
	ad(uint a, uint d):address(a),data(d){};
	friend std::ostream & operator <<(std::ostream &os, class ad a)
	{
		os<<hex<<a.address<<":"<<a.data<<" ";	
		return os;
	}
};

class TmJSNeuronMem : public Testmode /*, public RamTestIntfUint*/ {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmjs_NeuronMem"; return ss.str();}
public:

	HicannCtrl *hc; 
//	LinkLayer<ci_payload,ARQPacketStage2> *ll;
	CtrlModule *rc;
	ci_data_t rcvdata ;
	ci_addr_t rcvaddr ;
	
	// helper members
	uint read(uint adr){
		rc->read_cmd(adr,0);
		rc->get_data(rcvaddr,rcvdata);
		return rcvdata;
	}
	class ad read_ad(uint adr){
		rc->read_cmd(adr,0);
		rc->get_data(rcvaddr,rcvdata);
		return ad(rcvaddr,rcvdata);		
	}
	
	void write(uint adr, uint data){
			rc->write_cmd(adr, data,0);
	}		

	
	// -----------------------------------------------------	
	// OCP constants
	// -----------------------------------------------------	

	string rname;
	int ocp_base;
	int mem_size;

	static const int TAG             = 0;

 	static const int MEM_BASE        = 0;
// 	static const int MEM_SIZE        = 10; depends, see select()
	static const int MEM_WIDTH       = 8; // bit
	static const int MEM_TEST_NUM    = 2; // times size

 	static const int REG_BASE        = 0x40;  // bit #6 set 


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	dbg(0) << "ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
//		ll = hc->tags[TAG]; 
//		ll->SetDebugLevel(3); // no ARQ deep debugging

	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		L1SwitchControl *lc;		//for testing

		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		lc = &hc->getLC_cl();
		// ----------------------------------------------------
		// do ramtest of switch matrices
		// ----------------------------------------------------

		uint startaddr = 0;
//		uint maxaddr   = 63;
		uint datawidth = 4;

		uint testdata = 0;
		rcvdata  = 0;
		rcvaddr  = 0;

		bool result = true;

		srand(1);
		
		for(uint i=startaddr;i<=5;i++){
			testdata = rand()%(1<<datawidth);
			lc->write_cfg(i, testdata);
			lc->read_cfg(i);
			
			lc->get_read_cfg(rcvaddr, rcvdata);
			
			cout << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
			
			if(i==rcvaddr && testdata==rcvdata)
				cout << "OK :-)" << endl;
			else{
				cout << "ERROR :-(" << endl;
				result = false;
				break;
			}
		}

		if(!result)return false;

// now test neuron mem
  	ocp_base = OcpNeuronBuilder.startaddr; mem_size=512;
	
		uint error=0;
		uint sctrl=0x210; //sram control reg

		dbg(0) << "NeuronMem base adr: 0x" << hex << ocp_base << endl;
		rc = new CtrlModule(hc, TAG, ocp_base, 0x3ff); //NeuronMem blocks use 9 address bits + SRAM control bit

		//read sram timing
		rc->read_cmd(sctrl,0);
		rc->get_data(rcvaddr,rcvdata);
		cout << hex <<" adr \t0x"  << rcvaddr << ", \t0b"  << rcvdata <<endl;
		rc->read_cmd(sctrl+1,0);
		rc->get_data(rcvaddr,rcvdata);
		cout << hex <<" adr \t0x"  << rcvaddr << ", \t0b"  << rcvdata <<endl;
		//change sram timing
		write(sctrl,0x8);write(sctrl+1,0x55);
		cout << hex <<read_ad(sctrl)<<read_ad(sctrl+1) <<endl;



		dbg(0)<<"test NeuronMem sram:"<<rname<<endl;

		srand(randomseed);
		const uint rmask[4]={0x140003f,0x080003f,0x140003f,0x0bfffff}; //neurons utilize different number of bits per address
		uint tdata[0x3ff];

		mem_size=512;
		
		for(int i=0;i<mem_size;i++)
			{
				if(!(i%32))cout<<i<<" "<<endl;
				tdata[i]=rand() & rmask[i%4];
				write(i,tdata[i]);
			}
		
		srand(randomseed);
		for(int i=0;i<mem_size;i++)
			{
				read(i);
				if( (rcvdata& rmask[i%4] )!= tdata[i]){
					uint sdata=read(i);
					error++;
					dbg(0) << hex << "radr \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,25) <<" "<< bino(sdata,25) << "	data written: 0b" <<
					bino(tdata[i],25)<<endl;
				}
			}
			
		cout <<endl<<"Errors: "<<error<<endl;
	
		return 1;

	}
};


class LteeTmJSNeuronMem : public Listee<Testmode>{
public:
	LteeTmJSNeuronMem() : Listee<Testmode>(string("tmjs_neuronmem"), string("neuroncontrol memtest")){};
	Testmode * create(){return new TmJSNeuronMem();};
};

LteeTmJSNeuronMem ListeeTmJSNeuronMem;

