// Company         :   kip
// Author          :   Andreas Gruebl          
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_iboardv2.cpp
// Project Name    :   p_facets
// Subproject Name :   HICANN_system            
//                    			
// Create Date     :   09/21/2010
// Last Change     :   
// by              :   
//------------------------------------------------------------------------


#include "common.h"

#include "s2comm.h"
#include "s2ctrl.h"
#include "testmode.h" 

//only if needed
//#include "ctrlmod.h"
//#include "synapse_control.h" //synapse control class
//#include "l1switch_control.h" //layer 1 switch control class

//functions defined in getch.c
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

// testmode used for faraday pll testing of real HICANN chip.
class TmIboardV2 : public Testmode{

public:

	// resistor values on IBoard V2 (to be moved elsewhere...)
        static const double vdd11_r1;
        static const double vdd11_r2;
  
	static const double vdd5_r1;
	static const double vdd5_r2;
	
	static const double vddbus_r1;
	static const double vddbus_r2;
	
	static const double vdd25_r1;
	static const double vdd25_r2;
	
	static const double vol_r1;
	static const double vol_r2;
	
	static const double voh_r1;
	static const double voh_r2;
	
	static const double vref_r1;
	static const double vref_r2;
	
	// inastumentation amplifier external resistors:
	static const double rg_vdd;
	static const double rg_vdddnc;
	static const double rg_vdda;
	static const double rg_vddadnc;
	static const double rg_vdd33;
	static const double rg_vol;
	static const double rg_voh;
	static const double rg_vddbus;
	static const double rg_vdd25;
	static const double rg_vdd5;
	static const double rg_vdd11;
	
	// current measurement resistors
	static const double rm_vdd;
	static const double rm_vdddnc;
	static const double rm_vdda;
	static const double rm_vddadnc;
	static const double rm_vdd33;
	static const double rm_vol;
	static const double rm_voh;
	static const double rm_vddbus;
	static const double rm_vdd25;
	static const double rm_vdd5;
	static const double rm_vdd11;
	
	double v_div (uint r1, uint r2, double v){
		return v * ((double)r2 / ((double)r1 + (double)r2));
	}
	
	double v_noninv (uint r1, uint r2, double v){
		return (1 + (double)r1/(double)r2) * v;
	}
	
