// Company         :   kip
// Author          :   Andreas Grübl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//------------------------------------------------------------------------


// testmode to perform "almost full" digital test on one HICANN each while controlling the wafer tester

#include "common.h"
#include <boost/algorithm/string.hpp>

#include "hicann_ctrl.h"
#include "dnc_control.h"
#include "fpga_control.h"
#include "testmode.h" 
#include "ramtest.h"
#include "syn_trans.h"

#include "linklayer.h"

// required for checking only
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys.h"

//only if needed
#include "synapse_control.h" //synapse control class
#include "l1switch_control.h" //layer 1 switch control class
#include "repeater_control.h" //repeater control class
#include "neuron_control.h" //neuron control class (merger, background genarators)
#include "neuronbuilder_control.h" //neuron builder control class
#include "fg_control.h" //floating gate control
#include "spl1_control.h" //spl1 control class

#define USENR         1		
#define fpga_master   1
#define DATA_TX_NR    100
#define RUNS          5000

using namespace std;
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


class TmAGSpl1Loopback : public Testmode{

public:
	
	enum rc_loc {rc_l=0,rc_r,rc_tl,rc_bl,rc_tr,rc_br}; //repeater locations

	// HICANN control
  HicannCtrl  *hc;
  SPL1Control *spc;
	NeuronBuilderControl *nbc;
	NeuronControl *nc; //neuron control
	RepeaterControl *rca[6]; //array of the 6 repeater blocks
	SynapseControl *sc_top, *sc_bot;
	L1SwitchControl *lcl,*lcr,*lctl,*lcbl,*lctr,*lcbr;

	DNCControl  *dc;

	FPGAControl *fc;

	// test variables
	uint hicann_nr, hicann_jtag_nr;

	int ocp_base;
	int mem_size;
	ci_data_t rcvdata;
	ci_addr_t rcvaddr;
	uint error;

	// ********** test methods **********
	bool test_jtag_id(){
		uint64_t hicann_id = 0x14849434;
		uint64_t id=0;
		jtag->read_id(id, jtag->pos_hicann);
		cout << "HICANN ID: 0x" << hex << id << ":";
		if(id == hicann_id){
			cout << " ok :-)" << endl;
			return true;
		} else {
			cout << " fail :-(" << endl;
			return false;
		}
	}

	bool test_switchram(uint runs) {
		// get pointer to all crossbars
		lcl = &hc->getLC_cl();
		lcr = &hc->getLC_cr();
		lctl =&hc->getLC_tl();
		lcbl =&hc->getLC_bl();
		lctr =&hc->getLC_tr();
		lcbr =&hc->getLC_br();

		// full L1 switch test
		cout << endl << "########## TEST SWITCH RAM ##########" << endl;
		
		uint error=0;
		uint lerror=0;
		L1SwitchControl *l1;
		uint rmask; 
		for(int l=0;l<runs;l++){
			lerror=0;
			for(int r=0;r<6;r++){
				switch (r){
				case 0:l1=lcl;mem_size=64;rmask=15;break;
				case 1:l1=lcr;mem_size=64;rmask=15;break;
				case 2:l1=lctl;mem_size=112;rmask=0xffff;break;
				case 3:l1=lctr;mem_size=112;rmask=0xffff;break;
				case 4:l1=lcbl;mem_size=112;rmask=0xffff;break;
				case 5:l1=lcbr;mem_size=112;rmask=0xffff;break;
				}

				uint rseed = ((randomseed+(1000*r))+l);
				srand(rseed);
				uint tdata[0x80];
				for(int i=0;i<mem_size;i++)
					{
						tdata[i]=rand() & rmask;
						l1->write_cmd(i,tdata[i],0);
					}

				srand(rseed);
				for(int i=0;i<mem_size;i++)
				{
					l1->read_cmd(i,0);
					l1->get_data(rcvaddr,rcvdata);
					if(rcvdata != tdata[i]){
						error++;
						lerror++;
						cout << hex << "ERROR: row \t0x"<<i<<" adr \t0x"  << rcvaddr << ", \t0b"  << bino(rcvdata,16) << "	data written: 0b" <<
							bino(tdata[i],16)<<endl;
					}
				}
				cout <<"Switchramtest loop " << l << " errors: " << lerror << ", total errors: " << error << endl;
			}
			cout << endl;
		
		}
		return (error == 0);
	}

