// Company         :   kip                      
// Author          :   Alexander Kononov            
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tm_jitter.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jan 18 11
// Last Change     :   Tue May 30 11    
// by              :   akononov
//------------------------------------------------------------------------
#ifndef HICANNV2
#define HICANNV2

#include "common.h"
#include "s2comm.h"
#include "s2ctrl.h"
#include "hicann_ctrl.h"
#include "testmode.h" //Testmode and Lister classes
#include "repeater_control.h" //repeater control class

using namespace facets;
using namespace std;

class TmAHRep : public Testmode{
    enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater location
	static const int TAG             = 0;
    HicannCtrl *hc;
    RepeaterControl *rca[6];
	LinkLayer<ci_payload,ARQPacketStage2> *ll;
    public:
	bool test() {
                    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
                    hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
                    debug_level = 4;

                    log(Logger::INFO) << "Try Init() ..." ;
                    if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
                        log(Logger::ERROR) << "ERROR: Init failed, abort";
                        return 0;
                    }
                    log(Logger::INFO) << "Init() ok";

                    // initialize controllers
                    log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
                    rca[rc_l] =&hc->getRC_cl(); // get pointer to left repeater of HICANN 0
                    rca[rc_r] =&hc->getRC_cr();
                    rca[rc_tl]=&hc->getRC_tl();
                    rca[rc_bl]=&hc->getRC_bl();
                    rca[rc_tr]=&hc->getRC_tr();
                    rca[rc_br]=&hc->getRC_br();

                    ///use the test output loopback to test repeaters
                    vector<uint> times(3,0);
                    vector<uint> nnums(3,0);
                    uint repnr=0, time=100, nnum=0;
                    char sw;
                    rc_loc loca;
                    cout << "Choose repeater block: " << endl;
                    cout << "1: Center left" << endl;
                    cout << "2: Top left" << endl;
                    cout << "3: Bottom left" << endl;
                    cout << "4: Top right" << endl;
                    cout << "5: Bottom right" << endl;
                    cout << "6: Center right" << endl;
                    cin >> sw;
                    cout << "Choose repeater number: " << endl;
                    cin >> repnr;

                    if (sw == '1') loca=rc_l;
                    else if (sw == '2') loca=rc_tl;
                    else if (sw == '3') loca=rc_bl;
                    else if (sw == '4') loca=rc_tr;
                    else if (sw == '5') loca=rc_br;
                    else loca=rc_r;
                    rca[loca]->reset();

                    cout << "time period (not exact!): " << endl;
                    cin >> time;
                    uint odd=0;
                    if (repnr%2) odd=1;

                    rca[loca]->tout(repnr,true);
                    rca[loca]->tdata_write(0,0,nnum,time);
                    rca[loca]->tdata_write(0,1,nnum,time);
                    rca[loca]->tdata_write(0,2,nnum,time);
                    rca[loca]->tdata_write(1,0,nnum,time);
                    rca[loca]->tdata_write(1,1,nnum,time);
                    rca[loca]->tdata_write(1,2,nnum,time);

                    rca[loca]->startout(odd);

                    rca[loca]->tin(repnr,true);
                    usleep(500); //time for the dll to lock
                    rca[loca]->stopin(odd); //reset the full flag
                    rca[loca]->startin(odd); //start recording received data to channel 0
                    usleep(1000);  //recording lasts for ca. 4 µs - 1ms

                    for (int i=0; i<3; i++){
                        rca[loca]->tdata_read(odd, i, nnums[i], times[i]);
                        cout << "Received neuron number " << dec << nnums[i] << " at time of " << times[i] << " cycles" << endl;
                    }
                    rca[loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
                    rca[loca]->stopin(odd);
                    rca[loca]->stopout(odd);
                    rca[loca]->tout(repnr, false); //set tout back to 0 to prevent conflicts

        };
};


class LteeTmAHRep : public Listee<Testmode>{
public:
	LteeTmAHRep() : Listee<Testmode>(string("tm_ahreptest"),string("Testing connection to tdi and tdo of repeater blocks")){};
	Testmode * create(){return new TmAHRep();};
};
LteeTmAHRep ListeeTmAHRep;
#endif
