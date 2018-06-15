#include "syn_trans.h"

#include <assert.h>
#include <cstdlib>

namespace facets {
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
Syn_trans::Syn_trans(CtrlModule* rc)
{
  assert(rc != 0);
  m_rc = rc;
}
//---------------------------------------------------------------------------
Syn_trans::Config_reg
Syn_trans::get_config_reg()
{
  ci_payload res;
  Config_reg rv;

  m_rc->read_cmd(cfg_addr, 0);
  m_rc->get_data(&res);

  rv.unpack(res.data);

  return rv;
}
//---------------------------------------------------------------------------
void
Syn_trans::set_config_reg(const Syn_trans::Config_reg& cfg)
{
  m_rc->write_cmd(cfg_addr, cfg.pack(), 0);
}
//---------------------------------------------------------------------------
Syn_trans::Control_reg
Syn_trans::get_control_reg()
{
  ci_payload res;
  Control_reg rv;

  m_rc->read_cmd(creg_addr, 0);
  m_rc->get_data(&res);
  //cout << "got: 0x" << hex << res.data << dec << endl;

  rv.unpack(res.data);

  return rv;
}
//---------------------------------------------------------------------------
void
Syn_trans::set_control_reg(const Syn_trans::Control_reg& creg)
{
  m_rc->write_cmd(creg_addr, creg.pack(), 0);
}
//---------------------------------------------------------------------------
void
Syn_trans::get_lut(Syn_trans::Lut_data& rv, int pattern)
{
  ci_payload res;

  m_rc->read_cmd(lut_addr+(pattern<<1), 0);
  m_rc->read_cmd(lut_addr+(pattern<<1)+1, 0);

  for(int i=0; i<2; i++) {
    m_rc->get_data(&res);
    for(int j=0; j<8; j++)
      rv[j+i*8] = (res.data & (0xf << (28-4*j))) >> (28-4*j);
  }
}
//---------------------------------------------------------------------------
void
Syn_trans::set_lut(const Lut_data& data, int pattern)
{
  uint32_t p;

  for(int i=0; i<2; i++) {
    p = 0;
    for(int j=0; j<8; j++) {
      p |= (data[j+i*8] & 0xf) << (28-4*j);
    }

    m_rc->write_cmd(lut_addr+(pattern<<1)+i, p, 0);
  }
}
//---------------------------------------------------------------------------
void
Syn_trans::set_synin(const Syn_io& synin)
{
  uint32_t buf;

  for(int i=0; i<4; i++) {
    buf = 0;
    for(int j=0; j<8; j++)
      buf |= (synin[i][j] & 0xf) << (28-4*j);

    m_rc->write_cmd(synin_addr + i, buf, 0);
  }
}
//---------------------------------------------------------------------------
void
Syn_trans::get_synout(Syn_io& synout)
{
  // uint32_t buf;
  ci_payload res;

  for(int i=0; i<4; i++)
    m_rc->read_cmd(synout_addr + i, 0);

  for(int i=0; i<4; i++) {
    m_rc->get_data(&res);

    for(int j=0; j<8; j++)
      synout[i][j] = (res.data & (0xf << (28-4*j))) >> (28-4*j);
  }
}
//---------------------------------------------------------------------------
void
Syn_trans::set_synrst(const Syn_corr& synrst)
{
  uint32_t buf;

  for(int i=0; i<2; i++) {
    buf = 0;

    for(int j=0; j<4; j++) {
      buf |= ((synrst[j][i] & 0xff) << (j*8));
    }

    m_rc->write_cmd(synrst_addr + i, buf, 0);
  }
}
//---------------------------------------------------------------------------
void
Syn_trans::get_synrst(Syn_corr& synrst)
{
  ci_payload res;

  for(int i=0; i<2; i++)
    m_rc->read_cmd(synrst_addr + i, 0);

  for(int i=0; i<2; i++) {
    m_rc->get_data(&res);

    for(int j=0; j<4; j++)
      synrst[j][i] = (res.data & (0xff << (j*8))) >> (j*8);
  }
}
//---------------------------------------------------------------------------
void
Syn_trans::wait_while_busy()
{
  ci_payload res;

  do {
    m_rc->read_cmd(busy_addr, 0);
    m_rc->get_data(&res);
  } while( res.data & 0x7 );
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
uint32_t
Syn_trans::Config_reg::pack() const
{
  uint32_t rv;

  rv = 0;

  // packing patterns
  for(int i=0; i<4; i++) {
    rv |= ((pattern[0] >> i) & 1) << i*2;
    rv |= ((pattern[1] >> i) & 1) << (i*2+1);
  }

  // packing delays
  rv |= (endel & 0xf) << 8;
  rv |= (predel & 0xf) << 12;

  // global enables
  rv |= (gen[0] ? 1 : 0) << 16;
  rv |= (gen[1] ? 1 : 0) << 17;
  rv |= (gen[2] ? 1 : 0) << 18;
  rv |= (gen[3] ? 1 : 0) << 19;

  // dll reset
  rv |= (dllresetb[0] ? 1 : 0) << 20;
  rv |= (dllresetb[1] ? 1 : 0) << 21;

  // delay values
  rv |= (oedel & 0xf) << 22;
  rv |= (wrdel & 0x3) << 26;

  return rv;
}
//---------------------------------------------------------------------------
void
Syn_trans::Config_reg::unpack(const uint32_t& v)
{
  oedel = (v & (0x1e00000)) >> 22;
  wrdel = (v & (0x6000000)) >> 26;

  for(int i=0; i<4; i++) {
    pattern[0] |= (v & (0x1 << (i*2))) >> i;
    pattern[1] |= (v & (0x1 << (i*2+1))) >> (i+1);
  }

  endel = (v & 0xf00) >> 8;
  predel = (v & 0xf000) >> 12;

  gen[0] = (v & 0x10000);
  gen[1] = (v & 0x20000);
  gen[2] = (v & 0x40000);
  gen[3] = (v & 0x80000);

  dllresetb[0] = v & 0x100000;
  dllresetb[1] = v & 0x200000;
}
//---------------------------------------------------------------------------
void
Syn_trans::Config_reg::randomize()
{
  uint32_t tmp = random();
  unpack(tmp);
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
uint32_t
Syn_trans::Control_reg::pack() const
{
  uint32_t rv;

  rv = 0;

  rv |= cmd & 0xf;
  rv |= (encr ? 1 : 0) << 4;
  rv |= (continuous ? 1 : 0) << 5;
  rv |= (newcmd ? 1 : 0) << 6;
  rv |= (adr & 0xff) << 8;
  rv |= (lastadr & 0xff) << 16;
  rv |= (sel & 0x7) << 24;
  rv |= (without_reset ? 1 : 0) << 27;
  rv |= (scc ? 1 : 0) << 28;
  rv |= (sca ? 1 : 0) << 29;

  return rv;
}
//---------------------------------------------------------------------------
void
Syn_trans::Control_reg::unpack(const uint32_t& v)
{
  cmd = static_cast<Syn_trans::Dsc_cmd>(v & 0xf);
  encr = v & 0x10;
  continuous = v & 0x20;
  newcmd = v & 0x40;
  adr = (v & 0xff00) >> 8;
  lastadr = (v & 0xff0000) >> 16;
  sel = (v & 0x7000000) >> 24;
  without_reset = (v & 0x8000000);
  scc = (v &          0x10000000);
  sca = (v &          0x20000000);

  //cout << "scc: " << scc << " , sca : " << sca << endl;
}
//---------------------------------------------------------------------------
void
Syn_trans::Control_reg::randomize()
{
  unpack(random());
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
}
