    // Company         :   kip
    // Author          :   Simon Friedmann
    //                              
    //------------------------------------------------------------------------

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"
#include "hicann_ctrl.h"
    //#include "l1switch_control.h" //layer 1 switch control class
    //#include "linklayer.h"
#include "testmode.h" 
    //#include "ramtest.h"
    //#include "syn_trans.h"
//#include "read_bin.h"
#include "ctrlmod.h"
#include <queue>
#include <vector>
#include <istream>
#include <fstream>
#include <boost/format.hpp>
#include <sys/stat.h>
#include <stdexcept>
#include <assert.h>

#include "s2pp_tools.h"


using namespace facets;


class TmSFHepp
    :   public Testmode 
{
protected:
    virtual string ClassName() {stringstream ss; ss << "tmsf_hepp"; return ss.str();}

    HicannCtrl *hc; 
    CtrlModule *rc;

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
        }

        cout << "Initiating readback..." << endl;

        for(int i=low_addr; i<=high_addr; i++) {
            ci_payload res;
            uint32_t expected, received;

            rc->read_cmd(i, 0);
            rc->get_data(&res);         
            cout << "--- got a: 0x" << hex << setw(8) << setfill('0') << res.data << dec << endl;
            
            if( two_reads_mode ) {
                rc->read_cmd(i, 0);
                rc->get_data(&res);
                cout << "--- got b: 0x" << hex << setw(8) << setfill('0') << res.data << dec << endl;
            }

            //if( (i>>1) % 2 == 0 ) {
                //expected = test_data[i] & 0xff;
                //received = res.data & 0xff;
            //} else {
                //expected = (test_data[i] & 0xff00) >> 8;
                //received = (res.data & 0xff00) >> 8;
            //}
            expected = test_data[i];
            received = res.data;

            if( expected != received ) {
                cout 
                    << "  *** Ramtest failed at address = 0x" << hex << setw(8) << setfill('0') << i
                    << " (expected: 0x" << setw(8) << setfill('0') << (int)expected
                    << ", received: 0x" << setw(8) << setfill('0') << (int)received << dec << ")"
                    << endl;
                rv = false;
            }
        }

        cout << "Ramtest complete" << endl;
        return rv;
    }
    
    bool test_program(CtrlModule* cm) {
        static unsigned int const num_durations = 4;
        static unsigned int const duration_addr = 0xc40/4;
        static unsigned int const success_addr = 0x3e8/4;

        bool rv;

        log(Logger::INFO) << "Executing testprogram" << endl; 

        unsigned int const code_size = s2pp::load_program(cm, "testprogram.raw");
        // pulse reset
        s2pp::core_reset(cm, s2pp::RESET_ON);  // hold reset active and start clock
        s2pp::core_reset(cm, s2pp::RESET_OFF); // release reset

        ci_payload res;
        while( cm->get_data(&res, false) ) {
            cout << "unexpected read data: " << res.data << endl;
        }

        while( !s2pp::sleeping(cm) );

        std::vector<uint32_t> dump(code_size+100);
        cout << "reading back memory image" << endl;
        s2pp::read_block(cm, 0, dump.begin(), dump.end());

        if( dump[success_addr] == 0 )
            rv = false;

        for(unsigned int i=0; i<num_durations; i++) {
            cout << "duration[" << i << "] = 0x" << std::hex 
                << ((static_cast<uint64_t>(dump[duration_addr + i*2]) << 32) | static_cast<uint64_t>(dump[duration_addr + i*2 +1]))
                << std::dec
                << endl;
        }

        ofstream ofs("dump.bin", ios::binary);
        s2pp::write_bin(ofs, dump.begin(), dump.end());

        return rv;
    }
  
    bool l1_switchramtest_tl(CtrlModule* cm, int const size, int const repetition = 1) {
        static uint32_t const l1_tl_addr = 0xe000;

        std::vector<uint32_t> mem(size);
        ci_payload res;
        bool rv = true;

        for(int i=0; i<repetition; i++) {
            for(int j=0; j<size; j++) {
                mem[j] = random() & 0xffff;
                cm->write_cmd(l1_tl_addr + j, mem[j], 0);
            }

            for(int j=0; j<size; j++) {
                cm->read_cmd(l1_tl_addr + j, 0);
                while( cm->get_data(&res) == 0 ) {
#ifdef NCSIM
                    wait(10, SC_NS);
#endif
                }

                if( res.data != mem[j] ) {
                    cerr << "mismatch at " << j << " expected: 0x" 
                        << hex << mem[j]
                        << " got: 0x" << res.data << dec 
                        << endl;
                    rv = false;
                }
            }
        }

        return rv;
    }

    bool test_tag0(CtrlModule* cm_tag_0, CtrlModule* cm_tag_1) {
        static unsigned int const success_addr = 0x114/4;

        bool rv = true;

        log(Logger::INFO) << "Beginning tag0 test" << endl;

        s2pp::core_reset(cm_tag_1, s2pp::RESET_ON | s2pp::CLK_FORCE_OFF);
        unsigned int const code_size = s2pp::load_program(cm_tag_1, "../../../../hepp/source/firmware/switchramtest.raw");
        s2pp::core_reset(cm_tag_1, s2pp::RESET_ON);
        s2pp::core_reset(cm_tag_1, s2pp::RESET_OFF);

        ci_payload res;
        while( cm_tag_1->get_data(&res, false) ) {
            cout << "unexpected read data: " << res.data << endl;
        }

        rv = l1_switchramtest_tl(cm_tag_0, 100, 3);

        while( !s2pp::sleeping(cm_tag_1) );

        std::vector<uint32_t> dump(code_size+100);
        cout << "reading back memory image" << endl;
        s2pp::read_block(cm_tag_1, 0, dump.begin(), dump.end());

        if( dump[success_addr] == 0 )
            rv = false;

        ofstream ofs("dump.bin", ios::binary);
        s2pp::write_bin(ofs, dump.begin(), dump.end());

        return rv;
    }

