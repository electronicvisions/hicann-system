#include "experiment.h"

#include <string>
#include <stdexcept>

#include <RCF/RcfServer.hpp>
#include <RCF/TcpEndpoint.hpp>
#include <RCF/FilterService.hpp>
#include <RCF/ZlibCompressionFilter.hpp>
#include <RCF/SessionObjectFactoryService.hpp>


void Tests2::start(std::string ip, std::string port) {
	// TODO: take ip+port, #hicanns, hicannNo only!
	int pid = fork();
	if (pid == 0)
		execl("/bin/echo", "echo", "tests2", "-bje2f", "1", "0", "-ip", ip.c_str(), "-jp", port.c_str(), "-m", "jd_mappingmaster", NULL);
}

void Tests2::stop() {
	// has to be asynchronous... system() blocks
	throw std::runtime_error("killing processes is not implemented");
}



int main(int argc, char const * argv[])
{
	using namespace RCF;
	int const server_port = 6668; // another elaborate guess

	RcfInitDeinit rcfInit;
	RcfServer* server = new RCF::RcfServer(RCF::TcpEndpoint(server_port));

	// set max msg size
	size_t const msg_size = 1024*1024*100; // 100 MB
	server->getServerTransport().setMaxMessageLength(msg_size);

	SessionObjectFactoryServicePtr sofs(new SessionObjectFactoryService());
	server->addService(sofs);

	// bind functions to service
	sofs->bind<I_Tests2, Tests2>("Tests2");

	// compression?
	RCF::FilterServicePtr filterServicePtr(new RCF::FilterService());
	filterServicePtr->addFilterFactory( RCF::FilterFactoryPtr(new RCF::ZlibStatefulCompressionFilterFactory()));
	server->addService(filterServicePtr);

	std::cout << "starting RCF server" << std::endl;
	server->startInThisThread();

	return 0;
}
