// Company         :   kip
// Author          :   Johannes Schemmel           
//                    			
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
extern "C" {
	int getch(void);
	int kbhit(void);
}

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

class TmSubm2StdpCtrl : public Testmode /*, public RamTestIntfUint*/ {
protected:
	virtual std::string ClassName() {stringstream ss; ss << "tmsubm2_stdpctrl"; return ss.str();}
public:

	HicannCtrl *hc; 
	CtrlModule *rc;
	
	// -----------------------------------------------------	
	// OCP constants
	// -----------------------------------------------------	

	int synblock;
	std::string rname;
	int ocp_base;
	int mem_size;

	ci_data_t rcvdata;
	ci_addr_t rcvaddr;


	void select() { 	
		switch(synblock) {
		case 0 :
			rname="Top";
			ocp_base = OcpStdpCtrl_t.startaddr;
			mem_size=0x7fff;
			rc = &hc->getSC(HicannCtrl::SYNAPSE_TOP);
			break;
		case 1 :
			rname="Bottom";
			ocp_base = OcpStdpCtrl_b.startaddr;
			mem_size=0x7fff;
			rc = &hc->getSC(HicannCtrl::SYNAPSE_BOTTOM);
			break;
		}
	}

	uint read(uint adr){
		rc->read_cmd(adr,0);
		rc->get_data(rcvaddr,rcvdata);
		return rcvdata;
	}
	class ad read_ad(uint adr){
		rc->read_cmd(adr,0);

#ifdef NCSIM 
		wait(1, SC_US);//Hack for simulation	

#endif //ncsim 
		rc->get_data(rcvaddr,rcvdata); 
		return ad(rcvaddr,rcvdata);		
	}
	
	void write(uint adr, uint data){
			rc->write_cmd(adr, data,0);
	}		

	//generate repeater address by permuting address bits (HICANN error!!!)
//	uint repadr(uint adr) {
//		uint ra=0;
//		for(int i=0;i<7;i++)ra|= (adr& (1<<i))?(1<<(6-i)):0;
//		return ra;
//	}
	int speed; //for faster simulations

	static const int TAG             = 1; //STDP uses second tag

 	static const int MEM_BASE        = 0;
// 	static const int MEM_SIZE        = 10; depends, see select()
	static const int MEM_WIDTH       = 8; // bit
	static const int MEM_TEST_NUM    = 2; // times size

 	static const int REG_BASE        = 0x40;  // bit #6 set 

	// RamTest interface
/*
	inline void clear() {};
	inline void write(int addr, uint value) { ll->write_cmd(ocp_base + addr, value, 10); }
	inline void read(int addr) { ll->read_cmd(ocp_base + addr, 10); }
	inline bool rdata_(uint& value, int block) { cmd_pck_t p; int r=ll->get_data(&p, block); value=p.data; return (bool)r; }
	inline bool rdata(uint& value) { return rdata_(value, false); }
	inline bool rdatabl(uint& value) { return rdata_(value, true); }
*/

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		rcvdata=0;rcvaddr=0;
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

		synblock=1;
		select();

	 	dbg(0) << "StdpCtrl base adr: 0x" << hex << ocp_base << endl;

		// ----------------------------------------------------
		// do ramtest of switch matrices
		// ----------------------------------------------------

		L1SwitchControl *lc;		//for testing

		// get pointer to left crossbar layer 1 switch control class of HICANN 0
		lc = &hc->getLC(HicannCtrl::L1SWITCH_CENTER_LEFT);

		uint startaddr = 0;
		uint maxaddr   = 63;
		uint datawidth = 4;

		uint testdata = 0;
		//stdp control definitions from tb_stdpctrl.sv
		static const uint sram=0x0,
								sram_enctrl=0x0,
								sram_endrv=0x100,
								sram_engmax=0x200;
								
		const uint scfg=0x500;
		const uint regs=0x400;
		const uint synw=0x600;
		
		const uint ctrlreg=regs,
						cnfgreg=regs+1,
						lut=regs+2,
						synd=regs+6;
	
		//STDP control ctrlreg bit definitions
		const uint cmd=0,			//command for digital stdp control
						encr=4, 		  //enable correlation readback (saves power if not in use)		
						continuous=5,	//continuouse auto stdp
						newcmd=6,     //for a new command, write cmd and newcmd=!newcmd		
						adr=8,        //target row, first row in auto stdp mode
						lastadr=16;   //last row in auto stdp mode
						