	bool test_spl1_loopback(bool from_fpga, uint loops){
		bool result = true;
		uint64_t systime = 0xffffffff;
		uint neuron, rec_neuron;
		uint64_t jtag_sendpulse = 0;
		uint64_t jtag_recpulse = 0;

		jtag->HICANN_stop_time_counter();
		jtag->HICANN_start_time_counter();
		jtag->FPGA_set_systime_ctrl(1);

		cout << endl << "########## TEST SPL1 LOOPBACK via " << (from_fpga?"FPGA":"JTAG") << " ##########" << endl;
		
		nc->nc_reset();		
		nc->dnc_enable_set(1,0,1,0,1,0,1,0);
		nc->loopback_on(0,1);
		nc->loopback_on(2,3);
		nc->loopback_on(4,5);
		nc->loopback_on(6,7);
						
		spc->write_cfg(0x55ff);
			
		if(from_fpga){
      //clear routing memory
      printf("tmag_spl1_loopback: Clearing routing memory\n");
      for(int addr=0;addr<64;++addr)
      	jtag->FPGA_set_routing_data(hicann_nr,(1<<6)+addr,0); // l1 channel 1
      for(int addr=0;addr<64;++addr)
      	jtag->FPGA_set_routing_data(hicann_nr,(3<<6)+addr,0); // l1 channel 3

			// set DNC to ignore time stamps and directly forward l2 pulses
			dc->setTimeStampCtrl(0x0);
			// set transmission directions in DNC (for HICANN 0 only)
//			uint64_t dirs = (uint64_t)0x55<<(hicann_nr*8);
			uint64_t dirs = 0x5555555555555555ULL;
			cout << "dirs = 0x" << hex << dirs << endl;
      dc->setDirection(dirs); // set transmission directions in DNC (for HICANN 0 only)
      //jtag->DNC_set_loopback(0xff); // DNC loopback
		} else {
			jtag->HICANN_set_test(0x2);
			jtag->HICANN_set_double_pulse(0);
		}
					
		srand(randomseed);
		
		for(uint ch=0;ch<8;ch+=2){
			uint errors = 0;
			for(uint loop=0;loop<loops;loop++){
				uint nrn = rand()%64;

				jtag->HICANN_read_systime(systime);
  			neuron = (ch << 6) | nrn;
				jtag_sendpulse = (neuron<<15) | (systime+1000)&0x7fff; 
		
				if(from_fpga){
					fc->sendSingleL2Pulse(0,jtag_sendpulse);
					fc->getFpgaRxData(jtag_recpulse);
				} else {
					jtag->HICANN_start_pulse_packet(jtag_sendpulse);
					jtag->HICANN_get_rx_data(jtag_recpulse);
				}
		
				rec_neuron = (jtag_recpulse>>15)&0x1ff;
				if(neuron+64 != rec_neuron){
					cout << "ERROR: sent 0x" << hex << neuron << ", got 0x" << rec_neuron << ", mask 0x" << (neuron^rec_neuron) << 
			    		  " | RAW: sent 0x" << jtag_sendpulse << ", got 0x" << jtag_recpulse << endl;;
					result = false;
					errors++;
				}
			}
			cout << "###SPL1 Loopback Channel " << ch << ", ERRORS: " << errors << endl;
		}
  	return result;
	}
				
	
	// ######################## main test method #########################
	bool test() 
  {
    if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
      cout << "tm_wafertest: ERROR: object 'chip' in testmode not set, abort" << endl;
      return 0;
    }
    if (!jtag) {
      cout << "tm_wafertest: ERROR: object 'jtag' in testmode not set, abort" << endl;
      return 0;
    }

    // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
    hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT+0];
		
		hicann_jtag_nr = hc->addr();
		hicann_nr      = 7-hicann_jtag_nr;
		jtag->set_hicann_pos(hicann_jtag_nr);
    
		// use DNC
		dc = (DNCControl*) chip[FPGA_COUNT];
		
		// use FPGA
		fc = (FPGAControl*) chip[0];
		
		// get pointers to control modules
		spc  = &hc->getSPL1Control();
		nc   = &hc->getNC();
		nbc  = &hc->getNBC();

		lcl = &hc->getLC_cl();  // get pointer to layer 1 switch control class of HICANN 0
		lcr = &hc->getLC_cr();
		lctl= &hc->getLC_tl();
		lcbl= &hc->getLC_bl();
		lctr= &hc->getLC_tr();
		lcbr= &hc->getLC_br();

		rca[rc_l] =&hc->getRC_cl(); // get pointer to repeater of HICANN 0
		rca[rc_r] =&hc->getRC_cr();
		rca[rc_tl]=&hc->getRC_tl();
		rca[rc_bl]=&hc->getRC_bl();
		rca[rc_tr]=&hc->getRC_tr();
		rca[rc_br]=&hc->getRC_br();
		
		// Reset HICANN and JTAG
    jtag->reset_jtag();
		jtag->FPGA_set_fpga_ctrl(1);
    jtag->reset_jtag();
		cout << "Try Init() ..." ;
		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
			cout << "ERROR: Init failed, continue with next HICANN..." << endl;
		}
		cout << "Init() ok" << endl;

		cout << "resetting crossbars and switches" << endl;
		lcl->reset();
		lcr->reset();
		lctl->reset();
		lctr->reset();
		lcbl->reset();
		lcbr->reset();
		cout << "resetting repeater blocks" << endl;
		rca[rc_l]->reset();
		rca[rc_tl]->reset();
		rca[rc_tr]->reset();
		rca[rc_bl]->reset();
		rca[rc_br]->reset();
		rca[rc_r]->reset();
		cout << "resetting neuron builder" << endl;
		nbc->initzeros();
		cout << "resetting neuron control" << endl;
		nc->nc_reset();

		bool fpga_comm = dynamic_cast<S2C_JtagPhys2Fpga*>(comm) ? true : false;

		// ############## start test options loop ############
    char c;
    bool cont=true;
    do{

      cout << "Select test option:" << endl;
      cout << "0: JTAG reset only " << endl;
      cout << "1: reset JTAG and system " << endl;
      cout << "2: init comm" << endl;
      cout << "3: test spl1 loopback" << endl;
      cout << "4: test switch memory" << endl;
      cout << "5: read HICANN JTAG ID" << endl;
      cout << "6: perform switchramtest like in tmag_switchramtest" << endl;
/*      cout << "7: re-construct JTAG class" << endl;
      cout << "8: re-construct Comm class" << endl;
      cout << "9: re-construct HICANN class" << endl;*/
      cout << "x: exit" << endl;

      cin >> c;

      switch(c){
		
      case '0':{
      	jtag->reset_jtag();
      } break;

      case '1':{
				// Reset HICANN and JTAG
				jtag->set_hicann_pos(hc->addr());
      	jtag->reset_jtag();
				jtag->FPGA_set_fpga_ctrl(1);
      	jtag->reset_jtag();
      } break;

      case '2':{
			 	dbg(0) << "Try Init() ..." ;
				if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
				 	dbg(0) << "ERROR: Init failed, abort" << endl;
					return 0;
				}
			 	dbg(0) << "Init() ok" << endl;
// 				jtag->set_hicann_pos(hc->addr());
// 	
// 				if(dynamic_cast<S2C_JtagPhys2Fpga*>(comm)) comm->fpga_dnc_init((jtag->pos_dnc-1)-hc->addr(), true);
// 
// 				//initialize JTAG read access:
// 				comm->Clear();
// 
// 				jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
// 				jtag->HICANN_arq_write_ctrl(0x0, 0x0);
// 
// 				for(uint t=0;t<2;t++) hc->tags[t]->arq.Reset();
      } break;

      case '3':{
				uint spl1loops = 512;

				cout << "Testing SPl1 loopback... " << flush;
				cout << test_spl1_loopback(fpga_comm, spl1loops) << endl;
      } break;
			
      case '4':{
				uint loops = 1;
				cout << "Testing   Switch ram... " << flush;
				cout << test_switchram(loops) << endl;
      } break;
			
      case '5':{
				uint64_t jtagid=0xf;
				jtag->read_id(jtagid,jtag->pos_hicann);
				cout << "HICANN ID: 0x" << hex << jtagid << endl;
      } break;

      case '6':{
			 	dbg(0) << "Try Init() ..." ;
				if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
				 	dbg(0) << "ERROR: Init failed, abort" << endl;
					return 0;
				}
			 	dbg(0) << "Init() ok" << endl;

				uint64_t jtagid=0xf;
				jtag->read_id(jtagid,jtag->pos_hicann);
				cout << "HICANN ID: 0x" << hex << jtagid << endl;

				uint startaddr = 0;
				uint maxaddr   = 63;
				uint datawidth = 4;

				uint testdata = 0;
				rcvdata  = 0;
				rcvaddr  = 0;

				srand(2);

				for(uint i=startaddr;i<=maxaddr;i++){
					testdata = rand()%(1<<datawidth);
					lcl->write_cfg(i, testdata);
					lcl->read_cfg(i);
					lcl->get_read_cfg(rcvaddr, rcvdata);
			
					cout << hex << "test \t0x" << i << ", \t0x" << testdata << " against \t0x" << rcvaddr << ", \t0x" << rcvdata << ": ";
			
					if(i==rcvaddr && testdata==rcvdata)
						cout << "OK :-)" << endl;
					else{
						cout << "ERROR :-(" << endl;
					}
				}
      } break;

