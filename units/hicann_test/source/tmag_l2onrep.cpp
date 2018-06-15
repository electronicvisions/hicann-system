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

#include "dnc_control.h"
#include "fpga_control.h"

// required for checking only
#include "s2c_jtagphys_2fpga.h"

//only if needed
#include "spl1_control.h"     //spl1 control class

#include "neuronbuilder_control.h"
#include "l1switch_control.h" //layer 1 switch control class
#include "linklayer.h"
#include "testmode.h" 
#include "ramtest.h"
#include <queue>

//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

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


class TmAGL2OnRep : public Testmode /*, public RamTestIntfUint*/ {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmjs_repeater"; return ss.str();}
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

	DNCControl  *dc;
	FPGAControl *fc;
  SPL1Control *spc;
  LinkLayer<ci_payload,ARQPacketStage2> *ll;

	HicannCtrl *hc; 
	CtrlModule *rc,*nc,*sc; //repeater control, neuron control, synapse control for read/write functions
	CtrlModule *rl,*rr,*rtl,*rbl,*rtr,*rbr;	//all repeaters
	CtrlModule *rca[6];
	CtrlModule *st,*sb; //synapse control top/bottom
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
    uint hicannr = 0;
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
    hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
		// use DNC
		dc = (DNCControl*) chip[FPGA_COUNT];
		
		// use FPGA
		fc = (FPGAControl*) chip[0];
		
    // get pointers to control modules
    spc = &hc->getSPL1Control();

/*	 	dbg(0) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	dbg(0) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	dbg(0) << "Init() ok" << endl;
*/
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

		// reset values for PLL control register:
		uint ms = 0x01;
		uint ns = 0x05;
		uint pdn = 0x1;
		uint frg = 0x1;
		uint tst = 0x0;
		
		// power boad i2c address
		uint pwraddr = 0x3e;

		// ----------------------------------------------------
		// repeater testmode
		// ----------------------------------------------------

		char c;
		bool cont=true;
		do{
			cout << "PLL control reg values: ns=0x" << hex << setw(2) << ns << " ms=0x" << ms << " pdn=" << setw(1) << pdn << " frg=" << frg << " tst=" << tst << endl << endl;

			cout << "Select test option:" << endl;
			cout << "1: send HICANN reset pulse" << endl;
			cout << "2: JTAG reset" << endl;
			cout << "3: set pll frequency divider" << endl;
			cout << "4: enable power board" << endl;
			cout << "5: disable power board" << endl;
			cout << "6: set and verify I2C prescaler reg" << endl;
			cout << "c: perform SPL1 loopback test via FPGA JTAG interface using DNCControl functions (like b)" << endl;
			cout << "l: quick l1 and communication check" << endl;
			cout << "L: full l1 switch test" << endl;
			cout << "s: activate sending repeaters and background" << endl;
			cout << "h: activate sending repeaters for hicann->hicann test" << endl;
			cout << "U: disable bg gen/enable dnc input on channel 0" << endl;
			cout << "V: config DNC data path and enable fpga bg gen" << endl;
			cout << "Y: init FPGA-DNC communication" << endl;
			cout << "Z: ARQ reset/init" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){

			case '1':{
				jtag->FPGA_set_fpga_ctrl(1);
			} break;
			
			case '2':{
				cout << "reset test logic" << endl;
				jtag->reset_jtag();
			} break;
			
			case '3':{
				cout << "value=ns/ms:" << endl << "ns? "; cin >> ns;
				cout << "ms? "; cin >> ms;

				jtag->HICANN_set_pll_far_ctrl(ms, ns, pdn, frg, tst);
			} break;

			case '4':{
				jtag->FPGA_i2c_byte_write(pwraddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(0xff,    0, 1, 0);
			} break;

			case '5':{
				jtag->FPGA_i2c_byte_write(pwraddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(0xfe,    0, 1, 0);
			} break;

			case '6':{
				uint presc;
				
				cout << "HEX value for presc (16 bit)? "; cin >> hex >> presc;
				
				jtag->FPGA_i2c_setctrl(jtag->ADDR_PRESC_M, (presc>>8));
				jtag->FPGA_i2c_setctrl(jtag->ADDR_PRESC_L, presc);
				
				unsigned char rprescl=0, rprescm=0;
				jtag->FPGA_i2c_getctrl(jtag->ADDR_PRESC_L, rprescl);
				jtag->FPGA_i2c_getctrl(jtag->ADDR_PRESC_M, rprescm);
				
				cout << "read back : 0x" << setw(2) << hex << ((((uint)rprescm)<<8)|(uint)rprescl) << endl;
			} break;

      case 'c':{
				uint systime = 0xffffffff;
				uint neuron  = 0x83;
				uint64_t jtag_recpulse = 0;

				ncw(0x3, 0xffaa);
				
				spc->write_cfg(0x055ff);
				
				// set DNC to ignore time stamps and directly forward l2 pulses
				dc->setTimeStampCtrl(0x0);

				// set transmission directions in DNC (for HICANN 0 only)
				dc->setDirection(0x55);
				
				neuron = 0x23;
				uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
				
				fc->sendSingleL2Pulse(0,jtag_sendpulse);
				fc->getFpgaRxData(jtag_recpulse);

				dbg(0) << "FPGA RX: 0x" << hex << jtag_recpulse << endl;

				
				if ((uint)((jtag_recpulse>>15)&0x1ff) != (neuron+1*64)){
				  cout << "TUD_TESTBENCH:ERROR:TESTFAIL: Single pulse packet not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
				} else {
				  cout << "TUD_TESTBENCH:NOTE:TESTPASS: Transmission of pulse packet via JTAG->HICANN(L1)->JTAG successful." << endl;
				}

      } break;
			
			case 'l':{
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
						//result = false;
						//break;
					}
				}
				if(!result)return false;
			}break;

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

			case 'U':{
				ncw(nc_dncloopb,0xff00); // enable spl1 input on channel 0 -> bg should be off
		    cout<<ncr_ad(nc_dncloopb)<<endl;
			}break;

			case 'V':{
				uint synaddr;
				cout << "target synapse address on HICANN 0, channel 0 (hex)? 0x"; cin >> hex >> synaddr;

				spc->write_cfg(0x0ffff);
				
				// set DNC to ignore time stamps and directly forward l2 pulses
				dc->setTimeStampCtrl(0x0);
				// set transmission directions in DNC (for HICANN 0 only)
				dc->setDirection(0xff);
				
				// start background generator on FPGA
				uint bgconf=0;
				bgconf |=     0x1 << 0;  // enable generation of bg pulses
				bgconf |=   ((0x7<<6) | (synaddr&0x3F)) << 1;  // 12 bit neuron address: 3bit HICANN, 3bit SPL1 channel, 6bit L1 address
				bgconf |= 0x100 << 13; // 17 bit time delay between pulses 
				//jtag->FPGA_set_cont_pulse_ctrl(bgconf);
				jtag->FPGA_set_cont_pulse_ctrl(1, 0x80, 0, 0x100, 0x55555, (synaddr&0x3F), (synaddr&0x3F), comm->jtag2dnc(hc->addr()));

			}break;

      case 'Y':{
        if(dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj()))
      		if (dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())->fpga_dnc_init() != 1) {
      			dbg(0) << "ERROR: init_fpga_dnc failed, abort" << endl;
      			return 0;
					}
      } break;
			
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
			if(kbhit()>0)cont=false;
		}while(cont);
		return 1;
	}
};

class LteeTmAGL2OnRep : public Listee<Testmode>{
public:
	LteeTmAGL2OnRep() : Listee<Testmode>(string("tmag_l2onrep"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmAGL2OnRep();};
};

LteeTmAGL2OnRep ListeeTmAGL2OnRep;