		//STDP control sequencer command codes
		const uint cmd_idle=0,// no active command
						cmd_read=1,		// reads correlation pattern and weights
						cmd_proc=2,		// like READ, but performs one STDP cycle and updates weights
						cmd_write=3,	// writes weight register to synapse row
						cmd_auto=4,		// starts auto stdp: PROC cycles from adr to lastadr
						cmd_wdec=5,		// write weight register to synapse address decode
						cmd_rdec=6,		// read synapse address decode in weight register
						cmd_readpat=7,// reads correlation pattern 
						cmd_readpatres=8;	// reads correlation pattern and resets synapses
		
		const uint cfg_pattern=0, //2x4bit 
							cfg_endel=8,		//enable delay
							cfg_predel=12, 	//precharge delay
							cfg_gen=16, 		//global enable bits of synapse drivers
							cfg_dllresetb=20; //left/right synapse block dllresetb bit

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

	// ----------------------------------------------------
	// now test StdpCtrl
	// ----------------------------------------------------
		speed = 80;
		
		cout<<"Enter speed= ";
		cin>>speed;
		dbg(0)<<"start StdpCtrl access:"<<rname<<endl;

		cout<<"dump StdpCtrl control registers"<<endl;
		rc->read_cmd(scfg,0);
		rc->get_data(rcvaddr,rcvdata);
		cout << hex << "radr \t0x"<<scfg<<" adr \t0x"  << rcvaddr << ", \t0"  << rcvdata << endl;

		cout<<"change StdpCtrl control registers"<<endl;
		write(scfg, 0x1234);		//higher read delay
		write(scfg+1,0x4567);
		cout << read_ad(scfg)<<read_ad(scfg+1)<< endl;

		cout<<"write lut"<<endl;
		write(cnfgreg,0x1355a);
		write(lut, 0x00112233);
		write(lut+1, 0x44556677);
		write(lut+2, 0x8899aabb);
		write(lut+3, 0xccddeeff);

		cout<<"read lut"<<endl;
		cout << read_ad(cnfgreg);
		cout << read_ad(lut);
		cout << read_ad(lut+1);
		cout << read_ad(lut+2);
		cout << read_ad(lut+3)<<endl; 
		
		cout<<"test syndriver sram"<<endl;

		uint startrow=4;
		uint maxrows=0x2e0;
		uint tdata[32*256*2];
		uint error=0;
		srand(randomseed);

		for(uint r=startrow;r<maxrows;r+=4 * speed){
			if( (r&0xff)>223){r+=0x100;r&=0x300;}
			write(sram+r,tdata[r]=rand()&0xff);
			write(sram+r+1,tdata[r+1]=rand()&0xff);
			write(sram+r+2,(tdata[r+2]=rand()&0xff)<<8);
			write(sram+r+3,(tdata[r+3]=rand()&0xff)<<8);
		}

