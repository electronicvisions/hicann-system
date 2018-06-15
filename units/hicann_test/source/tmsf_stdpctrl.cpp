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
#include "syn_trans.h"
#include <queue>
#include <assert.h>

//functions defined in getch.c
extern "C" {
    int getch(void);
    int kbhit(void);
}

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

class TmSFStdpCtrl : public Testmode /*, public RamTestIntfUint*/ {
protected:
    virtual string ClassName() {stringstream ss; ss << "tmsf_stdpctrl"; return ss.str();}
public:

    HicannCtrl *hc; 
//  LinkLayer<ci_payload,ARQPacketStage2> *ll;
    CtrlModule *rc;
    L1SwitchControl *l1c;
    
    // -----------------------------------------------------    
    // OCP constants
    // -----------------------------------------------------    

    int synblock;
    string rname;
    int ocp_base;
    int mem_size;

    ci_data_t rcvdata;
    ci_addr_t rcvaddr;


    void select() {     
        switch(synblock) {
        case 0 : rname="Top";       ocp_base = OcpStdpCtrl_t.startaddr; mem_size=0x7fff; break;
        case 1 : rname="Bottom";    ocp_base = OcpStdpCtrl_b.startaddr; mem_size=0x7fff; break;
        }
    }

    uint read(uint adr){
        rc->read_cmd(adr,0);
        rc->get_data(rcvaddr,rcvdata);
        return rcvdata;
    }
    class ad read_ad(uint adr){
        rc->read_cmd(adr,0);
        int res = rc->get_data(rcvaddr,rcvdata);
        if( res == 0 )
            cout << "get_data failed" << endl;
        else
            cout << "get_data ok" << endl;
            
        return ad(rcvaddr,rcvdata);     
    }
    
    void write(uint adr, uint data){
            rc->write_cmd(adr, data,0);
    }       

    //generate repeater address by permuting address bits (HICANN error!!!)
//  uint repadr(uint adr) {
//      uint ra=0;
//      for(int i=0;i<7;i++)ra|= (adr& (1<<i))?(1<<(6-i)):0;
//      return ra;
//  }


    static const int TAG             = 1; //STDP uses second tag

    static const int MEM_BASE        = 0;
//  static const int MEM_SIZE        = 10; depends, see select()
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
            log << "ERROR: object 'chip' in testmode not set, abort" << endl;
            return 0;
        }
    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
        hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
