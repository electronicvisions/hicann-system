// -*- mode: c++ -*-
// Company           :   tud
// Author            :   hartmann
// E-Mail            :   stephan.hartmann@tu-dresden.de
//
// Filename          :   fpga jtag_cmdbase.h
// Project Name      :   p_facets
// Subproject Name   :   s_fpgaproto
// Description       :   fpga jtag command base
//
// Create Date       :   Tue Feb  1 14:05:49 2011
// Last Change       :   $Date: 2011-06-30 09:24:33 +0200 (Thu, 30 Jun 2011) $
// by                :   $Author: hartmann $
//------------------------------------------------------------
#ifndef __FPGA_JTAG_CMDBASE_H__
#define __FPGA_JTAG_CMDBASE_H__

// FPGA instructions
#define CMD_BYPASS (0x3FF)
#define CMD_IDCODE (0x3C9)
#define CMD_USER1  (0x3C2)
#define CMD_USER2  (0x3C3)
#define CMD_USER3  (0x3E2)
#define CMD_USER4  (0x3E3)

#define IRW 10

template<typename JTAG>
class jtag_cmdbase : public JTAG
{
private:
  unsigned char  jtag_irw;
  unsigned char  jtag_ir_post;
  unsigned char  jtag_ir_pre;
  unsigned char  jtag_dr_post;
  unsigned char  jtag_dr_pre;
  unsigned char  chain;
  unsigned short jtag_ir;

protected:
  void setJtagInstr(unsigned short command)
  {
    if ( jtag_ir == command )
      return;

    jtag_ir = command;

    /* Go to SELECT-DR-SCAN */
    this->jtag_write(0,1);

    /* Go to SELECT-IR-SCAN */
    this->jtag_write(0,1);

    /* Go to CAPTURE-IR */
    this->jtag_write(0,0);

    /* Go to SHIFT-IR */
    this->jtag_write(0,0);

    /* Shift Bypass */
    unsigned char i;
    for (i = 0; i < jtag_ir_post; i++)
      this->jtag_write(1,0);

    for (i = 0; i < (IRW + jtag_ir_pre); i++)
      {
        if ( i < IRW )
          {
            this->jtag_write(command & 1, i==(IRW+jtag_ir_pre-1));
            command >>= 1;
          }
        else
          this->jtag_write(1, i==(IRW+jtag_ir_pre-1));
      }

    /* Go to UPDATE-IR */
    this->jtag_write(0,1);

    /* Go to RUN-TEST/IDLE */
    this->jtag_write(0,0);

    this->jtag_execute();
  }

  template<typename T>
  void setJtagData(T wdata, unsigned char width)
  {
    unsigned char i;
    /* Go to SELECT-DR-SCAN */
    this->jtag_write(0,1);

    /* Go to CAPTURE-DR */
    this->jtag_write(0,0);

    /* Go to SHIFT-DR */
    this->jtag_write(0,0);

    for (i = 0; i < (width+jtag_dr_pre); i++)
      {
        if ( i < width )
          {
            this->jtag_write(wdata & 1, i == width+jtag_dr_pre-1);
            wdata >>=1;
          }
        else
          this->jtag_write(0, i == width+jtag_dr_pre-1);
      }

    /* Go to UPDATE-DR */
    this->jtag_write(0,1);

    /* Go to RUN-TEST/IDLE */
    this->jtag_write(0,0);

    this->jtag_execute();
  }

  template<typename T>
  void getJtagData(T& rdata, unsigned char width)
  {
    unsigned char i;
    /* Go to SELECT-DR-SCAN */
    this->jtag_write(0,1);

    /* Go to CAPTURE-DR */
    this->jtag_write(0,0);

    /* Go to SHIFT-DR */
    this->jtag_write(0,0);

    for ( i = 0; i < jtag_dr_post; i++)
      this->jtag_write(0,0);

    for(i = 0; i < width; i++)
      {
        /* Go to EXIT1-DR */
        this->jtag_write(0, i == (width-1), jtag_lib::TDO_READ);
      }

    /* Go to UPDATE-DR */
    this->jtag_write(0,1);

    /* Go to RUN-TEST/IDLE */
    this->jtag_write(0,0);

    this->jtag_execute();
    this->jtag_read(rdata, width);
  }

  template<typename T, typename U>
  void setGetJtagData(T wdata, U& rdata, unsigned char width)
  {
    unsigned char i;
    /* Go to SELECT-DR-SCAN */
    this->jtag_write(0,1);

    /* Go to CAPTURE-DR */
    this->jtag_write(0,0);

    /* Go to SHIFT-DR */
    this->jtag_write(0,0);

    for ( i = 0; i < jtag_dr_post; i++)
      this->jtag_write(0,0);

    for(  i = 0; i < (width+jtag_dr_pre); i++)
      {
        if ( i < width )
          {
            this->jtag_write(wdata & 1, i == width+jtag_dr_pre-1, jtag_lib::TDO_READ);
            wdata >>=1;
          }
        else
          this->jtag_write(0, i == width+jtag_dr_pre-1);
      }

    /* Go to UPDATE-DR */
    this->jtag_write(0,1);

    /* Go to RUN-TEST/IDLE */
    this->jtag_write(0,0);

    this->jtag_execute();
    this->jtag_read(rdata, width);
  }

  unsigned short getFpgaUserInstr()
  {
    switch ( chain )
      {
      case 2  : return CMD_USER2;
      case 3  : return CMD_USER3;
      case 4  : return CMD_USER4;
      default : return CMD_USER1;
      }
  }

public:
  jtag_cmdbase() : jtag_irw(0),
                   jtag_ir_post(0),
                   jtag_ir_pre(0),
                   jtag_dr_post(0),
                   jtag_dr_pre(0),
                   chain(1),
                   jtag_ir(0)
  {
  }

  bool resetJtag()
  {
    this->jtag_write(0,1,10);

    /* Go to RUN-TEST/IDLE */
    this->jtag_write(0,0);
    this->jtag_execute();

    jtag_ir = 0;
    return true;
  }

  bool initJtag()
  {
    jtag_irw     = IRW;
    jtag_ir      = 0;
    jtag_ir_post = 0;
    jtag_ir_pre  = 0;
    jtag_dr_post = 0;
    jtag_dr_pre  = 0;

    resetJtag();

    return true;
  }

  bool setJtagChain(unsigned ir_post, unsigned ir_pre, unsigned dr_post, unsigned dr_pre)
  {
    jtag_ir_post = ir_post;
    jtag_ir_pre  = ir_pre;
    jtag_dr_post = dr_post;
    jtag_dr_pre  = dr_pre;
    return true;
  }

  bool setFpgaChain(unsigned char chain)
  {
    if ( !chain || chain > 4 )
      return false;
    this->chain = chain;
    return true;
  }

  template<typename T>
  bool readFpgaID(T& rdata)
  {
    setJtagInstr(CMD_IDCODE);
    getJtagData(rdata, 32);
        return true;
  }

  template<typename T>
  bool bypass(T wdata, T& rdata, unsigned char width)
  {
    setJtagInstr(CMD_BYPASS);
    if ( width > 0 )
      setGetJtagData(wdata, rdata, width);
    return true;
  }
};

#endif
