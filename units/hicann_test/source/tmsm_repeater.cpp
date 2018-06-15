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
#include "neuronbuilder_control.h"
#include "l1switch_control.h" //layer 1 switch control class
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"

#include <queue>
#include <limits>

//functions defined in getch.c
extern "C" int getch(void);

using namespace std;
using namespace facets;

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


class TmSMRepeater : public Testmode /*, public RamTestIntfUint*/ {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmsm_repeater"; return ss.str();}
public:

// -----------------------------------------------------------
// CHIP constants
// -----------------------------------------------------------

// synapse control
enum sc_address {sc_sram=0x0,
								sc_enctrl=0x0,
								sc_endrv=0x100,
								sc_engmax=0x200,
								sc_scfg=0x500,
								sc_regs=0x400,
								sc_synw=0x600};

enum sc_regs {sc_ctrlreg=sc_regs,
							sc_cnfgreg=sc_regs+1,
							sc_lut=sc_regs+2,
							sc_synd=sc_regs+6};
	
//STDP control ctrlreg bit definitions
enum sc_ctrlreg {sc_cmd_p=0,			//command for digital stdp control
								sc_encr_p=4, 		  //enable correlation readback (saves power if not in use)		
								sc_continuous_p=5,	//continuouse auto stdp
								sc_newcmd_p=6,     //for a new command, write cmd and newcmd=!newcmd		
								sc_adr_p=8,        //target row, first row in auto stdp mode
								sc_lastadr_p=16};   //last row in auto stdp mode
						
//STDP control sequencer command codes
enum sc_cmds {sc_cmd_idle=0,// no active command
						sc_cmd_read=1,		// reads correlation pattern and weights
						sc_cmd_proc=2,		// like READ, but performs one STDP cycle and updates weights
						sc_cmd_write=3,	// writes weight register to synapse row
						sc_cmd_auto=4,		// starts auto stdp: PROC cycles from adr to lastadr
						sc_cmd_wdec=5,		// write weight register to synapse address decode
						sc_cmd_rdec=6,		// read synapse address decode in weight register
						sc_cmd_readpat=7,// reads correlation pattern 
						sc_cmd_readpatres=8};	// reads correlation pattern and resets synapses
		
enum sc_cfg {sc_cfg_pattern_p=0, //2x4bit 
							sc_cfg_endel_p=8,		//enable delay
							sc_cfg_predel_p=12, 	//precharge delay
							sc_cfg_gen_p=16, 		//global enable bits of synapse drivers
							sc_cfg_dllresetb_p=20}; //left/right synapse block dllresetb bit

//cbot: control/config bottom, ctop: top, pbot: predrive bottom, ptop: top, gmax top/bottom identical
enum sc_sdctrl {sc_cbot_enstdf=0x80,
								sc_cbot_en=0x40,
								sc_cbot_locin=0x20,
								sc_cbot_topin=0x10,
								sc_selgm1=8,
								sc_selgm0=4,
								sc_seni=2,
								sc_senx=1,
								sc_ctop_dep=0x80,
								sc_ctop_cap2=0x40,
								sc_ctop_cap1=0x20,
								sc_ctop_cap0=0x10 };

enum sc_sdgmax {sc_dac0_p=0,
								sc_dac1_p=4 };

enum sc_sdpredrv {sc_pbot_preout0_p=0,
								sc_pbot_preout2_p=4,
								sc_ptop_preout1_p=0,
								sc_ptop_preout3_p=4 };

// neuron control
enum nc_regs {nc_enable=0,nc_select=1,nc_slow=2, nc_dncloopb=3, nc_randomreset=4, nc_phase=5, nc_seed=6, nc_period=7};

// repeater control 
enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater location
enum rc_regs {rc_config=0x80,rc_status=0x80,rc_td0=0x81,rc_td1=0x87,rc_scfg=0xa0};
enum rc_srambits {rc_touten=0x80,rc_tinen=0x40,rc_recen=0x20,rc_dir=0x10,rc_ren1=8,rc_ren0=4,rc_len1=2,rc_len0=1};
enum rc_config
{rc_fextcap1=0x80,rc_fextcap0=0x40,rc_drvresetb=0x20,rc_dllresetb=0x10,rc_start_tdi1=8,rc_start_tdi0=4,rc_start_tdo1=2,rc_start_tdo0=1};

// -------------------------------------------------------
// members
// ------------------------------------------------------

	HicannCtrl *hc; 
	CtrlModule *rc,*nc,*sc; //repeater control, neuron control, synapse control for read/write functions
	CtrlModule *rl,*rr,*rtl,*rbl,*rtr,*rbr;	//all repeaters
	CtrlModule *rca[6];
	CtrlModule *st,*sb; //synapse control top/bottom
  LinkLayer<ci_payload,ARQPacketStage2> *ll;
	string rc_name[6];

	int repeater;
	string rname;
	int ocp_base;
	int mem_size;
	ci_data_t rcvdata;
	ci_addr_t rcvaddr;
	uint error;

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

