#ifndef SYN_TRANS_H__
#define SYN_TRANS_H__

#include "ctrlmod.h"

namespace facets {

//---------------------------------------------------------------------------
class Syn_trans
{
  public:
    class Config_reg;
    class Control_reg;

    enum Read_pattern {
      CORR_READ_CAUSAL  = 0x8,
      CORR_READ_ACAUSAL = 0x1
    };

    enum Dsc_cmd {
      DSC_IDLE = 0,
      DSC_READ = 1,
      DSC_START_RDEC = 2,
      DSC_WRITE = 3,
      DSC_AUTO = 4,
      DSC_WDEC = 5,
      DSC_RDEC = 6,
      DSC_START_READ = 7,
      DSC_READPATRES = 8,
      DSC_CLOSE_ROW = 9,
      DSC_RST_CORR = 10
    };

    typedef uint8_t Lut_data[16];
    typedef uint8_t Syn_io[4][8];
    typedef uint8_t Syn_corr[4][2];

    // accessed through sramCtrl
    static const unsigned int syndrv_ctrl  = 0x0000;
    static const unsigned int syndrv_drv   = 0x0100;
    static const unsigned int syndrv_gmax  = 0x0200;
    // register area
    static const unsigned int creg_addr    = 0x4000;
    static const unsigned int cfg_addr     = 0x4001;
    static const unsigned int busy_addr    = 0x4002;
    static const unsigned int lut_addr     = 0x4100;
    static const unsigned int synin_addr   = 0x4200;
    static const unsigned int synout_addr  = 0x4300;
    static const unsigned int syncorr_addr = 0x4400;
    static const unsigned int synrst_addr  = 0x4500;

    Syn_trans(CtrlModule* rc);


    Config_reg get_config_reg();
    void set_config_reg(const Config_reg& cfg);
    Control_reg get_control_reg();
    void set_control_reg(const Control_reg& creg);
    void get_lut(Lut_data& rv, int pattern);
    void set_lut(const Lut_data& data, int pattern);
    void set_synin(const Syn_io& synin);
    void get_synout(Syn_io& synout);
    void set_synrst(const Syn_corr& synrst);
    void get_synrst(Syn_corr& synrst);
    void wait_while_busy();

    /*static uint32_t pat_to_bits(const Read_pattern& pat) {
      uint32_t rv;

      switch(pat) {
        case CORR_READ_CAUSAL:   rv = 0x8;   break;
        case CORR_READ_ACAUSAL:  rv = 0x1;   break;
        default:                 rv = 0x0;
      }

      return rv;
    }

    static Read_pattern bits_to_pat(const uint32_t& bits) {
      Read_pattern rv;

      switch(bits) {
        case 0x8:  rv = CORR_READ_CAUSAL;    break;
        case 0x1:  rv = CORR_READ_ACAUSAL;   break;
        default:   rv = CORR_READ_UNKNOWN;
      }

      return rv;
    }*/


  protected:
    CtrlModule* m_rc;
};
//---------------------------------------------------------------------------
class Syn_trans::Config_reg
{
  public:
    Config_reg() {
      dllresetb[0] = false;
      dllresetb[1] = false;
      gen[0] = false;
      gen[1] = false;
      gen[2] = false;
      gen[3] = false;
      predel = 0xf;
      endel  = 0xf;
      pattern[0] = Syn_trans::CORR_READ_CAUSAL;
      pattern[1] = Syn_trans::CORR_READ_ACAUSAL;
    }

    Config_reg(const uint32_t& v) { unpack(v); }

    int oedel;
    int wrdel;
    bool dllresetb[2];
    bool gen[4];
    int predel;
    int endel;
    int pattern[2];
    
    uint32_t pack() const;
    void unpack(const uint32_t& v);
    void randomize();

    bool operator == (const Config_reg& o) const {
      return (dllresetb[0] == o.dllresetb[0])
          && (dllresetb[1] == o.dllresetb[1])
          && (gen[0] == o.gen[0])
          && (gen[1] == o.gen[1])
          && (gen[2] == o.gen[2])
          && (gen[3] == o.gen[3])
          && (predel == o.predel)
          && (endel == o.endel)
          && (pattern[0] == o.pattern[0])
          && (pattern[1] == o.pattern[1])
          && (oedel == o.oedel)
          && (wrdel == o.wrdel);
    }
};
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
class Syn_trans::Control_reg
{
  public:
    Control_reg() {
      sca = true;
      scc = true;
      without_reset = false;
      sel = 0;
      lastadr = 0;
      adr = 0;
      newcmd = false;
      continuous = false;
      encr = false;
      cmd = DSC_IDLE;
    }

    bool sca;
    bool scc;
    bool without_reset;
    int sel;
    int lastadr;
    int adr;
    bool newcmd;
    bool continuous;
    bool encr;
    Dsc_cmd cmd;

    uint32_t pack() const;
    void unpack(const uint32_t& v);
    void randomize();

    bool operator == (const Control_reg& o) const {
      return (sel == o.sel)
          && (lastadr == o.lastadr)
          && (adr == o.adr)
          && (newcmd == o.newcmd)
          && (continuous == o.continuous)
          && (encr == o.encr)
          && (cmd == o.cmd)
          && (without_reset == o.without_reset)
          && (sca == o.sca)
          && (scc == o.scc);
    }
};
//---------------------------------------------------------------------------


}


#endif