/*      case '7':{
				std::vector<uint32_t> irwidth = jtag->irwidth;
				uint32_t chain_length = jtag->chain_length;
				uint32_t irw_total    = jtag->irw_total;
				uint32_t pos_fpga     = jtag->pos_fpga;
				uint32_t pos_dnc      = jtag->pos_dnc;
				uint32_t pos_hicann   = jtag->pos_hicann;
		

				delete jtag;
    		jtag = new myjtag_full();

				jtag->irwidth        = irwidth;
				jtag->chain_length   = chain_length;
				jtag->irw_total	     = irw_total;   
				jtag->pos_fpga 	     = pos_fpga;    
				jtag->pos_dnc		     = pos_dnc;     
				jtag->pos_hicann     = pos_hicann;  
				jtag->pulsefifo_mode = 0;
				
				jtag->actual_command.clear();
				for(uint i=0;i<chain_length;++i) {
					jtag->actual_command.push_back(0xffffffff);
				}
				
      } break;
			
      case '8':{
				if(dynamic_cast<S2C_JtagPhys2Fpga*>(comm)){
					delete comm;
					S2C_JtagPhys2Fpga* jtag_p2f = new S2C_JtagPhys2Fpga(jtag);
					comm = static_cast<Stage2Comm*>(jtag_p2f);
				} else {
					delete comm;
    			S2C_JtagPhys* jtag_p = new S2C_JtagPhys(jtag);;
				  comm = static_cast<Stage2Comm*>(jtag_p);
				}
      } break;
			
      case '9':{
				uint hicann_addr = hc->addr();
				delete hc;
				hc   = new HicannCtrl(comm,hicann_addr);
		    spc  = hc->getSPL1Control();
		    nc   = hc->getNC();
				lctl = hc->getLC_tl();
      } break; */
			
      case 'x': cont = false; 
      }

    } while(cont);

    return true;
  }

};


class LteeTmAGSpl1Loopback : public Listee<Testmode>{
public:
  LteeTmAGSpl1Loopback() : Listee<Testmode>(string("tmag_spl1_loopback"),string("test spl1 loopback")){};
  Testmode * create(){return new TmAGSpl1Loopback();};
};

LteeTmAGSpl1Loopback ListeeTmAGSpl1Loopback;

