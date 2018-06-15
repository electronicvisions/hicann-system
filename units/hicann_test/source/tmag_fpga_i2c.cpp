// Company         :   kip
// Author          :   Stefan Philipp            
// E-Mail          :   philipp@kip.uni-heidelberg.de
//                    			
// Filename        :   plltestag.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   12/2008
// Last Change     :   
// by              :   
//------------------------------------------------------------------------


//example file for a custom testmode implementation

#include "common.h"

//#ifdef NCSIM
//#include "systemc.h"
//#endif

#include "s2comm.h"
#include "linklayer.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" 

// jtag
#include "common.h"
//#include "verification_control.h"
//#include "s2c_jtag.h"

//only if needed
#include "ctrlmod.h"
#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class

using namespace std;

//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace facets;

// testmode used for faraday pll testing of real HICANN chip.
class TmFpgaI2CAG : public Testmode{

public:

	vector<bool> get_data(int data)
	{
		vector<bool> d;
		d.clear();
		for(int i=0;i<16;i++) d.push_back(((data >> i) & 1) == 1);
		return d;
	}

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "fpgai2cag: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "fpgai2cag: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

//		cout << "reset test logic" << endl;
//		jtag->reset_jtag(); //had to be removed to allow vol/voh adjustments

		char c;
		bool cont=true;
		do{

			cout << "Select test option:" << endl;
			cout << "1: send HICANN reset pulse" << endl;
			cout << "2: perform jtag bypass/loopback test" << endl;
			cout << "7: set DAC via I2C" << endl;
			cout << "8: set and verify I2C prescaler reg" << endl;
			cout << "9: read ADC" << endl;
			cout << "a: read ADC using S2C interface" << endl;
			cout << "b: set DAC using S2C interface" << endl;
			cout << "c: read FPGA and HICANN JTAG ID" << endl;
			cout << "d: test JTAG bulk read using bypass regs" << endl;
			cout << "e: set vol via i2c DAC" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){
		
			case '1':{
				jtag->reset_jtag(); //moved jtag reset to hicann reset
				jtag->FPGA_set_fpga_ctrl(1);
			} break;
			
			case '2':{
/*				uint test=0x12345678;
				uint veri=0x0f0f0f0f;
		
				jtag->bypass(test, 32, veri);
				cout << "read jtag bypass: 0x" << hex << veri << endl;
*/			} break;
			
			case '7':{
				uint dacbaseaddr = 0x98;
				
				uint dacaddr = 0;
				uint dacchnl = 0;
				uint dacval  = 0;
				
				cout << "Which DAC (0/1)? ";         cin >> dacaddr;
				cout << "Which DAC channel (0-3)? "; cin >> dacchnl;
				cout << "New DAC Value (0x0-0xffff)? "; cin >> hex >> dacval;
				
				uint dacfulladdr = dacbaseaddr | (dacaddr << 1); // see DAC datasheet
				uint dacfullcmd  = (1 << 4) | (dacchnl << 1);
				
				// enable I2C master
				jtag->FPGA_i2c_setctrl(jtag->ADDR_CTRL, 0x80);
				
				jtag->FPGA_i2c_byte_write(dacfulladdr, 1, 0, 0); cout << "dacfulladdr = 0x" << hex << dacfulladdr << endl;
				jtag->FPGA_i2c_byte_write(dacfullcmd,  0, 0, 0); cout << "dacfullcmd = 0x" << hex << dacfullcmd << endl;
				jtag->FPGA_i2c_byte_write((dacval>>8), 0, 0, 0); cout << "(dacval>>8) = 0x" << hex << (dacval>>8) << endl;
				jtag->FPGA_i2c_byte_write( dacval,     0, 1, 0); cout << "dacval = 0x" << hex << dacval << endl;
			} break;

			case '8':{
				uint presc=0;
				
				cout << "HEX value for presc (16 bit)? "; cin >> hex >> presc;
				
				jtag->FPGA_i2c_setctrl(jtag->ADDR_PRESC_M, (presc>>8));
				jtag->FPGA_i2c_setctrl(jtag->ADDR_PRESC_L, presc);
				
				unsigned char rprescl=0, rprescm=0;
				jtag->FPGA_i2c_getctrl(jtag->ADDR_PRESC_L, rprescl);
				jtag->FPGA_i2c_getctrl(jtag->ADDR_PRESC_M, rprescm);
				
				cout << "read back : 0x" << setw(2) << hex << ((((uint)rprescm)<<8)|(uint)rprescl) << endl;
			} break;

			case '9':{
				uint adcbaseaddr = 0x40;
				
				uint adcaddr = 0;
				uint adcchnl = 0;
				uint adcval  = 0;
				unsigned char adcvall = 0, adcvalm = 0;
				
				cout << "Which ADC (3/4)? ";         cin >> adcaddr;
				cout << "Which ADC channel (0-7)? "; cin >> adcchnl;
				
				uint adcfulladdr = adcbaseaddr | (adcaddr << 1); // see DAC datasheet
				uint adcfullcmd  = (8 << 4) | (adcchnl << 4);
				
				// enable I2C master
				jtag->FPGA_i2c_setctrl(jtag->ADDR_CTRL, 0x80);
				
				jtag->FPGA_i2c_byte_write(adcfulladdr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(adcfullcmd,  0, 1, 0);

				// add read bit and re-send I2C address
				adcfulladdr |= 0x1;
				jtag->FPGA_i2c_byte_write(adcfulladdr, 1, 0, 0);

				jtag->FPGA_i2c_byte_read(0, 0, 0, adcvalm);
				cout << "adcvalm = 0x" << hex << static_cast<uint>(adcvalm) << endl;
				
				jtag->FPGA_i2c_byte_read(0, 1, 1, adcvall);
				cout << "adcvall = 0x" << hex << static_cast<uint>(adcvall) << endl;
				
				adcval = (static_cast<uint>(adcvalm)<<8) | static_cast<uint>(adcvall);
				adcval >>= 2;     // 10 Bit ADC -> only 2 Bit of msb-Byte are used.
				adcval &=  0x3ff; 
				cout << "Full ADC value: 0x" << hex << adcval << endl;
			} break;

			case 'a':{
				SBData adcval(SBData::ivddbus);
				comm->readAdc(adcval);
				cout << "read ADC voltage: " << adcval.ADvalue() << endl;
			} break;
			
			case 'b':{
				char d;
				cout<<"Select DAC channel (1: vdd25, 2: vol, 3: voh, 4: vddbus, x: quit)?";
				cin>>d;
				double dv;

				switch (d){
					case '1':{
						cout << "Value for VDD25-DAC (DAC 0, channel 0)? ";
						cin >> dv;
					
						SBData dacval(SBData::vdd25, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '2':{
						cout << "Value for VOL-DAC (DAC 0, channel 0)? ";
						cin >> dv;
						
						SBData dacval(SBData::vol, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '3':{
		 				cout << "Value for VOH-DAC (DAC 0, channel 0)? ";
						cin >> dv;
					
						SBData dacval(SBData::voh, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '4':{
						cout << "Value for VDDBUS-DAC (DAC 0, channel 0)? ";
						cin >> dv;
						
						SBData dacval(SBData::vddbus, dv);
						comm->setDac(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					 }break;
					default: break;
				}
			} break;
			
			case 'c':{
				uint64_t id=0;
				
				jtag->read_id(id, jtag->pos_fpga);
				cout << "FPGA ID: 0x" << hex << id << endl;

				jtag->read_id(id, jtag->pos_hicann);
				cout << "HICANN ID: 0x" << hex << id << endl;
				
			} break;
			
			case 'd':{
				uint bypass = 0xf0f0f0f0;
				uint64_t result = 0;
				
				// set both devices to bypass  
				jtag->set_jtag_instr_chain(jtag->CMD_BYPASS, jtag->pos_fpga);

				jtag->set_get_jtag_data_chain(bypass,result,32,0);
				
				cout << "Result = 0x" << hex << (result>>2) << endl;

			} break;
			
			case 'e':{
				double dv;
				cout << "Value for VOL-DAC (DAC 0, channel 0)? ";
				cin >> dv;
				
				SBData dacval(SBData::vol, dv);
				comm->setDac(dacval);
				cout << "set DAC voltage: " << dacval.ADvalue() << endl;
			} break;
			case 'x':cont=false;
			}

		} while(cont);

		return true;
	};
};


class LteeTmFpgaI2CAG : public Listee<Testmode>{
public:
	LteeTmFpgaI2CAG() : Listee<Testmode>(string("tmag_fpga_i2c"),string("testing access via FPGA")){};
	Testmode * create(){return new TmFpgaI2CAG();};
};

LteeTmFpgaI2CAG ListeeTmFpgaI2CAG;

