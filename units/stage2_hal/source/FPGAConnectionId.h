#pragma once

#include <boost/asio/ip/address.hpp>

struct FPGAConnectionId {
	typedef boost::asio::ip::address_v4 IPv4;
	typedef uint16_t UDPPort;

	FPGAConnectionId(IPv4 const & ip); // implicit conversion allowed
	explicit FPGAConnectionId(IPv4::bytes_type const &);

#if __cplusplus >= 201103L
	// we are pod :)
	FPGAConnectionId() = delete;
	FPGAConnectionId(FPGAConnectionId const &) = default;
	FPGAConnectionId(FPGAConnectionId &&) = default;
	FPGAConnectionId & operator=(FPGAConnectionId const &) = default;
	FPGAConnectionId & operator=(FPGAConnectionId &&) = default;
	~FPGAConnectionId() = default;
#else // old C++
private:
	FPGAConnectionId();
public:
#endif

	IPv4 const get_fpga_ip() const;

	// TODO: we could disable building this by default
	// debug access via jtag needs additional port
	void set_jtag_port(UDPPort const);
	UDPPort get_jtag_port() const;

	bool operator==(FPGAConnectionId const &) const;
	friend std::size_t hash_value(FPGAConnectionId const &);

private:
	IPv4 fpga_ip;
	UDPPort fpga_jtag_port;
};