	// Set DAC using the desired output voltage as argument
	bool setSupply(SBData value) {
		double vrefdac = 2.5;
		double dacvout = 0;
		uint dacres = 16;
		uint dac0addr = 0x98, dac1addr = 0x9a;

		uint dacaddr, daccmd;
	
		SBData::ADtype t=value.getADtype();
		switch(t){
			case SBData::vol:{
				dacaddr = dac1addr;
				daccmd = 0x14;
				dacvout = value.ADvalue() * (vol_r1 + vol_r2) / vol_r2;
				break;}
			case SBData::voh:{
				dacaddr = dac1addr;
				daccmd = 0x10;
				dacvout = value.ADvalue() * (voh_r1 + voh_r2) / voh_r2;
				break;}
			case SBData::vddbus:{
				dacaddr = dac1addr;
				daccmd = 0x12;
				dacvout = value.ADvalue() * (vddbus_r1 + vddbus_r2) / vddbus_r2;
				break;}
			case SBData::vdd25:{
				dacaddr = dac0addr;
				daccmd = 0x10;
				dacvout = value.ADvalue() / (1 + vdd25_r1/vdd25_r2);
				break;}
			case SBData::vdd5:{
				dacaddr = dac0addr;
				daccmd = 0x12;
				dacvout = value.ADvalue() / (1 + vdd5_r1/vdd5_r2);
				break;}
			case SBData::vdd11:{
				dacaddr = dac0addr;
				daccmd = 0x14;
				dacvout = value.ADvalue() / (1 + vdd11_r1/vdd11_r2);
				break;}
			case SBData::vref_dac:{
				dacaddr = dac0addr;
				daccmd = 0x16;
				dacvout = value.ADvalue() * (vref_r1 + vref_r2) / vref_r2;
				break;}
			default: {
				log(Logger::ERROR) << "Wrong DAC write type specified!";
				return false;
			}
		}	

		// sanity check:
		if(dacvout > vrefdac) {
			log(Logger::ERROR) << "DAC input value larger than vrefdac!";
			return false;
		}

		uint dacval = dacvout / vrefdac * (double)((1<<dacres)-1);
		
		cout << "dacaddr: 0x" << hex << dacaddr << ", daccmd: 0x" << daccmd << ", dacval: 0x" << dacval << endl;
		cout << "Eff. DAC output: " << dacvout << ", dacval: 0x" << hex << dacval << endl;

		// enable I2C master, just to be sure... ;-)
		jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);
		for (int j = 0; j<50;j++){
			jtag->FPGA_i2c_byte_write(dacaddr, 1, 0, 0);
			jtag->FPGA_i2c_byte_write(daccmd,  0, 0, 0);
			jtag->FPGA_i2c_byte_write((dacval>>8), 0, 0, 0);
			jtag->FPGA_i2c_byte_write( dacval,     0, 1, 0);
		}
		return true;
	}


	// read ADC and calculate current
	bool getSupplyCurrent(SBData& result) {
		double vref     = 3.3; // adc reference voltage
		double vref_ina = 1.0; // instrumentation amplifier reference input
		uint adcres     = 10;  // adc resolution
		uint adc1addr   = 0x42, adc2addr = 0x44; // addr already shifted <<1; rw bit added below
	
		uint adcaddr, adccmd, adcval;
		double value=0, curvalue=0;

		double rg=0, rm=0;
		
		SBData::ADtype t=result.getADtype();
		switch(t){
			case SBData::ivdd:        {adcaddr = adc2addr; adccmd = 0x90; rg = rg_vdd;     rm = rm_vdd;     break;}
			case SBData::ivdda:       {adcaddr = adc1addr; adccmd = 0xf0; rg = rg_vdda;    rm = rm_vdda;    break;}
			case SBData::ivdd_dncif:  {adcaddr = adc1addr; adccmd = 0xb0; rg = rg_vdddnc;  rm = rm_vdddnc;  break;}
			case SBData::ivdda_dncif: {adcaddr = adc2addr; adccmd = 0xa0; rg = rg_vddadnc; rm = rm_vddadnc; break;}
			case SBData::ivdd33:      {adcaddr = adc2addr; adccmd = 0x80; rg = rg_vdd33;   rm = rm_vdd33;   break;}
			case SBData::ivol:        {adcaddr = adc1addr; adccmd = 0xa0; rg = rg_vol;     rm = rm_vol;     break;}
			case SBData::ivoh:        {adcaddr = adc1addr; adccmd = 0x90; rg = rg_voh;     rm = rm_voh;     break;}
			case SBData::ivddbus:     {adcaddr = adc1addr; adccmd = 0x80; rg = rg_vddbus;  rm = rm_vddbus;  break;}
			case SBData::ivdd25:      {adcaddr = adc1addr; adccmd = 0xd0; rg = rg_vdd25;   rm = rm_vdd25;   break;}
			case SBData::ivdd5:       {adcaddr = adc1addr; adccmd = 0xc0; rg = rg_vdd5;    rm = rm_vdd5;    break;}
			case SBData::ivdd11:      {adcaddr = adc1addr; adccmd = 0xe0; rg = rg_vdd11;   rm = rm_vdd11;   break;}
			default:
				log(Logger::ERROR) << "Wrong ADC read type specified!";
				return false;
		}
		
		unsigned char adcvall = 0, adcvalm = 0;
				
		// enable I2C master, just to be sure... ;-)
		jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

		// send conversion command
		jtag->FPGA_i2c_byte_write(adcaddr, 1, 0, 0);
		jtag->FPGA_i2c_byte_write(adccmd,  0, 1, 0);

		// add read bit and re-send I2C address
		adcaddr |= 0x1;
		jtag->FPGA_i2c_byte_write(adcaddr, 1, 0, 0);

		// read conversion result
		jtag->FPGA_i2c_byte_read(0, 0, 0, adcvalm);
		jtag->FPGA_i2c_byte_read(0, 1, 1, adcvall);

		//cout << "ADC first byte: 0x" << hex << (uint)adcvalm << ", second byte: 0x" << (uint)adcvall << endl;

		// perform sanity checks:
//		if(adcvalm & 0x8) log(Logger::WARNING) << "ADC Alert Flag set!";
//		if((adcvalm & 0x70) != (adccmd & 0x70)){
//			log(Logger::ERROR) << "Wrong ADC answer channel!";
//			return false;
//		}

		adcval = (static_cast<uint>(adcvalm)<<8) | static_cast<uint>(adcvall);
		adcval >>= 2;     // 10 Bit ADC -> D0 and D1 not used
		adcval &=  0x3ff; // 

		value = (double)adcval / (double)((1<<adcres)-1) * vref;
		// calculate current based on ina circuitry
		curvalue = ((value - vref_ina) / (5.0 + 80000.0 / rg)) / rm;

		result.setADvalue(curvalue);

		//log(Logger::INFO) << "ADC read " << curvalue << "A";

		return true;	
	}

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
		 	cout << "IboardV2: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "IboardV2: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}

