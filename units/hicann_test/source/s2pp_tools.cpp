
#include "s2pp_tools.h"

namespace s2pp {

	void core_reset(CtrlModule* cm, int mode) {
		static const unsigned int reset_reg_addr = 0x4000;
		//static const unsigned int reset_reg_addr = 0x8000;

		uint32_t rst_val = (mode & RESET_ON) ? 0x0 : 0x1;
		rst_val |= (mode & CLK_FORCE_OFF) ? 0x00 : 0x20;
		rst_val |= (mode & CLK_FORCE_ON) ? 0x10 : 0x00;
		cm->write_cmd(reset_reg_addr, rst_val, 0);
	}

    bool sleeping(CtrlModule* cm) {
		using namespace std;

        static const unsigned int status_reg_addr = 0x4001;
        //static const unsigned int status_reg_addr = 0x8001;

        //ci_payload dummy;
        //while( cm->get_data(&dummy, false) ) {
            //cout << "unexpected read data: " << dummy.data << endl;
        //}

        uint32_t status_reg;
        ci_payload res;
        cout << "checking sleep state" << endl;
        cm->read_cmd(status_reg_addr, 0);
        int ok = 0;
        do {
            cout << "get_data for sleep " << ok << endl;
#ifdef NCSIM
            wait(10, SC_NS);
#endif
        } while( (ok = cm->get_data(&res)) == 0 );

        if( res.data & 0x1 ) {
            cout << "now awake (" << res.data << ")" << endl;
            return true;
        } else {
            cout << "still sleeping (" << res.data << ")" << endl;
            return false;
        }
    }

    unsigned int load_program(CtrlModule* cm, const std::string& filename) {
        using namespace std;
        
        static const unsigned int mem_base = 0;
        static const unsigned int mem_size = 3 * 1024;
        static const unsigned int nop_symbol = 0x60000000;

        struct stat st;

        if( stat(filename.c_str(), &st) != 0 ) 
            throw runtime_error(
                    (boost::format("%s: Cannot stat '%s'")
                     % __PRETTY_FUNCTION__
                     % filename).str());

        unsigned int const code_size = st.st_size / sizeof(uint32_t);

        vector<uint32_t> mem_img(code_size);
        ifstream fin(filename.c_str(), ios_base::binary);

        if( !fin.good() )
            throw runtime_error(
                    (boost::format("%s: Opening file '%s' for binary input failed") 
                     % __PRETTY_FUNCTION__
                     % filename).str());

        s2pp::read_bin(fin, mem_img.begin(), mem_img.end());

        //log << "Writing memory image..." << endl;
        write_block(cm, mem_base, mem_img.begin(), mem_img.end());
        //log << "Writing memory image done." << endl;


        //if( code_size < mem_size ) {
            //log << "Padding with zeros..." << endl;
            //vector<uint32_t> zeros(mem_size - code_size, 0);
            //write_block(cm, mem_base + code_size, zeros.begin(), zeros.end());
        //}
        
        if( code_size < mem_size ) {
            for(int i=0; i<3 && (code_size + i < mem_size); i++)
                cm->write_cmd(mem_base + code_size + i, nop_symbol, 0);
            
            //cm->write_cmd(mem_base + mem_size - 2, 0, 0);
            //cm->write_cmd(mem_base + mem_size - 1, 0, 0);
        }

        return code_size;
    }

}


// vim: noexpandtab ts=4 sw=4 softtabstop=0 nosmarttab:
