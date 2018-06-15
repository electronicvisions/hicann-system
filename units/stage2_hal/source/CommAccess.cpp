#include "CommAccess.h"
#include <iostream>

#include "HMFRunLog.h"

// make PIMPL not war
struct CommAccessImpl {
	CommAccessImpl(FPGAConnectionId::IPv4 const & ip) :
		connid(ip),
		runlog(connid)
	{}

	CommAccessImpl(FPGAConnectionId const & connid) :
		connid(connid),
		runlog(connid)
	{}
	CommAccessImpl(FPGAConnectionId const & connid, FPGAConnectionId::IPv4 const & pmu_ip) :
		connid(connid),
		pmu_ip(pmu_ip),
		runlog(connid)
	{}


	// don't create same thing multiple times
	FPGAConnectionId connid;
	FPGAConnectionId::IPv4 pmu_ip;
	HMFRunLog runlog;
};

CommAccess::CommAccess(FPGAConnectionId::IPv4 const & ip) :
	pimpl(new CommAccessImpl(ip))
{}

CommAccess::CommAccess(FPGAConnectionId const & connid) :
	pimpl(new CommAccessImpl(connid))
{}

CommAccess::CommAccess(FPGAConnectionId const & connid, FPGAConnectionId::IPv4 const & pmu_ip) :
	pimpl(new CommAccessImpl(connid, pmu_ip))
{}


FPGAConnectionId const CommAccess::getFPGAConnectionId() const {
	if (!pimpl) {
		std::cerr << "ARRGGHH, CommAccess has not been correctly initialized?" << std::endl;
		pimpl.reset(new CommAccessImpl(FPGAConnectionId::IPv4::from_string("127.0.0.1")));
	}
	return pimpl->connid;
}

FPGAConnectionId::IPv4 const CommAccess::getPMUIP() const {
	return pimpl->pmu_ip;
}

CommAccess::~CommAccess() {
}
