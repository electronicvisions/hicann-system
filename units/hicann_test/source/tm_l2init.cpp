// Company         :   kip
// Author          :   Andreas GrÃ¼bl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//------------------------------------------------------------------------


//example testmode for spl1 test via JTAG

#include "common.h"

#include "hicann_ctrl.h"
#include "dnc_control.h"
#include "fpga_control.h"
#include "testmode.h" 

#include "linklayer.h"

// required for checking only
#include "s2c_jtagphys_2fpga.h"

//only if needed
#include "neuron_control.h"   //neuron / "analog" SPL1 control class
#include "spl1_control.h"     //spl1 control class

//#define USENR         0
#define fpga_master   1
#define DATA_TX_NR    1000
#define RUNS          16

using namespace std;
using namespace facets;

class TmL2init : public Testmode{

public:

    uint hicann_nr;
		
		HicannCtrl  *hc;
    DNCControl  *dc;
    FPGAControl *fc;
    NeuronControl *nc;  
    SPL1Control *spc;
    LinkLayer<ci_payload,ARQPacketStage2> *ll;

    CtrlModule *rc; //repeater control for read/write functions
    CtrlModule *rca[6];
    CtrlModule *rl,*rr,*rtl,*rbl,*rtr,*rbr; //all repeaters
    string rc_name[6];

    //generate repeater address by permuting address bits (HICANN error!!!)
    static uint repadr(uint adr) {
        uint ra=0;
        for(int i=0;i<7;i++)ra|= (adr& (1<<i))?(1<<(6-i)):0;
        return ra;
    }

    // repeater control 
    enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater location
    enum rc_regs {rc_config=0x80,rc_status=0x80,rc_td0=0x81,rc_td1=0x87,rc_scfg=0xa0};
    enum rc_srambits {rc_touten=0x80,rc_tinen=0x40,rc_recen=0x20,rc_dir=0x10,rc_ren1=8,rc_ren0=4,rc_len1=2,rc_len0=1};
    enum rc_config {rc_fextcap1=0x80,rc_fextcap0=0x40,rc_drvresetb=0x20,rc_dllresetb=0x10,rc_start_tdi1=8,rc_start_tdi0=4,rc_start_tdo1=2,rc_start_tdo0=1};

    void rcw(uint adr, uint data){
        rc->write_cmd(adr, data,0);
    }       

    bool test() 
    {
    if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
        cout << "tm_l2init: ERROR: object 'chip' in testmode not set, abort" << endl;
        return 0;
    }
    if (!jtag) {
        cout << "tm_l2init: ERROR: object 'jtag' in testmode not set, abort" << endl;
        return 0;
    }

//      if(!dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())){
//          cout << ":tm_l2init: ERROR: wrong S2Comm class used! -> Must use S2C_JtagPhys2Fpga!" << endl;
//          return 0;
//      }

    uint hcnr = 0;
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
    hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hcnr];
        
    // use DNC
    dc = (DNCControl*) chip[FPGA_COUNT];
        
    // use FPGA
    fc = (FPGAControl*) chip[0];
        
    // get pointers to control modules
    spc = &hc->getSPL1Control();
    nc  = &hc->getNC();


		uint hicann_jtag_nr = hc->addr();
		hicann_nr = 7-hicann_jtag_nr;
		jtag->set_hicann_pos(hicann_jtag_nr);

        // intialize repeater control modules

    rl  = new CtrlModule(hc, 0, OcpRep_l.startaddr, 0xff); //repeater blocks use 7 address bits + SRAM control bit
    rr  = new CtrlModule(hc, 0, OcpRep_r.startaddr, 0xff);
    rtl = new CtrlModule(hc, 0, OcpRep_tl.startaddr, 0xff);
    rbl = new CtrlModule(hc, 0, OcpRep_bl.startaddr, 0xff);
    rtr = new CtrlModule(hc, 0, OcpRep_tr.startaddr, 0xff);
    rbr = new CtrlModule(hc, 0, OcpRep_br.startaddr, 0xff);

    rca[rc_l]=rl;   rc_name[rc_l]="HorizLeft";
    rca[rc_r]=rr;   rc_name[rc_r]="HorizRight";
    rca[rc_tl]=rtl; rc_name[rc_tl]="TopLeft";
    rca[rc_bl]=rbl; rc_name[rc_bl]="BottomLeft";
    rca[rc_tr]=rtr; rc_name[rc_tr]="TopRight";
    rca[rc_br]=rbr; rc_name[rc_br]="BottomRight";

    // reset values for PLL control register:
    uint ms = 0x01;
    uint ns = 0x05;
    uint pdn = 0x1;
    uint frg = 0x1;
    uint tst = 0x0;

    uint64_t jtagid;

//    jtag->reset_jtag();
        
