// Company         :   kip
// Author          :   Alexander Kononov	          
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tmak_iboardv2.cpp
// Project Name    :   p_facets
// Subproject Name :   HICANN_system            
//                    			
// Create Date     :   10/15/2010
// Last Change     :   11/11/2010
// by              :   Alexander Kononov
//------------------------------------------------------------------------

/**
 *  All functions should be pretty self-explainatory, for more information
 *  see comments in iboardv2_ctrl.h and iboardv2_ctrl.cpp
 */

#include "common.h"
#include "s2comm.h"
#include "s2ctrl.h"
#include "testmode.h" 
#include "iboardv2_ctrl.h"

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class TmakIboardV2 : public Testmode{

public:

	bool test(){
		
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	cout << "IboardV2: ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		if (!jtag) {
		 	cout << "IboardV2: ERROR: object 'jtag' in testmode not set, abort" << endl;
			return 0;
		}
		IBoardV2Ctrl board(conf, jtag, 0);
		char c;
		bool cont=true;
		do{

			cout << endl <<"Select test option:" << endl;
			cout << "1: send HICANN reset pulse" << endl;
			cout << "2: JTAG reset" << endl;
			cout << "3: read FPGA and HICANN JTAG ID" << endl;
			cout << "4: set JTAG prescaler to 4FF" << endl;
			cout << "7: set all voltages to meaningful values" << endl;
			cout << "8: read out all voltages from ADCs" << endl;
			cout << "9: read ADC voltages" << endl;
			cout << "a: read ADC currents" << endl;
			cout << "b: set DAC voltage" << endl;
			cout << "c: set both MUX to default values" << endl;
			cout << "d: switch MUX inputs" << endl;
			cout << "e: continuously monitor Iboard currents" << endl;
			cout << "x: exit" << endl;

			cin >> c;

			switch(c){

				case '1':{
					cout << "resetting HICANN" << endl;
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
				
				case '4':{
					cout << "setting JTAG to 4FF" << endl;
					board.setJtag();
				} break;
				
				// Sets all voltages on the board to values specified in the config file
				case '7':{
					if (board.setAllVolt()) cout << "All voltages set" << endl;
					else cout << "Error occured with setting voltages" << endl;
				} break;

				case '8':{
					cout << "read VDDA voltage: " << board.getVoltage(SBData::ivdda) << endl;
					cout << "read VDD_DNCIF voltage: " << board.getVoltage(SBData::ivdd_dncif) << endl;
					cout << "read VOL voltage: " << board.getVoltage(SBData::ivol) << endl;
					cout << "read VOH voltage: " << board.getVoltage(SBData::ivoh) << endl;
					cout << "read VDDBUS voltage: " << board.getVoltage(SBData::ivddbus) << endl;
					cout << "read VDD25 voltage: " << board.getVoltage(SBData::ivdd25) << endl;
					cout << "read VDD5 voltage: " << board.getVoltage(SBData::ivdd5) << endl;
					cout << "read VDD11 voltage: " << board.getVoltage(SBData::ivdd11) << endl;
					cout << "read AOUT0 voltage: " << board.getVoltage(SBData::aout0) << endl;
					cout << "read AOUT1 voltage: " << board.getVoltage(SBData::aout1) << endl;
					cout << "read ADC0 voltage: " << board.getVoltage(SBData::adc0) << endl;
					cout << "read ADC1 voltage: " << board.getVoltage(SBData::adc1) << endl;
					cout << "read ADC2 voltage: " << board.getVoltage(SBData::adc2) << endl; 
					cout << "read VDDA_DNCIF voltage: " << board.getVoltage(SBData::ivdda_dncif) << endl;
					cout << "read VDD33 voltage: " << board.getVoltage(SBData::ivdd33) << endl;
					cout << "read VDD voltage: " << board.getVoltage(SBData::ivdd) << endl;
				} break;
	
				case '9':{
					char ch;
					cout << "Select ADC voltage:" << endl;
					cout << "1: VOL" << endl;
					cout << "2: VOH" << endl;
					cout << "3: VDDBUS" << endl;
					cout << "4: VDD25" << endl;
					cout << "5: VDD5" << endl;
					cout << "6: VDD11" << endl;
					cout << "7: AOUT0" << endl;
					cout << "8: AOUT1" << endl;
					cout << "> " << endl;
					cin >> ch;
	
					switch (ch){
						case '1':{
							cout << "read VOL voltage: " << board.getVoltage(SBData::ivol) << endl;
						}break;
						case '2':{
							cout << "read VOH voltage: " << board.getVoltage(SBData::ivoh) << endl;
						}break;
						case '3':{
							cout << "read VDDBUS voltage: " << board.getVoltage(SBData::ivddbus) << endl;
						}break;
						case '4':{
							cout << "read VDD25 voltage: " << board.getVoltage(SBData::ivdd25) << endl;
						}break;
						case '5':{
							cout << "read VDD5 voltage: " << board.getVoltage(SBData::ivdd5) << endl;
						}break;
						case '6':{
							cout << "read VDD11 voltage: " << board.getVoltage(SBData::ivdd11) << endl;
						}break;
						case '7':{
							cout << "read AOUT0 voltage: " << board.getVoltage(SBData::aout0) << endl;
						}break;
						case '8':{
							cout << "read AOUT1 voltage: " << board.getVoltage(SBData::aout1) << endl;
						}break;
						
						default: {
							cout << "Wrong voltage specified" << endl;
							break;
						}
					}
				} break;
	
				case 'a':{
					char ch;
					cout << "Select ADC current:" << endl;
					cout << "1: VOL" << endl;
					cout << "2: VOH" << endl;
					cout << "3: VDDBUS" << endl;
					cout << "4: VDD25" << endl;
					cout << "5: VDD5" << endl;
					cout << "6: VDD11" << endl;
					cout << "> " << endl;
					cin >> ch;
	
					switch (ch){
						case '1':{
							cout << "read VOL current: " << board.getCurrent(SBData::ivol) << endl;
						}break;
						case '2':{
							cout << "read VOH current: " << board.getCurrent(SBData::ivoh) << endl;
						}break;
						case '3':{
							cout << "read VDDBUS current: " << board.getCurrent(SBData::ivddbus) << endl;
						}break;
						case '4':{
							cout << "read VDD25 current: " << board.getCurrent(SBData::ivdd25) << endl;
						}break;
						case '5':{
							cout << "read VDD5 current: " << board.getCurrent(SBData::ivdd5) << endl;
						}break;
						case '6':{
							cout << "read VDD11 current: " << board.getCurrent(SBData::ivdd11) << endl;
						}break;
						default: {
							cout << "Wrong current specified" << endl;
							break;
						}
					}
				} break;
	
				case 'b':{
					char d;
					cout << "Select DAC voltage:" << endl;
					cout << "1: VOL" << endl;
					cout << "2: VOH" << endl;
					cout << "3: VDDBUS" << endl;
					cout << "4: VDD25" << endl;
					cout << "5: VDD5" << endl;
					cout << "6: VDD11" << endl;
					cout << "7: VREF_DAC" << endl;
					cout << "> " << endl;
					cin >> d;
					double dv;
	
					cout << "Value for this voltage? ";
					cin >> dv;
	
					switch (d){
						case '1':{
							if (board.setVoltage(SBData::vol, dv)) cout << "set VOL voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						case '2':{
							if (board.setVoltage(SBData::voh, dv)) cout << "set VOH voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						case '3':{
							if (board.setVoltage(SBData::vddbus, dv)) cout << "set VDDBUS voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						case '4':{
							if (board.setVoltage(SBData::vdd25, dv)) cout << "set VDD25 voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						case '5':{
							if (board.setVoltage(SBData::vdd5, dv)) cout << "set VDD5 voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						case '6':{
							if (board.setVoltage(SBData::vdd11, dv)) cout << "set VDD11 voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						case '7':{
							if (board.setVoltage(SBData::vref_dac, dv)) cout << "set VREF_DAC voltage: " << dv << endl;
							else cout << "Error at voltage setting" << endl;
						}break;
						default: {
							cout << "Wrong voltage specified" << endl;
							break;
						}
					}
				} break;
				
				case 'c':{
					board.setBothMux();
					cout << "Both MUX set to default values" << endl;
				} break;
				
				case 'd':{
					int m, c;
					cout << "What MUX? (0/1)" << endl;
					cout << "> " << endl;
					cin >> m;
					cout << "What channel? (0-7)" << endl;
					cout << "> " << endl;
					cin >> c;
					if(board.switchMux(m,c)) cout << "MUX " << m << " set to input " << c << endl;
					else cout << "Error at MUX setting" << endl;
				} break;
				
				case 'e':{
					while (1){
						cout << "IVDD:        " << setprecision(4) << board.getCurrent(SBData::ivdd) << "\n";
						cout << "IVDDA:       " << setprecision(4) << board.getCurrent(SBData::ivdda) << "\n";
						cout << "IVDD_DNCIF:  " << setprecision(4) << board.getCurrent(SBData::ivdd_dncif) << "\n";
						cout << "IVDDA_DNCIF: " << setprecision(4) << board.getCurrent(SBData::ivdda_dncif) << "\n";
						cout << "IVDD33:      " << setprecision(4) << board.getCurrent(SBData::ivdd33) << "\n";
						cout << "IVOL:        " << setprecision(4) << board.getCurrent(SBData::ivol) << "\n";
						cout << "IVOH:        " << setprecision(4) << board.getCurrent(SBData::ivoh) << "\n";
						cout << "IVDDBUS:     " << setprecision(4) << board.getCurrent(SBData::ivddbus) << "\n";
						cout << "IVDD25:      " << setprecision(4) << board.getCurrent(SBData::ivdd25) << "\n";
						cout << "IVDD5:       " << setprecision(4) << board.getCurrent(SBData::ivdd5) << "\n";
						cout << "IVDD11:      " << setprecision(4) << board.getCurrent(SBData::ivdd11) << "\n";
						cout << flush;
						if(kbhit()) // if keyboard has input pending, break from loop
							{
								cout << "ending current monitoring" << endl;
								getch(); // "resets" kbhit by removing a key from the buffer
								break;
							}
						cout << "\033[11A"; // linux shell cursor movement "trick" to go 11 lines up
						usleep(200000);
					}
				} break;
				
				case 'x': cont = false; 
			}
			if(kbhit()>0)cont=false;
		}
		while(cont);
	}
};



class LteeTmakIboardV2 : public Listee<Testmode>{
public:
	LteeTmakIboardV2() : Listee<Testmode>(string("tmak_iboardv2"),string("iBoard V2 functionality using IBoardV2Ctrl")){};
	Testmode * create(){return new TmakIboardV2();};
};

LteeTmakIboardV2 ListeeTmakIboardV2;
