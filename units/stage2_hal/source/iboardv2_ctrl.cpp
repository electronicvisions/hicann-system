/**---------------------------------------------------------------------
 * Company         :   kip                      
 * Author          :   Andreas Gruebl, Alexander Kononov
 * E-Mail          :   kononov@kip.uni-heidelberg.de
 *                    			
 * Filename        :   iboardv2_ctrl.cpp
 * Project Name    :   p_facets
 * Subproject Name :   HICANN_system                  
 *                    			
 * Create Date     :   Nov 02 10
 * Last Change     :   Nov 10 10    
 * by              :   Alexander Kononov        
 *
 * ---------------------------------------------------------------------
 *    The original implementation of the iBoard functionality was
 *    created by Andreas Gruebl in a testmode "tm_iboardv2"
 *    and later imported into this class by Alexander Kononov
 * ---------------------------------------------------------------------
 */

#include "iboardv2_ctrl.h"

using namespace std;
using namespace facets;

/*
 * Constructor. Instances of stage2_conf and myjtagfull are passed to
 * IBoardV2Ctrl from tests2. Also the iBoard configuration ID is used
 * to configure the iBoard (resistor values, voltages etc...)
 */
IBoardV2Ctrl::IBoardV2Ctrl(stage2_conf *conf, myjtag_full *jtag, uint boardID)
    : _conf(conf),
      _jtag(jtag),
      log(Logger::instance("hicann-system.IBoardV2Ctrl", Logger::INFO, ""))
{
	iboardConfigure(boardID);
	setJtag(); // set jtag to 4FF by default
	log(Logger::DEBUG1) << "IBoardV2Ctrl instance created";
}

/*
 * Simplified version of the previous constructor. Loads default values
 */
IBoardV2Ctrl::IBoardV2Ctrl(myjtag_full *jtag)
    : _jtag(jtag), log(Logger::instance("hicann-system.IBoardV2Ctrl", Logger::INFO, ""))
{
	iboardConfigure(0);
	setJtag(); // set jtag to 4FF by default
	log(Logger::DEBUG1) << "IBoardV2Ctrl instance created";
}

IBoardV2Ctrl::~IBoardV2Ctrl(){}

/*
 * Load an IBoardV2 configuration from a file ("iboardv2_cfg.xml"
 * is the default), the filename must be passed to tests2 as an argument
 * in a command line because tests2 creates the "conf" object and
 * it cannot be modified "on the fly" without consequences
 */
