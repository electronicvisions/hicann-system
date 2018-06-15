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

class TmAHRepLoop : public Testmode{
    enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater location
	static const int TAG             = 0;
    HicannCtrl *hc;
    RepeaterControl *rca[6];
	LinkLayer<ci_payload,ARQPacketStage2> *ll;
    public:
	bool test() {
                    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
                    hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
                    ll = hc->tags[TAG];
                    ll->SetDebugLevel(0); // no ARQ deep debugging
                    debug_level = 4;

                    log(Logger::INFO) << "Try Init() ..." ;
                    if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
                        log(Logger::ERROR) << "ERROR: Init failed, abort";
                        return 0;
                    }
                    log(Logger::INFO) << "Init() ok";

                    // initialize controllers
                    log(Logger::INFO) << "Getting control instances from hicann_control... " << endl;
                    rca[rc_l]=hc->getRC_cl(); // get pointer to left repeater of HICANN 0
                    rca[rc_r]=hc->getRC_cr();
                    rca[rc_tl]=hc->getRC_tl();
                    rca[rc_bl]=hc->getRC_bl();
                    rca[rc_tr]=hc->getRC_tr();
                    rca[rc_br]=hc->getRC_br();

                    ///use the test output loopback to test repeaters
                    vector<uint> times(3,0);
                    vector<uint> nnums(3,0);
                    uint repnr=0, time=100, nnum=0;
                    uint repnr_max;

                    cout << "time period (not exact!): " << endl;
                    cin >> time;

                    for (char sw = 0; sw<6; sw++) {
                        cout << "------------------------" << endl << "Repeater block ";
                        rc_loc loca;

                        if (sw == 1) {
                            loca=rc_l;
                            repnr_max = 32;
                            cout << "left" << endl;
                        }
                        else if (sw == 2) {
                            loca=rc_tl;
                            repnr_max = 64;
                            cout << "top left" << endl;
                        }
                        else if (sw == 3) {
                            loca=rc_bl;
                            repnr_max = 64;
                            cout << "bottom left" << endl;
                        }
                        else if (sw == 4) {
                            loca=rc_tr;
                            repnr_max = 64;
                            cout << "top right" << endl;
                        }
                        else if (sw == 5) {
                            loca=rc_br;
                            repnr_max = 64;
                            cout << "bottom right" << endl;
                        }
                        else {
                            loca=rc_r;
                            repnr_max = 32;
                            cout << "right" << endl;
                        }
                        rca[loca]->reset();
                        rca[loca]->reset_dll();

                        for (repnr=0; repnr<repnr_max; repnr=repnr+1) {
                            cout << " Repeater number " << repnr << endl;
                            sleep(.5);

                            uint odd=0;
                            if (sw == 5 || sw == 3) {
                                odd = (repnr+1)%2;
                            }
                            else
                                odd = (repnr+1)%2;

                            rca[loca]->tout(repnr,true);

                            rca[loca]->tdata_write(0,0,nnum,time);
                            rca[loca]->tdata_write(0,1,nnum,time);
                            rca[loca]->tdata_write(0,2,nnum,time);
                            rca[loca]->tdata_write(1,0,nnum,time);
                            rca[loca]->tdata_write(1,1,nnum,time);
                            rca[loca]->tdata_write(1,2,nnum,time);

                            rca[loca]->startout(odd);

                            rca[loca]->tin(repnr,true);
                            usleep(5000); //time for the dll to lock
                            rca[loca]->stopin(odd); //reset the full flag
                            rca[loca]->startin(odd); //start recording received data to channel 0
                            usleep(10000);  //recording lasts for ca. 4 µs - 1ms

                            cout << "Results: ";
                            for (int i=0; i<3; i++){
                                rca[loca]->tdata_read(odd, i, nnums[i], times[i]);
                                cout << dec << nnums[i] << " @ " << times[i] << ", ";
                            }
                            cout << endl;
                            rca[loca]->stopin(odd); //reset the full flag, one time is not enough somehow...
                            rca[loca]->stopout(odd);

                            rca[loca]->tout(repnr, false); //set tout back to 0 to prevent conflicts
                        }
                    }
        };
};


class LteeTmAHRepLoop : public Listee<Testmode>{
public:
	LteeTmAHRepLoop() : Listee<Testmode>(string("tm_ahlooptest"),string("Testing connection to tdi and tdo of repeater blocks")){};
	Testmode * create(){return new TmAHRepLoop();};
};
LteeTmAHRepLoop ListeeTmAHRepLoop;
#endif
