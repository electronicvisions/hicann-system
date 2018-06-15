// Company         :   kip
// Author          :   Andreas Gruebl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   tmag_jtagchainrnd.cpp
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

class TmJtagChainRnd : public Testmode {
protected:
	virtual string ClassName() {stringstream ss; ss << "tmag_jtagchainrnd"; return ss.str();}
public:

	// -----------------------------------------------------
	// test function
	// -----------------------------------------------------

	bool test() {

		vector<ReticleControl*> ret;

		bool result=true;
		bool ignore_first=false;
		uint64_t shift_offset = 0;
		uint loops=1000; // default value
		uint errors=0;
		uint64_t testin, testout;

		// testmode options
		std::vector<boost::program_options::basic_option<char> >::iterator it;
		for (it = argv_options->options.begin(); it != argv_options->options.end(); it++) {
			if (std::string("num").compare(it->string_key) == 0) {
				std::istringstream buffer(it->value[0]);
				buffer >> loops;
			} else if (std::string("ignore_first").compare(it->string_key) == 0) {
				ignore_first=true;
				cout << "CAUTION ... IGNORE_FIRST turned on ... HACKY, HACKY, FRICKELEI !!!" << endl;
			} else if (std::string("ignore_msbs").compare(it->string_key) == 0) {
				shift_offset = 9;
				cout << "CAUTION ... IGNORE_MSBS turned on ... even more HACKY, HACKY, FRICKELEI !!!" << endl;
			}
		}
		cout << "Number of loops set to " << dec << loops << endl;

		srand(1);
		for(uint i=0; i<loops; i++){
			// random test value generation
			testin  = (uint64_t)rand();
			testin |= ((uint64_t)rand() << (32-shift_offset));
			// chain test
			jtag->bypass<uint64_t>(testin, 64, testout);
			// result shift to compensate chain length (CAUTION, CUTS UPPER BITS!)
			testout = testout>>(jtag->pos_fpga+1);
			// result compare
			if (testin != testout) {
				cout << setfill('0') << "ERROR: pos: " << setw(4) << dec << i << ", in=0x" << hex << setw(16) << testin << ", out=0x" << testout << ", xor=0x" << (testin^testout) << endl;
				if (ignore_first && i==0) {
					cout << "IGNORED due to --ignore_first flag" << endl;
				} else {
					errors++;
				}
			}
		}
		if (errors==0) {
			cout << endl << "All good :-)" << endl;
		} else {
			cout << endl << "Total " << dec << errors << " errors out of " << loops << " tests." << endl;
			result = false;
		}
		return result;
	}
};


class LteeTmJtagChainRnd : public Listee<Testmode>{
public:
	LteeTmJtagChainRnd() : Listee<Testmode>(string("tmag_jtagchainrnd"), string("jtag chain test")){};
	Testmode * create(){return new TmJtagChainRnd();};
};

LteeTmJtagChainRnd ListeeTmJtagChainRnd;