bool IBoardV2Ctrl::iboardConfigure(int ID) {
	// ID = 0 is the "default" value, it will also work if for some reason
	// a wrong configuration file must be specified or the boardID is illegal
	log(Logger::INFO) << "Loading configuration for iBoard number " << ID;
	switch(ID){
		case 0:{
			// resistor values on IBoard V2
			vdd11_r1 	= 75000.0;
			vdd11_r2 	= 20000.0;
			vdd5_r1 	= 27000.0;
			vdd5_r2  	= 20000.0;
			vddbus_r1	= 20000.0;
			vddbus_r2	= 56000.0;
			vdd25_r1  	= 10000.0;
			vdd25_r2  	= 47000.0;
			vol_r1 		= 20000.0;
			vol_r2 		= 56000.0;
			voh_r1 		= 20000.0;
			voh_r2 		= 56000.0;
			vref_r1 	= 20000.0;
			vref_r2 	= 20000.0;
			
			// instumentation amplifier external resistors:
			rg_vdd     	= 5360.0;
			rg_vdddnc  	= 4000.0;
			rg_vdda    	= 5360.0;
			rg_vddadnc 	= 200.0;
			rg_vdd33   	= 200.0;
			rg_vol     	= 200.0;
			rg_voh     	= 200.0;
			rg_vddbus  	= 1500.0;
			rg_vdd25   	= 200.0;
			rg_vdd5    	= 1500.0;
			rg_vdd11   	= 200.0;
			
			// current measurement resistors
			rm_vdd     	= 0.01;
			rm_vdddnc  	= 0.01;
			rm_vdda    	= 0.01;
			rm_vddadnc 	= 0.03;
			rm_vdd33   	= 0.03;
			rm_vol     	= 0.01;
			rm_voh     	= 0.01;
			rm_vddbus  	= 0.01;
			rm_vdd25   	= 0.01;
			rm_vdd5    	= 0.01;
			rm_vdd11   	= 0.1;
			
			// MUX configuration
			mux0addr	=0x9c;
			mux1addr	=0x9e;
			mux0cmd		=0x80;
			mux1cmd		=0x80;
			
			// DAC configuration
			vrefdac		=2.5;
			dacres		=16;
			dac0addr	=0x98;
			dac1addr	=0x9a;
			dacvol		=0x14;
			dacvoh		=0x10;
			dacvddbus	=0x12;
			dacvdd25	=0x10;
			dacvdd5		=0x12;
			dacvdd11	=0x14;
			dacvref_dac	=0x16;
			
			// ADC configuration
			vrefadc			=3.3;
			vref_ina        =1.0;
			adcres			=10;
			adc0addr		=0x42;
			adc1addr		=0x44;
			readiter		=9;
			adcivdda		=0xf0;
			adcivdd_dncif	=0xb0;
			adcivdda_dncif	=0xa0;
			adcivdd33		=0x80;
			adcivol			=0xa0;
			adcivoh			=0x90;
			adcivdd         =0x90;
			adcivddbus		=0x80;
			adcivdd25		=0xd0;
			adcivdd5		=0xc0;
			adcivdd11		=0xe0;
			adcaout0		=0xf0;
			adcaout1		=0xe0;
			adcadc0			=0xb0;
			adcadc1			=0xc0;
			adcadc2			=0xd0;
			
			// Default voltages to be set with setAllVolt
			vdd5value   =  5.0;
			vdd25value  =  2.5;
			vdd11value  = 10.5;
			vohvalue    =  0.9;
			volvalue    =  0.7;
			vddbusvalue =  1.2;
			return true;
		}
		default: {                     // ID other than 0
			if (!doubles.empty())	{  // make sure the vectors are empty
				doubles.clear();
				integers.clear();
			}
			// load config vectors from the xml file
			if (_conf->getIBoardV2(ID, &doubles , &integers)) {
			// if success, set the variables to their values
				vrefdac=doubles[0];
				dacres=integers[0];
				dac0addr=integers[1];
				dac1addr=integers[2];
				dacvol=integers[3];
				dacvoh=integers[4];
				dacvddbus=integers[5];
				dacvdd25=integers[6];
				dacvdd5=integers[7];
				dacvdd11=integers[8];
				dacvref_dac=integers[9];
				
				vrefadc=doubles[1];
				vref_ina=doubles[2];
				adcres=integers[10];
				adc0addr=integers[11];
				adc1addr=integers[12];
				readiter=integers[13];
				adcivdda=integers[14];
				adcivdd_dncif=integers[15];
				adcivdda_dncif=integers[16];
				adcivdd33=integers[17];
				adcivol=integers[18];
				adcivoh=integers[19];
				adcivdd=integers[20];
				adcivddbus=integers[21];
				adcivdd25=integers[22];
				adcivdd5=integers[23];
				adcivdd11=integers[24];
				adcaout0=integers[25];
				adcaout1=integers[26];
				adcadc0=integers[27];
				adcadc1=integers[28];
				adcadc2=integers[29];
				
				mux0addr=integers[30];
				mux1addr=integers[31];
				mux0cmd=integers[32];
				mux1cmd=integers[33];
				
				vdd11_r1=doubles[3];
				vdd11_r2=doubles[4];
				vdd5_r1=doubles[5];
				vdd5_r2=doubles[6];
				vddbus_r1=doubles[7];
				vddbus_r2=doubles[8];
				vdd25_r1=doubles[9];
				vdd25_r2=doubles[10];
				vol_r1=doubles[11];
				vol_r2=doubles[12];
				voh_r1=doubles[13];
				voh_r2=doubles[14];
				vref_r1=doubles[15];
				vref_r2=doubles[16];
				rg_vdd=doubles[17];
				rg_vdddnc=doubles[18];
				rg_vdda=doubles[19];
				rg_vddadnc=doubles[20];
				rg_vdd33=doubles[21];
				rg_vol=doubles[22];
				rg_voh=doubles[23];
				rg_vddbus=doubles[24];
				rg_vdd25=doubles[25];
				rg_vdd5=doubles[26];
				rg_vdd11=doubles[27];
				rm_vdd=doubles[28];
				rm_vdddnc=doubles[29];
				rm_vdda=doubles[30];
				rm_vddadnc=doubles[31];
				rm_vdd33=doubles[32];
				rm_vol=doubles[33];
				rm_voh=doubles[34];
				rm_vddbus=doubles[35];
				rm_vdd25=doubles[36];
				rm_vdd5=doubles[37];
				rm_vdd11=doubles[38];
				
				vdd5value=doubles[39];
				vdd25value=doubles[40];
				vdd11value=doubles[41];
				vohvalue=doubles[42];
				volvalue=doubles[43];
				vddbusvalue=doubles[44];
				return true;
			}
			else { // if not success load default configuration
				log(Logger::INFO) << "Loading default configuration 0";
				iboardConfigure(0);
				return true;
			}
		}
	}
}

