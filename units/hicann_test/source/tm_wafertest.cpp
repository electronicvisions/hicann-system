// Company         :   kip
// Author          :   Andreas Grübl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//------------------------------------------------------------------------


// ECM: due to cleanup_comm_hierarchy and cleanup_depointerize => LOTS OF
// CHANGES! I'm not sure that this will work (esp. manual Init() stuff).

// testmode to perform "almost full" digital test on one HICANN each while controlling the wafer tester

#include "common.h"
#include <boost/algorithm/string.hpp>

#include "hicann_ctrl.h"
#include "fpga_control.h"
#include "testmode.h" 
#include "ramtest.h"
#include "syn_trans.h"

#include "linklayer.h"

// required for checking only
#include "s2c_jtagphys_2fpga_arq.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys.h"

//only if needed
#include "neuron_control.h"   //neuron / "analog" SPL1 control class
#include "spl1_control.h"     //spl1 control class
#include "l1switch_control.h" //layer 1 switch control class
#include "fg_control.h" //floating gate control class
#include "repeater_control.h" //repeater control class

// for accessing the wafer tester
#include "Serial.h"
#include "ChipTest.h"
#include "PowerSupply.h"

#define USENR         1		
#define fpga_master   1
#define DATA_TX_NR    100
#define RUNS          5000

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


class TmWaferTest : public Testmode{

public:
	
  static const int TAG             = 1; //STDP uses second tag
  static const int MEM_BASE        = 0;
  static const int MEM_WIDTH       = 8; // bit
  static const int MEM_TEST_NUM    = 2; // times size
  static const int REG_BASE        = 0x40;  // bit #6 set 

		// SRAM controller timing (used for all controllers)
	static const uint read_delay      = 32;
	static const uint setup_precharge = 8;
	static const uint write_delay     = 8;

	// Wafer tester
  Serial *s_port_ct;
  Serial *s_port_ps;
  ChipTests *chip_test;
	PowerSupply *power_supply;

	// HICANN control
  HicannCtrl  *hc;
	FPGAControl *fc;
  NeuronControl *nc;	
  SPL1Control *spc;
  LinkLayer<ci_payload,ARQPacketStage2> *ll;

	L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;//for testing
	FGControl *fc_tr, *fc_tl, *fc_bl, *fc_br;

	RepeaterControl *rc; //repeater control for read/write functions
	RepeaterControl *rl,*rr,*rtl,*rbl,*rtr,*rbr;	//all repeaters

   int synblock;
	
	// repeater control 
	int repeater;
	string rname;
	int ocp_base;
	int mem_size;
	ci_data_t rcvdata;
	ci_addr_t rcvaddr;
	uint error;

	void select_rep() {
		switch(repeater) {
		case 0 : rname="TopLeft";    rc = rtl; ocp_base = OcpRep_tl.startaddr; mem_size=64; break;
		case 1 : rname="TopRight";   rc = rtr; ocp_base = OcpRep_tr.startaddr; mem_size=64; break;
		case 2 : rname="HorizLeft";  rc = rl;  ocp_base = OcpRep_l.startaddr; mem_size=32; break;
		case 3 : rname="HorizRight"; rc = rr;  ocp_base = OcpRep_r.startaddr; mem_size=32; break;
		case 4 : rname="BottomLeft"; rc = rbl; ocp_base = OcpRep_bl.startaddr; mem_size=64; break;
		default: rname="BottomRight";rc = rbr; ocp_base = OcpRep_br.startaddr; mem_size=64; break;
		}
	}