//      ll = hc->tags[TAG]; 
//      ll->SetDebugLevel(3); // no ARQ deep debugging
		l1c = &hc->getLC_cl();

        log << "Try Init() ..." ;

        if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
            log << "ERROR: Init failed, abort" << endl;
            return 0;
        }

        log << "Init() ok" << endl;

        // ----------------------------------------------------
        // do ramtest of switch matrices
        // ----------------------------------------------------

        bool result = true;

        srand(1);

        log(Logger::INFO) <<"start StdpCtrl access:"<<rname<<endl;

        log(Logger::INFO) << "testing block 1" << endl;
        result = test_block(hc, 1) && result;
        log(Logger::INFO) << "testing block 0" << endl;
        result = test_block(hc, 0) && result;

        if( result )
            cout << "Both blocks tested successfully." << endl;
        else
            cout << "At least one block failed" << endl;

        return result;
    }


    bool test_block(HicannCtrl* hc, int block)
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
        } test_report;

        static const int reg_loops = 10; 
        static const int synapse_ramtest_loops = 2;
        static const int syndrv_ramtest_loops = 10;
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
        select();

        std::auto_ptr<CtrlModule> rc(new CtrlModule(hc, TAG, ocp_base, 0x7fff));
        log << "StdpCtrl base adr: 0x" << hex << ocp_base << endl;

        bool result = true;
        Syn_trans syn(rc.get());
        Syn_trans::Config_reg cfg;
        Syn_trans::Control_reg creg;
        Syn_trans::Lut_data lut_a, lut_b, lut_ab;
        Syn_trans::Syn_corr synrst;

        // check config register
        test_report.cfg = true;
        for(int i=0; i<reg_loops; i++) {
            Syn_trans::Config_reg cfg_check;
            cfg.randomize();
            syn.set_config_reg(cfg);
            cfg_check = syn.get_config_reg();

            if( !(cfg == cfg_check ) ) {
                cout << "read/write check of config register failed" << endl;
                result = false;
                test_report.cfg = false;
            }
        }

       log(Logger::INFO) << "Random test of config register (" 
           << reg_loops << "x) passed" << endl;

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
                cout << "read/write check of control register failed" << endl;
                cout << "  got: 0x" << hex << creg_check.pack()
                    << "  expected: 0x" << creg.pack() << dec << endl;
                result = false;
                test_report.creg = false;
            }
        }

       log(Logger::INFO) << "Random test of control register (" 
           << reg_loops << "x) passed" << endl;

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
                    cout << "Read/write test of LUT failed at index " << dec << j << endl;
                    result = false;
                    test_report.lut = false;
                }
            }
        }

       log(Logger::INFO) << "Random test of STDP LUTs (" 
           << reg_loops << "x) passed" << endl;

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
                        cout << "Read/write test of SYNRST failed at pattern " << dec << i
                            << " slice " << j << endl;
                        test_report.synrst = false;
                    }
                }
            }
        }

        log(Logger::INFO) << "Random test of correlation reset register (" 
           << reg_loops << "x) passed" << endl;
        

        log(Logger::INFO) << "Performing random correlation reset commands ("
            << reg_loops << "x)" << endl;

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

        log(Logger::INFO) << "Ramtest on four locations of every syndriver SRAM block"
            << endl;

        test_report.syndrv_sram_ext = ramtest(rc.get(), Syn_trans::syndrv_ctrl,
                Syn_trans::syndrv_ctrl+0xdf, 
                true);

        test_report.syndrv_sram_ext = test_report.syndrv_sram_ext
            && ramtest(rc.get(), Syn_trans::syndrv_drv,
                Syn_trans::syndrv_drv+0xdf, 
                true);
        
        test_report.syndrv_sram_ext = test_report.syndrv_sram_ext 
            && ramtest(rc.get(), Syn_trans::syndrv_gmax,
                Syn_trans::syndrv_gmax+0xdf, 
                true);

        //ramtest(rc.get(), Syn_trans::syndrv_gmax + 0x100,
                //Syn_trans::syndrv_gmax+0x101,
                //true);

        log(Logger::INFO) << "Starting synapse weights ramtest (" 
           << synapse_ramtest_loops << "x)" << endl;
        test_report.syn_weight = true;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_weight = syn_ramtest(syn, 
                    syn_ramtest_start, 
                    syn_ramtest_stop)
                && test_report.syn_weight;

        log(Logger::INFO) << "Starting synapse decoder addresses ramtest ("
           << synapse_ramtest_loops << "x)" << endl;
        test_report.syn_dec = true;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_dec = syn_ramtest_dec(syn,
                    syn_ramtest_start,
                    syn_ramtest_stop) 
                && test_report.syn_dec;

        // read again with correlation activated
        log(Logger::INFO) << "Starting synapse weights ramtest with correlation readout ("
           << synapse_ramtest_loops << "x)" << endl;
        test_report.syn_weight_encr = true;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_weight_encr = syn_ramtest(syn, syn_ramtest_start, syn_ramtest_stop, true)
                && test_report.syn_weight_encr;

        log(Logger::INFO) << "Starting synapse decoder addresses ramtest with correlation readout ("
           << synapse_ramtest_loops << "x)" << endl;
        test_report.syn_dec_encr = true;
        for(int i=0; i<synapse_ramtest_loops; i++)
            test_report.syn_dec_encr = syn_ramtest_dec(syn, syn_ramtest_start, syn_ramtest_stop, true)
                && test_report.syn_dec_encr;

        //test_report.syndrv_sram = ramtest(rc.get(), syndrv_ramtest_start,//Syn_trans::syndrv_ctrl,
                //syndrv_ramtest_stop,//Syn_trans::syndrv_gmax+0xff, 
                //true);
        log(Logger::INFO) << "Starting synapse driver SRAM ramtest on CTRL block ("
           << syndrv_ramtest_loops << "x)" << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = ramtest(rc.get(), Syn_trans::syndrv_ctrl,
                    Syn_trans::syndrv_ctrl+syndrv_block_size-1, 
                    true)
                && test_report.syndrv_sram;

        log(Logger::INFO) << "Starting synapse driver SRAM ramtest on DRV block ("
           << syndrv_ramtest_loops << "x)" << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = test_report.syndrv_sram && ramtest(rc.get(), Syn_trans::syndrv_drv,
                    Syn_trans::syndrv_drv+syndrv_block_size-1, 
                    true)
                && test_report.syndrv_sram;
        
        log(Logger::INFO) << "Starting synapse driver SRAM ramtest on GMAX block ("
           << syndrv_ramtest_loops << "x)" << endl;
        for(int i=0; i<syndrv_ramtest_loops; i++)
            test_report.syndrv_sram = test_report.syndrv_sram && ramtest(rc.get(), Syn_trans::syndrv_gmax,
                    Syn_trans::syndrv_gmax+syndrv_block_size-1, 
                    true)
                && test_report.syndrv_sram;
        //ramtest(rc.get(), 0x100, 

        //result &= ramtest(0x4100, 0x4103, false, false);  // ramtest on LUT
        //result &= ramtest(Syn_trans::syndrv_ctrl, Syn_trans::syndrv_gmax+0xff, true, false);   // provisional ramtest on syndrv SRAM

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

        //log(Logger::ERROR) << "<<<< STDP report >>>>"
        cout << "<<<< STDP report >>>>"
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
            << endl;

        return result;
    }

    bool ramtest(CtrlModule* rc, 
        int low_addr, 
        int high_addr,
        bool two_reads_mode = false)
    {
        std::map<int,uint> test_data;
        bool rv = true;

        cout << "Starting ramtest from " << low_addr << " to " 
            << high_addr << '\n';
        for(int i=low_addr; i<=high_addr; i++) {
            uint tdat = rand();
            test_data[i] = tdat;

            rc->write_cmd(i, tdat, 0);
            // dummy writes to compensate execution delay of prev. write_cmd
            l1c->write_cfg(0, 0);
            l1c->write_cfg(0, 0);
        }

        cout << "Initiating readback..." << endl;

        for(int i=low_addr; i<=high_addr; i++) {
            ci_payload res;
            uint8_t expected, received;

            rc->read_cmd(i, 0);
            rc->get_data(&res);         
            cout << "--- got a: 0x" << hex << setw(8) << setfill('0') << res.data << dec << endl;
            
            if( two_reads_mode ) {
                rc->read_cmd(i, 0);
                rc->get_data(&res);
                cout << "--- got b: 0x" << hex << setw(8) << setfill('0') << res.data << dec << endl;
            }

            if( (i>>1) % 2 == 0 ) {
                expected = test_data[i] & 0xff;
                received = res.data & 0xff;
            } else {
                expected = (test_data[i] & 0xff00) >> 8;
                received = (res.data & 0xff00) >> 8;
            }

            if( expected != received ) {
                cout 
                    << "  *** Ramtest failed at address = 0x" << hex << setw(8) << setfill('0') << i
                    << " (expected: 0x" << setw(2) << setfill('0') << (int)expected
                    << ", received: 0x" << setw(2) << setfill('0') << (int)received << dec << ")"
                    << endl;
                rv = false;
            }
        }

        cout << "Ramtest complete" << endl;
        return rv;
    }

    bool syn_ramtest(Syn_trans& syn, int row_a = 0, int row_b = 223, bool encr = false)
    {
        return syn_ramtest_templ<Syn_trans::DSC_START_READ, 
                Syn_trans::DSC_READ,
                Syn_trans::DSC_WRITE>(syn, row_a, row_b, encr);
    }

    bool syn_ramtest_dec(Syn_trans& syn, int row_a = 0, int row_b = 223, bool encr = false)
    {
        return syn_ramtest_templ<Syn_trans::DSC_START_RDEC, 
                Syn_trans::DSC_RDEC,
                Syn_trans::DSC_WDEC>(syn, row_a, row_b, encr);
    }

    template<Syn_trans::Dsc_cmd row_cmd, Syn_trans::Dsc_cmd read_cmd, Syn_trans::Dsc_cmd write_cmd>
    bool syn_ramtest_templ(Syn_trans& syn, const int row_a = 0, const int row_b = 223, bool encr = false)
    {
        bool result = true;
        Syn_trans::Syn_io sio[224][8];
        Syn_trans::Syn_io sio_check;
        Syn_trans::Control_reg creg;

        creg.cmd = Syn_trans::DSC_IDLE;
        creg.encr = encr;
        creg.continuous = false;
        creg.newcmd = true;
        creg.adr = 0;
        creg.lastadr = 223;
        creg.sel = 0;
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
                            cout << "Read/write test on synapse weight memory failed at "
                                << dec << "row: " << row << " col_set: " << col_set 
                                << " slice: " << i << " column: " << j
                                << " (expected: " << hex << (int)sio[row][col_set][i][j]
                                << ", got: " << (int)sio_check[i][j] << ")"
                                << dec << endl;
                            result = false;
                        }
			cout << "." << flush;
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

};


class LteeTmSFStdpCtrl : public Listee<Testmode>{
public:
    LteeTmSFStdpCtrl() : Listee<Testmode>(string("tmsf_stdpctrl"), string("test stdpctrl synapse access etc")){};
    Testmode * create(){return new TmSFStdpCtrl();};
};

LteeTmSFStdpCtrl ListeeTmSFStdpCtrl;