		for(uint r=startrow;r<maxrows;r+=4* speed){
			if( (r&0xff)>223){r+=0x100;r&=0x300;}
			if(!(r%8))cout<<endl;
			cout <<read_ad(sram+r);
			if(((rcvdata)&0xff) !=tdata[r])
				{error++;

					cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata,8)<<" written: "<<bino(tdata[r],8)<<endl;
				 read_ad(sram+r);//read a second time
				 if(((rcvdata)&0xff)!=tdata[r])
					{cout<<endl<<"Second read: "<<rcvaddr<<" read: "<<bino(rcvdata,8)<<" written: "<<bino(tdata[r],8)<<endl;}
				}
			cout <<read_ad(sram+r+1);
			if((rcvdata&0xff)!=tdata[r+1])
				{error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata,8)<<" written: "<<bino(tdata[r+1],8)<<endl;
				 read_ad(sram+r+1);//read a second time
				 if((rcvdata&0xff)!=tdata[r+1])
					{cout<<endl<<"Second read: "<<rcvaddr<<" read: "<<bino(rcvdata,8)<<" written: "<<bino(tdata[r+1],8)<<endl;}
				}
			cout <<read_ad(sram+r+2);
			if(((rcvdata>>8)&0xff) !=tdata[r+2])
				{error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata>>8,8)<<" written: "<<bino(tdata[r+2],8)<<endl;
				 read_ad(sram+r+2);//read a second time
				 if(((rcvdata>>8)&0xff)!=tdata[r+2])
					{cout<<endl<<"Second read: "<<rcvaddr<<" read: "<<bino(rcvdata>>8,8)<<" written: "<<bino(tdata[r+2],8)<<endl;}
				}
			cout <<read_ad(sram+r+3);
			if(((rcvdata>>8)&0xff)!=tdata[r+3])
				{error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata>>8,8)<<" written: "<<bino(tdata[r+3],8)<<endl;
				 read_ad(sram+r+3);//read a second time
				 if(((rcvdata>>8)&0xff)!=tdata[r+3])
					{cout<<endl<<"Second read: "<<rcvaddr<<" read: "<<bino(rcvdata>>8,8)<<" written: "<<bino(tdata[r+3],8)<<endl;}
				}
		}
		cout<<dec<<"errors: "<<error<<endl;

		if(error!=0) return 0;
		srand(randomseed);
		startrow=4;
		maxrows=224;
		for(uint r=startrow;r<maxrows;r+=speed){
			cout<<hex<<"test weight and decode write, row: "<<r<<endl;
//			for(int i=0;i<32;i++)write(synw+i,i+(r<<8)|((~i & 0xffff) <<16) );
			for(int i=0;i<32;i++)write(synw+i,tdata[r*64+i]=rand());
			/*		write(synw, 0x00001111);
			write(synw+1, 0x22223333);
			write(synw+2, 0x44445555);
			write(synw+3, 0x66667777);
			write(synw+4, 0x88889999);
			write(synw+5, 0xaaaabbbb);
			write(synw+6, 0xccccdddd);
			write(synw+7, 0xeeeeffff);*/

			write(ctrlreg, cmd_write | (r<<adr));
			cout <<read_ad(ctrlreg);
			/*		write(synw,0x01234567);
			write(synw+1,0x89abcdef);*/
//			for(int i=0;i<32;i++)write(synw+i,(0xff-i+(r<<8))|((~(0xff-i) & 0xffff) <<16) );
			for(int i=0;i<32;i++)write(synw+i,tdata[r*64+32+i]=rand());

			write(ctrlreg, cmd_wdec| (r<<adr)|(1<<newcmd) );
			cout << read_ad(ctrlreg)<<endl;

			cout<<"read back weights"<<endl;
			write(ctrlreg, cmd_read | (r<<adr));
			cout << read_ad(ctrlreg);

			for(int i=0;i<32;i++,i%8?cout:cout<<endl){cout<<read_ad(synd+i);if(rcvdata!=tdata[r*64+i])
				{error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata)<<" written: "<<bino(tdata[r*64+i])<<endl;}
			}

			cout<<"read back decoder"<<endl;
			write(ctrlreg, cmd_rdec | (r<<adr)|(1<<newcmd));
			cout << read_ad(ctrlreg);

			for(int i=0;i<32;i++,i%8?cout:cout<<endl){cout << read_ad(synd+i);if(rcvdata!=tdata[r*64+32+i])
				{error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata)<<" written: "<<bino(tdata[r*64+32+i])<<endl;}
			}
			cout<<dec<<"errors: "<<error<<endl;
			/*if(kbhit()){//does not work in simulation
			  switch(getch()){
			  case 'x':r=0xffff;break;//exit outer loop
				}
			}*/	

		}
		
		cout<<dec<<"errors: "<<error<<endl;

		if(error!=0) return 0;
		for(uint r=startrow;r<maxrows;r+=speed){
			cout<<hex<<"second read back, row: "<<r<<endl;

			cout<<"read back weights"<<endl;
			write(ctrlreg, cmd_read | (r<<adr));
			cout << read_ad(ctrlreg);

			for(int i=0;i<32;i++,i%8?cout:cout<<endl){cout<<read_ad(synd+i);if(rcvdata!=tdata[r*64+i]){
				error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata)<<" written: "<<bino(tdata[r*64+i])<<endl;}
			}
			cout<<"read back decoder"<<endl;
			write(ctrlreg, cmd_rdec | (r<<adr)|(1<<newcmd));
			cout << read_ad(ctrlreg);

			for(int i=0;i<32;i++,i%8?cout:cout<<endl){cout << read_ad(synd+i);if(rcvdata!=tdata[r*64+32+i]){
				error++;cout<<endl<<"Error at address: "<<rcvaddr<<" read: "<<bino(rcvdata)<<" written: "<<bino(tdata[r*64+32+i])<<endl;}
			}
			cout<<dec<<"errors: "<<error<<endl;
			/*if(kbhit()){
			  switch(getch()){
			  case 'x':r=0xffff;break;//exit outer loop
				}
			}*/	
		}
		if(error==0)	
			return 1;
		else
			return 0;

	}
};


class LteeTmSubm2StdpCtrl : public Listee<Testmode>{
public:
	LteeTmSubm2StdpCtrl() : Listee<Testmode>(std::string("tmsubm2_stdpctrl"), std::string("test stdpctrl synapse access etc")){};
	Testmode * create(){return new TmSubm2StdpCtrl();};
};

LteeTmSubm2StdpCtrl ListeeTmSubm2StdpCtrl;