	uint rcr(uint adr){
		rc->read_cmd(adr,0);
		rc->get_data(rcvaddr,rcvdata);
		return rcvdata;
	}
	class ad rcr_ad(uint adr){
		rc->read_cmd(adr,0);
		rc->get_data(rcvaddr,rcvdata);
		return ad(rcvaddr,rcvdata);		
	}
	
	void rcw(uint adr, uint data){
			rc->write_cmd(adr, data,0);
	}		

	uint ncr(uint adr){
		nc->read_cmd(adr,0);
		nc->get_data(rcvaddr,rcvdata);
		return rcvdata;
	}
	class ad ncr_ad(uint adr){
		nc->read_cmd(adr,0);
		nc->get_data(rcvaddr,rcvdata);
		return ad(rcvaddr,rcvdata);		
	}
	
	void ncw(uint adr, uint data){
			nc->write_cmd(adr, data,0);
	}		

	uint scr(uint adr){
		sc->read_cmd(adr,0);
		sc->get_data(rcvaddr,rcvdata);
		return rcvdata;
	}
	class ad scr_ad(uint adr){
		sc->read_cmd(adr,0);
		sc->get_data(rcvaddr,rcvdata);
		return ad(rcvaddr,rcvdata);		
	}
	
	void scw(uint adr, uint data){
			sc->write_cmd(adr, data,0);
	}		

	//generate repeater address by permuting address bits (HICANN error!!!)
	static uint repadr(uint adr) {
		uint ra=0;
		for(int i=0;i<7;i++)ra|= (adr& (1<<i))?(1<<(6-i)):0;
		return ra;
	}

	//write syndriver data by permuting data bits (HICANN error!!!) and shifting data for even rows
	void syndw(uint adr,uint data) {
		uint sd=0;
		for(int i=0;i<8;i++)sd|= (data& (1<<i))?(1<<(7-i)):0;
		if(!(adr&1))sd<<=8;
		scw(adr,sd);
	}

	//read syndriver data by permuting data bits (HICANN error!!!) and shifting data for even rows
	uint syndr(uint adr) {
		uint data,sd;
		sd=scr(adr);
		for(int i=0;i<8;i++)data|= (sd& (1<<i))?(1<<(7-i)):0;
		if(!(adr&1))data>>=8;
		return data;
	}

	//write synapse row (decoder=0: write weights, =1: write decoder values), data[0], bits[3:0]:leftmost synapse
	void synw(uint row,vector<uint> &data,bool decoder) {
		for(int w=0;w<32;w++){
			uint sd=0;
			for(int i=0;i<32;i++)sd|= (data[w]& (1<<i))?(1<<(31-i)):0;
			scw(sc_synw+w,sd);
		}	
		bool nc=scr(sc_ctrlreg) & (1<<sc_newcmd_p);
		nc = !nc; //invert new command flag
		
		if(decoder)
			scw(sc_ctrlreg, sc_cmd_wdec| (row<<sc_adr_p)|(nc<<sc_newcmd_p) );
		else
			scw(sc_ctrlreg, sc_cmd_write| (row<<sc_adr_p)|(nc<<sc_newcmd_p) );
	}

	//gray to binary conversion for time readout in repeater control
	static uint gtob(uint time) {
		uint b, t=time&0x3ff;
		b=t&0x200;
		for(int i=8;i>=0;i--){b |= (t ^ (b>>1) )&(1<<i);}
		return b;
	}


	//data format: 10bit time, 6 bit nn
	//total order is a bit strange: lowbyte            higbyte
	//                              time[1:0] nn[5:0]  time[9:2]
	//also bit order of neuron number is reversed in readout!?

	class rc_td {
		uint time;
		uint nn;
	public:
		rc_td(void){};
		rc_td(byte_t lb, byte_t hb){set(lb,hb);};
		byte_t getLowByte(void){return ((time&0x3)<<6)|(nn&0x3f);};
		byte_t getHighByte(void){return (time&0x3ff)>>2;};
		void set(byte_t lb,byte_t hb){
			uint n;
			nn=0;
			n=lb&0x3f;time=((uint)hb<<2)|((lb&0xc0)>>6);
			for(uint i=0;i<6;i++){nn<<=1;if(n&(1<<i))nn|=1;}
		};
		uint getTimeB(){return gtob(time);};
		uint getNN(){return nn;};
		uint getTime(){return time;};
		uint setNN(uint n){return nn=n&0x3f;};
		uint setTime(uint t){return time=t&0x3ff;};
	};

						
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
    // get pointer to LinkLayer

	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;

		L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;		//for testing

		// get pointer to all crossbars
		lcl = &hc->getLC_cl();
		lcr = &hc->getLC_cr();
		lctl =&hc->getLC_tl();
		lcbl =&hc->getLC_bl();
		lctr =&hc->getLC_tr();
		lcbr =&hc->getLC_br();

		uint startaddr = 0;
//		uint maxaddr   = 63;
		uint datawidth = 4;

		uint testdata = 0;
		rcvdata  = 0;
		rcvaddr  = 0;

		bool result = true;

		srand(1);

		// intialize repeater control modules