//		cout << "reset test logic" << endl;
//		jtag->reset_jtag();

		char c;
		bool cont=true;
		do{
			if(keys.size()!=0){
				c = keys.front().c_str()[0];
				keys.erase(keys.begin());
			} else {
				cout << "Select test option:" << endl;
				cout << "1: send HICANN reset pulse" << endl;
				cout << "2: JTAG reset" << endl;
				cout << "3: read FPGA and HICANN JTAG ID" << endl;
				cout << "8: set and verify I2C prescaler reg" << endl;
				cout << "9: read ADC" << endl;
				cout << "a: read ADC using S2C interface" << endl;
				cout << "b: set DAC using S2C interface" << endl;
				cout << "c: read back DAC channel data" << endl;
				cout << "d: continuously monitor Iboard currents" << endl;
				cout << "e: write to an ADC register" << endl;
				cout << "f: read from an ADC register" << endl;
				cout << "g: set analog mux config" << endl;
				cout << "h: read back analog mux config" << endl;
				cout << "i: clear ADC alert registers" << endl;
				cout << "j: read ADCs' alert status reg" << endl;
				cout << "x: exit" << endl;
				cin >> c;
			}

			switch(c){
		
			case '1':{
				jtag->FPGA_set_fpga_ctrl(1);
			} break;
			
			case '2':{
				cout << "reset test logic" << endl;
				jtag->reset_jtag();
			} break;
			
			case '3':{
				uint64_t id=0;
				
				jtag->read_id(id, jtag->pos_fpga);
				cout << "FPGA ID: 0x" << hex << id << endl;

				jtag->read_id(id, jtag->pos_dnc);
				cout << "DNC ID: 0x" << hex << id << endl;

				jtag->read_id(id, jtag->pos_hicann);
				cout << "HICANN ID: 0x" << hex << id << endl;
				
			} break;
			
			case '8':{
				uint presc;
				
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
				SBData adcval(SBData::ivol);
				getSupplyCurrent(adcval);
				cout << "read ADC voltage: " << adcval.ADvalue() << endl;
			} break;
			
			case 'b':{
				char d;
				if(keys.size()!=0){
					d = keys.front().c_str()[0];
					keys.erase(keys.begin());
				} else {
					cout << "Select DAC channel:" << endl;
					cout << "1: VOL" << endl;
					cout << "2: VOH" << endl;
					cout << "3: VDDBUS" << endl;
					cout << "4: VDD25" << endl;
					cout << "5: VDD5" << endl;
					cout << "6: VDD11" << endl;
					cout << "7: VREF_DAC" << endl;
					cout << "> " << endl;
					cin >> d;
				}

				double dv;
				if(keys.size()!=0){
					dv = atof(keys.front().c_str());
					keys.erase(keys.begin());
				} else {
					cout << "Value for this DAC? ";
					cin >> dv;
				}

				switch (d){
					case '1':{
						SBData dacval(SBData::vol, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '2':{
						SBData dacval(SBData::voh, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '3':{
						SBData dacval(SBData::vddbus, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '4':{
						SBData dacval(SBData::vdd25, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					 }break;
					case '5':{
						SBData dacval(SBData::vdd5, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					}break;
					case '6':{
						SBData dacval(SBData::vdd11, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					 }break;
					case '7':{
						SBData dacval(SBData::vref_dac, dv);
						setSupply(dacval);
						cout << "set DAC voltage: " << dacval.ADvalue() << endl;
					 }break;
					default: break;
				}
			} break;
			
			case 'c':{
				char d;
				if(keys.size()!=0){
					d = keys.front().c_str()[0];
					keys.erase(keys.begin());
				} else {
					cout << "Select DAC channel:" << endl;
					cout << "1: VOL" << endl;
					cout << "2: VOH" << endl;
					cout << "3: VDDBUS" << endl;
					cout << "4: VDD25" << endl;
					cout << "5: VDD5" << endl;
					cout << "6: VDD11" << endl;
					cout << "7: VREF_DAC" << endl;
					cout << "> " << endl;
					cin >> d;
				}

				uint dac0addr = 0x98, dac1addr = 0x9a;
				uint dacaddr, daccmd;
	
				switch (d){
					case '1': {dacaddr = dac1addr; daccmd = 0x14; break;}
					case '2': {dacaddr = dac1addr; daccmd = 0x10; break;}
					case '3': {dacaddr = dac1addr; daccmd = 0x12; break;}
					case '4': {dacaddr = dac0addr; daccmd = 0x10; break;}
					case '5': {dacaddr = dac0addr; daccmd = 0x12; break;}
					case '6': {dacaddr = dac0addr; daccmd = 0x14; break;}
					case '7': {dacaddr = dac0addr; daccmd = 0x16; break;}
					default:  {cout << "***WRONG INPUT***" << endl;break;}
				}

				uint value;
				unsigned char valuem,valuel;
				
				// select channel				
				jtag->FPGA_i2c_byte_write(dacaddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(daccmd,  0, 0, 0);
				
				// re-send address with read bit set
				dacaddr |= 0x1;
				jtag->FPGA_i2c_byte_write(dacaddr, 1, 0, 0);
				
				// 2-byte read
				jtag->FPGA_i2c_byte_read(0, 0, 0, valuem);
				jtag->FPGA_i2c_byte_read(0, 1, 1, valuel);
				
				value = (static_cast<uint>(valuem)<<8) | static_cast<uint>(valuel);
				
				cout << "read from DAC: 0x" << hex << value;
			} break;
			
			case 'd':{
				SBData value;
				
				for(uint i=0;i<1;i++){
					value.setAdc(SBData::ivdd); getSupplyCurrent(value);
					cout << "VDD:        " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdda); getSupplyCurrent(value);
					cout << "VDDA:       " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdd_dncif); getSupplyCurrent(value);
					cout << "VDD_DNCIF:  " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdda_dncif); getSupplyCurrent(value);
					cout << "VDDA_DNCIF: " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdd33); getSupplyCurrent(value);
					cout << "VDD33:      " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivol); getSupplyCurrent(value);
					cout << "VOL:        " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivoh); getSupplyCurrent(value);
					cout << "VOH:        " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivddbus); getSupplyCurrent(value);
					cout << "VDDBUS:     " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdd25); getSupplyCurrent(value);
					cout << "VDD25:      " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdd5); getSupplyCurrent(value);
					cout << "VDD5:       " << setprecision(4) << value.ADvalue() << endl;
					value.setAdc(SBData::ivdd11); getSupplyCurrent(value);
					cout << "VDD11:      " << setprecision(4) << value.ADvalue() << endl;
					
					cout << flush;
					//usleep(500000);
					
					// reset cursor:
					//system("clear");
				}
			} break;
			
			case 'e':{
				uint adc,adcaddr;
				uint reg, value;
				
				cout << "which ADC (0/1)? "; cin >> adc;
				if(adc==0) adcaddr=0x42; else adcaddr=0x44;
				
				cout << "address (4bit hex)? "; cin >> hex >> reg;
				cout << "value  (16bit hex)? "; cin >> hex >> value;
				cout << "read reg 0x" << hex << reg << ", value 0x" << (uint)value << endl;
				
				// enable I2C master, just to be sure... ;-)
				jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

				// set adc address pointer
				jtag->FPGA_i2c_byte_write(  adcaddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write((reg&0xf), 0, 0, 0);

				// 2-byte write
				jtag->FPGA_i2c_byte_write(((value>>8)&0xff), 0, 0, 0);
				jtag->FPGA_i2c_byte_write(     (value&0xff), 0, 1, 0);
			} break;
			
			case 'f':{
				uint adc,adcaddr;
				uint reg, value;
				unsigned char valuem,valuel;
				
				cout << "which ADC (0/1)? "; cin >> adc;
				if(adc==0) adcaddr=0x42; else adcaddr=0x44;
				
				cout << "address (4bit hex)? "; cin >> reg;
				
				// enable I2C master, just to be sure... ;-)
				jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

				// set adc address pointer
				jtag->FPGA_i2c_byte_write(  adcaddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write((reg&0xf), 0, 1, 0);

				// re-send I2C address with read bit set
				adcaddr |= 0x1;
				jtag->FPGA_i2c_byte_write(adcaddr, 1, 0, 0);

				// 2-byte read
				jtag->FPGA_i2c_byte_read(0, 0, 0, valuem);
				jtag->FPGA_i2c_byte_read(0, 1, 1, valuel);

				value = (static_cast<uint>(valuem)<<8) | static_cast<uint>(valuel);
				
				cout << "read from ADC: 0x" << hex << value << endl;
			} break;
			
			case 'g':{
				uint mux,muxaddr;
				uint value;
				
				cout << "which MUX (0/1)? "; cin >> mux;
				if(mux==0) muxaddr=0x9c; else muxaddr=0x9e;
				
				cout << "8bit hex config? "; cin >> hex >> value;
				cout << "read 0x" << hex << (uint)value << endl;
				
				// enable I2C master, just to be sure... ;-)
				jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

				// set mux config
				jtag->FPGA_i2c_byte_write(muxaddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_write(value,   0, 1, 0);

			} break;
			
			case 'h':{
				uint mux,muxaddr;
				unsigned char value;
				
				cout << "which MUX (0/1)? "; cin >> mux;
				if(mux==0) muxaddr=0x9d; else muxaddr=0x9f;
				
				// enable I2C master, just to be sure... ;-)
				jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

				// set mux config
				jtag->FPGA_i2c_byte_write(muxaddr, 1, 0, 0);
				jtag->FPGA_i2c_byte_read(0, 1, 1, value);

				cout << "read from MUX: 0x" << hex << (uint)value << endl;
			} break;
			
			case 'i':{
				// enable I2C master, just to be sure... ;-)
				jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

				// adc 1
				// set adc address pointer
				jtag->FPGA_i2c_byte_write(0x42, 1, 0, 0); // address first adc
				jtag->FPGA_i2c_byte_write(0x01, 0, 0, 0);
				// 1-byte write
				jtag->FPGA_i2c_byte_write(0xff, 0, 1, 0);

				// adc 2
				// set adc address pointer
				jtag->FPGA_i2c_byte_write(0x44, 1, 0, 0); // address second adc
				jtag->FPGA_i2c_byte_write(0x01, 0, 0, 0);
				// 1-byte write
				jtag->FPGA_i2c_byte_write(0xff, 0, 1, 0);
			} break;
			
			case 'j':{
				unsigned char alert_reg;
				
				// enable I2C master, just to be sure... ;-)
				jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

				// adc 1
				// set adc address pointer
				jtag->FPGA_i2c_byte_write(0x42, 1, 0, 0); // address first adc
				jtag->FPGA_i2c_byte_write(0x01, 0, 1, 0);

				// 1-byte read
				jtag->FPGA_i2c_byte_write(0x43, 1, 0, 0); // address first adc
				jtag->FPGA_i2c_byte_read(0, 1, 1, alert_reg);
				
				cout << "ADC0 alert register: 0x" << hex << (uint)alert_reg << endl;

				// adc 2
				// set adc address pointer
				jtag->FPGA_i2c_byte_write(0x45, 1, 0, 0); // address second adc
				jtag->FPGA_i2c_byte_write(0x01, 0, 1, 0);

				// 1-byte read
				jtag->FPGA_i2c_byte_write(0x43, 1, 0, 0); // address second adc
				jtag->FPGA_i2c_byte_read(0, 1, 1, alert_reg);
				
				cout << "ADC0 alert register: 0x" << hex << (uint)alert_reg << endl;

			} break;
			
			case 'x': cont = false; 
			}
			
			if(kbhit()>0)cont=false;

		} while(cont);

		return true;
    };
};


const double TmIboardV2::vdd11_r1 = 75000.0;
const double TmIboardV2::vdd11_r2 = 20000.0;

const double TmIboardV2::vdd5_r1  = 27000.0;
const double TmIboardV2::vdd5_r2  = 20000.0;

const double TmIboardV2::vddbus_r1 = 20000.0;
const double TmIboardV2::vddbus_r2 = 56000.0;

const double TmIboardV2::vdd25_r1  = 10000.0;
const double TmIboardV2::vdd25_r2  = 47000.0;

const double TmIboardV2::vol_r1 = 20000.0;
const double TmIboardV2::vol_r2 = 56000.0;

const double TmIboardV2::voh_r1 = 20000.0;
const double TmIboardV2::voh_r2 = 56000.0;

const double TmIboardV2::vref_r1 = 20000.0;
const double TmIboardV2::vref_r2 = 20000.0;

const double TmIboardV2::rg_vdd     = 5360.0;
const double TmIboardV2::rg_vdddnc  = 4000.0;
const double TmIboardV2::rg_vdda    = 5360.0;
const double TmIboardV2::rg_vddadnc = 200.0;
const double TmIboardV2::rg_vdd33   = 200.0;
const double TmIboardV2::rg_vol     = 200.0;
const double TmIboardV2::rg_voh     = 200.0;
const double TmIboardV2::rg_vddbus  = 1500.0;
const double TmIboardV2::rg_vdd25   = 200.0;
const double TmIboardV2::rg_vdd5    = 1500.0;
const double TmIboardV2::rg_vdd11   = 200.0;

const double TmIboardV2::rm_vdd     = 0.01;
const double TmIboardV2::rm_vdddnc  = 0.01;
const double TmIboardV2::rm_vdda    = 0.01;
const double TmIboardV2::rm_vddadnc = 0.03;
const double TmIboardV2::rm_vdd33   = 0.03;
const double TmIboardV2::rm_vol     = 0.01;
const double TmIboardV2::rm_voh     = 0.01;
const double TmIboardV2::rm_vddbus  = 0.01;
const double TmIboardV2::rm_vdd25   = 0.01;
const double TmIboardV2::rm_vdd5    = 0.01;
const double TmIboardV2::rm_vdd11   = 0.1;


class LteeTmIboardV2 : public Listee<Testmode>{
public:
	LteeTmIboardV2() : Listee<Testmode>(string("tm_iboardv2"),string("access/test all analog functionality on IBoard V2")){};
	Testmode * create(){return new TmIboardV2();};
};

LteeTmIboardV2 ListeeTmIboardV2;

