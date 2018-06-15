#include "experiment.h"

#include <stdexcept>
#include <limits>
#include <RCF/RcfServer.hpp>
#include <RCF/TcpEndpoint.hpp>
#include <RCF/FilterService.hpp>
#include <RCF/ZlibCompressionFilter.hpp>
#include <RCF/SessionObjectFactoryService.hpp>

int main(int argc, char const * argv[])
{
	RCF::RcfInitDeinit rcfInit;
	RcfClient<I_Tests2> client(RCF::TcpEndpoint(6668));
	std::cout << std::numeric_limits<unsigned int>::max() << std::endl;
	client.getClientStub().setRemoteCallTimeoutMs(std::numeric_limits</*unsigned didn't work here*/int>::max());
	client.getClientStub().createRemoteSessionObject("Tests2");
	client.start("192.168.1.7", "1701");

	try {
		client.stop(); // may throw ;p
	} catch (RCF::RemoteException e) {
		std::cout << "catched RCF::RemoteException, rethrowing!" << std::endl;
		throw;
	}
	return 0;
}