    rl  = new CtrlModule(hc, 0, OcpRep_l.startaddr, 0xff); //repeater blocks use 7 address bits + SRAM control bit
    rr  = new CtrlModule(hc, 0, OcpRep_r.startaddr, 0xff);
    rtl = new CtrlModule(hc, 0, OcpRep_tl.startaddr, 0xff);
    rbl = new CtrlModule(hc, 0, OcpRep_bl.startaddr, 0xff);
    rtr = new CtrlModule(hc, 0, OcpRep_tr.startaddr, 0xff);
    rbr = new CtrlModule(hc, 0, OcpRep_br.startaddr, 0xff);

		rca[rc_l]=rl;		rc_name[rc_l]="HorizLeft";
		rca[rc_r]=rr;		rc_name[rc_r]="HorizRight";
		rca[rc_tl]=rtl;	rc_name[rc_tl]="TopLeft";
		rca[rc_bl]=rbl;	rc_name[rc_bl]="BottomLeft";
		rca[rc_tr]=rtr;	rc_name[rc_tr]="TopRight";
		rca[rc_br]=rbr;	rc_name[rc_br]="BottomRight";

	st = new CtrlModule(hc, 1, OcpStdpCtrl_t.startaddr , 0x7fff); //StdpCtrl blocks use xx address bits + SRAM control bit + one additional control bit??
	sb = new CtrlModule(hc, 1, OcpStdpCtrl_b.startaddr , 0x7fff);

	// create neuron control module
    nc = new CtrlModule(hc, 0, OcpNeuronCtrl.startaddr, 14);

		// ----------------------------------------------------
		// repeater testmode
		// ----------------------------------------------------

