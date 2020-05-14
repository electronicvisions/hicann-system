#include "types-fpga.h"

// TODO: add tests?

// SF Serialization
#include <SF/Archive.hpp>

void serialize(SF::Archive &ar, fpga_pulse_packet_t& p) {
	// note: p is already allocated (and correctly aligned!)
	if (ar.isWrite()) {
		// calc length of user data
		size_t const byteCnt = reinterpret_cast<char*>(&(p.entry[p.N])) - reinterpret_cast<char*>(&p);
		ar & byteCnt;
		ar.getOstream()->writeRaw(reinterpret_cast<char*>(&p), byteCnt);
	} else {
		size_t byteCnt;
		ar & byteCnt;
		ar.getIstream()->read(reinterpret_cast<char*>(&p), byteCnt);
	}
}