/* Read out a voltage with ADC.
 * Pay attention that all voltages are set to meaningful values before
 * executing this (for example VDD11 mist be lower than 9.5V) or else it
 * may not work because the ADC inputs are not valid (you get maximum value)
 */
double IBoardV2Ctrl::getVoltage(SBData::ADtype vname) { 
	double endvalue = 0;
	uint adcaddr, adccmd, adcval;
	double value = 0;                       //used for mean calculation
	unsigned char adcvall = 0, adcvalm = 0; // LSB and MSB

	// Sideband Data from s2types is used, some are currents, some are voltages
	switch(vname){
		case SBData::ivdda:       {adcaddr = adc0addr; adccmd = adcivdda; break;}
		case SBData::ivdd_dncif:  {adcaddr = adc0addr; adccmd = adcivdd_dncif; break;}
		case SBData::ivol:        {adcaddr = adc0addr; adccmd = adcivol; break;}
		case SBData::ivoh:        {adcaddr = adc0addr; adccmd = adcivoh; break;}
		case SBData::ivddbus:     {adcaddr = adc0addr; adccmd = adcivddbus; break;}
		case SBData::ivdd25:      {adcaddr = adc0addr; adccmd = adcivdd25; break;}
		case SBData::ivdd5:       {adcaddr = adc0addr; adccmd = adcivdd5; break;}
		case SBData::ivdd11:      {adcaddr = adc0addr; adccmd = adcivdd11; break;}
		case SBData::ivdda_dncif: {adcaddr = adc1addr; adccmd = adcivdda_dncif; break;}
		case SBData::ivdd33:      {adcaddr = adc1addr; adccmd = adcivdd33; break;}
		case SBData::ivdd:        {adcaddr = adc1addr; adccmd = adcivdd; break;}
		case SBData::aout0:       {adcaddr = adc1addr; adccmd = adcaout0; break;}
		case SBData::aout1:       {adcaddr = adc1addr; adccmd = adcaout1; break;}
		case SBData::adc0:        {adcaddr = adc1addr; adccmd = adcadc0; break;}
		case SBData::adc1:        {adcaddr = adc1addr; adccmd = adcadc1; break;}
		case SBData::adc2:        {adcaddr = adc1addr; adccmd = adcadc2; break;}
		default:
			log(Logger::ERROR) << "Wrong ADC voltage specified!";
			return 0.0; // ECM: was NULL (...)
	}
	
	log(Logger::DEBUG0) << "Reading ADC";
	for (uint i=0; i<readiter; i++){
		_jtag->FPGA_i2c_setctrl((unsigned char)_jtag->ADDR_CTRL, (unsigned char)0x80); // enable I2C master

		// send conversion command
		_jtag->FPGA_i2c_byte_write(adcaddr, 1, 0, 0);
		_jtag->FPGA_i2c_byte_write(adccmd,  0, 1, 0);

		// add read bit and re-send I2C address
		_jtag->FPGA_i2c_byte_write((adcaddr | 0x1), 1, 0, 0);

		// read conversion result
		_jtag->FPGA_i2c_byte_read(0, 0, 0, adcvalm);
		_jtag->FPGA_i2c_byte_read(0, 1, 1, adcvall);

/*		// perform sanity checks:
		if(adcvalm & 0x8) log(Logger::WARNING) << "ADC Alert Flag set!";
		if((adcvalm & 0x70) != (adccmd & 0x70)){
			log(Logger::ERROR) << "Wrong ADC answer channel!";
			return false;
		}
*/
		adcval = (static_cast<uint>(adcvalm)<<8) | static_cast<uint>(adcvall);
		adcval >>= 2;     // 10 Bit ADC -> D0 and D1 not used
		adcval &=  0x3ff; // make sure only last 10 bits are valid

		value = (double)adcval / (double)((1<<adcres)-1) * vrefadc;
		if (i > 0) endvalue += value; // first value is not valid (ADC
	}                                 // line charging takes too long)
	endvalue /= (readiter-1);
	return endvalue;	
}