		char c;
		bool cont=true;
		do{
			cout << "Select test option:" << endl;
			cout << "l: quick l1 and communication check" << endl;
			cout << "L: full l1 switch test" << endl;
			cout << "t: change sram timing" << endl;
			cout << "r: full repeater sram test" << endl;
			cout << "s: activate sending repeaters and background" << endl;
			cout << "h: activate sending repeaters for hicann->hicann test" << endl;
			cout << "c: connect all crossbar switches" << endl;
			cout << "d: disconnect all crossbar switches" << endl;
			cout << "1: write left cb" << endl;
			cout << "2: write right cb" << endl;
			cout << "3: write top left syndriver switches" << endl;
			cout << "n: activate sending repeaters and background for neuron stimulation" << endl;
			cout << "i: enable synaptic input" << endl;
			cout << "I: disable synaptic input" << endl;
			cout << "e: erase synapse arrays and drivers" << endl;
			cout << "D: dump receiver test in data" << endl;
			cout << "Z: ARQ reset/init" << endl;
			cout << "w: Set weights of row 0"<< endl;
			cout << "x: exit" << endl;

			cin >> c;
#ifndef NCSIM
// when running in ncsim we cannot ignore the remaining characters in cin's buffer
			std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
#endif
			switch(c){
			case 'l':{
				if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
					dbg(0) << "ERROR: Init failed, abort" << endl;
					return 0;
				}
				// ----------------------------------------------------
				// do ramtest of switch matrices
				// ----------------------------------------------------
				for(uint i=startaddr;i<=5;i++){
					testdata = rand()%(1<<datawidth);
					lcl->write_cfg(i, testdata);
					lcl->read_cfg(i);

					lcl->get_read_cfg(rcvaddr, rcvdata);
		
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
			}break;

			case 't':
				cout<<"repeater block number (0-5:tl,tr,bl,br,hl,hr,6=set all to defaults)?"<<endl;
				cin>>repeater;
				if(repeater<6){
	        select();
	        dbg(0) << "Repeater base adr: 0x" << hex << ocp_base << endl;
	        rc = new CtrlModule(hc, 0, ocp_base, 0xff); //repeater blocks use 7 address bits + SRAM control bit
	        cout<<"trd: " << rcr(0xa0) << " twr: " << (rcr(0xa1)&0xf);
	        cout << " tsu: " << ((rcr(0xa1)>>4)&0xf) <<endl;
	        cout <<"timing: trd, twr, tsu?"<<endl;
	        uint trd,twr,tsu;
	        cin >>trd>>twr>>tsu;
	        rcw(0xa0,trd);rcw(0xa1,(twr&0xf)|((tsu&0xf)<<4));
	        cout<<"trd: " << rcr(0xa0) << " twr: " << (rcr(0xa1)&0xf);
	        cout << " tsu: " << ((rcr(0xa1)>>4)&0xf) <<endl;
  				delete rc;
				}else{
					for(int r=0;r<6;r++){
						repeater=r;
						select();
					 	cout << "Repeater base adr: 0x" << hex << ocp_base;
						rc = new CtrlModule(hc, 0, ocp_base, 0xff); //repeater blocks use 7 address bits + SRAM control bit
		        rcw(0xa0,8);rcw(0xa1,0x11);
		        cout<<" trd: " << rcr(0xa0) << " twr: " << (rcr(0xa1)&0xf);
		        cout<< " tsu: " << ((rcr(0xa1)>>4)&0xf) <<endl;
						delete rc;
					}				
				}
				break;
				
			case 'r':
			// now test repeater
				error=0;
				for(int r=0;r<6;r++){
					repeater=r;
					select();
				 	dbg(0) << "Repeater base adr: 0x" << hex << ocp_base << endl;
					rc = new CtrlModule(hc, 0, ocp_base, 0xff); //repeater blocks use 7 address bits + SRAM control bit

					dbg(0)<<"test repeater sram:"<<rname<<endl;

					srand(randomseed+r);
					const uint rmask=0x5f; //always keep recen and touten zero!
					uint tdata[0x40];
					for(int i=0;i<mem_size;i++)
						{
							tdata[i]=rand()%256 & rmask;
							rc->write_cmd(repadr(i),tdata[i],0);
						}

					srand(randomseed+r);
					for(int i=0;i<mem_size;i++)
						{
							rc->read_cmd(repadr(i),0);
							rc->get_data(rcvaddr,rcvdata);
							if(rcvdata != tdata[i])error++;
							dbg(0) << hex << "radr \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,8) << "	data written: 0b" <<
								bino(tdata[i],8)<<endl;
						}
					delete rc;
				}
					
				cout <<"Errors: "<<error<<endl;
			break;
	
			case 'L':{
			// full L1 switch test
				error=0;
				L1SwitchControl *l1;
				uint rmask; 
				for(int r=0;r<6;r++){
					switch (r){
					case 0:l1=lcl;mem_size=64;rmask=15;break;
					case 1:l1=lcr;mem_size=64;rmask=15;break;
					case 2:l1=lctl;mem_size=112;rmask=0xffff;break;
					case 3:l1=lctr;mem_size=112;rmask=0xffff;break;
					case 4:l1=lcbl;mem_size=112;rmask=0xffff;break;
					case 5:l1=lcbr;mem_size=112;rmask=0xffff;break;
					}

					srand(randomseed+r);
					uint tdata[0x80];
					for(int i=0;i<mem_size;i++)
						{
							tdata[i]=rand() & rmask;
							l1->write_cmd(i,tdata[i],0);
						}

					srand(randomseed+r);
					for(int i=0;i<mem_size;i++)
						{
							l1->read_cmd(i,0);
							l1->get_data(rcvaddr,rcvdata);
							if(rcvdata != tdata[i])error++;
							dbg(0) << hex << "row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<
								bino(tdata[i],16)<<endl;
						}
				}
					
				cout <<"Errors: "<<error<<endl;
			}break;

			case 's': { //activate sending repeaters and background generators to output 1 MHz events, check them by using tdout
				uint random = 0x0000;//random mask for bg

				//clear repeaters
				for(uint l=0;l<6;l++){
					rc = rca[l];
					for(int r=0;r<64;r++)rcw(repadr(r),0); //clear repeater
					rcw(rc_config, rc_dllresetb|rc_drvresetb);
				}
				
// --- config neuron control

				//initialize neuron control: bg as input: 0xff, merger tree level 0: all 0, level 1: 1,0, level 2: 1
//				ncw(nc_enable,1);//merge spl10 with bg0
				ncw(nc_enable,0);//only bg
				ncw(nc_select,0x50ff);
				//connected to spl1: bg0,l1_0,l2_0,bg3,l3_0,bg5,l1_3,bg7
				ncw(nc_slow,0x59a9);
//				ncw(nc_slow,0xffff);
				//enable all spl1 outputs
				ncw(nc_dncloopb,0);
				//select a period of 1 MHz for all bg
				for(int r=0;r<8;r++)
					ncw(nc_period+r,100);							
				//set random seed 
				ncw(nc_seed,200);
				//stop all bg
				ncw(nc_randomreset,random);//disable bg channels
				//start all bg
				ncw(nc_randomreset,random|0xff);				
//				cout<<"neuron control: "<<endl;
//				for(int r=0;r<15;r++){
//					cout<<ncr_ad(r)<<endl;
//				}
				
// --- config repeaters
				rc_td tout;
				//initialize tdo registers for dll locking
				for(uint l=0;l<6;l++){
					rc=rca[l];
					for(int r=0;r<12;r+=2){
						tout.setNN(0);tout.setTime(50);
						rcw(rc_td0+r,tout.getLowByte());rcw(rc_td0+r+1,tout.getHighByte());
					}
				}

// -- senders	 for sending repeater omit rc_recen for driving internal l1 line!

				rc=rtl;
				rcw(repadr(16),rc_recen|rc_touten ); 
				rc=rl;
				rcw(repadr(0),rc_tinen|rc_touten|rc_dir ); //sends to h0,connected to v0
//				rcw(repadr(31),rc_recen|rc_tinen|rc_touten|rc_dir ); //sends to h62,connected to v126->can not work reliably due to repeater bug
//				rcw(repadr(28),rc_tinen|rc_touten|rc_dir ); //sends to h56,connected to v120
	
// --- receivers
				rc=rtl;
//				rcw(repadr(63),rc_recen|rc_touten); //listen to v126
//				rcw(repadr(60),rc_recen|rc_touten); //listen to v120
//				rcw(repadr(0),rc_recen|rc_touten); //listen to v0
//				rc=rbr;
//				rcw(repadr(1),rc_recen|rc_touten); 
				rc=rtr;
//				rcw(repadr(63),rc_recen|rc_touten); 
//				rcw(repadr(0),rc_recen|rc_touten); 
//				rc=rbl;
//				rcw(repadr(1),rc_recen|rc_touten); 

				
		//start dll operation and tdi and tdo
				for(uint l=0;l<6;l++){
					rc=rca[l];
					rcw(rc_config, rc_drvresetb|rc_dllresetb |rc_start_tdo1|rc_start_tdo0 );
				}		

				//tiem for dlls to lock
				cout<<"neuron control: "<<endl;
				for(int r=0;r<15;r++){
					cout<<ncr_ad(r)<<endl;
				}
				
				//now change neuron numbers of sender after dll has locked
				rc=rl;
				tout.setNN(0x1);tout.setTime(2);rcw(rc_td0+0,tout.getLowByte());rcw(rc_td0+0+1,tout.getHighByte());
				tout.setNN(0x20);tout.setTime(2);rcw(rc_td0+2,tout.getLowByte());rcw(rc_td0+2+1,tout.getHighByte());
				tout.setNN(0x6);tout.setTime(2);rcw(rc_td0+4,tout.getLowByte());rcw(rc_td0+4+1,tout.getHighByte());
				tout.setNN(0);tout.setTime(5);rcw(rc_td0+6,tout.getLowByte());rcw(rc_td0+6+1,tout.getHighByte());
				tout.setNN(0x1);tout.setTime(5);rcw(rc_td0+8,tout.getLowByte());rcw(rc_td0+8+1,tout.getHighByte());
				tout.setNN(0);tout.setTime(5);rcw(rc_td0+10,tout.getLowByte());rcw(rc_td0+10+1,tout.getHighByte());

				//start receiveing operation on tdi and tdo
				for(uint l=0;l<6;l++){
					rc=rca[l];
					rcw(rc_config, rc_drvresetb|rc_dllresetb |rc_start_tdo1|rc_start_tdo0|rc_start_tdi1|rc_start_tdi0 );
				}		

				//dump tdi data
				for(uint l=0;l<6;l++){
					rc=rca[l];
					cout<<rc_name[l]<<"\t tdi: "<<rcr_ad(rc_status)<<endl;
					for(int line=0;line<3;line++){ //use three rows for output
						for(int col=0;col<2;col++){//and two columns
							rc_td tin(rcr(rc_td0+line*2+col*6),rcr(rc_td0+line*2+col*6+1));
							cout<<dec<<"t: "<<tin.getTimeB()<<"\tnn: "<<hex<<tin.getNN()<<"\t";			
						}
						cout<<endl;
					}
				}				
			}break;
			case 'c': 
				//connect all cb switches
				for(uint i=0;i<64;i++)lcl->write_cfg(i,0xf);
				for(uint i=0;i<64;i++)lcr->write_cfg(i,0xf);
			break;
			case 'd': 
				//disconnect all switches
				for(uint i=0;i<64;i++){lcl->write_cfg(i,0);lcr->write_cfg(i,0);}
				for(uint i=0;i<112;i++){lctl->write_cfg(i,0);lcbl->write_cfg(i,0);
																lctr->write_cfg(i,0);lcbr->write_cfg(i,0);}
			break;
			case '1': {
				//connect one
				cout <<"Enter bit-pattern for crossbar left (row, pattern)";
				uint r,d;
				cin >>r>>d;
				lcl->write_cfg(r,d);
			}break;
			case '2': {
				//connect one
				cout <<"Enter bit-pattern for crossbar right (row, pattern)";
				uint r,d;
				cin >>r>>d;
				lcr->write_cfg(r,d);
			}break;
			case '3': {
				//connect left syndriver
				cout <<"Enter syndriver left start row, number of rows and bit number?";
				uint r,n,d,cnt=0;
				cin >>r>>n>>d;
				for(uint i=0;i<n;i++){
						if(r+16*i+1 < 112){
							lctl->write_cfg(r+16*i,1<<d);	//connect lines with identical vertical switch positons, offset is 16 because there are two identical switch setups for internal and external syndrivers
							lctl->write_cfg(r+16*i+1,1<<d);
							cnt++;
						}
					}
				cout<<"Wrote "<<dec<<2*cnt<<" rows with pattern "<<bino(1<<d,16)<<endl;
				}	break;

			case 'n': { //activate sending repeaters and background generators to output 1 MHz events, for neuron stimulation

				cout <<"Enter stimulus freq?";
				uint fstim;
				cin>>fstim;
				uint random = 0x0000;//random mask for bg

				//clear repeaters
				for(uint l=0;l<6;l++){
					rc = rca[l];
					for(int r=0;r<64;r++)rcw(repadr(r),0); //clear repeater
					rcw(rc_config, rc_dllresetb|rc_drvresetb);
				}
				
// --- config neuron control

				//initialize neuron control: bg as input: 0xff, merger tree level 0: all 0, level 1: 1,0, level 2: 1
//				ncw(nc_enable,1);//merge spl10 with bg0
				ncw(nc_enable,0);//only bg
				ncw(nc_select,0x50ff);
				//connected to spl1: bg0,l1_0,l2_0,bg3,l3_0,bg5,l1_3,bg7
				ncw(nc_slow,0x59a9);
//				ncw(nc_slow,0xffff);
				//enable all spl1 outputs
				ncw(nc_dncloopb,0);
				//select a period of 1 MHz for all bg
				for(int r=0;r<8;r++)
					ncw(nc_period+r,fstim);							
				//set random seed 
				ncw(nc_seed,200);
				//stop all bg
				ncw(nc_randomreset,random);//disable bg channels
				//start all bg
				ncw(nc_randomreset,random|0xff);				
//				cout<<"neuron control: "<<endl;
//				for(int r=0;r<15;r++){
//					cout<<ncr_ad(r)<<endl;
//				}
				
// --- config repeaters
				rc_td tout;
				//initialize tdo registers for dll locking
				for(uint l=0;l<6;l++){
					rc=rca[l];
					for(int r=0;r<12;r+=2){
						tout.setNN(0);tout.setTime(50);
						rcw(rc_td0+r,tout.getLowByte());rcw(rc_td0+r+1,tout.getHighByte());
					}
				}

// -- senders	 for sending repeater omit rc_recen for driving internal l1 line!

				rc=rtl;
//				rcw(repadr(16),rc_recen|rc_touten ); 
				rc=rl;
//				rcw(repadr(0),rc_tinen|rc_touten|rc_dir ); //sends to h0,connected to v0
//				rcw(repadr(31),rc_recen|rc_tinen|rc_touten|rc_dir ); //sends to h62,connected to v126->can not work reliably due to repeater bug
				rcw(repadr(28),rc_tinen|rc_touten|rc_dir ); //sends to h56,connected to v120
	
// --- receivers
				rc=rtl;
//				rcw(repadr(63),rc_recen|rc_touten); //listen to v126
				rcw(repadr(60),rc_recen|rc_touten); //listen to v120
//				rcw(repadr(0),rc_recen|rc_touten); //listen to v0
//				rc=rbr;
//				rcw(repadr(1),rc_recen|rc_touten); 
				rc=rtr;
//				rcw(repadr(63),rc_recen|rc_touten); 
//				rcw(repadr(0),rc_recen|rc_touten); 
//				rc=rbl;
//				rcw(repadr(1),rc_recen|rc_touten); 

				
		//start dll operation and tdi and tdo
				for(uint l=0;l<6;l++){
					rc=rca[l];
					rcw(rc_config, rc_drvresetb|rc_dllresetb );
				}		

				//tiem for dlls to lock
				cout<<"neuron control: "<<endl;
				for(int r=0;r<15;r++){
					cout<<ncr_ad(r)<<endl;
				}
				
				//start receiveing operation on tdi and tdo
				for(uint l=0;l<6;l++){
					rc=rca[l];
					rcw(rc_config, rc_drvresetb|rc_dllresetb |rc_start_tdi1|rc_start_tdi0 );
				}		

				//dump tdi data
				for(uint l=0;l<6;l++){
					rc=rca[l];
					cout<<rc_name[l]<<"\t tdi: "<<rcr_ad(rc_status)<<endl;
					for(int line=0;line<3;line++){ //use three rows for output
						for(int col=0;col<2;col++){//and two columns
							rc_td tin(rcr(rc_td0+line*2+col*6),rcr(rc_td0+line*2+col*6+1));
							cout<<dec<<"t: "<<tin.getTimeB()<<"\tnn: "<<hex<<tin.getNN()<<"\t";			
						}
						cout<<endl;
					}
				}				
			}break;
			case 'i': {
				//enable preout on analog output mux
				NeuronBuilderControl *nbc;
				nbc = &hc -> getNBC();
				nbc->setOutputMux(1,4);//(0,0 für vctrl, 1,1 für pre)
				nbc->setOutputEn(true,true);
			
				
				sc=st; //only top at the moment
				scw(sc_cnfgreg,(3<<sc_cfg_dllresetb_p)|(3<<sc_cfg_predel_p)|(5<<sc_cfg_endel_p) );

				//enable input for rows 0 and 2
				uint r=4;
				cout <<"Ssyndrive top row selected "<<r<<endl;
				vector<uint> weights(32,0xffffffff),address(32,0); //one row, all weights max, decoder 0
				synw(r,weights,false);
				synw(r,address, true);
				synw(r+2,weights,false);
				synw(r+2,address, true);
	//debug	
				bool nc=scr(sc_ctrlreg) & (1<<sc_newcmd_p);
				nc = !nc; //invert new command flag

				cout<<"read back weights"<<endl;
				scw(sc_ctrlreg, sc_cmd_read | (r<<sc_adr_p)|(nc<<sc_newcmd_p));
				cout << scr_ad(sc_ctrlreg);
				for(int i=0;i<32;i++,i%8?cout:cout<<endl){cout<<scr_ad(sc_synd+i);}
				cout<<endl;
				cout<<"read back decoder"<<endl;
				nc = !nc; 
				scw(sc_ctrlreg, sc_cmd_rdec | (r<<sc_adr_p)|(nc<<sc_newcmd_p));
				cout << scr_ad(sc_ctrlreg);
				for(int i=0;i<32;i++,i%8?cout:cout<<endl){cout << scr_ad(sc_synd+i);}	
				cout<<endl;
				// set up synapse driver, r=bot, r+2=top
				syndw(sc_enctrl+r,sc_senx|sc_cbot_en|sc_cbot_locin);
				syndw(sc_enctrl+r+2,sc_senx);
				syndw(sc_endrv+r,0);
				syndw(sc_endrv+r+2,0);
				syndw(sc_engmax+r,0xff);
				syndw(sc_engmax+r+2,0xff);
		
				// set up second synapse driver row below and activate topin
				r=r-4;
				syndw(sc_enctrl+r,sc_senx|sc_cbot_en|sc_cbot_topin);
				syndw(sc_enctrl+r+2,sc_senx);
				syndw(sc_endrv+r,0);
				syndw(sc_endrv+r+2,0);
				syndw(sc_engmax+r,0xff);
				syndw(sc_engmax+r+2,0xff);

//debug				 
				cout <<scr_ad(sc_enctrl+r);
				cout <<scr_ad(sc_enctrl+r+2);
				cout <<scr_ad(sc_endrv+r);
				cout <<scr_ad(sc_endrv+r+2);
				cout <<scr_ad(sc_engmax+r);
				cout <<scr_ad(sc_engmax+r+2)<<endl;
 
			} break;

			case 'w': {

				sc=st; //only top at the moment
				scw(sc_cnfgreg,(3<<sc_cfg_dllresetb_p)|(3<<sc_cfg_predel_p)|(5<<sc_cfg_endel_p) );

				uint r=4;
				uint weight;
				cout <<"Enter weight:";
				cin>>weight;
				weight = weight + 0x10 * weight;
				weight = weight + 0x100* weight;
				weight = weight + 0x10000* weight;
				cout<<hex<<weight<<endl;
				cout <<"Ssyndrive top row selected "<<r<<endl;
				vector<uint> weights(32,weight),address(32,0); //one row, all weights max, decoder 0
				synw(r,weights,false);
				synw(r,address, true);
				synw(r+2,weights,false);
				synw(r+2,address, true);
	//debug	
 
			
			} break;
			case 'e': {
				cout<<"Clearing synapse array and synapse drivers"<<endl;
				vector<uint> weights(32,0),address(32,0); //one row, all weights 0, decoder 0
				sc=st; //top
				for (uint r=0;r<224;r++){
					cout<<r<<" ";
					if(!(r%16))cout<<endl;
					synw(r,weights,false);
					synw(r,address, true);
					syndw(sc_enctrl+r,0);
					syndw(sc_endrv+r,0);
					syndw(sc_engmax+r,0);
				}
				sc=sb; //bottom
				for (uint r=0;r<224;r++){
					cout<<r<<" ";
					if(!(r%16))cout<<endl;
					synw(r,weights,false);
					synw(r,address, true);
					syndw(sc_enctrl+r,0);
					syndw(sc_endrv+r,0);
					syndw(sc_engmax+r,0);
				}
				} break;
				case 'h':{
					// hicann->hicann communication test
					// sending from hicann 0 to hicann 1 (jtag id)
					cout <<"Enter hicann jtag id?";
					uint h;
					cin >>h;
					uint random = 0x0000;//random mask for bg

					//clear repeaters
					for(uint l=0;l<6;l++){
						rc = rca[l];
						for(int r=0;r<64;r++)rcw(repadr(r),0); //clear repeater
						rcw(rc_config, rc_dllresetb|rc_drvresetb);
					}
				
					// --- config neuron control

					//initialize neuron control: bg as input: 0xff, merger tree level 0: all 0, level 1: 1,0, level 2: 1
					ncw(nc_enable,0);//only bg
					ncw(nc_select,0x50ff);
					//connected to spl1: bg0,l1_0,l2_0,bg3,l3_0,bg5,l1_3,bg7
					ncw(nc_slow,0x59a9);
					//enable all spl1 outputs
					ncw(nc_dncloopb,0);
					//select a period of 1 MHz for all bg
					for(int r=0;r<8;r++)
						ncw(nc_period+r,50);							
					//set random seed 
					ncw(nc_seed,200);
					//stop all bg
					ncw(nc_randomreset,random);//disable bg channels
					//start all bg
//					ncw(nc_randomreset,random|0xff);				dont start for tin-mode of edge repeater
					ncw(nc_randomreset,random|0xff);	
				
					// --- config repeaters
					rc_td tout;
					//initialize tdo registers for dll locking
					for(uint l=0;l<6;l++){
						rc=rca[l];
						for(int r=0;r<12;r+=2){
							tout.setNN(0);tout.setTime(50);
							rcw(rc_td0+r,tout.getLowByte());rcw(rc_td0+r+1,tout.getHighByte());
						}
					}
					if(h==1){
//*********** sending hicann ************					
						cout <<"Configuring sender"<<endl;
						rc=rl;
						rcw(repadr(0),rc_tinen|rc_touten|rc_dir ); //sends to h0,connected to v32 left
						lcl->write_cfg(0,2);
  					rc=rtl;
						rcw(repadr(16),rc_tinen|rc_recen|rc_touten ); //receives from v32, sends to second hicann 

		        //start dll operation and tdo
		        for(uint l=0;l<6;l++){
		          rc=rca[l];
		          rcw(rc_config, rc_drvresetb|rc_dllresetb |rc_start_tdo1|rc_start_tdo0 );
		        }

		        //time for dlls to lock
		        cout<<"neuron control: "<<endl;
		        for(int r=0;r<15;r++){
		          cout<<ncr_ad(r)<<endl;
		        }

						ncw(nc_randomreset,0);		//disable background,	it should not mix with tin-mode of edge repeater
						
		        //now change neuron numbers of sender after dll has locked
		        rc=rtl;
		        tout.setNN(0xa);tout.setTime(3);rcw(rc_td0+0,tout.getLowByte());rcw(rc_td0+0+1,tout.getHighByte());
		        tout.setNN(0xb);tout.setTime(3);rcw(rc_td0+2,tout.getLowByte());rcw(rc_td0+2+1,tout.getHighByte());
		        tout.setNN(0xc);tout.setTime(3);rcw(rc_td0+4,tout.getLowByte());rcw(rc_td0+4+1,tout.getHighByte());
		        tout.setNN(0);tout.setTime(5);rcw(rc_td0+6,tout.getLowByte());rcw(rc_td0+6+1,tout.getHighByte());
		        tout.setNN(0);tout.setTime(5);rcw(rc_td0+8,tout.getLowByte());rcw(rc_td0+8+1,tout.getHighByte());
		        tout.setNN(0);tout.setTime(5);rcw(rc_td0+10,tout.getLowByte());rcw(rc_td0+10+1,tout.getHighByte());

		        //start receiveing operation on tdi and tdo
		        for(uint l=0;l<6;l++){
		          rc=rca[l];
		          rcw(rc_config, rc_drvresetb|rc_dllresetb |rc_start_tdi1|rc_start_tdi0 |rc_start_tdo1|rc_start_tdo0 );
		        }

					}else{
//*********** receiving hicann ************					
						cout <<"Configuring receiver"<<endl;
						lcr->write_cfg(0,2);
						rc=rtr;
						rcw(repadr(16),rc_recen|rc_dir|rc_touten); //receives from first hicann, sends to v32 right 

						rc=rl;
						rcw(repadr(0),rc_recen|rc_touten ); //receives from v32, sends to ext

						//start receiveing operation on tdi and tdo
						for(uint l=0;l<6;l++){
							rc=rca[l];
							rcw(rc_config, rc_drvresetb|rc_dllresetb |rc_start_tdi1|rc_start_tdi0 );
						}		
					}
	
					//dump tdi data
					for(uint l=0;l<6;l++){
						rc=rca[l];
						cout<<rc_name[l]<<"\t tdi: "<<rcr_ad(rc_status)<<endl;
						for(int line=0;line<3;line++){ //use three rows for output
							for(int col=0;col<2;col++){//and two columns
								rc_td tin(rcr(rc_td0+line*2+col*6),rcr(rc_td0+line*2+col*6+1));
								cout<<dec<<"t: "<<tin.getTimeB()<<"\tnn: "<<hex<<tin.getNN()<<"\t";			
							}
							cout<<endl;
						}
					}				
			
			}break;
			case 'D':{
				//dump tdi data
				for(uint l=0;l<6;l++){
					rc=rca[l];
					rcw(rc_config, rc_drvresetb|rc_dllresetb|rc_start_tdi1|rc_start_tdi0 |rc_start_tdo1|rc_start_tdo0);
	  		}		
	
				for(uint l=0;l<6;l++){																									   
					rc=rca[l];																														   
					cout<<rc_name[l]<<"\t tdi: "<<rcr_ad(rc_status)<<endl;								   
					for(int line=0;line<3;line++){ //use three rows for output						   
						for(int col=0;col<2;col++){//and two columns												   
							rc_td tin(rcr(rc_td0+line*2+col*6),rcr(rc_td0+line*2+col*6+1)); 	   
							cout<<dec<<"t: "<<tin.getTimeB()<<"\tnn: "<<hex<<tin.getNN()<<"\t";  
						} 																																	   
						cout<<endl; 																												   
					} 																																		   
				} 																																			   
				for(uint l=0;l<6;l++){
					rc=rca[l];
					rcw(rc_config,  rc_drvresetb|rc_dllresetb|rc_start_tdo1|rc_start_tdo0);
	  		}		

			}break;

      case 'Z':{
      	dbg(0) << "Try Init() ..." ;

      	if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
      		dbg(0) << "ERROR: Init failed, abort" << endl;
      		return 0;
				}
	
      	dbg(0) << "Init() ok" << endl;
				
      	dbg(0) << "Reset software part of ARQ.";
      	ll->arq.Reset();
				
      } break;
			
			case'x':cont=false;
			}		
		}while(cont);
		return 1;
	}
};

class LteeTmSMRepeater : public Listee<Testmode>{
public:
	LteeTmSMRepeater() : Listee<Testmode>(string("tmsm_repeater"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmSMRepeater();};
};

LteeTmSMRepeater ListeeTmSMRepeater;