public:
    // -----------------------------------------------------    
    // test function
    // -----------------------------------------------------    

    bool test() 
    {
        Logger::AlterLevel log_level_alter(Logger::DEBUG0);
        std::stringstream msg;

        if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
            log << "ERROR: object 'chip' in testmode not set, abort" << endl;
            return 0;
        }
        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
        hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
        //ll = hc->tags[TAG]; 
        //ll->SetDebugLevel(3); // no ARQ deep debugging

        log << "Try Init() ..." ;

        if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
            log << "ERROR: Init failed, abort" << endl;
            return 0;
        }

        log << "Init() ok" << endl;

        std::auto_ptr<CtrlModule> cm_tag_1(new CtrlModule(hc->tags[1], 0x0000, 0xffff));
        std::auto_ptr<CtrlModule> cm_tag_0(new CtrlModule(hc->tags[0], 0x0000, 0xffff));

        //
        // start tests
        //


        bool rv = true;

        log << "Performing ramtest on HEPP memory block..." << endl;
        rv = ramtest(cm_tag_1.get(), 0x0000, 0x05ff, false);
        if( !rv ) 
            msg << "ramtest failed with clk_by4\n";

        {  // use clk_by2
            jtag->set_jtag_instr_chain(jtag_cmdbase::CMD_CLK_SEL_BY2, 0);
            jtag->set_jtag_data_chain(1, 1, 0);
        }

        log << "Performing ramtest on HEPP memory block..." << endl;
        rv = ramtest(cm_tag_1.get(), 0x0000, 0x05ff, false);
        if( !rv ) 
            msg << "ramtest failed with clk_by2\n";

        {  // use clk_by4
            jtag->set_jtag_instr_chain(jtag_cmdbase::CMD_CLK_SEL_BY2, 0);
            jtag->set_jtag_data_chain(0, 1, 0);
        }


        //rv = test_program(cm_tag_1.get()) && rv;
        rv = test_tag0(cm_tag_0.get(), cm_tag_1.get()) && rv;


        //
        // start reporting
        //

        if( rv )
            cout << "all good" << endl;
        else {
            cout << "failed" << endl;
            cout << msg << endl;
        }

        return rv;
    }

};


class LteeTmSFHepp : public Listee<Testmode>{
public:
    LteeTmSFHepp() : Listee<Testmode>(string("tmsf_hepp"), string("test HICANN embedded plasticity processor")){};
    Testmode * create(){return new TmSFHepp();};
};

LteeTmSFHepp ListeeTmSFHepp;


// vim: expandtab ts=4 sw=4 softtabstop=4 smarttab:
