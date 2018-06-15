#pragma once

#ifdef NCSIM
#include "systemc.h"
#endif

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <boost/format.hpp>
#include <sys/stat.h>
#include "ctrlmod.h"

namespace s2pp {
    enum Reset_mode {
        RESET_OFF = 0x0,
        RESET_ON = 0x1,
        CLK_FORCE_OFF = 0x2,
        CLK_FORCE_ON = 0x4
    };

	template<typename Iterator>
	Iterator read_bin(std::istream& strm, Iterator a, Iterator b) {
		Iterator i;
		uint8_t val[sizeof(uint32_t)];

		if( !strm.good() )
			throw std::runtime_error( (boost::format("%s: Stream not good")
						% __PRETTY_FUNCTION__).str() );

		//if( a == b )
			//std::cout << "reading nothing" << std::endl;

		for(i=a; (i!=b) && strm.good(); i++) {
			strm.read(reinterpret_cast<char*>(&val), sizeof(uint32_t));

			*i = 0;
			for(int j=sizeof(uint32_t)-1; j>=0; j--) {
				*i = *i | (val[sizeof(uint32_t)-1-j] << (j*8));

				//std::cout << "r: 0x" << std::hex << static_cast<unsigned int>(val[j]) << ' ';
			}

			//std::cout << ".\n";
		}

		return i;
	}

	template<typename Iterator>
	void write_bin(std::ostream& strm, Iterator a, Iterator b) {
		if( !strm.good() )
			throw std::runtime_error( (boost::format("%s: Stream not good")
						% __PRETTY_FUNCTION__).str() );

		Iterator i;
		for(i=a; (i != b) && strm.good(); i++) {
			for(int j=sizeof(uint32_t)-1; j>=0; j--) {
				strm.put( (*i >> (j*8)) & 0xff );
			}
		}
	}

	template<typename Iterator>
	void write_block(CtrlModule* cm, const unsigned int& addr, Iterator a, Iterator b) {
		for(int i=0; a != b; a++, i++)
			cm->write_cmd(addr + i, *a, 0);
	}

	template<typename Iterator>
	void read_block(CtrlModule* cm, const unsigned int& addr, Iterator a, Iterator b) {
		ci_payload res;

		for(int i=0; a != b; a++, i++) {
			cm->read_cmd(addr + i, 0);
			while( cm->get_data(&res) == 0 ) {
#ifdef NCSIM
				wait(10, SC_NS);
#endif
			}
			*a = res.data;
		}
	}

	extern void core_reset(CtrlModule* cm, int mode);
    extern bool sleeping(CtrlModule* cm);
    extern unsigned int load_program(CtrlModule* cm, const std::string& filename); 

	template<typename Container>
	unsigned int read_program(std::string const& filename, Container& container) {
        using namespace std;
        
        struct stat st;

        if( stat(filename.c_str(), &st) != 0 ) 
            throw runtime_error(
                    (boost::format("%s: Cannot stat '%s'")
                     % __PRETTY_FUNCTION__
                     % filename).str());

        unsigned int const code_size = st.st_size / sizeof(typename Container::value_type);

		container.clear();
		container.resize(code_size);
        ifstream fin(filename.c_str(), ios_base::binary);

        if( !fin.good() )
            throw runtime_error(
                    (boost::format("%s: Opening file '%s' for binary input failed") 
                     % __PRETTY_FUNCTION__
                     % filename).str());

        s2pp::read_bin(fin, container.begin(), container.end());

        return code_size;
	}
}

// vim: noexpandtab ts=4 sw=4 softtabstop=0 nosmarttab:
