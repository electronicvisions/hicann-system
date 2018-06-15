#pragma once

// functionality:
// * log access to hardware
// * TODO: authorize access


#include <boost/scoped_ptr.hpp>
#include "HMFRunLog.h"

struct CommAccessImpl;

class CommAccess {
public:
	// Constructors requests hardware sub-parts
	// "normal" mode -- based on ip of FPGA
	CommAccess(FPGAConnectionId::IPv4 const& ip);

	// jtag mode -- needs more connection details (hidden in FPGAConnectionId)
	CommAccess(FPGAConnectionId const & connid);
	CommAccess(FPGAConnectionId const & connid, FPGAConnectionId::IPv4 const& pmu_ip);

	FPGAConnectionId const getFPGAConnectionId() const;
	FPGAConnectionId::IPv4 const getPMUIP() const;

	// Destructor relinquishes hardware sub-part
	~CommAccess();

#if __cplusplus >= 201103L
	CommAccess() = delete;
	// we manage resources, so we don't like being copied and stuff
	CommAccess(CommAccess const &) = delete;
	CommAccess(CommAccess &&) = delete;
	CommAccess& operator=(CommAccess const &) = delete;
	CommAccess& operator=(CommAccess &&) = delete;
#else // old C++
private:
	CommAccess();
	CommAccess(CommAccess const &);
	CommAccess& operator=(CommAccess const &);
public:
#endif

private:
	mutable struct boost::scoped_ptr<CommAccessImpl> pimpl;
};
