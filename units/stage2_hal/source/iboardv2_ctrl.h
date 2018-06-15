/**---------------------------------------------------------------------
 * Company         :   kip                      
 * Author          :   Andreas Gruebl, Alexander Kononov
 * E-Mail          :   kononov@kip.uni-heidelberg.de
 *                    			
 * Filename        :   iboardv2_ctrl.h                
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

#ifndef _IBOARDV2_H_
#define _IBOARDV2_H_

#include "logger.h"
#include "stage2_conf.h"  // needed to be able to configure the iBoards individually

namespace facets {

class IBoardV2Ctrl
{
private:

	// some things have to be passed from testmode... (tests2 creates most objects)
	stage2_conf* _conf;
	myjtag_full* _jtag;
	
	// std::vectors, holding the parameters for the iBoard
	std::vector<double> doubles;
	std::vector<uint> integers;
	
	// logger
	Logger& log;
	
	// resistor values on IBoard V2
	double vdd11_r1;
	double vdd11_r2;
	double vdd5_r1;
	double vdd5_r2;
	double vddbus_r1;
	double vddbus_r2;
	double vdd25_r1;
	double vdd25_r2;
	double vol_r1;
	double vol_r2;
	double voh_r1;
	double voh_r2;
	double vref_r1;
	double vref_r2;
	
	// inastumentation amplifier external resistors:
	double rg_vdd    ;
	double rg_vdddnc ;
	double rg_vdda   ;
	double rg_vddadnc;
	double rg_vdd33  ;
	double rg_vol    ;
	double rg_voh    ;
	double rg_vddbus ;
	double rg_vdd25  ;
	double rg_vdd5   ;
	double rg_vdd11  ;
	
	// current measurement resistors
	double rm_vdd    ;
	double rm_vdddnc ;
	double rm_vdda   ;
	double rm_vddadnc;
	double rm_vdd33  ;
	double rm_vol    ;
	double rm_voh    ;
	double rm_vddbus ;
	double rm_vdd25  ;
	double rm_vdd5   ;
	double rm_vdd11  ;
	
	// MUX configuration
	uint mux0addr; // address on an I2C bus
	uint mux1addr; // address on an I2C bus
	uint mux0cmd ;
	uint mux1cmd ;
	
	// DAC configuration
	double vrefdac; // reference voltage for DAC
	uint dacres;    // DAC resolution
	uint dac0addr;	// address on an I2C bus
	uint dac1addr;  // address on an I2C bus
	uint dacvol;    // below are the commands to set the corresponding outputs
	uint dacvoh;
	uint dacvddbus;
	uint dacvdd25;
	uint dacvdd5 ;
	uint dacvdd11;
	uint dacvref_dac;

	// ADC configuration
	double vrefadc;  // reference voltage for ADC
	double vref_ina; // reference voltage for instrumental amplifiers
	uint adcres;     // ADC resolution 
	uint adc0addr;   // address on an I2C bus
	uint adc1addr;   // address on an I2C bus
	uint readiter;   //number of iterations for reading
	uint adcivdda;   // below are the commands to get the corresponding values
	uint adcivdd_dncif;
	uint adcivdda_dncif;
	uint adcivdd33;
	uint adcivol;
	uint adcivoh;
	uint adcivdd;
	uint adcivddbus;
	uint adcivdd25;
	uint adcivdd5;
	uint adcivdd11;
	uint adcaout0;
	uint adcaout1;
	uint adcadc0;
	uint adcadc1;
	uint adcadc2;
	
	// Default voltages to be set with setAllVolt
	double vdd5value;
	double vdd25value;
	double vdd11value;
	double vohvalue;
	double volvalue;
	double vddbusvalue;
	
public:

	// boardID should be written on the iBoard in marker :)
	IBoardV2Ctrl(stage2_conf* conf, myjtag_full* jtag, uint boardID);

	// this constructor is simplified and uses default parameters
	IBoardV2Ctrl(myjtag_full* jtag);

	~IBoardV2Ctrl();

	// read out a voltage with ADC
	double getVoltage(SBData::ADtype vname);

	// read out a current with ADC
	double getCurrent(SBData::ADtype cname);
	
	// set one specific voltage, returns 0 on error
	bool setVoltage(SBData::ADtype vname, double value);
	
	// set all voltages to meaningful values
	bool setAllVolt();
	
	// switch MUX to use a certain input line (0 to 7), default is 7
	bool switchMux(uint muxid, uint channel);
	
	//set both MUX to standard values
	void setBothMux();
	
	// set jtag prescaler to 4FF to ensure DAC write success
	void setJtag();
	
	// load an IBoardV2 configuration from a file ("iboardv2_cfg.xml")
	bool iboardConfigure(int boardID); // 0 is default
};
		/* for further comments see the iboardv2_ctrl.cpp file */

} // namespace facets
#endif // _IBOARDV2_H_