	void select_syn() {     
		switch(synblock) {
		case 0 : rname="Top";       ocp_base = OcpStdpCtrl_t.startaddr; mem_size=0x7fff; break;
		case 1 : rname="Bottom";    ocp_base = OcpStdpCtrl_b.startaddr; mem_size=0x7fff; break;
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

	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater location
	enum rc_regs {rc_config=0x80,rc_status=0x80,rc_td0=0x81,rc_td1=0x87,rc_scfg=0xa0};
	enum rc_srambits {rc_touten=0x80,rc_tinen=0x40,rc_recen=0x20,rc_dir=0x10,rc_ren1=8,rc_ren0=4,rc_len1=2,rc_len0=1};
	enum rc_config
	{rc_fextcap1=0x80,rc_fextcap0=0x40,rc_drvresetb=0x20,rc_dllresetb=0x10,rc_start_tdi1=8,rc_start_tdi0=4,rc_start_tdo1=2,rc_start_tdo0=1};

	enum teststat {pass, fail, noexec};
	
	friend std::ostream & operator <<(std::ostream &os, teststat ts){
		os<<(ts==pass?"pass":(ts==fail?"fail":"not executed"));
		return os;
	}
	
	friend std::fstream & operator <<(std::fstream &os, teststat ts){
		os<<(ts==pass?"pass":(ts==fail?"fail":"not executed"));
		return os;
	}
						
	teststat test_current, test_id, test_fpga_hicann_init, test_spl1_fpga, test_spl1_jtag,
	         test_sw, test_rep, test_fg, test_synblk0, test_synblk1;


	// ********** test methods **********
	bool test_jtag_id(fstream& rf){
		uint64_t hicann_id = 0x14849434;
		uint64_t id=0;
		jtag->read_id(id, jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << id << ":";
		rf << "HICANN ID: 0x" << hex << id << ":";
		if(id == hicann_id){
			cout << " ok :-)" << endl;
			rf << " ok :-)" << endl;
			return true;
		} else {
			cout << " fail :-(" << endl;
			rf << " fail :-(" << endl;
			return false;
		}
	}

	bool test_switchram(uint runs, fstream& rf) {
		// get pointer to all crossbars
		lcl = &hc->getLC_cl();
		lcr = &hc->getLC_cr();
		lctl =&hc->getLC_tl();
		lcbl =&hc->getLC_bl();
		lctr =&hc->getLC_tr();
		lcbr =&hc->getLC_br();

		// full L1 switch test
		rf << endl << "########## TEST SWITCH RAM ##########" << endl;
		
		uint error=0;
		uint lerror=0;
		L1SwitchControl *l1;
		uint rmask; 
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<6;r++){
				switch (r){
				case 0:l1=lcl;mem_size=64;rmask=15;break;
				case 1:l1=lcr;mem_size=64;rmask=15;break;
				case 2:l1=lctl;mem_size=112;rmask=0xffff;break;
				case 3:l1=lctr;mem_size=112;rmask=0xffff;break;
				case 4:l1=lcbl;mem_size=112;rmask=0xffff;break;
				case 5:l1=lcbr;mem_size=112;rmask=0xffff;break;
				}

				uint rseed = ((randomseed+(1000*r))+l);
				srand(rseed);
				uint tdata[0x80];
				for(int i=0;i<mem_size;i++)
					{
						tdata[i]=rand() & rmask;
						l1->write_cmd(i,tdata[i],0);
					}

				for(int i=0;i<mem_size;i++)
				{
					l1->read_cmd(i,0);
				}

				hc->Flush();

				for(int i=0;i<mem_size;i++)
				{
					l1->get_data(rcvaddr,rcvdata);
					if(rcvdata != tdata[i]){
						error++;
						lerror++;
						rf << hex << "ERROR: row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<
							bino(tdata[i],16)<<endl;
					}
				}
				rf <<"Switchramtest loop " << l << " errors: " << lerror << ", total errors: " << error << endl;
			}
			rf << endl;
		
		}
		return (error == 0);
	}

	bool test_repeaterram(uint runs, fstream& rf) {
		// intialize repeater control modules
    rl  = &hc->getRC_cl();
    rr  = &hc->getRC_cr();
    rtl = &hc->getRC_tl();
    rbl = &hc->getRC_bl();
    rtr = &hc->getRC_tr();
    rbr = &hc->getRC_br();

		// test repeater sram
		rf << endl << "########## TEST REPEATER SRAM ##########" << endl;

		uint error=0;
		uint lerror=0;
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<6;r++){
				repeater=r;
				select_rep();
					
				// set sram timings to testmode's defaults
				rc->set_sram_timings(read_delay, setup_precharge, write_delay);
				
				rf << "Repeater base adr: 0x" << hex << ocp_base << endl;
				rf << "test repeater sram:"<<rname<<endl;

				uint rseed = ((randomseed+(1000*r))+l);
				srand(rseed);
				const uint rmask=0x5f; //always keep recen and touten zero!
				uint tdata[0x40];
				for(int i=0;i<mem_size;i++){
					tdata[i]=rand()%256 & rmask;
					//rc->write_cmd(repadr(i),tdata[i],0);
					rc->write_cmd(i,tdata[i],0);
				}

				srand(rseed);
				for(int i=0;i<mem_size;i++){
					//rc->read_cmd(repadr(i),0);
					rc->read_cmd(i,0);
					rc->get_data(rcvaddr,rcvdata);
					if(rcvdata != tdata[i]){
						error++;
						lerror++;
						rf << hex << "radr \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,8) << "	data written: 0b" <<
							bino(tdata[i],8)<<endl;
					}
				}
				delete rc;
			}
			rf <<"Repeater SRAM test loop " << l << ", errors: " << lerror << ", total errors: " << error << endl;
		}
		rf << endl;
		return (error == 0);
	}

	bool test_fgram(uint runs, fstream& rf) {
		// get pointer to all floating gate controllers
		fc_tl =&hc->getFC_tl();
		fc_bl =&hc->getFC_bl();
		fc_tr =&hc->getFC_tr();
		fc_br =&hc->getFC_br();

		// full floating gate controller memory test
		rf << endl << "########## TEST FLOATING GATE CONTROLLEr MEMORY ##########" << endl;
		
		uint error=0;
		uint lerror=0;
		FGControl *fc;
		uint rmask; 
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<4;r++){
				switch (r){
				case 0:fc=fc_tl;mem_size=65;rmask=0xfffff;break;
				case 1:fc=fc_tr;mem_size=65;rmask=0xfffff;break;
				case 2:fc=fc_bl;mem_size=65;rmask=0xfffff;break;
				case 3:fc=fc_br;mem_size=65;rmask=0xfffff;break;
				}

				for(uint bank=0;bank<2;bank++){
					uint rseed = ((randomseed+(1000*r))+l);
					srand(rseed);
					uint tdata[0x100];
					for(int i=0+bank*128;i<mem_size+bank*128;i++)
						{
							tdata[i]=rand() & rmask;
							fc->write_cmd(i,tdata[i],0);
						}

					srand(rseed);
					for(int i=0+bank*128;i<mem_size+bank*128;i++)
					{
						fc->read_cmd(i,0);
						fc->get_data(rcvaddr,rcvdata);
						if(rcvdata != tdata[i]){
							error++;
							lerror++;
							rf << hex << "ERROR: bank \t" << bank << ", row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<
								bino(tdata[i],16)<<endl;
						}
					}
					rf <<"FG controller ram test loop " << l << " errors: " << lerror << ", total errors: " << error << endl;
				}
			}
			rf << endl;
		}
		return (error == 0);
	}

	bool test_block(HicannCtrl* hc, int block, int reg_loops, int synapse_ramtest_loops, int syndrv_ramtest_loops, fstream& rf)
    {
        struct {
            bool cfg;
            bool creg;
            bool lut;
            bool synrst;
            bool syn_weight;
            bool syn_weight_encr;
            bool syn_dec;
            bool syn_dec_encr;
            bool syndrv_sram;
            bool syndrv_sram_ext;
        } test_report = { true, true, true, true, true, true, true, true, true, true };

        static const int syn_ramtest_start = 4;
        static const int syn_ramtest_stop = 223;
        static const int syndrv_ramtest_start = Syn_trans::syndrv_ctrl;
        static const int syndrv_ramtest_stop = Syn_trans::syndrv_gmax + 0xff;
        static const int syndrv_block_size = 224;
        //static const int syndrv_ramtest_stop = Syn_trans::syndrv_ctrl + 3;

        assert((syn_ramtest_start >= 0) && (syn_ramtest_start <= 223));
        assert((syn_ramtest_start >= 0) && (syn_ramtest_stop <= 223));
        assert(syn_ramtest_start <= syn_ramtest_stop);

        synblock=block;
        select_syn();

				rf << endl << "########## TEST STDP CTRL ##########" << endl;

        std::auto_ptr<CtrlModule> rc(new CtrlModule(hc, TAG, ocp_base, 0x7fff));
        rf << "StdpCtrl base adr: 0x" << hex << ocp_base << endl;

        bool result = true;
        Syn_trans syn(rc.get());
        Syn_trans::Config_reg cfg;
        Syn_trans::Control_reg creg;
        Syn_trans::Lut_data lut_a, lut_b, lut_ab;
        Syn_trans::Syn_corr synrst;

        // set syndriver SRAM controller timings
				//hc->getSC(0).configure();
				//hc->getSC(1).configure();
				hc->getSC(0).set_sram_timings(read_delay, setup_precharge, write_delay);
				hc->getSC(1).set_sram_timings(read_delay, setup_precharge, write_delay);
				
				// check config register
        test_report.cfg = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Config_reg cfg_check;
            cfg.randomize();
            syn.set_config_reg(cfg);
            cfg_check = syn.get_config_reg();

            if( !(cfg == cfg_check ) ) {
                rf << "read/write check of config register failed" << endl;
                result = false;
                test_report.cfg = false;
            }
        }
				rf << "Random test of config register (" << reg_loops << "x) completed" << endl << endl;

        cfg.dllresetb[0] = false;  // turning on DLL-resets
        cfg.dllresetb[1] = false;
        cfg.gen[0] = true;
        cfg.gen[1] = true;
        cfg.gen[2] = true;
        cfg.gen[3] = true;
        cfg.pattern[0] = Syn_trans::CORR_READ_CAUSAL;
        cfg.pattern[1] = Syn_trans::CORR_READ_ACAUSAL;
        cfg.predel = 0xf;
        cfg.endel = 0xf;
        cfg.oedel = 0xf;
        cfg.wrdel = 0x2;

        syn.set_config_reg(cfg);


        // check control register
        test_report.creg = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Control_reg creg_check;

            creg.randomize();
            creg.cmd = Syn_trans::DSC_IDLE;
            syn.set_control_reg(creg);
            creg_check = syn.get_control_reg();

            if( !(creg == creg_check) ) {
                rf << "read/write check of control register failed" << endl;
                rf << "  got: 0x" << hex << creg_check.pack()
                   << "  expected: 0x" << creg.pack() << dec << endl;
                result = false;
                test_report.creg = false;
            }
        }
				rf << "Random test of control register (" << reg_loops << "x) completed" << endl << endl;

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.encr = false;
        creg.continuous = false;
        creg.newcmd = true;
        creg.adr = 0;
        creg.lastadr = 224;
        creg.sel = 0;
        creg.without_reset = true;
        creg.sca = true;
        creg.scc = true;

        syn.set_control_reg(creg);


        // check LUT registers
        test_report.lut = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Lut_data lut_a_check, lut_b_check, lut_ab_check;

            for(int j=0; j<16; j++) {
                lut_a[j] = random() & 0xf;
                lut_b[j] = random() & 0xf;
                lut_ab[j] = random() & 0xf;
            }

            syn.set_lut(lut_a, 0);
            syn.set_lut(lut_b, 1);
            syn.set_lut(lut_ab, 2);

            syn.get_lut(lut_a_check, 0);
            syn.get_lut(lut_b_check, 1);
            syn.get_lut(lut_ab_check, 2);

            for(int j=0; j<16; j++) {
                if( !(lut_a[j] == lut_a_check[j]) 
                    && !(lut_b[j] == lut_b_check[j])
                    && !(lut_ab[j] == lut_ab_check[j]) )
                {
                    rf << "Read/write test of LUT failed at index " << dec << j << endl;
                    result = false;
                    test_report.lut = false;
                }
            }
        }
				rf << "Random test of STDP LUTs (" << reg_loops << "x) completed" << endl << endl;

        for(int j=0; j<16; j++) {
            lut_a[j] = 0xf;
            lut_b[j] = 0x0;
            lut_ab[j] = j;
        }

        syn.set_lut(lut_a, 0);
        syn.set_lut(lut_b, 1);
        syn.set_lut(lut_ab, 2);


        // check SYNRST register
        test_report.synrst = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Syn_corr synrst_check;

            for(int i=0; i<2; i++) {
                for(int j=0; j<4; j++) {
                    synrst[j][i] = random() & 0xff;
                }
            }

            syn.set_synrst(synrst);
            syn.get_synrst(synrst_check);

            for(int i=0; i<2; i++) {
                for(int j=0; j<4; j++) {
                    if( synrst[j][i] != synrst_check[j][i] ) {
                        rf << "Read/write test of SYNRST failed at pattern " << dec << i
                           << " slice " << j << endl;
                        test_report.synrst = false;
                    }
                }
            }
        }
        rf << "Random test of correlation reset register (" << reg_loops << "x) completed" << endl << endl;
        

        rf << "Performing random correlation reset commands (" << reg_loops << "x)..." << endl;

        // reset correlation command test
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Syn_corr synrst_check;

            for(int i=0; i<2; i++) {
                for(int j=0; j<4; j++) {
                    synrst[j][i] = random() & 0xff;
                }
            }

            syn.set_synrst(synrst);

            creg.cmd = Syn_trans::DSC_START_READ;
            creg.adr = 2;
            syn.set_control_reg(creg);
            syn.wait_while_busy();

            creg.cmd = Syn_trans::DSC_RST_CORR;
            syn.set_control_reg(creg);
            syn.wait_while_busy();

            creg.cmd = Syn_trans::DSC_CLOSE_ROW;
            syn.set_control_reg(creg);
            syn.wait_while_busy();
        }

        
        creg.cmd = Syn_trans::DSC_IDLE;
        creg.adr = 0;
        syn.set_control_reg(creg);

        for(int i=0; i<2; i++) {
            for(int j=0; j<4; j++) {
                synrst[j][i] = 0xff;
            }
        }

        syn.set_synrst(synrst);

        rf << "Starting synapse weights ramtest (" << synapse_ramtest_loops << "x)..." << endl;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_weight = syn_ramtest(syn, rf, 
                    syn_ramtest_start, 
                    syn_ramtest_stop)
                && test_report.syn_weight;

        rf << "Starting synapse decoder addresses ramtest (" << synapse_ramtest_loops << "x)..." << endl;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_dec = syn_ramtest_dec(syn, rf,
                    syn_ramtest_start,
                    syn_ramtest_stop) 
                && test_report.syn_dec;

        // read again with correlation activated (only one run each)
				uint corr_runs = 1;
        rf << "Starting synapse weights ramtest with correlation readout (" << corr_runs << "x)..." << endl;
        for(int i=0; i<corr_runs; i++)
            test_report.syn_weight_encr = syn_ramtest(syn, rf, syn_ramtest_start, syn_ramtest_stop, false)
                && test_report.syn_weight_encr;

        rf << "Starting synapse decoder addresses ramtest with correlation readout (" << corr_runs << "x)..." << endl;
        for(int i=0; i<corr_runs; i++)
            test_report.syn_dec_encr = syn_ramtest_dec(syn, rf, syn_ramtest_start, syn_ramtest_stop, false)
                && test_report.syn_dec_encr;

        rf << "Starting synapse driver SRAM ramtest on CTRL block (" << syndrv_ramtest_loops << "x)..." << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = ramtest(rc.get(), Syn_trans::syndrv_ctrl,
                    Syn_trans::syndrv_ctrl+syndrv_block_size-1, 
                    rf, true)
                && test_report.syndrv_sram;

        rf << "Starting synapse driver SRAM ramtest on DRV block (" << syndrv_ramtest_loops << "x)..." << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = test_report.syndrv_sram && ramtest(rc.get(), Syn_trans::syndrv_drv,
                    Syn_trans::syndrv_drv+syndrv_block_size-1, 
                    rf, true)
                && test_report.syndrv_sram;
        
        rf << "Starting synapse driver SRAM ramtest on GMAX block (" << syndrv_ramtest_loops << "x)..." << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = test_report.syndrv_sram && ramtest(rc.get(), Syn_trans::syndrv_gmax,
                    Syn_trans::syndrv_gmax+syndrv_block_size-1, 
                    rf, true)
                && test_report.syndrv_sram;


        // run one update cycle of auto_update
        creg.cmd = Syn_trans::DSC_AUTO;
        creg.sel = 0;
        creg.encr = true;
        creg.adr = 0;
        creg.lastadr = 2;
        creg.continuous = 0;

        syn.set_control_reg(creg);
        syn.wait_while_busy();

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.lastadr = 223;
        syn.set_control_reg(creg);


        result = test_report.cfg 
            && test_report.creg 
            && test_report.lut 
            && test_report.synrst
            && test_report.syn_weight
            && test_report.syn_weight_encr
            && test_report.syn_dec
            && test_report.syn_dec_encr
            && test_report.syndrv_sram
            && test_report.syndrv_sram_ext;

        rf << "<<<< STDP report >>>>"
            << dec
            << "\nreg_loops         = " << reg_loops
            << "\nstart_row         = " << syn_ramtest_start << " (full: 0)"
            << "\nstop_row          = " << syn_ramtest_stop << " (full: 223)"
            << "\nsyndrv_block_size = " << syndrv_block_size << " (full: 224)"
            << "\n----" 
            << "\ncfg:.........." << (test_report.cfg ? "ok" : "failed")
            << "\ncreg:........." << (test_report.creg ? "ok" : "failed")
            << "\nlut:.........." << (test_report.lut ? "ok" : "failed")
            << "\nsynrst:......." << (test_report.synrst ? "ok" : "failed")
            << "\nsyn weights:.." << (test_report.syn_weight ? "ok" : "failed")
            << "\n +with encr:.." << (test_report.syn_weight_encr ? "ok" : "failed")
            << "\nsyn decoders:." << (test_report.syn_dec ? "ok" : "failed")
            << "\n +with encr:.." << (test_report.syn_dec_encr? "ok" : "failed")
            << "\nsyndrv sram:.." << (test_report.syndrv_sram ? "ok" : "failed")
            << "\n +ext:........" << (test_report.syndrv_sram_ext ? "ok" : "failed")
            << endl << endl;

        return result;
    }

    bool ramtest(CtrlModule* rc, 
        int low_addr, 
        int high_addr,
      	fstream& rf, bool two_reads_mode = false)
    {
        std::map<int,uint> test_data;
        bool rv = true;

        Syn_trans syn(rc);
				Syn_trans::Control_reg dummy_creg;
        dummy_creg.cmd = Syn_trans::DSC_IDLE;

        rf << "***Starting ramtest from " << low_addr << " to " << high_addr << '\n';
        for(int i=low_addr; i<=high_addr; i++) {
            uint tdat = rand();
            test_data[i] = tdat;

            rc->write_cmd(i, tdat, 0);
            // dummy writes to compensate execution delay of prev. write_cmd
            for(uint i=0; i<4; i++) syn.set_control_reg(dummy_creg);
       }

        rf << "Initiating readback..." << endl;

        for(int i=low_addr; i<=high_addr; i++) {
            ci_payload res;
            uint8_t expected, received;

            rc->read_cmd(i, 0);
            // dummy writes to compensate execution delay of prev. read_cmd
            for(uint i=0; i<8; i++) syn.set_control_reg(dummy_creg);
            
            if( two_reads_mode ) {
                rc->read_cmd(i, 0);
                rc->get_data(&res);
                //rf << "--- got b: 0x" << hex << setw(8) << setfill('0') << res.data << dec << endl;
            }

						rc->get_data(&res);         
            //rf << "--- got a: 0x" << hex << setw(8) << setfill('0') << res.data << dec << endl;
            
            if( (i>>1) % 2 == 0 ) {
                expected = test_data[i] & 0xff;
                received = res.data & 0xff;
            } else {
                expected = (test_data[i] & 0xff00) >> 8;
                received = (res.data & 0xff00) >> 8;
            }

            if( expected != received ) {
                rf 
                    << "  *** Ramtest failed at address = 0x" << hex << setw(8) << setfill('0') << i
                    << " (expected: 0x" << setw(2) << setfill('0') << (int)expected
                    << ", received: 0x" << setw(2) << setfill('0') << (int)received << dec << ")"
                    << endl;
                rv = false;
            }
        }

        rf << "***Ramtest complete" << endl << endl;
        return rv;
    }

    bool syn_ramtest(Syn_trans& syn, fstream& rf, int row_a = 0, int row_b = 223, bool encr = false)
    {
        return syn_ramtest_templ<Syn_trans::DSC_START_READ, 
                Syn_trans::DSC_READ,
                Syn_trans::DSC_WRITE>(syn, rf, row_a, row_b, encr);
    }

    bool syn_ramtest_dec(Syn_trans& syn, fstream& rf, int row_a = 0, int row_b = 223, bool encr = false)
    {
        return syn_ramtest_templ<Syn_trans::DSC_START_RDEC, 
                Syn_trans::DSC_RDEC,
                Syn_trans::DSC_WDEC>(syn, rf, row_a, row_b, encr);
    }

    template<Syn_trans::Dsc_cmd row_cmd, Syn_trans::Dsc_cmd read_cmd, Syn_trans::Dsc_cmd write_cmd>
    bool syn_ramtest_templ(Syn_trans& syn, fstream& rf, const int row_a = 0, const int row_b = 223, bool encr = false)
    {
        bool result = true;
        Syn_trans::Syn_io sio[224][8];
        Syn_trans::Syn_io sio_check;
        Syn_trans::Control_reg creg;

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.encr = encr;
        creg.continuous = false;
        creg.newcmd = true;
        creg.adr = 4;
        creg.lastadr = 223;
        creg.sel = 4;
        creg.without_reset = true;
        creg.sca = true;
        creg.scc = true;

        for(int row=row_a; row<=row_b; row++) {
            creg.adr = row;

            for(int col_set=0; col_set<8/*8*/; col_set++) {
                creg.sel = col_set;

                for(int i=0; i<4; i++)
                    for(int j=0; j<8; j++)
                        sio[row][col_set][i][j] = random() & 0xf;

                syn.set_synin(sio[row][col_set]);

                creg.cmd = write_cmd;
                syn.set_control_reg(creg);
                syn.wait_while_busy();
            }
        }

        for(int row=row_a; row<=row_b; row++) {
            creg.adr = row;
            creg.cmd = row_cmd;
            syn.set_control_reg(creg);
            syn.wait_while_busy();

            for(int col_set=0; col_set<8/*8*/; col_set++) {
                creg.sel = col_set;
                creg.cmd = read_cmd;
                syn.set_control_reg(creg);
                syn.wait_while_busy();

                syn.get_synout(sio_check);

                for(int i=0; i<4; i++) {
                    for(int j=0; j<8; j++) {
                        if( sio[row][col_set][i][j] != sio_check[i][j] ) {
                            rf << "Read/write test on synapse weight memory failed at "
                                << dec << "row: " << row << " col_set: " << col_set 
                                << " slice: " << i << " column: " << j
                                << " (expected: " << hex << (int)sio[row][col_set][i][j]
                                << ", got: " << (int)sio_check[i][j] << ")"
                                << dec << endl;
                            result = false;
                        }
											//rf << "." << flush;
                    }
                }
            }

            creg.adr = row;
            creg.cmd = Syn_trans::DSC_CLOSE_ROW;
            syn.set_control_reg(creg);
            syn.wait_while_busy();
        }

        return result;
    }


	bool test_spl1_loopback(bool from_fpga, uint loops, fstream& rf){
		bool result = true;
		uint64_t systime = 0xffffffff;
		uint neuron, rec_neuron;
		uint64_t jtag_sendpulse = 0;
		uint64_t jtag_recpulse = 0;

		// full floating gate controller memory test
		rf << endl << "########## TEST SPL1 LOOPBACK via " << (from_fpga?"FPGA":"JTAG") << " ##########" << endl;
		
		//jtag->HICANN_set_test(0);
		jtag->HICANN_set_layer1_mode(0x55);
	
		nc->nc_reset();		
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
						
		spc->write_cfg(0x55ff);
			
		srand(randomseed);
		
		for(uint ch=0;ch<8;ch+=2){
			uint errors = 0;
			for(uint loop=0;loop<loops;loop++){
				uint nrn = rand()%64;

				jtag->HICANN_read_systime(systime);
  			neuron = (ch << 6) | nrn;
				jtag_sendpulse = (neuron<<15) | (systime+1000)&0x7fff; 
		
				if(from_fpga){
					fc->sendSingleL2Pulse(0,jtag_sendpulse);
					fc->getFpgaRxData(jtag_recpulse);
				} else {
					jtag->HICANN_set_test(0x2);
					jtag->HICANN_set_double_pulse(0);
					jtag->HICANN_start_pulse_packet(jtag_sendpulse);
					jtag->HICANN_get_rx_data(jtag_recpulse);
				}
		
				rec_neuron = (jtag_recpulse>>15)&0x1ff;
				if(neuron+64 != rec_neuron){
					rf << "ERROR: sent 0x" << hex << neuron << ", got 0x" << rec_neuron << 
			    		  " | RAW: sent 0x" << jtag_sendpulse << ", got 0x" << jtag_recpulse <<endl;
					result = false;
					errors++;
				}
			}
			rf << "###SPL1 Loopback Channel " << ch << ", ERRORS: " << errors << endl;
		}
  	return result;
	}
				
	
	bool report_current(fstream& rf){
		if(power_supply == NULL){
			cerr << "invalid pointer to PowerSupply class!" << endl;
			return false;
		}
		
		istringstream buffer(power_supply->GetState());
		bool res;
		buffer >> res;
		rf << "########## Check Power Consumption ##########" << endl;
		rf << "Power supply output status: " << res << endl;
		if(!res){
			rf << "---> Turning off power supply and clearing protection status..." << endl;
			cerr << "---> Turning off power supply and clearing protection status..." << endl;
			power_supply->PowerOff();
			power_supply->ClearProt();
			power_supply->SetCurrProt(1, true);
			power_supply->SetCurrProt(2, true);
			power_supply->SetCurrProt(3, true);
			return false;
		}
		for(uint c=1;c<4;c++){
			rf << "Current in CH " << c << ": " << power_supply->GetCurrent(c);
		}
		rf << endl;
		return true;
	}


void write_summary(fstream& resf){
	resf << endl << "########## SUMMARY: ##########" << endl;
	resf << "Current draw:           " << test_current << endl;
	resf << "JTAG ID:                " << test_id << endl;
	resf << "FPGA Init:              " << test_fpga_hicann_init << endl;
	resf << "SPL1 loopback via FPGA: " << test_spl1_fpga << endl;
	resf << "SPL1 loopback via JTAG: " << test_spl1_jtag << endl;
	resf << "Switch config memory:   " << test_sw << endl;
	resf << "Repeater config memory: " << test_rep << endl;
	resf << "FG controller memory:   " << test_fg << endl;
	resf << "Synapse Block 0 memory: " << test_synblk0 << endl;
	resf << "Synapse Block 1 memory: " << test_synblk1 << endl;
}

  
	// ######################## main test method #########################
	bool test() 
  {
    if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
      cout << "tm_wafertest: ERROR: object 'chip' in testmode not set, abort" << endl;
      return 0;
    }
    if (!jtag) {
      cout << "tm_wafertest: ERROR: object 'jtag' in testmode not set, abort" << endl;
      return 0;
    }

	string ps_dev = "/dev/ttyUSB0";
	string ct_dev = "/dev/ttyUSB1";
  
    uint hicannr = 0;
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
    hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
		
	// use FPGA
	fc = (FPGAControl*) chip[0];
		
    // get pointers to control modules
    spc = &hc->getSPL1Control();
    nc  = &hc->getNC();

		// ############## start test options loop ############
    char c;
    bool cont=true;
    do{

      cout << "Select test option:" << endl;
      cout << "1: reset jtag " << endl;
      cout << "2: perform wafer test" << endl;
      cout << "3: fully test single HICANN (no tester access)" << endl;
      cout << "4: read channel current" << endl;
      cout << "5: quick test single HICANN (no tester and power supply access)" << endl;
      cout << "6: test 8 HICANNs in one reticle's JTAG chain (SystemPCB setup!)" << endl;
      cout << "t: test new software features (changes)..." << endl;
      cout << "p: test power supply remote control" << endl;
      cout << "c: power-cycle iboard (and possibly FPGA-board)-supply" << endl;
      cout << "x: exit" << endl;

      if (argv_options->options.size()){
        // process command line option and exit.
        cont=false;

        if(argv_options->options[0].string_key == "opt"){
          c = argv_options->options[0].value.front()[0];
          log(Logger::INFO) << "Executing option " << c << flush;
        } else {
          log(Logger::ERROR) << "First option MUST read 'opt' and give testmode command" << flush;
          return false;
        }
      } else {
        // wait for keybord input and stay in loop
        cin >> c;
      }


      switch(c){
		
      case '1':{
      	jtag->reset_jtag();
      } break;
			
      case '4':{
				string result = "";
				string command = " \n";
				
				s_port_ps = new Serial();
				s_port_ps->setSer( ps_dev );
				int ps = s_port_ps->StartComm();
				cout << "Serial::StartComm: " << ps << endl;

				// instantiate PowerSupply class
				power_supply = new PowerSupply(s_port_ps);
				result = "";
				command = "*idn?\n";
				cout << "testing..." << endl;
				result = power_supply->Command(command);
				cout << "result: " << result << endl;

				uint ch;
				cout << "Which channel (1-3)? "; cin >> ch;
				
				stringstream ss;
				ss << ":CHAN" << ch << ":MEAS:CURR?\n";
				string s = ss.str();
				cout << "GetCurrent: " << s << endl;
	
				*s_port_ps << s;
  
				string response;
				*s_port_ps >> response;
				cout << "GetCurrent response: " << response;
				
				s_port_ps->CloseComm();
				delete s_port_ps;
				delete power_supply;
      } break;
			
      case '2':{
				uint hs_testpckts = 10000;
				uint hs_err_threshold = 5;
				uint loops = 1;
				uint synramloops = 1;
				uint spl1loops = 100;

				// backup comm object in case hs not working
				S2C_JtagPhys* s2c_jtag;
				Stage2Comm* local_comm;

				// get ARQ comm pointer, if available
				S2C_JtagPhys2Fpga * hs_comm = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);
				assert(hs_comm);
	
				vector<string> tmp;
				string col, row;
				
				// instantiate serial ports
				s_port_ct = new Serial();
				s_port_ct->setSer( ct_dev );
				int sc = s_port_ct->StartComm();
				cout << "Serial::StartComm: " << sc << endl;

				s_port_ps = new Serial();
				s_port_ps->setSer( ps_dev );
				int ps = s_port_ps->StartComm();
				cout << "Serial::StartComm: " << ps << endl;

				// instantiate ChipTest class
				chip_test = new ChipTests(s_port_ct);
				string result = "";
				string command = " \n";
				cout << "testing..." << endl;
				command = "ReportKernelVersion \n";
				result = chip_test->Command(command);
				cout << "result: " << result << endl;

				// instantiate PowerSupply class
				power_supply = new PowerSupply(s_port_ps);
				result = "";
				command = "*idn?\n";
				cout << "testing..." << endl;
				result = power_supply->Command(command);
				cout << "result: " << result << endl;

				// setup power suppply
				cout << "setting up power supply..." << fflush;
				//power_supply->PowerOff();
				iboardPowerDown();
				power_supply->Clear();
				power_supply->ClearProt();
				
 				//power_supply->SetVoltage(1, 13.80);
 				//power_supply->SetVoltage(2, 10.00);
 				//power_supply->SetVoltage(3,  6.00);
 				//power_supply->SetCurrent(1, 0.350);
 				//power_supply->SetCurrent(2, 2.000);
 				//power_supply->SetCurrent(3, 0.250);
				
				power_supply->SetCurrProt(1, true);
				power_supply->SetCurrProt(2, true);
				power_supply->SetCurrProt(3, true);
				cout << "done." << endl;

				// move chuck to separation and start at the first reticle in the wafer map.
				command = "MoveChuckSeparation \n";
				result = chip_test->Command(command);
				
				command = "StepNextDie 0 0 0 \n";
//				command = "StepNextDie 1 8 0 \n";
				cout << "Sending command " << command << " to serial port..." << endl;
				result = chip_test->Command(command);
				cout << "PROBENT replied " << result << endl;
				
				// get wafer map position from tester
				boost::split(tmp, result, boost::is_any_of(":"));
 				string posall = tmp.back();
				boost::split(tmp, posall, boost::is_any_of(" "));
				col = tmp[0]; if(col.empty()) col = "emptycol";
				row = tmp[1]; if(row.empty()) row = "emptyrow";

				// now loop over all reticles:
				bool end_of_wafer = false;
				bool last_hc_defect = false;
				uint cons_defect_hicanns = 0;
				while(!end_of_wafer){
					// loop over HICANNs:
//					for(uint h=1;h<9;h++){
					for(uint h=8;h>0;h=h-1){
						if(cons_defect_hicanns == 8){
							cout << "*******************************************************************" << endl;
							cout << "*** Cancelling wafer test due to 8 consecutive HICANNs failing! ***" << endl;
							cout << "*******************************************************************" << endl;
							end_of_wafer = true;
							break;
						}
						
						// reset local comm pointers - make shure to delete corr. objects!
						s2c_jtag = NULL;
						local_comm = NULL;

						// retrieve pointers to ctrl classes in case they have been overwritten
						hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
						fc = (FPGAControl*) chip[0];
						spc = &hc->getSPL1Control();
						nc  = &hc->getNC();

						test_current = noexec;
						test_id = noexec;
						test_fpga_hicann_init = noexec;
						test_spl1_fpga = noexec;
						test_spl1_jtag = noexec;
						test_sw = noexec;
						test_rep = noexec;
						test_fg = noexec;
						test_synblk0 = noexec;
						test_synblk1 = noexec;

						// open report file
						fstream resf;
						stringstream filename;
						filename << "wafer_results/digital_test_" << col << "_" << row << "_" << h << ".txt";
						resf.open(filename.str().c_str(), fstream::out | fstream::trunc);

						cout << "----------------------------------" << endl;
						cout << "Testing Reticle X" << col << " Y" << row << ", HICANN #" << h << endl;
						cout << "----------------------------------" << endl;

						command = "MoveChuckSeparation \n";
						result = chip_test->Command(command);
						
						stringstream ss;
						ss << "StepChuckSubsite " << h << " \n";
						command = ss.str();
						cout << "Sending command " << command << " to serial port..." << endl;
						result = chip_test->Command(command);
						cout << "PROBENT replied " << result << endl;
					
						command = "MoveChuckContact \n";
						result = chip_test->Command(command);

						// turn on power
						power_supply->Clear();
						power_supply->ClearProt();
						//power_supply->PowerOn();
						iboardPowerUp();
						
						// check current
						if(!report_current(resf)){
							resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
							command = "MoveChuckSeparation \n";
							result = chip_test->Command(command);
							test_current = fail;
							if(last_hc_defect) cons_defect_hicanns++;
							last_hc_defect = true;						
							write_summary(resf); continue;
						} test_current = pass;
						
						////// check current on 1.8V and possibly "scratch" a little to improve...
						////double cur_18;
						////istringstream( power_supply->GetCurrent(3) ) >> cur_18;
						////cout << "Current on 1.8V: " << cur_18 << "A" << endl;
						////resf << "Current on 1.8V: " << cur_18 << "A" << endl;
						
						////if(cur_18 < 0.14){  // scratch...
							////resf << endl << "*** Scratching to improve contact... ***" << endl << endl;
							////power_supply->PowerOff();
							////command = "MoveChuckAlign \n";
							////result = chip_test->Command(command);
							////command = "MoveChuckContact \n";
							////result = chip_test->Command(command);
							////power_supply->PowerOn();
						////}

						// reset FPGA and HICANN
						jtag->reset_jtag();
						fc->reset();

						// perform tests...
						if(!test_jtag_id(resf)){
							cout << "JTAG ID could not be read. Quit this HICANN and continue..." << endl;
							//power_supply->PowerOff();
							iboardPowerDown();
							command = "MoveChuckSeparation \n";
							result = chip_test->Command(command);
							test_id = fail;
							if(last_hc_defect) cons_defect_hicanns++;
							last_hc_defect = true;						
							write_summary(resf); continue;
						} test_id = pass;
				
						last_hc_defect = false;
						cons_defect_hicanns = 0;
 						
						// try initializing FPGA communication
		 				cout << "Initializing FPGA-HICANN Gigabit connection... " << flush;
		 				resf << "Initializing FPGA-HICANN Gigabit connection... " << flush;
		 				test_fpga_hicann_init = (hc->GetCommObj()->Init(hc->addr()) == Stage2Comm::ok) ? pass : fail;
		 				cout << test_fpga_hicann_init << endl;
		 				resf << test_fpga_hicann_init << endl;
		
						// test high-speed comm
		 				cout << "Testing FPGA-HICANN Gigabit connection... " << flush;
		 				resf << "Testing FPGA-HICANN Gigabit connection... " << flush;
						uint hs_errors = hs_comm->test_transmission(comm->jtag2dnc(hc->addr()), hs_testpckts);
						cout << hs_errors << " errors in " << hs_testpckts << " packets " << endl;
						resf << hs_errors << " errors in " << hs_testpckts << " packets " << endl;
		
						// carry on JTAG-only in case of too many high-speed errors
						if(test_fpga_hicann_init != pass || hs_errors > hs_err_threshold){
							s2c_jtag = new S2C_JtagPhys(comm->getConnId(), jtag);
							local_comm = static_cast<Stage2Comm*>(s2c_jtag);
							// construct control objects
							fc = new FPGAControl(local_comm,0);
							hc = new HicannCtrl(local_comm,0);
							// get pointers to control modules
							spc = &hc->getSPL1Control();
							nc  = &hc->getNC();
		
							// reset FPGA and HICANN
							jtag->reset_jtag();
							fc->reset();
		
							cout << "Try JTAG Init() ..." ; resf << "Try JTAG Init() ..." ;
							if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
								cout << "ERROR: JTAG Init failed, continue with next HICANN..." << endl;
								resf << "ERROR: JTAG Init failed, continue with next HICANN..." << endl;
								//power_supply->PowerOff();
								iboardPowerDown();
								command = "MoveChuckSeparation \n";
								result = chip_test->Command(command);
								write_summary(resf); break;
							}
							cout << "Init() ok" << endl; resf << "Init() ok" << endl;
						}
		
						// ********** now continue testing using selected comm model or JTAG, resp. **********						
		
						//cout << "Testing   SPl1 loopback ... " << flush;
						//test_spl1_jtag = test_spl1_loopback(false, spl1loops, resf) ? pass : fail;
						//cout << test_spl1_jtag << endl;
						//// check current
						//if(!report_current(resf)){
							//resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
							//command = "MoveChuckSeparation \n";
							//result = chip_test->Command(command);
							//write_summary(resf); break;
						//}
						cout << "Testing   Switch ram... " << flush;
						hc->GetCommObj()->Init(hc->addr());
						test_sw = test_switchram(loops, resf) ? pass : fail;
						cout << test_sw << endl;
						// check current
						if(!report_current(resf)){
							resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
							command = "MoveChuckSeparation \n";
							result = chip_test->Command(command);
							write_summary(resf); break;
						}
						cout << "Testing Repeater ram... " << flush;
						hc->GetCommObj()->Init(hc->addr());
						test_rep = test_repeaterram(loops, resf) ? pass : fail;
						cout << test_rep << endl;
						// check current
						if(!report_current(resf)){
							resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
							command = "MoveChuckSeparation \n";
							result = chip_test->Command(command);
							write_summary(resf); break;
						}
						cout << "Testing Floating Gate Controller ram... " << flush;
						test_fg = test_fgram(loops, resf) ? pass : fail;
						cout << test_fg << endl;
						// check current
						if(!report_current(resf)){
							resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
							command = "MoveChuckSeparation \n";
							result = chip_test->Command(command);
							write_summary(resf); break;
						}
						// skip synapse memory tests in case previous tests all failed
						if(test_spl1_jtag==fail && test_sw==fail && test_rep==fail && test_fg==fail){
							cout << "Skipping time consuming synapse memory test due to previous errors..." << endl;
							resf << "Skipping time consuming synapse memory test due to previous errors..." << endl;
							//power_supply->PowerOff();
							iboardPowerDown();
							command = "MoveChuckSeparation \n";
							result = chip_test->Command(command);
							write_summary(resf); break;
						} else {
							cout << "Testing  Syn Block 0... " << flush;
							test_synblk0 = test_block(hc, 0, loops, synramloops, loops, resf) ? pass : fail;
							cout << test_synblk0 << endl;
							// check current
							if(!report_current(resf)){
								resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
								command = "MoveChuckSeparation \n";
								result = chip_test->Command(command);
								write_summary(resf); break;
							}
							cout << "Testing  Syn Block 1... " << flush;
							test_synblk1 = test_block(hc, 1, loops, synramloops, loops, resf) ? pass : fail;
							cout << test_synblk1 << endl;
							// check current
							if(!report_current(resf)){
								resf << "*** ERROR: Cancelling current test due to excessive current draw! ***" << endl;
								command = "MoveChuckSeparation \n";
								result = chip_test->Command(command);
								write_summary(resf); break;
		 					}
						}

						// turn off power
						//power_supply->PowerOff();
						iboardPowerDown();
						
						command = "MoveChuckSeparation \n";
						result = chip_test->Command(command);
						
						write_summary(resf);
						
						if(s2c_jtag){
							delete fc;
							delete hc;
							delete s2c_jtag;
						}
					}//for
					
 					// goto next reticle and exit on end of wafer map
 					command = "StepNextDie \n";
 					cout << "Sending command " << command << " to serial port..." << endl;
 					result = chip_test->Command(command);
 					cout << "PROBENT replied " << result << endl;
 
 					boost::split(tmp, result, boost::is_any_of(":"));
  					string ReturnCode = tmp.front();
  					cout << "Return code = " << ReturnCode << "\n";
  					if ( ReturnCode == "713" || ReturnCode == "703" ) {
  					  cerr << "End of wafer map reached!" << endl;
  					  end_of_wafer = true;
  					  break;
  					}
 					// get wafer map position from tester
 					boost::split(tmp, result, boost::is_any_of(":"));
  					string posall = tmp.back();
 					boost::split(tmp, posall, boost::is_any_of(" "));
 					col = tmp[0];
 					row = tmp[1];
					
 					//// goto previous reticle and exit on position 0/0
 					//command = "StepFailedBack \n";
 					//cout << "Sending command " << command << " to serial port..." << endl;
 					//result = chip_test->Command(command);
 					//cout << "PROBENT replied " << result << endl;
 
					//command = "ReadMapPosition \n";
					//cout << "Command: " << command << endl;
					//result = chip_test->Command(command);
					//cout << "Result: " << result << endl << endl;

 					//// get wafer map position from result string
 					//boost::split(tmp, result, boost::is_any_of(":"));
  					//string posall = tmp.back();
 					//boost::split(tmp, posall, boost::is_any_of(" "));
 					//col = tmp[0];
 					//row = tmp[1];
					
					int col_int, row_int;
					istringstream(col) >> col_int;
					istringstream(row) >> row_int;
					
					if(col_int == -2 && row_int == 0){
						end_of_wafer = true;
						break;
					}

				}//while(1)

				s_port_ps->CloseComm();
				s_port_ct->CloseComm();
      } break;
			
      case '3':{
				uint hs_testpckts = 1000;
				uint hs_err_threshold = 5;
				uint loops = 1;
				uint synramloops = 1;
				uint spl1loops = 100;

				// backup comm opject in case hs not working
				S2C_JtagPhys* s2c_jtag = NULL;
				Stage2Comm* local_comm = NULL;

				// retrieve pointers to ctrl classes in case they have been overwritten
				hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+hicannr];
				fc = (FPGAControl*) chip[0];
				spc = &hc->getSPL1Control();
				nc  = &hc->getNC();

				// now test HICANN:
				test_current = noexec;
				test_id = noexec;
				test_fpga_hicann_init = noexec;
				test_spl1_fpga = noexec;
				test_spl1_jtag = noexec;
				test_sw = noexec;
				test_rep = noexec;
				test_fg = noexec;
				test_synblk0 = noexec;
				test_synblk1 = noexec;

				// get ARQ comm pointer, if available
				S2C_JtagPhys2Fpga * hs_comm = dynamic_cast<S2C_JtagPhys2Fpga*>(comm);
				assert(hs_comm);
	
				// open report file
				fstream resf;
				stringstream filename;
				filename << "wafer_results/digital_test_single.txt";
				resf.open(filename.str().c_str(), fstream::out | fstream::trunc);

				// reset FPGA and HICANN
				jtag->reset_jtag();
				fc->reset();

				// perform tests...
				if(!test_jtag_id(resf)){
					cout << "JTAG ID could not be read. Quit this HICANN and continue..." << endl;
					//power_supply->PowerOff();
					test_id = fail;
					write_summary(resf); break;
				} test_id = pass;
				
				// try initializing FPGA communication
 				cout << "Initializing FPGA-HICANN Gigabit connection... " << flush;
 				resf << "Initializing FPGA-HICANN Gigabit connection... " << flush;
 				test_fpga_hicann_init = (hc->GetCommObj()->Init(hc->addr()) == Stage2Comm::ok) ? pass : fail;
 				cout << test_fpga_hicann_init << endl;
 				resf << test_fpga_hicann_init << endl;

				// test high-speed comm
 				cout << "Testing FPGA-HICANN Gigabit connection... " << flush;
 				resf << "Testing FPGA-HICANN Gigabit connection... " << flush;
				uint hs_errors = hs_comm->test_transmission(comm->jtag2dnc(hc->addr()), hs_testpckts);
				cout << hs_errors << " errors in " << hs_testpckts << " packets " << endl;
				resf << hs_errors << " errors in " << hs_testpckts << " packets " << endl;

				// carry on JTAG-only in case of too many high-speed errors
				if(test_fpga_hicann_init != pass || hs_errors > hs_err_threshold){
					s2c_jtag = new S2C_JtagPhys(comm->getConnId(), jtag);
					local_comm = static_cast<Stage2Comm*>(s2c_jtag);
					// construct control objects
					fc = new FPGAControl(local_comm,0);
					hc = new HicannCtrl(local_comm,0);
					// get pointers to control modules
					spc = &hc->getSPL1Control();
					nc  = &hc->getNC();

					// reset FPGA and HICANN
					jtag->reset_jtag();
					fc->reset();

					cout << "Try JTAG Init() ..." ; resf << "Try JTAG Init() ..." ;
					if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
						cout << "ERROR: JTAG Init failed, continue with next HICANN..." << endl;
						resf << "ERROR: JTAG Init failed, continue with next HICANN..." << endl;
						//power_supply->PowerOff();
						write_summary(resf); break;
					}
					cout << "Init() ok" << endl; resf << "Init() ok" << endl;
				}

				// ********** now continue testing using **********						

				//cout << "Testing   SPl1 loopback ... " << flush;
				//test_spl1_jtag = test_spl1_loopback(false, spl1loops, resf) ? pass : fail;
				//cout << test_spl1_jtag << endl;

				cout << "Testing   Switch ram... " << flush;
				test_sw = test_switchram(loops, resf) ? pass : fail;
				cout << test_sw << endl;

				cout << "Testing Repeater ram... " << flush;
				test_rep = test_repeaterram(loops, resf) ? pass : fail;
				cout << test_rep << endl;

				cout << "Testing Floating Gate Controller ram... " << flush;
				test_fg = test_fgram(loops, resf) ? pass : fail;
				cout << test_fg << endl;

				// skip synapse memory tests in case previous tests all failed
				if(test_spl1_jtag==fail && test_sw==fail && test_rep==fail && test_fg==fail){
					cout << "Skipping time consuming synapse memory test due to previous errors..." << endl;
					resf << "Skipping time consuming synapse memory test due to previous errors..." << endl;
					//power_supply->PowerOff();
					write_summary(resf); break;
				} else {
					cout << "Testing  Syn Block 0... " << flush;
					test_synblk0 = test_block(hc, 0, loops, synramloops, loops, resf) ? pass : fail;
					cout << test_synblk0 << endl;

					cout << "Testing  Syn Block 1... " << flush;
					test_synblk1 = test_block(hc, 1, loops, synramloops, loops, resf) ? pass : fail;
					cout << test_synblk1 << endl;
				}
				
				write_summary(resf);
				
				if(s2c_jtag){
					delete fc;
					delete hc;
					delete s2c_jtag;
				}
      } break;
			
      case '5':{
				test_current = noexec;
				test_id = noexec;
				test_fpga_hicann_init = noexec;
				test_spl1_fpga = noexec;
				test_spl1_jtag = noexec;
				test_sw = noexec;
				test_rep = noexec;
				test_fg = noexec;
				test_synblk0 = noexec;
				test_synblk1 = noexec;

				uint loops = 6;
				uint synramloops = 3;
				uint spl1loops = 512;
				
				// perform tests...
				fstream resf;
				resf.open("digital_test_single.txt", fstream::out | fstream::trunc);
				
				// Reset HICANN and JTAG
				jtag->reset_jtag();
				jtag->FPGA_set_fpga_ctrl(1);
				jtag->reset_jtag();
				cout << "Try Init() ..." ; resf << "Try Init() ..." ;
				if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
					cout << "ERROR: Init failed, continue with next HICANN..." << endl;
					resf << "ERROR: Init failed, continue with next HICANN..." << endl;
					write_summary(resf);
				}
				cout << "Init() ok" << endl; resf << "Init() ok" << endl;

				cout << "Testing   SPl1 loopback via JTAG... " << flush;
				test_spl1_jtag = test_spl1_loopback(false, spl1loops, resf) ? pass : fail;
				cout << test_spl1_jtag << endl;

				cout << "Testing   Switch ram... " << flush;
				hc->GetCommObj()->Init(hc->addr());
				test_sw = test_switchram(loops, resf) ? pass : fail;
				cout << test_sw << endl;

				cout << "Testing Repeater ram... " << flush;
				hc->GetCommObj()->Init(hc->addr());
				test_rep = test_repeaterram(loops, resf) ? pass : fail;
				cout << test_rep << endl;

				cout << "Testing Floating Gate Controller ram... " << flush;
				test_fg = test_fgram(loops, resf) ? pass : fail;
				cout << test_fg << endl;

 				// skip synapse memory tests in case previous tests all failed
 				if(test_spl1_jtag==fail || test_sw==fail || test_rep==fail || test_fg==fail){
 					cout << "Skipping time consuming synapse memory test due to previous errors..." << endl;
 					resf << "Skipping time consuming synapse memory test due to previous errors..." << endl;
 					write_summary(resf);
 				} else {
 					cout << "Testing  Syn Block 0... " << flush;
 					test_synblk0 = test_block(hc, 0, loops, synramloops, loops, resf) ? pass : fail;
 					cout << test_synblk0 << endl;
 
 					cout << "Testing  Syn Block 1... " << flush;
 					test_synblk1 = test_block(hc, 1, loops, synramloops, loops, resf) ? pass : fail;
 					cout << test_synblk1 << endl;
 				}
 				
				write_summary(resf);
				
      } break;
			
      case '6':{
				uint loops = 6;
				uint synramloops = 3;
				uint spl1loops = 512;
				time_t rawtime;
				
				// results file
				fstream resf;
				resf.open("digital_test_one_reticle.txt", fstream::out | fstream::trunc);
				
				delete fc;
				delete hc;
				// loop over all HICANNs in reticle:
				for(uint h=0;h<8;h++){
					time(&rawtime);
					resf << endl << "########################" << endl;
					resf         << "# Testing HICANN Nr. " << h << " at " << asctime(localtime(&rawtime));
					resf         << "########################" << endl << endl;

					test_current = noexec;
					test_id = noexec;
					test_fpga_hicann_init = noexec;
					test_spl1_fpga = noexec;
					test_spl1_jtag = noexec;
					test_sw = noexec;
					test_rep = noexec;
					test_fg = noexec;
					test_synblk0 = noexec;
					test_synblk1 = noexec;

					// ********** testing using direct JTAG access **********						
					S2C_JtagPhys* s2c_jtag = new S2C_JtagPhys(comm->getConnId(), jtag);
					Stage2Comm *local_comm = static_cast<Stage2Comm*>(s2c_jtag);
					// construct FPGA object
					fc = new FPGAControl(local_comm,0);
					// construct HICANN object
					hc = new HicannCtrl(local_comm,h);

					// get pointers to control modules
					spc = &hc->getSPL1Control();
					nc  = &hc->getNC();

					// Set HICANN JTAG address, reset HICANN and JTAG
					jtag->set_hicann_pos(h);
					jtag->reset_jtag();
					jtag->FPGA_set_fpga_ctrl(1);
					jtag->reset_jtag();
					log(Logger::INFO) << "Try Init() ..." ; resf << "Try Init() ..." ;
					if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
						log(Logger::INFO) << "ERROR: Init failed, continue with next HICANN..." << endl;
						resf << "ERROR: Init failed, continue with next HICANN..." << endl;
						continue;
					}
					log(Logger::INFO) << "Init() ok" << endl; resf << "Init() ok" << endl;

					log(Logger::INFO) << "Testing   SPl1 loopback via JTAG... " << flush;
					test_spl1_jtag = test_spl1_loopback(false, spl1loops, resf) ? pass : fail;
					log(Logger::INFO) << test_spl1_jtag << endl;

					log(Logger::INFO) << "Testing   Switch ram... " << flush;
					hc->GetCommObj()->Init(hc->addr());
					test_sw = test_switchram(loops, resf) ? pass : fail;
					log(Logger::INFO) << test_sw << endl;

					log(Logger::INFO) << "Testing Repeater ram... " << flush;
					hc->GetCommObj()->Init(hc->addr());
					test_rep = test_repeaterram(loops, resf) ? pass : fail;
					log(Logger::INFO) << test_rep << endl;

					log(Logger::INFO) << "Testing Floating Gate Controller ram... " << flush;
					test_fg = test_fgram(loops, resf) ? pass : fail;
					log(Logger::INFO) << test_fg << endl;

					// skip synapse memory tests in case previous tests all failed
					if(test_spl1_jtag==fail || test_sw==fail || test_rep==fail || test_fg==fail){
						log(Logger::INFO) << "Skipping time consuming synapse memory test due to previous errors..." << endl;
						resf << "Skipping time consuming synapse memory test due to previous errors..." << endl;
						continue;
					} else {
						log(Logger::INFO) << "Testing  Syn Block 0... " << flush;
						test_synblk0 = test_block(hc, 0, loops, synramloops, loops, resf) ? pass : fail;
						log(Logger::INFO) << test_synblk0 << endl;

						log(Logger::INFO) << "Testing  Syn Block 1... " << flush;
						test_synblk1 = test_block(hc, 1, loops, synramloops, loops, resf) ? pass : fail;
						log(Logger::INFO) << test_synblk1 << endl;
					}
					
					write_summary(resf);
					
					delete s2c_jtag;
					delete fc;
					delete hc;
				}//for
				
      } break;
			
	case 'p':{
		char test;
		string command, result;

		s_port_ps = new Serial();
		s_port_ps->setSer( ps_dev );
		int ps = s_port_ps->StartComm();
		//cout << "Serial::StartComm: " << ps << endl;

		// instantiate PowerSupply class
		power_supply = new PowerSupply(s_port_ps);
		result = "";
		command = "*idn?\n";
		cout << "testing..." << endl;
		result = power_supply->Command(command);
		cout << "result: " << result << endl;

		cout << "press enter to turn IBoard on"; cin >> test;
		iboardPowerUp();
		
		cout << "press enter to turn reset on"; cin >> test;
		comm->set_fpga_reset(jtag->get_ip(), true, true, true, true, true);

		cout << "press enter to turn reset off"; cin >> test;
		comm->set_fpga_reset(jtag->get_ip(), false, false, false, false, false);
		
		cout << "press enter to turn IBoard off"; cin >> test;
		iboardPowerDown();
		cout << "press enter to turn IBoard on"; cin >> test;
		iboardPowerUp();

		s_port_ps->CloseComm();
	} break;

      case 'c':{
				string result = "";
				string command = " \n";
				
				s_port_ps = new Serial();
				s_port_ps->setSer( ps_dev );
				int ps = s_port_ps->StartComm();
				cout << "Serial::StartComm: " << ps << endl;

				// instantiate PowerSupply class
				power_supply = new PowerSupply(s_port_ps);
				result = "";
				command = "*idn?\n";
				cout << "testing..." << endl;
				result = power_supply->Command(command);
				cout << "result: " << result << endl;

				cout << "Turning off supply output, waiting 5s..." << endl << flush;
				power_supply->PowerOff();
				sleep(5);
				
				cout << "Turning on supply output" << endl;
				cout << "IMPORTANT: in case FPGA board is also connced, remember to wait for design to load (~25sec)!" << endl << flush;
				power_supply->PowerOn();
				
				s_port_ps->CloseComm();
				delete power_supply;
				delete s_port_ps;
      } break;
			
      case 't':{
				vector<string> tmp;
				string result, command;
				
				// instantiate serial port
				s_port_ct = new Serial();
				s_port_ct->setSer( ct_dev );
				int sc = s_port_ct->StartComm();
				cout << "Serial::StartComm: " << sc << endl;

				// instantiate ChipTest class
				chip_test = new ChipTests(s_port_ct);
				result = "";

				// move chuck to separation and start at the first reticle in the wafer map.
				command = "MoveChuckSeparation \n";
				result = chip_test->Command(command);
				
				command = "StepNextDie 2 1 0 \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;
				
				command = "StepFailedBack \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "ReadMapPosition \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "StepFailedBack \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "ReadMapPosition \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "StepFailedBack \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "ReadMapPosition \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "StepFailedBack \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				command = "ReadMapPosition \n";
				cout << "Command: " << command << endl;
				result = chip_test->Command(command);
				cout << "Result: " << result << endl << endl;

				
      } break;
			
      case 'x': cont = false; 
      }

    } while(cont);

    return true;
  }

  void fpga_shutdown(myjtag_full& jtag) {
    printf("\nStopping FPGA Link\n");
    jtag.K7FPGA_stop_fpga_link();
  }

  void hicann_shutdown(myjtag_full& jtag) {
    printf("\nStopping HICANN high-speed part\n");
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

	void iboardPowerUp(void){
		power_supply->SetVoltage(1, 13.8);
	}
	
	void iboardPowerDown(void){
		power_supply->SetVoltage(1, 0.0);
	}

};


class LteeTmWaferTest : public Listee<Testmode>{
public:
  LteeTmWaferTest() : Listee<Testmode>(string("tm_wafertest"),string("automated digtial only wafer tests")){};
  Testmode * create(){return new TmWaferTest();};
};

LteeTmWaferTest ListeeTmWaferTest;

