#include "FPGAConnectionId.h"

typedef FPGAConnectionId::IPv4 IPv4;
typedef FPGAConnectionId::UDPPort UDPPort;

FPGAConnectionId::FPGAConnectionId(IPv4 const & ip) :
	fpga_ip(ip)
{}

FPGAConnectionId::FPGAConnectionId(IPv4::bytes_type const & ip) :
	fpga_ip(ip)
{}

IPv4 const FPGAConnectionId::get_fpga_ip() const {
	return fpga_ip;
}

void FPGAConnectionId::set_jtag_port(UDPPort const p) {
	fpga_jtag_port = p;
}

UDPPort FPGAConnectionId::get_jtag_port() const {
	return fpga_jtag_port;
}

bool FPGAConnectionId::operator==(FPGAConnectionId const & b) const {
	return ((fpga_ip == b.fpga_ip) &&
		(fpga_jtag_port == b.fpga_jtag_port));
}

#include <boost/functional/hash.hpp>
std::size_t hash_value(FPGAConnectionId const & f) {
	std::size_t s = 0;
	boost::hash_combine(s, f.fpga_ip.to_ulong());
	boost::hash_combine(s, f.fpga_jtag_port);
	return s;
}