//    jtag->read_id(jtagid,jtag->pos_hicann);
//    cout << "HICANN ID: 0x" << hex << jtagid << endl;

    jtag->read_id(jtagid,1);
    cout << "DNC ID: 0x" << hex << jtagid << endl;

    jtag->read_id(jtagid,2);
    cout << "FPGA ID: 0x" << hex << jtagid << endl;

    char c;
    bool cont=true;
    do{

        cout << "Select test option:" << endl;
        cout << "1: reset jtag " << endl;
        cout << "2: perform SPL1 loopback test via HICANN JTAG interface" << endl;
        cout << "3: reset arq" << endl;
        cout << "4: read value of system time counter" << endl;
        cout << "5: Init FPGA<->DNC communication" << endl;
        cout << "6: Shutdown DNC communication (DNC and HICANN's DNC interface)" << endl;
        cout << "7: read back systime counter with reset" << endl;
        cout << "8: enable/disable DNC interface (1/0)" << endl;
        cout << "9: read DNC interface communication status" << endl;
        cout << "a: measure DNC - HICANN connection" << endl;
        cout << "0: measure DNC - HICANN connection with s2comm init routine" << endl;
        cout << "A: measure DNC - HICANN connection using fpga_dnc_init from comm class" << endl;
        cout << "b: perform SPL1 loopback test via FPGA JTAG interface" << endl;
        cout << "c: perform SPL1 loopback test via FPGA JTAG interface using DNCControl functions (like b)" << endl;
        cout << "d: can't really remember..." << endl;
        cout << "e: setup fpga background gen to spl1 channel 0" << endl;
        cout << "f: set pll frequency divider" << endl;
        cout << "g: send HICANN reset pulse" << endl;
        cout << "h: JTAG reset" << endl;
        cout << "i: read FPGA and HICANN JTAG ID" << endl;
        cout << "k: enable HICANN lvds pads" << endl;
        cout << "m: Init all Gbit connections to best data eye value (measure DNC-HICANN)" << endl;
        cout << "w: like c with loopbaack on channel 8->7 only" << endl;
        cout << "Z: ARQ reset/init" << endl;
        cout << "x: exit" << endl;
        
        cin >> c;
        
        switch(c){
        
        case '1':{
            jtag->reset_jtag();
        } break;
        
        case '2':{
            uint64_t systime = 0xffffffff;
            uint neuron;
            uint64_t jtag_recpulse = 0;
            
            jtag->HICANN_set_test(0);
            jtag->HICANN_set_layer1_mode(0x55);
            
            nc->nc_reset();     
            nc->dnc_enable_set(1,0,1,0,1,0,1,0);
            nc->loopback_on(0,1);
            nc->loopback_on(2,3);
            nc->loopback_on(4,5);
            nc->loopback_on(6,7);
            
            spc->write_cfg(0x55ff);
            
            jtag->HICANN_set_test(0x2);
            
            jtag->HICANN_read_systime(systime);
            
            neuron = 0x23;
            uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
            cout << "JTAG pulse: 0x" << hex << jtag_sendpulse << endl;
            
            jtag->HICANN_set_double_pulse(0);
            jtag->HICANN_start_pulse_packet(jtag_sendpulse);
            
            // wait ???
            
            //              uint64 jtag_recpulse = 0;
            jtag->HICANN_get_rx_data(jtag_recpulse);
            printf("HICANN L1 RX: %.016llX\n",static_cast<long long unsigned int>(jtag_recpulse));
            
            if ((uint)((jtag_recpulse>>15)&0x1ff) != (neuron+1*64)){
                cout << "TUD_TESTBENCH:ERROR:TESTFAIL: Single pulse packet not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
            } else {
                cout << "TUD_TESTBENCH:NOTE:TESTPASS: Transmission of pulse packet via JTAG->HICANN(L1)->JTAG successful." << endl;
            }
            
        } break;
            
      case '3':{
        dbg(0) << "Reset software part of ARQ.";
        ll->arq.Reset();
                
      } break;
            
      case '4': {
        uint64_t syst=0xf0f0;
        if(!jtag->HICANN_read_systime(syst))
            cout << "read systime failed!" << endl;
                else
                    cout << "--read systime: " << syst << endl;
      } break;

      case '5': {
          if(dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj()))
						dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())->fpga_dnc_init(static_cast<unsigned char>(hicann_nr));
					else
						cout << "You are using the wrong comm model!!!" << endl;
      } break;

      case '6': {
				if(dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())){
					dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())->dnc_shutdown();
					dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())->hicann_shutdown();
				} else
					cout << "You are using the wrong comm model!!!" << endl;

      } break;

      case '7': {
                uint tst;
                cout << "activate and deactivate reset, then press return!" << endl;
                cin >> tst;
        
                cout << "reset test logic" << endl;
                jtag->reset_jtag();

                cout << "sending TMS_SYS_START pulse..." << endl;
                jtag->jtag_write(0, 1);
                jtag->jtag_write(0, 0);

                cout << "reset test logic" << endl;
                jtag->reset_jtag();

                uint64_t syst=0xf0f0;
                if(!jtag->HICANN_read_systime(syst))
                  cout << "read systime failed!" << endl;
                else
                  cout << "--read systime: " << syst << endl;
      } break;

      case '8': {
                uint en=0;
                
                cout << "enable or disable (1/0)? "; cin>>en;
                jtag->HICANN_set_reset(en);

      } break;

      case '9': {
                uint64_t en=0;
                
                jtag->HICANN_read_status(en);
                cout << "status: 0x" << hex << en << endl;
      } break;

      case 'a': {
                dnc_hicann_measure(*jtag);
      } break;
      case '0': {
                dnc_hicann_measure_s2c(*jtag);
      } break;
      case 'A': {
        if(dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj()))
        	for(int dnc_del=0;dnc_del<32;dnc_del++)
						for(int hicann_del=12;hicann_del<13;hicann_del++)
							dynamic_cast<S2C_JtagPhys2Fpga*>(hc->GetCommObj())->fpga_dnc_init(hc->addr(), true, dnc_del, hicann_del);
				else
					cout << "You are using the wrong comm model!!!" << endl;
      } break;
      case 'm': {
                fpga_dnc_hicann_measure_init(*jtag);
      } break;

      case 'b':{
                uint64_t systime = 0xffffffff;
                uint neuron  = 0x83;
                uint64_t jtag_recpulse = 0;

                jtag->HICANN_set_layer1_mode(0x55);
                printf("Enter 1\n");getchar();
                nc->write_data(0x3, 0x55aa);
                printf("Enter 2\n");getchar();
                
                spc->write_cfg(0x55ff);
                printf("Enter 3\n");getchar();
                
    
                jtag->HICANN_read_systime(systime);
                printf("Enter 4\n");getchar();

                jtag->DNC_set_timestamp_ctrl(0x0);
                jtag->DNC_set_l1direction(0x55);
                printf("Enter 5\n");getchar();
                
                neuron = 0x23;
                uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
                
                jtag->FPGA_start_pulse_event(0,jtag_sendpulse);
                printf("Enter 6\n");getchar();

                jtag->DNC_set_channel(8 + hicann_nr);
                jtag->DNC_get_rx_data(jtag_recpulse);
                printf("Enter 7\n");getchar();

                dbg(0) << "DNC TX: 0x" << hex << jtag_recpulse << endl;


                jtag->DNC_set_channel(hicann_nr);
                jtag->DNC_get_rx_data(jtag_recpulse);

                dbg(0) << "DNC RX: 0x" << hex << jtag_recpulse << endl;

                jtag->FPGA_get_rx_data(jtag_recpulse);

                dbg(0) << "FPGA RX: 0x" << hex << jtag_recpulse << endl;

                
                if ((uint)((jtag_recpulse>>15)&0x1ff) != (neuron+1*64)){
                  cout << "TUD_TESTBENCH:ERROR:TESTFAIL: Single pulse packet not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
                } else {
                  cout << "TUD_TESTBENCH:NOTE:TESTPASS: Transmission of pulse packet via JTAG->HICANN(L1)->JTAG successful." << endl;
                }

      } break;
            
      case 'c':{
                uint64_t systime = 0xffffffff;
                uint neuron  = 0x83;
                uint64_t jtag_recpulse = 0;

                nc->write_data(0x3, 0xffaa);
                
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
                dbg(0) << "SPL1 addr: 0x" << hex << ((jtag_recpulse>>15)&0x1ff) << endl;

                
                if ((uint)((jtag_recpulse>>15)&0x1ff) != (neuron+1*64)){
                  cout << "TUD_TESTBENCH:ERROR:TESTFAIL: Single pulse packet not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
                } else {
                  cout << "TUD_TESTBENCH:NOTE:TESTPASS: Transmission of pulse packet via JTAG->HICANN(L1)->JTAG successful." << endl;
                }

      } break;
            
      case 'w':{
                uint64_t systime = 0xffffffff;
                uint neuron  = 0x83;
                uint64_t jtag_recpulse = 0;

                nc->write_data(0x3, 0xff40);
                
                spc->write_cfg(0x080ff);
                
                // set DNC to ignore time stamps and directly forward l2 pulses
                dc->setTimeStampCtrl(0x0);

                // set transmission directions in DNC (for HICANN 0 only)
                dc->setDirection(0x80);
                
                neuron = 0x23;
                uint64 jtag_sendpulse = (neuron<<15) | (systime+100)&0x7fff;
                
                fc->sendSingleL2Pulse(0,jtag_sendpulse);
                fc->getFpgaRxData(jtag_recpulse);

                dbg(0) << "FPGA RX: 0x" << hex << jtag_recpulse << endl;
                dbg(0) << "SPL1 addr: 0x" << hex << ((jtag_recpulse>>15)&0x1ff) << endl;

                
                if ((uint)((jtag_recpulse>>15)&0x1ff) != (neuron+6*64)){
                  cout << "TUD_TESTBENCH:ERROR:TESTFAIL: Single pulse packet not transmitted via JTAG->HICANN(L1)->JTAG." << endl;
                } else {
                  cout << "TUD_TESTBENCH:NOTE:TESTPASS: Transmission of pulse packet via JTAG->HICANN(L1)->JTAG successful." << endl;
                }

      } break;
            
      case 'd':{

                uint64_t systime = 0xffffffff;
                uint64_t jtag_recpulse = 0;
        
                //clear repeaters
                for(uint l=0;l<6;l++){
                    rc = rca[l];
                    for(int r=0;r<64;r++)rcw(repadr(r),0); //clear repeater
                    rcw(rc_config, rc_dllresetb|rc_drvresetb);
                }

                rc=rl;
                rcw(repadr(0),rc_tinen|rc_touten|rc_dir ); //sends to h0,connected to v0

                // setup HICANN
                nc->write_data(0x3, 0x55aa);
                spc->write_cfg(0x055ff);
                
                // set DNC to ignore time stamps and directly forward l2 pulses
                dc->setTimeStampCtrl(0x0);
                // set transmission directions in DNC (for HICANN 0 only)
                dc->setDirection(0x55);
                
                // start background generator on FPGA
                uint bgconf=0;
                bgconf |=     0x1 << 0;  // enable generation of bg pulses
                bgconf |=   0x000 << 1;  // 12 bit neuron address: 3bit HICANN, 3bit SPL1 channel, 6bit L1 address
                bgconf |= 0x10000 << 13; // 17 bit time delay between pulses 

cout << "bgconf: 0x" << hex << bgconf << endl;
                jtag->FPGA_set_cont_pulse_ctrl(1, 0x01, 0, 0x2000, 0x55555, 0, 0, comm->jtag2dnc(hc->addr()));
                //jtag->FPGA_set_cont_pulse_ctrl(bgconf);
                
                // check for changing systime entries
                for(uint i=0;i<100;i++){
                    jtag->FPGA_get_rx_data(jtag_recpulse);
                    dbg(0) << "FPGA RX: 0x" << hex << jtag_recpulse << endl;
                }
                
            } break;

      case 'e':{

                uint64_t systime = 0xffffffff;
                uint64_t jtag_recpulse = 0;

                uint synaddr;
                cout << "target synapse address on HICANN 0, channel 0 (hex)? 0x"; cin >> hex >> synaddr;
        
                // setup HICANN
                nc->write_data(0x3, 0x0100); // only enable dnc input on channel 0, no loopback
                spc->write_cfg(0x00101);     // only enable channel 0 in dnc interface's spl1 control
                
                // set DNC to ignore time stamps and directly forward l2 pulses
                dc->setTimeStampCtrl(0x0);
                // set transmission directions in DNC (for HICANN 0 only)
                dc->setDirection(0x01);
                
                // start background generator on FPGA
                uint bgconf=0;
                bgconf |=            0x1 << 0;  // enable generation of bg pulses
                bgconf |= (synaddr&0x3f) << 1;  // 12 bit neuron address: 3bit HICANN, 3bit SPL1 channel, 6bit L1 address
                bgconf |=        0x10000 << 13; // 17 bit time delay between pulses 
                jtag->FPGA_set_cont_pulse_ctrl(1, 0x01, 0, 0x2000, 0x55555, (synaddr&0x3f), (synaddr&0x3f), comm->jtag2dnc(hc->addr()));
                //jtag->FPGA_set_cont_pulse_ctrl(bgconf);
                
                // check for changing systime entries
                for(uint i=0;i<100;i++){
                    jtag->FPGA_get_rx_data(jtag_recpulse);
                    dbg(0) << "FPGA RX: 0x" << hex << jtag_recpulse << endl;
                }
                
            } break;

      case 'k':{

                cout << "Enable LVDS pads on HICANN for signal measurement";
        
								jtag->HICANN_set_reset(1);
								jtag->HICANN_set_GHz_pll_ctrl(0x7);
								jtag->HICANN_lvds_pads_en(0);
								jtag->HICANN_set_link_ctrl(0x020);

            } break;

			case 'f':{
								cout << "value=ns/ms:" << endl << "ns? "; cin >> ns;
								cout << "ms? "; cin >> ms;

								jtag->HICANN_set_pll_far_ctrl(ms, ns, pdn, frg, tst);
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
            
            case 'g':{
                jtag->FPGA_set_fpga_ctrl(1);
            } break;
            
            case 'h':{
                cout << "reset test logic" << endl;
                jtag->reset_jtag();
            } break;
            
            case 'i':{
                uint64_t id=0;
                
                jtag->read_id(id, jtag->pos_fpga);
                cout << "FPGA ID: 0x" << hex << id << endl;

                jtag->read_id(id, jtag->pos_dnc);
                cout << "DNC ID: 0x" << hex << id << endl;

                jtag->read_id(id, jtag->pos_hicann);
                cout << "HICANN ID: 0x" << hex << id << endl;
                
            } break;
            
      case 'x': cont = false; 
      }

    } while(cont);

    return true;
  }

  void dnc_shutdown(myjtag_full& jtag) {
    //printf("\nDeactivating all DNC components\n");
    jtag.DNC_set_GHz_pll_ctrl(0xe);
    jtag.DNC_stop_link(0x1ff);
    jtag.DNC_set_lowspeed_ctrl(0x00);
    jtag.DNC_set_loopback(0x00);
    jtag.DNC_lvds_pads_en(0x1ff);
    jtag.DNC_set_init_ctrl(0x0aaaa);
    jtag.DNC_set_speed_detect_ctrl(0x0);
    jtag.DNC_set_GHz_pll_ctrl(0x0);
  }

  void hicann_shutdown(myjtag_full& jtag) {
    jtag.HICANN_set_reset(1);
    jtag.HICANN_set_GHz_pll_ctrl(0x7);
    jtag.HICANN_set_link_ctrl(0x141);
    jtag.HICANN_stop_link();
    jtag.HICANN_set_test(0); // disable memory test mode
    jtag.HICANN_lvds_pads_en(0x1);
    jtag.HICANN_set_reset(0);
    jtag.HICANN_set_bias(0x32);
    jtag.HICANN_set_GHz_pll_ctrl(0x0);
  }

  int dnc_hicann_measure(myjtag_full& jtag) {

    time_t now;
    now = time(NULL);
        uint hicann_addr = 0x4;
        
    srand((unsigned int)now);

    uint64_t jtagid;
    uint64_t rdata64;
    uint64_t wdata64;
    unsigned char state1;
    uint64_t state2;
    unsigned int error1;
    unsigned int error2;
    unsigned int init_error;
    unsigned int data_error;
    std::vector<bool> vec_in(144,false);
    std::vector<bool> vec_out(144,false);
    uint64_t delay_in;
    uint64_t delay_out;
        

    jtag.read_id(jtagid,jtag.pos_dnc);
    printf("id = %8x\t", static_cast<uint32_t>(jtagid));
    if(jtagid == 0x1474346f) {
      printf("JTAG ID DNC is OK\n");
    } else {
      printf("JTAG ID DNC ERR0R\n");
    }
    jtag.read_id(jtagid,0);
    printf("id = %8x\t", static_cast<uint32_t>(jtagid));
    if(jtagid == 0x14849434) {
      printf("JTAG ID HICANN is OK\n");
    } else {
      printf("JTAG ID HICANN ERR0R\n");
    }
                
    init_error = 0;
    data_error = 0;

    //HICANN1//i=11, j=12;
    //HICANN2: i=10, j=9
//    for (int i=0; i<RUNS; ++i) //DNC
//      for (int j=0; j<RUNS ; ++j) {  //HICANN
    for (int i=0; i<63; ++i) //DNC
      for (int j=8; j<9 ; ++j) {  //Wafer pass/fail test


    //printf("\nDeactivating all components\n");
    jtag.DNC_set_GHz_pll_ctrl(0xe);
    jtag.DNC_stop_link(0x1ff);
    jtag.DNC_set_lowspeed_ctrl(0xff);
    jtag.DNC_set_loopback(0x00);
    jtag.DNC_lvds_pads_en(0x1ff);
    jtag.DNC_set_speed_detect_ctrl(0x0);
    jtag.DNC_set_GHz_pll_ctrl(0x0);

    jtag.HICANN_set_reset(1);
    jtag.HICANN_set_GHz_pll_ctrl(0x7);
    jtag.HICANN_set_link_ctrl(0x141);
    jtag.HICANN_stop_link();
    jtag.HICANN_set_test(0); // disable memory test mode
    jtag.HICANN_lvds_pads_en(0x1);
    jtag.HICANN_set_reset(0);
    jtag.HICANN_set_bias(0x32);
    jtag.HICANN_set_GHz_pll_ctrl(0x0);

    //printf("\nStart DNC interface\n");
    jtag.DNC_set_GHz_pll_ctrl(0xe);
    jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections
    jtag.DNC_set_lowspeed_ctrl(0xff);
    jtag.DNC_lvds_pads_en(0xff & (~(1<<(hicann_nr))));
    jtag.DNC_set_init_ctrl(0x0aaaa);

    jtag.HICANN_set_reset(1);
    jtag.HICANN_set_GHz_pll_ctrl(0x7);
    jtag.HICANN_lvds_pads_en(0);
    jtag.HICANN_set_link_ctrl(0x020);
                
    jtag.DNC_test_mode_en();

    for(int m=0   ; m<144;++m) {
      vec_in[m]  = 1;
      vec_out[m] = 1;
    }
    for(int m=6; m>0;  --m)
      vec_in[(16*6+(8-hicann_nr)*6)-m] = ((~(i & 0x3f)) >> (6-m)) & 0x1;
            
    /*          printf("\nDelay Vector DNC1 : 0x");
                jtag1.printBoolVect(vec_in);
                printf("\n");
    */
    jtag.DNC_set_data_delay(vec_in,vec_out);

    delay_in = ~(j & 0x3f);
    jtag.HICANN_set_data_delay(delay_in,delay_out);
    jtag.HICANN_set_test(1);

    //printf("HICANN init with data delay %.02X  %.02X\n",delay_out,0x3f &delay_in);

    //Check Status
    //      for (int i=0;i<20000;++i) jtag.read_channel_sts_2ch(8,state1);
    state1 = 0;
    state2 = 0;
    jtagid = 0;
        
    printf("Delay Setting DNC: %.2X \t HICANN: %.2X \t",63-i,63-j);
        
    while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 1) {
      jtag.HICANN_stop_link();
      jtag.DNC_stop_link(0x0ff);
      jtag.HICANN_start_link();
      jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
                        
      usleep(900000);
      jtag.DNC_read_channel_sts(hicann_nr,state1);
      //snprintf(buf, sizeof(buf), "DNC Status = 0x%02hx\n" ,state1 );
      //jtag.logging(buf);
      jtag.HICANN_read_status(state2);
      //snprintf(buf, sizeof(buf), "HICANN Status = 0x%02hx\n" ,state2 );
      //jtag.logging(buf);
      ++jtagid;
    }
        
    error1 = 0;
    error2 = 0;
        
    if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))
      {
        
        //printf("Sucessfull init  \n");
        //printf("Test DNC1 -> DNC2  \n");
        jtag.HICANN_set_test(1);
        jtag.DNC_test_mode_en();
                
        for (int num_data=0;num_data<DATA_TX_NR;++num_data) {
          wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
          jtag.DNC_start_config(hicann_nr,wdata64);
          //usleep(10);
          jtag.HICANN_get_rx_data(rdata64);
                                
          if (rdata64 != wdata64) {++error1;
            //printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
          }
        }
                
        //printf("Test DNC2 -> DNC1  \n");
                
        for (int num_data=0;num_data<DATA_TX_NR;++num_data) {
          wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
          jtag.HICANN_set_tx_data(wdata64);
          jtag.HICANN_start_cfg_pkg();
          //usleep(10);
          jtag.DNC_set_channel(hicann_nr);
          jtag.DNC_get_rx_data(rdata64);
                                
          if (rdata64 != wdata64) {++error2;
            //printf("Wrote: 0x%016llX Read: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
          }
        }
                
        jtag.DNC_test_mode_dis();
        jtag.HICANN_set_test(0);
                            
                
        printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);
        if (error1 || error2) ++data_error;
                
        
      } else {
        
      ++init_error;
      printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
    
    }
        

    for(int k=0;k<144;++k) vec_in[k] = 1;
    jtag.DNC_set_data_delay(vec_in,vec_out);
    /*printf("\nDelay Vector DNC1 : 0x");
      jtag.printBoolVect(vec_out);
      printf("\n");
      jtag.printDelayVect(vec_out);
      printf("\n");
    */
                    

    jtag.HICANN_set_data_delay(delay_in,delay_out);
    //printf("HICANN init with data delay %.02X\n",delay_out);

    jtag.DNC_test_mode_dis();
                    
    //Stop all components in dnc
    dnc_shutdown(jtag);
    hicann_shutdown(jtag);
        
      }
    
    printf("Successfull inits : %i  Successfull Data Transmissions : %i\n",RUNS-init_error,RUNS-init_error-data_error);

        return data_error;
  }

	int dnc_hicann_measure_s2c(myjtag_full& jtag) {
    std::vector<bool> vec_in(144,false);
    std::vector<bool> vec_out(144,false);
    uint64_t delay_in;
    uint64_t delay_out;
    uint64_t jtagid;
    unsigned char state1;
    uint64_t state2;
  	uint error1 = 0;
  	uint error2 = 0;
  
		// loop delay values
    for (int i=24; i<25; ++i) //DNC
      for (int j=0; j<63 ; ++j) {  //HICANN
		
		  	jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections
		  	jtag.DNC_set_lowspeed_ctrl(0xff);
		  	jtag.DNC_lvds_pads_en(0xff & (~(1<<(hicann_nr))));
		  	jtag.DNC_set_init_ctrl(0x0aaaa);
  
		  	jtag.HICANN_set_reset(1);
		  	jtag.HICANN_set_GHz_pll_ctrl(0x7);
		  	jtag.HICANN_lvds_pads_en(0);
		  	jtag.HICANN_set_link_ctrl(0x020);
  
  			for(int m=0   ; m<144;++m) {
  			  vec_in[m]  = 1;
  			  vec_out[m] = 1;
  			}
  			for(int m=6; m>0;  --m)
  			  vec_in[(16*6+(8-hicann_nr)*6)-m] = ((~(i & 0x3f)) >> (6-m)) & 0x1;
  
  			jtag.DNC_set_data_delay(vec_in,vec_out);
  
  			delay_in = ~(j & 0x3f);
  			jtag.HICANN_set_data_delay(delay_in,delay_out);
  			jtag.HICANN_set_test(1);
  			state1 = 0;
  			state2 = 0;
  			jtagid = 0;
  
  			printf("Delay Setting DNC: %.2X \t HICANN: %.2X \t",63-i,63-j);
  
  			while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 5) {
  			  jtag.HICANN_stop_link();
  			  jtag.DNC_stop_link(0x0ff);
  			  jtag.HICANN_start_link();
  			  jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
  			  jtag.DNC_set_tx_data(0x0);

  			  usleep(900000);

  			  jtag.DNC_read_channel_sts(hicann_nr,state1);
  			  printf("DNC Status = 0x%02hx,  " ,state1 );
  			  jtag.HICANN_read_status(state2);
  			  printf("HICANN Status = 0x%02hx\n" ,static_cast<int>(state2) );
  			  ++jtagid;
  			}
  
  			error1 = 1;
  			error2 = 1;
  
  			if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))	{
  			  
  			  //printf("Sucessfull init  \n");
  			  
  			} else {
  			  
  			  printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
  			  //return 0;
  			}
  
  
  			jtag.DNC_test_mode_dis();
  
  			//while (error1 != 0 || error2 != 0) {
  			  error1 = 0;
  			  error2 = 0;
  			  uint64_t rdata64;
  			  uint64_t rdata64_2;
  			  uint64_t wdata64;
  			  
  			  //printf("Test FPGA -> HICANN  \n");
  			  jtag.FPGA_set_channel(hicann_nr);
  			  jtag.DNC_set_channel(8+hicann_nr);
  			  jtag.HICANN_set_test(1);
  			  
  			  for (int num_data=0;num_data<DATA_TX_NR;++num_data) {
  			    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
  			    jtag.FPGA_set_tx_data(wdata64);
  			    jtag.FPGA_start_dnc_packet((fpga_master << 9) + (0x1 << 8) + (0x2 & 0xff));
  			    //usleep(10);
  			    jtag.HICANN_get_rx_data(rdata64);
  			    
  			    if (rdata64 != wdata64) { 
  			      ++error1;
  			      jtag.DNC_get_rx_data(rdata64_2);
  			      //printf("Wrote: 0x%016llX Read: 0x%016llX  DNC: 0x%016llX\n",
  			      //       static_cast<long long unsigned int>(wdata64),
  			      //       static_cast<long long unsigned int>(rdata64),
  			      //       static_cast<long long unsigned int>(rdata64_2));
  			    }
  			  }
  			  
  			  //printf("Test HICANN -> FPGA  \n");
  			  jtag.DNC_set_channel(hicann_nr);
  			  
  			  
  			  for (int num_data=0;num_data<DATA_TX_NR;++num_data) {
  			    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
  			    jtag.HICANN_set_tx_data(wdata64);
  			    jtag.HICANN_start_cfg_pkg();
  			    usleep(10);
  			    jtag.FPGA_get_rx_data(rdata64);
  			    
  			    if (rdata64 != wdata64) { 
  			      ++error2;
  			      jtag.DNC_get_rx_data(rdata64_2);
  			      //printf("Wrote: 0x%016llX Read: 0x%016llX  DNC: 0x%016llX\n",
  			      //       static_cast<long long unsigned int>(wdata64),
  			      //       static_cast<long long unsigned int>(rdata64),
  			      //       static_cast<long long unsigned int>(rdata64_2));
  			    }
  			  }
  			  
  			  jtag.HICANN_set_test(0);
  			  
  			  printf ("Errors in HICANN-FPGA transmission : %i  FPGA-HICANN transmission : %i\n",error2,error1);
  			  
  			//}
			}
		return (error2+error1);
  }

  int fpga_dnc_init(myjtag_full& jtag) {
    //char buf[128];
    unsigned char state1;
    uint64_t state2;
    unsigned int error1 = 1;
    unsigned int error2 = 1;
    unsigned int init_error;
    unsigned int data_error;
    std::vector<bool> vec_in(144,false);
    std::vector<bool> vec_out(144,false);
    uint64_t delay_in;
    uint64_t delay_out;
    int jtagid;

    init_error = 0;
    data_error = 0;

    dnc_shutdown(jtag);
    hicann_shutdown(jtag);

    //Stop all components in dnc
    jtag.FPGA_set_fpga_ctrl(0x1);
    //jtag.reset_jtag();
        
    for(int m=0   ; m<144;++m) {
      vec_in[m]  = 1;
      vec_out[m] = 1;
    }
        
    usleep(199999);
    printf("\nStart FPGA_IF\n");
        
    jtag.HICANN_set_test(1);
    jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections

    if(fpga_master) {
            
      jtag.FPGA_start_dnc_packet((fpga_master << 9) + 0x100);
            
    } else {
            
      jtag.FPGA_start_dnc_packet((fpga_master << 9) + 0x100);
    }
        
    state2 = 0;
        
    while ((state2 & 0x1) == 0) {
      for(int tt=0;tt<6;++tt)
    usleep(900000);
      jtag.DNC_read_channel_sts(8,state1);
      printf("DNC Status = 0x%02hx\n" ,state1 );

      jtag.FPGA_get_status(state2);
      printf("FPGA Status = 0x%02hx\n" ,(unsigned char)state2 );
    }
        
    error1 = 0;
    error2 = 0;
        
    if (!((state1 & 0x41) == 0x41) && (state2 & 0x1)) {
      printf("ERROR init with DNC state: %.02X and FPGA state %i\n",state1,(unsigned char)state2);
      return 0;
    }
        
    jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections
    jtag.DNC_set_lowspeed_ctrl(0xff);
    jtag.DNC_lvds_pads_en(0xff & (~(1<<(hicann_nr))));
    jtag.DNC_set_init_ctrl(0x0aaaa);

    jtag.HICANN_set_reset(1);
    jtag.HICANN_set_GHz_pll_ctrl(0x7);
    jtag.HICANN_lvds_pads_en(0);
    jtag.HICANN_set_link_ctrl(0x020);

    //HICANN 1.4 j=12; i=15
    //HICANN X2 1.7 j=11 i=10
    //HICANN TUD j=11;  i=12
    
    int i=10;  // Fixed values for special HICANN //pre 12
    int j=9;  // pre 11
    
    for(int m=0   ; m<144;++m) {
      vec_in[m]  = 1;
      vec_out[m] = 1;
    }
    for(int m=6; m>0;  --m)
      vec_in[(16*6+(8-hicann_nr)*6)-m] = ((~(i & 0x3f)) >> (6-m)) & 0x1;
    
    jtag.DNC_set_data_delay(vec_in,vec_out);

    delay_in = ~(j & 0x3f);
    jtag.HICANN_set_data_delay(delay_in,delay_out);
    jtag.HICANN_set_test(1);
    state1 = 0;
    state2 = 0;
    jtagid = 0;
        
    printf("Delay Setting DNC: %.2X \t HICANN: %.2X \t",63-i,63-j);
        
    while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 1) {
      jtag.HICANN_stop_link();
      jtag.DNC_stop_link(0x0ff);
      jtag.HICANN_start_link();
      jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
        
        
      usleep(900000);
      jtag.DNC_read_channel_sts(hicann_nr,state1);
      printf("DNC Status = 0x%02hx\n" ,state1 );
      jtag.HICANN_read_status(state2);
      printf("HICANN Status = 0x%02hx\n" ,(unsigned char)state2 );
      ++jtagid;
    }
    
    error1 = 1;
    error2 = 1;
    
    if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41)) {
        
      printf("Sucessfull init  \n");
        
    } else {
        
      printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,(unsigned char)state2);
      return 0;
    }
    
    
    jtag.DNC_test_mode_dis();

    while (error1 != 0 || error2 != 0) {
      error1 = 0;
      error2 = 0;
      uint64_t rdata64;
      uint64_t rdata64_2;
      uint64_t wdata64;
            
      printf("Test FPGA -> HICANN  \n");
      jtag.FPGA_set_channel(hicann_nr);
      jtag.DNC_set_channel(8+hicann_nr);
      jtag.HICANN_set_test(1);
            
      for (int num_data=0;num_data<DATA_TX_NR;++num_data) {
    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
    jtag.FPGA_set_tx_data(wdata64);
    jtag.FPGA_start_dnc_packet((fpga_master << 9) + (0x1 << 8) + (0x2 & 0xff));
    //usleep(10);
    jtag.HICANN_get_rx_data(rdata64);
                    
    if (rdata64 != wdata64) { 
      ++error1;
      jtag.DNC_get_rx_data(rdata64_2);
      printf("Wrote: 0x%016llX Read: 0x%016llX  DNC:  0x%016llX\n"
             "                          DIV1: 0x%016llX  DIV2: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64),static_cast<long long unsigned int>(rdata64_2),static_cast<long long unsigned int>(wdata64^rdata64),static_cast<long long unsigned int>(wdata64^rdata64_2));
    }
      }

      printf("Test HICANN -> FPGA  \n");
      jtag.DNC_set_channel(hicann_nr);
                

      for (int num_data=0;num_data<DATA_TX_NR;++num_data) {
    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
    jtag.HICANN_set_tx_data(wdata64);
    jtag.HICANN_start_cfg_pkg();
    //usleep(10);
    jtag.FPGA_get_rx_data(rdata64);
                            
    if (rdata64 != wdata64) { 
      ++error2;
      jtag.DNC_get_rx_data(rdata64_2);
      printf("Wrote: 0x%016llX Read: 0x%016llX  DNC:  0x%016llX\n"
             "                          DIV1: 0x%016llX  DIV2: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64),static_cast<long long unsigned int>(rdata64_2),static_cast<long long unsigned int>(wdata64^rdata64),static_cast<long long unsigned int>(wdata64^rdata64_2));
      }
    }
            
      jtag.HICANN_set_test(0);
            
      printf ("Errors in HICANN-FPGA transmission : %i  FPGA-HICANN transmission : %i\n",error2,error1);
            
      error1 = 0;
      error2 = 0;

    }
    return 1;
    }

    int fpga_dnc_hicann_measure_init(myjtag_full& jtag) {
        
        //char buf[128];
        unsigned char state1;
        uint64_t state2;
        unsigned int error1 = 1;
        unsigned int error2 = 1;
        unsigned int init_error;
        unsigned int data_error;
        std::vector<bool> vec_in(144,false);
        std::vector<bool> vec_out(144,false);
        uint64_t delay_in, delay_out;
        int jtagid;
        int hicann_ok1 = -1;
        int hicann_ok2 = -1;
        int hicann_ok = -1;
        int dnc_ok1 = -1;
        int dnc_ok2 = -1;
        int dnc_ok = -1;
        
        int lock_eye = 0;
        
        uint64_t wdata64 = 0;
        uint64_t rdata64 = 0;
        
        init_error = 0;
        data_error = 0;
        
        //Stop all components in dnc
        jtag.FPGA_start_dnc_packet(fpga_master << 9);
        dnc_shutdown(jtag);
        hicann_shutdown(jtag);
        
        //Stop all components in dnc
        jtag.DNC_set_tx_data(0x0);

        //printf("\nStart DNC interface\n");
        jtag.DNC_set_GHz_pll_ctrl(0xe);
        jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections
        jtag.DNC_set_lowspeed_ctrl(0xff);
        jtag.DNC_lvds_pads_en(0xff & (~(1<<(hicann_nr))));
        jtag.DNC_set_init_ctrl(0x0aaaa);
        jtag.DNC_test_mode_en();

        jtag.HICANN_set_reset(1);
        jtag.HICANN_set_GHz_pll_ctrl(0x7);
        jtag.HICANN_lvds_pads_en(0);
        jtag.HICANN_set_link_ctrl(0x020);
        jtag.HICANN_set_test(1);
		
		printf("Measure DNC-HICANN connection: \n");
        
        for (int i=63;i>=32;--i)    // DNC Delay
        for (int j=63;j>=32;--j) {  // HICANN Delay
            
            for(int m=0   ; m<144;++m) {
                vec_in[m]  = 1;
                vec_out[m] = 1;
            }
            for(int m=6; m>0;  --m)
                vec_in[(16*6+(8-hicann_nr)*6)-m] = ((i & 0x3f) >> (6-m)) & 0x1;
            
            jtag.DNC_set_data_delay(vec_in,vec_out);
            
            delay_in = (j & 0x3f);
            jtag.HICANN_set_data_delay(delay_in,delay_out);
            state1 = 0;
            state2 = 0;
            jtagid = 0;
            
            printf("Delay Setting DNC: %.2X \t HICANN: %.2X \t",i,j);
            
            while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 5) {
                jtag.HICANN_stop_link();
                jtag.DNC_stop_link(0x0ff);
                jtag.HICANN_start_link();
                jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
        #ifdef NCSIM
                wait(2,SC_US);
        #else
                usleep(900000);
        #endif
                jtag.DNC_read_channel_sts(hicann_nr,state1);
                //printf("DNC Status = 0x%02hx\n" ,state1 );
                jtag.HICANN_read_status(state2);
                //printf("HICANN Status = 0x%02hx\n" ,static_cast<int>(state2) );
                ++jtagid;
            }
        
            error1 = 0;
            error2 = 0;

            // Measure transmission with current delay setting
            
            if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))
            {
                
                //printf("Sucessfull init  \n");
                //printf("Test DNC -> HICANN  \n");

                for (int num_data=0;num_data<1000;++num_data) {
                    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                    jtag.DNC_start_config(hicann_nr,wdata64);
                    //usleep(10);
                    jtag.HICANN_get_rx_data(rdata64);
                                        
                    if (rdata64 != wdata64) {
                        ++error1;
                        //printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
                    }
                }
                        
                //printf("Test HICANN -> DNC  \n");
                        
                for (int num_data=0;num_data<1000;++num_data) {
                    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                    jtag.HICANN_set_tx_data(wdata64);
                    jtag.HICANN_start_cfg_pkg();
                    //usleep(10);
                    jtag.DNC_set_channel(hicann_nr);
                    jtag.DNC_get_rx_data(rdata64);

                    if (rdata64 != wdata64) {
                        ++error2;
                        //printf("Wrote: 0x%016llX Read: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
                    }
                }

                printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);
                if (error1 || error2) ++data_error;
                
                if (error2 < 500) {
                    // printf("Found first correct DNC setting %i\n",i);
                    dnc_ok1 = i;
                    i = 0;
                    j = 0;
                }
                
            } else {
                
                ++init_error;
                printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
            }
        } // end for i,j Delays
        
        if (dnc_ok1<0) {
            printf("INITERROR: No valid delay setting found! Check setup.\n");
            return 0;
        }
        
        // Find best HICANN setting
        for (int i=dnc_ok1;i>dnc_ok1-1;--i)    // DNC Delay
        for (int j=63;j>=32;--j) {  // HICANN Delay
            
            for(int m=0   ; m<144;++m) {
                vec_in[m]  = 1;
                vec_out[m] = 1;
            }
            for(int m=6; m>0;  --m)
                vec_in[(16*6+(8-hicann_nr)*6)-m] = ((i & 0x3f) >> (6-m)) & 0x1;
            
            jtag.DNC_set_data_delay(vec_in,vec_out);
            
            delay_in = (j & 0x3f);
            jtag.HICANN_set_data_delay(delay_in,delay_out);
            state1 = 0;
            state2 = 0;
            jtagid = 0;
            
            printf("Delay Setting DNC: %.2X \t HICANN: %.2X \t",i,j);
            
            while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 5) {
                jtag.HICANN_stop_link();
                jtag.DNC_stop_link(0x0ff);
                jtag.HICANN_start_link();
                jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
        #ifdef NCSIM
                wait(2,SC_US);
        #else
                usleep(900000);
        #endif
                jtag.DNC_read_channel_sts(hicann_nr,state1);
                //printf("DNC Status = 0x%02hx\n" ,state1 );
                jtag.HICANN_read_status(state2);
                //printf("HICANN Status = 0x%02hx\n" ,static_cast<int>(state2) );
                ++jtagid;
            }
        
            error1 = 0;
            error2 = 0;

            // Measure transmission with current delay setting
            
            if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))
            {
                
                //printf("Sucessfull init  \n");
                //printf("Test DNC -> HICANN  \n");

                for (int num_data=0;num_data<1000;++num_data) {
                    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                    jtag.DNC_start_config(hicann_nr,wdata64);
                    //usleep(10);
                    jtag.HICANN_get_rx_data(rdata64);
                                        
                    if (rdata64 != wdata64) {
                        ++error1;
                        lock_eye = 1;
                        //printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
                    }
                }
                        
                //printf("Test HICANN -> DNC  \n");
                        
                for (int num_data=0;num_data<1000;++num_data) {
                    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                    jtag.HICANN_set_tx_data(wdata64);
                    jtag.HICANN_start_cfg_pkg();
                    //usleep(10);
                    jtag.DNC_set_channel(hicann_nr);
                    jtag.DNC_get_rx_data(rdata64);

                    if (rdata64 != wdata64) {
                        ++error2;
                        //printf("Wrote: 0x%016llX Read: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
                    }
                }

                printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);
                if (error1 || error2) ++data_error;
                
                if ((error1 == 0) && (hicann_ok1 < 0) && (lock_eye == 1)) {
                    // printf("Found first correct HICANN setting\n");
                    hicann_ok1 = j;
                }

                if ((error1 != 0) && (hicann_ok1 >= 0)) {
                    // printf("Found end of first correct HICANN eye\n");
                    hicann_ok2 = j-1;
                    i = 0;
                    j = 0;
                    hicann_ok = (hicann_ok2+hicann_ok1)/2;
                }
            } else {
                
                ++init_error;
                printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
            }
        } // end for i,j Delays
        
        if (hicann_ok<0) {
            printf("INITERROR: No valid delay setting found! Check setup.\n");
            return 0;
        }

        dnc_ok1 = -1;
        lock_eye = 0;
        
        // Find best DNC setting
        for (int i=63;i>=0;--i)    // DNC Delay
        for (int j=hicann_ok;j>hicann_ok-1;--j) {  // HICANN Delay
            
            for(int m=0   ; m<144;++m) {
                vec_in[m]  = 1;
                vec_out[m] = 1;
            }
            for(int m=6; m>0;  --m)
                vec_in[(16*6+(8-hicann_nr)*6)-m] = ((i & 0x3f) >> (6-m)) & 0x1;
            
            jtag.DNC_set_data_delay(vec_in,vec_out);
            
            delay_in = (j & 0x3f);
            jtag.HICANN_set_data_delay(delay_in,delay_out);
            state1 = 0;
            state2 = 0;
            jtagid = 0;
            
            printf("Delay Setting DNC: %.2X \t HICANN: %.2X \t",i,j);
            
            while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 5) {
                jtag.HICANN_stop_link();
                jtag.DNC_stop_link(0x0ff);
                jtag.HICANN_start_link();
                jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
        #ifdef NCSIM
                wait(2,SC_US);
        #else
                usleep(900000);
        #endif
                jtag.DNC_read_channel_sts(hicann_nr,state1);
                //printf("DNC Status = 0x%02hx\n" ,state1 );
                jtag.HICANN_read_status(state2);
                //printf("HICANN Status = 0x%02hx\n" ,static_cast<int>(state2) );
                ++jtagid;
            }
        
            error1 = 0;
            error2 = 0;

            // Measure transmission with current delay setting
            
            if (((state1 & 0x73) == 0x41) && ((state2 & 0x73) == 0x41))
            {
                
                //printf("Sucessfull init  \n");
                //printf("Test DNC -> HICANN  \n");

                for (int num_data=0;num_data<1000;++num_data) {
                    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                    jtag.DNC_start_config(hicann_nr,wdata64);
                    //usleep(10);
                    jtag.HICANN_get_rx_data(rdata64);
                                        
                    if (rdata64 != wdata64) {
                        ++error1;
                        //printf("Wrote: 0x%016llX Read: 0x%016llX\n",(long long unsigned int)wdata64,(long long unsigned int)rdata64);
                    }
                }
                        
                //printf("Test HICANN -> DNC  \n");
                        
                for (int num_data=0;num_data<1000;++num_data) {
                    wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                    jtag.HICANN_set_tx_data(wdata64);
                    jtag.HICANN_start_cfg_pkg();
                    //usleep(10);
                    jtag.DNC_set_channel(hicann_nr);
                    jtag.DNC_get_rx_data(rdata64);

                    if (rdata64 != wdata64) {
                        ++error2;
                        lock_eye = 1;
                        //printf("Wrote: 0x%016llX Read: 0x%016llX\n",static_cast<long long unsigned int>(wdata64),static_cast<long long unsigned int>(rdata64));
                    }
                }

                printf ("Errors in HICANN-DNC transmission : %i  DNC-HICANN transmission : %i\n",error2,error1);
                if (error1 || error2) ++data_error;
                
                if ((error2 == 0) && (dnc_ok1 < 0) && (lock_eye == 1)) {
                    // printf("Found first correct DNC setting\n");
                    dnc_ok1 = i;
                }

                if ((error2 != 0) && (dnc_ok1 >= 0)) {
                    // printf("Found end of first correct DNC eye\n");
                    dnc_ok2 = i-1;
                    i = 0;
                    j = 0;
                    dnc_ok = (dnc_ok2+dnc_ok1)/2;
                }
            } else {
                
                ++init_error;
                printf("ERROR init with DNC state: %.02X and HICANN state %.02X\n",state1,static_cast<uint32_t>(state2));
            }
        } // end for i,j Delays
        
        if (dnc_ok<0) {
            printf("INITERROR: No valid delay setting found! Check setup.\n");
            return 0;
        } else {
		    printf("INITOK: Measured delay DNC: %i HICANN %i \n",dnc_ok, hicann_ok);
		}
        
        error1 = 1;
        error2 = 1;
        int full_init_trys = 0;
        
        while ((error1 + error2 > 0) && (full_init_trys < 5)) {
            //Stop all components in dnc
            dnc_shutdown(jtag);
            hicann_shutdown(jtag);
            
            //Stop all components in dnc
            jtag.DNC_set_GHz_pll_ctrl(0xe);
            jtag.FPGA_set_fpga_ctrl(0x1);
            jtag.DNC_set_tx_data(0x0);
            //jtag->reset_jtag();
            
            for(int m=0;m<144;++m) {
                vec_in[m]  = 1;
                vec_out[m] = 1;
            }
        #ifdef NCSIM
            wait(120,SC_US);
        #else
            usleep(199999);
        #endif
            printf("\nStart FPGA_IF\n");
            
            jtag.HICANN_set_test(1);
            jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections
        
            jtag.FPGA_start_dnc_packet((fpga_master << 9) + 0x100);
            state2 = 0;
            
            while ((state2 & 0x3) != 1) {
        #ifdef NCSIM
                wait(100,SC_US);
        #else
                for(int tt=0;tt<6;++tt)
                    usleep(900000);
        #endif
                jtag.DNC_read_channel_sts(8,state1);
                printf("DNC Status = 0x%02hx\n" ,state1 );
                            
                jtag.FPGA_get_status(state2);
                printf("FPGA Status = 0x%02hx\n" ,static_cast<int>(state2) );
            }
                        
            if (!((state1 & 0x41) == 0x41) && (state2 & 0x1)) {
                printf("ERROR init with DNC state: %.02X and FPGA state %i\n",state1,static_cast<int>(state2));
                return 0;
            }
            
            jtag.DNC_stop_link(0xff);  //Stop DNC-HICANN connections
            jtag.DNC_set_lowspeed_ctrl(0xff);
            jtag.DNC_lvds_pads_en(0xff & (~(1<<(hicann_nr))));
            jtag.DNC_set_init_ctrl(0x0aaaa);
            jtag.DNC_test_mode_dis();
        
            jtag.HICANN_set_reset(1);
            jtag.HICANN_set_GHz_pll_ctrl(0x7);
            jtag.HICANN_lvds_pads_en(0);
            jtag.HICANN_set_link_ctrl(0x020);
            jtag.HICANN_set_test(1);
                
            for(int m=0   ; m<144;++m) {
                vec_in[m]  = 1;
                vec_out[m] = 1;
            }
            for(int m=6; m>0;  --m)
                vec_in[(16*6+(8-hicann_nr)*6)-m] = ((dnc_ok & 0x3f) >> (6-m)) & 0x1;
            
            jtag.DNC_set_data_delay(vec_in,vec_out);
            
            delay_in = (hicann_ok & 0x3f);
            jtag.HICANN_set_data_delay(delay_in,delay_out);
            state1 = 0;
            state2 = 0;
            jtagid = 0;
            
            while ((((state1 & 0x73) != 0x41) || ((state2 & 0x73) != 0x41)) && jtagid < 5) {
                jtag.HICANN_stop_link();
                jtag.DNC_stop_link(0x0ff);
                jtag.HICANN_start_link();
                jtag.DNC_start_link(0x100 + (0xff & 1<<hicann_nr));
        #ifdef NCSIM
                wait(2,SC_US);
        #else
                usleep(900000);
        #endif
                jtag.DNC_read_channel_sts(hicann_nr,state1);
                //printf("DNC Status = 0x%02hx\n" ,state1 );
                jtag.HICANN_read_status(state2);
                //printf("HICANN Status = 0x%02hx\n" ,static_cast<int>(state2) );
                ++jtagid;
            }
        
            error1 = 0;
            error2 = 0;
    
            // Measure transmission with current delay setting
    
    
        // Final recheck of complete Gbit transmission line
        #ifdef NCSIM
            std::cout << "NCSIM: Successfull initialized  --> Ready to start TM" << std::endl;
        #else
            error1 = 0;
            error2 = 0;
            uint64_t rdata64;
            uint64_t rdata64_2;
            uint64_t wdata64;
            
            printf("Test FPGA -> HICANN  \n");
            jtag.FPGA_set_channel(hicann_nr);
            jtag.DNC_set_channel(8+hicann_nr);
            jtag.HICANN_set_test(1);
            
            for (int num_data=0;num_data<DATA_TX_C;++num_data) {
                wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                jtag.FPGA_set_tx_data(wdata64);
                jtag.FPGA_start_dnc_packet((fpga_master << 9) + (0x1 << 8) + (0x2 & 0xff));
                //usleep(10);
                jtag.HICANN_get_rx_data(rdata64);
                
                if (rdata64 != wdata64) { 
                    ++error1;
                    jtag.DNC_get_rx_data(rdata64_2);
                    printf("Wrote: 0x%016llX Read: 0x%016llX  DNC: 0x%016llX\n",
                        static_cast<long long unsigned int>(wdata64),
                        static_cast<long long unsigned int>(rdata64),
                        static_cast<long long unsigned int>(rdata64_2));
                }
            }
                        
            printf("Test HICANN -> FPGA  \n");
            jtag.DNC_set_channel(hicann_nr);
                        
                        
            for (int num_data=0;num_data<DATA_TX_C;++num_data) {
                wdata64 = (((uint64)((rand() % 0xffff)+1) << 48)) + (((uint64)((rand() % 0xffffff)+1) << 24)) + (uint64)((rand() % 0xffffff)+1);
                jtag.HICANN_set_tx_data(wdata64);
                jtag.HICANN_start_cfg_pkg();
                usleep(10);
                jtag.FPGA_get_rx_data(rdata64);
            
                if (rdata64 != wdata64) { 
                    //error2; //ECM: statement has no effect...
                    jtag.DNC_get_rx_data(rdata64_2);
                    printf("Wrote: 0x%016llX Read: 0x%016llX  DNC: 0x%016llX\n",
                    static_cast<long long unsigned int>(wdata64),
                    static_cast<long long unsigned int>(rdata64),
                    static_cast<long long unsigned int>(rdata64_2));
                }
            }
            
            printf ("Errors in HICANN-FPGA transmission : %i  FPGA-HICANN transmission : %i\n",error2,error1);
            ++full_init_trys;
    #endif
        }
        
        jtag.HICANN_set_test(0);
        return 1;
    }
};

class LteeTmL2init : public Listee<Testmode>{
public:
  LteeTmL2init() : Listee<Testmode>(string("tm_l2init"),string("test spl1 and layer2 interface on HICANN")){};
  Testmode * create(){return new TmL2init();};
};

LteeTmL2init ListeeTmL2init;

