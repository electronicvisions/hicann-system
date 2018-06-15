// Company         :   kip
// Author          :   Holger Zoglauer           
// E-Mail          :   zoglauer@kip.uni-heidelberg.de
//                    			
// Filename        :   tmhz_pwrctrl.cpp
// Project Name    :   p_facets
// Subproject Name :               
//                    			
// Create Date     :   10/2010
//------------------------------------------------------------------------

#ifdef NCSIM
#include "systemc.h"
#endif

#include "common.h"

#include "testmode.h"

//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

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


class TmHZPwrCtrl : public Testmode /*, public RamTestIntfUint*/ {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmjs_repeater"; return ss.str();}
public:

	// power board I2C address
	uint pwraddr = 0x3e;

	char c;
	bool cont=true;
	do{
		cout << "PLL control reg values: ns=0x" << hex << setw(2) << ns << " ms=0x" << ms << " pdn=" << setw(1) << pdn << " frg=" << frg << " tst=" << tst << endl << endl;

		cout << "Select test option:" << endl;
		cout << "1: send HICANN reset pulse" << endl;
		cout << "2: JTAG reset" << endl;
		cout << "3: start monitoring" << endl;
		cout << "4: enable power board" << endl;
		cout << "5: disable power board" << endl;
		cout << "6: calibrate on resistances" << endl;
		cout << "s: set and verify I2C prescaler reg" << endl;
		cout << "x: exit" << endl;

		cin >> c;

		switch(c)
		{
			case '1':
				jtag->FPGA_set_fpga_ctrl(1);
				break;

			case '2':
				cout << "reset test logic" << endl;
				jtag->reset_jtag();
				break;

			case '3':	// start monitoring, check for errors and read error values
				cout << "voltages to enable (in hex):";
				cin >> hex >> ENchoice;
				jtag->FPGA_i2c_byte_write(pwraddr,	1, 0, 0);	// call PIC
				jtag->FPGA_i2c_byte_write(ENchoice,	0, 1, 0);	// enable special voltages

				bool check=true;
				do{
					usleep(1000000);
					jtag->FPGA_i2c_byte_write(pwraddr,	1, 0, 0);	// call PIC
					jtag->FPGA_i2c_byte_write(0x28,		0, 1, 0);	// ask for errors
					pwraddr |= 0x1;									// add read bit and 
					jtag->FPGA_i2c_byte_write(pwraddr,	1, 0, 0);	// resend I2C address
					pwraddr &= 0xFE;								// clear read bit
					jtag->FPGA_i2c_byte_read(0, 0, 0, ERRct_h);		// read upper byte of ERRct
					jtag->FPGA_i2c_byte_read(0, 0, 0, ERRct_l);		// read lower byte of ERRct
					ERRct = ERRct_h * 256 + ERRct_l;				// combine upper and lower byte of ERRct
					cout << "error check, number of errors:" << ERRct << endl;
					if (ERRct == 0){jtag->FPGA_i2c_byte_read(0, 1, 1, ERRct);}	// read zero and close the connection
					else
					{
						jtag->FPGA_i2c_byte_read(0, 0, 0, ERRct_l);	// read zero and continue
						for(int j = 0; j <= ERRct; j++)
						{
							jtag->FPGA_i2c_byte_read(0, 0, 0, voltNo);
							jtag->FPGA_i2c_byte_read(0, 0, 0, Vmon_h);
							jtag->FPGA_i2c_byte_read(0, 0, 0, Vmon_l);
							jtag->FPGA_i2c_byte_read(0, 0, 0, Vfet_h);
							jtag->FPGA_i2c_byte_read(0, 0, 0, Vfet_l);
							jtag->FPGA_i2c_byte_read(0, 0, 0, Ron_h);
							jtag->FPGA_i2c_byte_read(0, 0, 0, Ron_l);
							Vmon = Vmon_h * 255 + Vmon_l;
							Vfet = Vfet_h * 255 + Vfet_l;
							Ron = Ron_h * 255 + Ron_l / 1000;
							cout << "V" << voltNo << "	" << Vmon << " mV	" << Vfet << " mV	" << Ron << " mOhm --> mesured current: " << ((Vmon - Vfet)/Ron) << endl;
						}
						jtag->FPGA_i2c_byte_read(0, 1, 1, ERRct);	// read zero and close the connection
						// for testing: if error occured turn of all voltages and stop monitoring
						jtag->FPGA_i2c_byte_write(pwraddr,	1, 0, 0);	// call PIC
						jtag->FPGA_i2c_byte_write(0xFF,	0, 1, 0);		// disable all voltages and stop ADCs

						check=false;
					}
					if(kbhit()>0)check=false;
				}while(check);

			case '4':
				jtag->FPGA_i2c_byte_write(pwraddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(0xF0,    0, 1, 0);
				break;

			case '5':
				jtag->FPGA_i2c_byte_write(pwraddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(0xFF,    0, 1, 0);
				break;

			case '6':
				jtag->FPGA_i2c_byte_write(pwraddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(0x40,    0, 1, 0);	// start calibration of the ON-resistances
				break;

			case 's':
				uint presc;

				cout << "HEX value for presc (16 bit)? "; cin >> hex >> presc;

				jtag->FPGA_i2c_setctrl(ADDR_PRESC_M, (presc>>8));
				jtag->FPGA_i2c_setctrl(ADDR_PRESC_L, presc);

				unsigned char rprescl=0, rprescm=0;
				jtag->FPGA_i2c_getctrl(ADDR_PRESC_L, rprescl);
				jtag->FPGA_i2c_getctrl(ADDR_PRESC_M, rprescm);

				cout << "read back : 0x" << setw(2) << hex << ((((uint)rprescm)<<8)|(uint)rprescl) << endl;
				break;

			case'x':
				cont=false;
		}

		if(kbhit()>0)cont=false;

	}while(cont);

	return 1;
};


class LteeTmHZPwrCtrl : public Listee<Testmode>{
public:
	LteeTmHZPwrCtrl() : Listee<Testmode>(string("tmhz_pwrctrl"), string("continous r/w/comp to distant mems e.g. switch matrices")){};
	Testmode * create(){return new TmHZPwrCtrl();};
};

LteeTmHZPwrCtrl ListeeTmHZPwrCtrl;