// read out a current with ADC
double IBoardV2Ctrl::getCurrent(SBData::ADtype cname) {
	double voltagevalue=0, currentvalue=0, rg=0, rm=0;
	// only current sideband data types can be used
	switch(cname){
		case SBData::ivdd:        {rg = rg_vdd;     rm = rm_vdd;     break;}
		case SBData::ivdda:       {rg = rg_vdda;    rm = rm_vdda;    break;}
		case SBData::ivdd_dncif:  {rg = rg_vdddnc;  rm = rm_vdddnc;  break;}
		case SBData::ivdda_dncif: {rg = rg_vddadnc; rm = rm_vddadnc; break;}
		case SBData::ivdd33:      {rg = rg_vdd33;   rm = rm_vdd33;   break;}
		case SBData::ivol:        {rg = rg_vol;     rm = rm_vol;     break;}
		case SBData::ivoh:        {rg = rg_voh;     rm = rm_voh;     break;}
		case SBData::ivddbus:     {rg = rg_vddbus;  rm = rm_vddbus;  break;}
		case SBData::ivdd25:      {rg = rg_vdd25;   rm = rm_vdd25;   break;}
		case SBData::ivdd5:       {rg = rg_vdd5;    rm = rm_vdd5;    break;}
		case SBData::ivdd11:      {rg = rg_vdd11;   rm = rm_vdd11;   break;}
		default:
			log(Logger::ERROR) << "Wrong ADC current specified";
			return 0.0; // ECM: was NULL (...)
	}
	// get corresponding voltage
	voltagevalue = IBoardV2Ctrl::getVoltage(cname);
	log(Logger::DEBUG2) << "Reading voltage: " << voltagevalue;

	// calculate current based on ina circuitry
	currentvalue = ((voltagevalue - vref_ina) / (5.0 + 80000.0 / rg)) / rm;
	return currentvalue;	
}

// set one specific voltage, returns 0 on error
bool IBoardV2Ctrl::setVoltage(SBData::ADtype vname, double value) {
	double dacvout = 0;
	uint dacaddr, daccmd;
	
	switch(vname){
		case SBData::vol:{
			dacaddr = dac1addr; daccmd = dacvol;
			dacvout = value * (vol_r1 + vol_r2) / vol_r2;
			break;}
		case SBData::voh:{
			dacaddr = dac1addr; daccmd = dacvoh;
			dacvout = value * (voh_r1 + voh_r2) / voh_r2;
			break;}
		case SBData::vddbus:{
			dacaddr = dac1addr; daccmd = dacvddbus;
			dacvout = value * (vddbus_r1 + vddbus_r2) / vddbus_r2;
			break;}
		case SBData::vdd25:{
			dacaddr = dac0addr; daccmd = dacvdd25;
			dacvout = value / (1 + vdd25_r1 / vdd25_r2);
			break;}
		case SBData::vdd5:{
			dacaddr = dac0addr; daccmd = dacvdd5;
			dacvout = value / (1 + vdd5_r1 / vdd5_r2);
			break;}
		case SBData::vdd11:{
			dacaddr = dac0addr; daccmd = dacvdd11;
			dacvout = value / (1 + vdd11_r1 / vdd11_r2);
			break;}
		case SBData::vref_dac:{
			dacaddr = dac0addr; daccmd = dacvref_dac;
			dacvout = value * (vref_r1 + vref_r2) / vref_r2;
			break;}
		default: {
			log(Logger::ERROR) << "Wrong voltage specified!";
			return false;
		}
	}	

	// sanity check:
	if(dacvout > vrefdac) {
		log(Logger::ERROR) << "DAC input value larger than vrefdac!";
		return false;
	}

	uint dacval = dacvout / vrefdac * (double)((1<<dacres)-1);
	
	log(Logger::DEBUG0) << "Setting DAC to " << value;
	_jtag->FPGA_i2c_setctrl((unsigned char)_jtag->ADDR_CTRL, (unsigned char)0x80); // enable I2C master
	for (int j = 0; j<50;j++){ // write 50 times to compensate for possible write errors
		_jtag->FPGA_i2c_byte_write(dacaddr, 1, 0, 0);
		_jtag->FPGA_i2c_byte_write(daccmd,  0, 0, 0);
		_jtag->FPGA_i2c_byte_write((dacval>>8), 0, 0, 0);
		_jtag->FPGA_i2c_byte_write( dacval,     0, 1, 0);
	}
return true;
}

