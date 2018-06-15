// Company         :   kip
// Author          :   Andreas Gruebl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tmag_jtagchaintest.cpp
//                    			
// Create Date     :   18.04.2012
//------------------------------------------------------------------------

#include "common.h"
#include "testmode.h"
#include "reticle_control.h"

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class TmJtagChainTest : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmag_jtagchaintest"; return ss.str();}
public:


	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() {
	
	vector<ReticleControl*> ret;
	
    char c;
    bool cont=true;
    do{

		cout << "Select test option:" << endl;
		cout << "1: reset jtag " << endl;
		cout << "2: JTAG bypass with user input" << endl;
		cout << "3: auto JTAG bypass test" << endl;
		cout << "4: perform vector bypass to detect chain interrupt" << endl;
		cout << "5: continuous simultaneous testing for all 4 DNC groups (with -bjw!)" << endl;
		cout << "6: low level JTAG reset and ID shift" << endl;
		cout << "7: like 6 with 32 bit granularity" << endl;
		cout << "8: JTAG reset and shift IR only, read back" << endl;
		cout << "9: Simulating errorneous behavior" << endl;
		cout << "a: set DNC LVDS enable bits" << endl;
		cout << "b: set HICANN LVDS enable bits" << endl;
		cout << "c: read HICANN JTAG ID using jtag_lib function" << endl;
		cout << "x: exit" << endl;

		cin >> c;

		switch(c){

		case '1':{
			jtag->reset_jtag();
		} break;

		case '2':{
			uint64_t testin=0;
			uint64_t testout=0;
			cout << "64Bit HEX-value for testing? "; cin >> hex >> testin;
			
			jtag->bypass<uint64_t>(testin, 64, testout);
			cout << "testout before shift: 0x" << hex << testout << endl;
			testout = testout>>(jtag->pos_fpga+1);
			
			cout << "result: 0x" << hex << testout << ", xor: 0x" << (testin^testout) << endl;
		} break;

		case '3':{
			uint loops=0;
			uint64_t testin, testout;
			
			cout << "Number of 64Bit random loops? "; cin >> loops;
			
			srand(1);
			for(uint i=0; i<loops; i++){
				testin  = rand();
				testin |= (uint64_t)rand();
				
				jtag->bypass<uint64_t>(testin, 64, testout);
				testout = testout>>(jtag->pos_fpga+1);
				
				if (testin != testout) {
					cout << "ERROR: in=0x" << hex << testin << ", out=0x" << testout << ", xor=0x" << (testin^testout) << endl;
				}
			}
		} break;

		case '4':{
			uint length=640;
			bool found=false;
			
			vector<bool> testin, testout;
			testin.clear();
			testout.clear();
			srand(1);
			for(uint i=0; i<length; i++){
				testin.push_back(rand()%2);
			}
			
			jtag->bypass(testin, testout);
			
			for (uint shift=0; shift<640-32; shift++){
				for(uint c=0; c<(uint)((length-shift)/32); c++){
					uint out=0;
					
					for(uint i=0; i<32; i++){
						out |= testout[c*32+i+shift]<<i;
					}
					if (out == 0xbabebeef) cout << "found FPGA ID, shift=" << shift << ", byte=" << c << endl;
					if (out == 0x1474346f) cout << "found DNC ID, shift=" << shift << ", byte=" << c << endl;
					if (out == 0x14849434) cout << "found HICANN ID, shift=" << shift << ", byte=" << c << endl;
				}
				if (found) break;
			}
		} break;
		
		case '6':{
			vector<uint> res_temp;
			res_temp.clear();
			
			uint shift_width=704;
	

			jtag->jtag_write(0,1,50);
	
			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			jtag->jtag_write(0,1);

			// Go to CAPTURE-DR
			jtag->jtag_write(0,0);	

			// Go to SHIFT-DR
			jtag->jtag_write(0,0);

			for(uint i=0; i < shift_width; i++)
				{
					jtag->jtag_write( 0, i==shift_width-1, jtag_lib::TDO_READ );
				}

			// Go to UPDATE-DR - no effect
			jtag->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			jtag->jtag_execute();

			// read bulk tdo buffer
			for(uint i=0;i<shift_width/4;i++){
				uint temp=0;
				jtag->jtag_read(temp, 4);
				res_temp.push_back(temp);
			}
			
			uint count=0;
			vector<uint>::iterator it;
			cout << "result: 0x";
			for(it=res_temp.end(); it >= res_temp.begin(); it--){
				cout << hex << ((*it)&0xf);
				count++;
			}
			cout << ", count: " << count << endl;
			
			
		} break;
		

		case 'c':{
			uint64_t jtagid=0xf;
			jtag->read_id(jtagid,jtag->pos_hicann);
			cout << "HICANN ID: 0x" << hex << jtagid << endl;
		} break;

		case '9':{
			vector<uint> res_temp;
			res_temp.clear();
			
			uint shift_width=704;
	

			jtag->jtag_write(0,1,10);
	
			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			jtag->jtag_write(0,1);

			// Go to CAPTURE-DR
			jtag->jtag_write(0,0);	

			// Go to SHIFT-DR
			jtag->jtag_write(0,0);

			for(uint i=0; i < shift_width; i++)
				{
					jtag->jtag_write( 0, !(i%6), jtag_lib::TDO_READ );
					jtag->jtag_set(0,!(i%2),1);
					jtag->jtag_set(0,i%2,0);
					jtag->jtag_set(0,i%2,1);
					jtag->jtag_set(0,!(i%2),0);
					jtag->jtag_execute();
				}

			// Go to UPDATE-DR - no effect
			jtag->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			jtag->jtag_execute();

			// read bulk tdo buffer
			for(uint i=0;i<shift_width/4;i++){
				uint temp=0;
				jtag->jtag_read(temp, 4);
				res_temp.push_back(temp);
			}
			
			vector<uint>::iterator it;
			cout << "result: 0x";
			for(it=res_temp.end(); it >= res_temp.begin(); it--){
				cout << hex << ((*it)&0xf);
			}
			cout << endl;
			
			
		} break;
		
		case '8':{
//		while(1){
			vector<uint> tgt = boost::assign::list_of(0xd)(0x7)(0xf)(0xd)(0x7)(0xf)(0xd)(0x7)(0xf)(0xd)(0x7)(0xf)(0xd)(0x7)(0xf);
			vector<uint> res_temp;
			res_temp.clear();
			
			uint shift_width=60;
	
			while(!kbhit()){
			jtag->jtag_write(0,1,10);
	
			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			jtag->jtag_write(0,1);

			// Go to SELECT-IR-SCAN
			jtag->jtag_write(0,1);

			// Go to CAPTURE-IR
			jtag->jtag_write(0,0);	

			// Go to SHIFT-IR
			jtag->jtag_write(0,0);

			for(uint i=0; i < shift_width; i++)
				{
					jtag->jtag_write( 0, i==shift_width-1, jtag_lib::TDO_READ );
				}

			// Go to UPDATE-IR - no effect
			jtag->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			jtag->jtag_execute();
			// read bulk tdo buffer
			for(uint i=0;i<shift_width/4;i++){
				uint temp=0;
				jtag->jtag_read(temp, 4);
				res_temp.push_back(temp);
			}
//			}
			
			uint errors = 0;
			cout << "result: 0x";
			for(int l=((res_temp.size())-1); l>=0; --l){
				if(((res_temp[l])&0xf) != ((tgt[l])&0xf) && ((res_temp[l])&0xf) != 0) errors++;
				cout << hex << ((res_temp[l])&0xf);
			}
			cout << " ,  errs: " << errors << endl;
			res_temp.clear();
			//if(errors==0) break;
		}
			
		} break;
		
		case '7':{
			vector<uint> res_temp;
			
			uint shift_width=672;
	
			jtag->jtag_write(0,1,4);
	
			jtag->jtag_execute();

			jtag->jtag_write(0,1,4);
	
			jtag->jtag_execute();

			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			// Go to SELECT-DR-SCAN
			jtag->jtag_write(0,1);

			// Go to CAPTURE-DR
			jtag->jtag_write(0,0);	

			// Go to SHIFT-DR
			jtag->jtag_write(0,0);

			jtag->jtag_execute();

			vector<uint>::iterator it;
			cout << "result: 0x";

			res_temp.clear();
			for(uint g=0; g < shift_width/4; g++) {
				for(uint i=0; i < 4; i++) {
					jtag->jtag_write( 0, i==shift_width-1, jtag_lib::TDO_READ );
				}
				jtag->jtag_execute();
				// read bulk tdo buffer
				for(uint i=0;i<1;i++){
					uint temp=0;
					jtag->jtag_read(temp, 4);
					res_temp.push_back(temp);
				}
			}

			for(it=res_temp.end(); it >= res_temp.begin(); it--){
				cout << hex << ((*it)&0xf);
			}
			cout << endl;
			
			// Go to UPDATE-DR - no effect
			jtag->jtag_write(0,1);

			// Go to RUN-TEST/IDLE
			jtag->jtag_write(0,0);

			jtag->jtag_execute();

		} break;
		
		case '5':{
			vector<ReticleControl*>::iterator it;
			long run=0;
			std::map<vector<ReticleControl*>::iterator, long> fails;
			
			//create reticles
			ReticleControl *tmp;
			//reticles 37-40
			//~ tmp=new ReticleControl(commodel,1,4);
			//~ ret.push_back(tmp);
			//~ ret.push_back(ret[0]->createNeighbor(1,5));
			//~ ret.push_back(ret[0]->createNeighbor(0,4));
			//~ ret.push_back(ret[0]->createNeighbor(2,4));
			
			//reticles 25-28
			tmp=new ReticleControl(commodel,5,6);
			ret.push_back(tmp);
			ret.push_back(ret[0]->createNeighbor(5,7));
			ret.push_back(ret[0]->createNeighbor(4,6));
			ret.push_back(ret[0]->createNeighbor(6,6));
			
			for (it=ret.begin(); it != ret.end(); it++){
				fails[it]=0;
			}
			
			//perform tests
			srand(1);
			while (1){
				uint length=640;
				bool found=false, unequal=false;
				vector<bool> testin, testout;
				uint64_t tin, tout;
				
				run++;
				cout << "Run " << run << endl;
				for (it=ret.begin(); it != ret.end(); it++){
					found=false, unequal=false;
					uint out=0, hicanns=0, dncs=0, fpgas=0, s=0;
					testin.clear();
					testout.clear();

					for(uint i=0; i<length; i++){
						testin.push_back(rand()%2);
					}
					
					(*it)->jtag->reset_jtag();
					(*it)->jtag->bypass(testin, testout);
					
					for (uint shift=0; shift<length-32; shift++){
						for(uint c=0; c<(length-shift)/32; c++){
							out=0;
							
							for(uint i=0; i<32; i++){
								out |= testout[c*32+i+shift]<<i;
							}
							
							if (out == 0xbabebeef) {fpgas++; found=true;}
							if (out == 0x1474346f) {dncs++; found=true;}
							if (out == 0x14849434) {hicanns++; found=true;}
						}
						if (found) break;
					}
					
					tin  = rand();
					tin |= (uint64_t)rand();
					(*it)->jtag->bypass<uint64_t>(tin, 64, tout);
					tout = tout>>((*it)->jtag->pos_fpga+1);
				
					cout << "For reticle " << dec << (*it)->reticleID(ReticleControl::powernumber) <<
					" found IDs: FPGA: " << fpgas << ", DNC: " << dncs << ", HICANN: " << hicanns << ", ";
					if (tin != tout){
						fails[it]++;
						cout << "testin!=testout, xor=0x" << hex << (tin^tout) << dec << "                    "<< endl;
					}
					else cout << "testin=testout                            " << endl;
				}
				
				if(kbhit()) // if keyboard has input pending, break from loop
					{
						cout << "ending monitoring" << endl;
						getch(); // "resets" kbhit by removing a key from the buffer
						break;
					}
				cout << "Total fails: ";
				for (it=ret.begin(); it != ret.end(); it++){
					cout << fails[it] << ", ";
				}
				cout << "(out of " << run << " runs)" << endl;
				
				cout << "\033[6A"; // linux shell cursor movement "trick" to go 5 lines up
				usleep(100000);
			}

			//delete reticles
			for (it=ret.begin(); it < ret.end(); it++){
				delete (*it);
			}
			ret.clear();
			ReticleControl::printInstantiated();
		} break;

		case 'a':{
			unsigned short lvds_en;
			cout << "LVDS enable pattern in DNC (HEX: 1bit FPGA, 8bit HICANN)? ";
			cin  >> hex >> lvds_en;
			cout << "read: 0x" << hex << lvds_en << endl;
			jtag->DNC_lvds_pads_en(lvds_en);
		} break;

		case 'b':{
			unsigned short lvds_en;
			cout << "LVDS enable pattern in HICANN (HEX: 8bit for all HICANNs)? ";
			cin  >> hex >> lvds_en;
			cout << "read: 0x" << hex << lvds_en << endl;
			for(uint i=0; i<8; i++){
				jtag->HICANN_lvds_pads_en(lvds_en&0x1);
				lvds_en >>= 1;
			}
		} break;

		case 'x': cont = false; 
}

    } while(cont);

    return true;
  }

};


class LteeTmJtagChainTest : public Listee<Testmode>{
public:
	LteeTmJtagChainTest() : Listee<Testmode>(string("tmag_jtagchaintest"), string("ramtest of L1 switch configuration memory")){};
	Testmode * create(){return new TmJtagChainTest();};
};

LteeTmJtagChainTest ListeeTmJtagChainTest;