// set all voltages to meaningful values
bool IBoardV2Ctrl::setAllVolt() { 
	log(Logger::INFO) << "Setting all voltages to meaningful values";
	if (IBoardV2Ctrl::setVoltage(SBData::vdd11, (vdd11value/2)) &&
	IBoardV2Ctrl::setVoltage(SBData::vdd5, vdd5value) &&
	IBoardV2Ctrl::setVoltage(SBData::vdd25, vdd25value) &&
	IBoardV2Ctrl::setVoltage(SBData::vdd11, vdd11value) &&
	IBoardV2Ctrl::setVoltage(SBData::voh, vohvalue) &&
	IBoardV2Ctrl::setVoltage(SBData::vol, volvalue) &&
	IBoardV2Ctrl::setVoltage(SBData::vddbus, vddbusvalue)){return true;}
	else return false;
}

// switch MUX to use a certain input line (0 to 7), default is 7
bool IBoardV2Ctrl::switchMux(uint muxid, uint channel) {
	uint muxcmd=0;
	uint muxaddr=0;
	_jtag->FPGA_i2c_setctrl((unsigned char)_jtag->ADDR_CTRL, (unsigned char)0x80); // enable I2C master
	bool result=true;
	switch(channel){
		case 0:{muxcmd=0x1; break;}
		case 1:{muxcmd=0x2; break;}
		case 2:{muxcmd=0x4; break;}
		case 3:{muxcmd=0x8; break;}
		case 4:{muxcmd=0x10; break;}
		case 5:{muxcmd=0x20; break;}
		case 6:{muxcmd=0x40; break;}
		case 7:{muxcmd=0x80; break;}
		default:{
			log(Logger::ERROR) << "No such channel at this MUX";
			result=false;
			break;
		}
	}
	switch(muxid){
		case 0:{muxaddr=mux0addr; break;}
		case 1:{muxaddr=mux1addr; break;}
		default:{
			log(Logger::ERROR) << "No such MUX";
			result=false;
			break;
		}
	}
	if(result){
		log(Logger::INFO) << "Switching MUX " << muxid << " to input from channel " << channel;
		_jtag->FPGA_i2c_byte_write(muxaddr, 1, 0, 0);
		_jtag->FPGA_i2c_byte_write(muxcmd, 0, 1, 0);
		result=true;
		}
	return result;
}

//set both MUX to standard values (both hicann0 analog outputs)
void IBoardV2Ctrl::setBothMux() { 
	_jtag->FPGA_i2c_setctrl((unsigned char)_jtag->ADDR_CTRL, (unsigned char)0x80); // enable I2C master
	log(Logger::DEBUG0) << "Setting both MUX to default values";
	_jtag->FPGA_i2c_byte_write(mux0addr, 1, 0, 0);
	_jtag->FPGA_i2c_byte_write(mux0cmd, 0, 1, 0);
	_jtag->FPGA_i2c_byte_write(mux1addr, 1, 0, 0);
	_jtag->FPGA_i2c_byte_write(mux1cmd, 0, 1, 0);
}

// set jtag prescaler to 4FF to ensure DAC write success
void IBoardV2Ctrl::setJtag() { 
	log(Logger::DEBUG0) << "Setting jtag prescaler to 0x4ff";
	_jtag->FPGA_i2c_setctrl((unsigned char)_jtag->ADDR_CTRL, (unsigned char)0x80);	// enable I2C master
	_jtag->FPGA_i2c_setctrl(_jtag->ADDR_PRESC_M, (0x4FF>>8));
	_jtag->FPGA_i2c_setctrl(_jtag->ADDR_PRESC_L, (unsigned char)0x4FF);
	unsigned char rprescl=0, rprescm=0;
	_jtag->FPGA_i2c_getctrl(_jtag->ADDR_PRESC_L, rprescl);
	_jtag->FPGA_i2c_getctrl(_jtag->ADDR_PRESC_M, rprescm);
	log(Logger::DEBUG0) << "Read back prescaler: 0x" << setw(2) << hex << ((((uint)rprescm)<<8)|(uint)rprescl);
}



