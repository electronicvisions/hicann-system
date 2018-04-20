// Company         :   tud
// Author          :   partzsch
// E-Mail          :   <email>
//
// Filename        :   ethernet_if_base.h
// Project Name    :   p_facets
// Subproject Name :   s_fpga
// Description     :   <short description>
//
//------------------------------------------------------------------------

// interface class for sending and receiving UDP packets

#ifndef __ETHERNET_SOFTWARE_IF_CPP__
#define __ETHERNET_SOFTWARE_IF_CPP__


#include "ethernet_software_if.h"
#include <stdio.h>
#include <string.h>

#ifdef NCSIM
  // simulation mode
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <sys/ioctl.h>

  #include "systemc.h"

  #include "eth_if.h"
  #include "eth_stim.h"
  #include "eth_udp_socket.h"
#else
  // host software mode
  #include <netdb.h>
  #include <unistd.h>
#endif


#ifndef REMOTE_SERVER_PORT
  #define REMOTE_SERVER_PORT 5000
#endif

#include "logger.h"

#include <boost/asio.hpp>


static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("hicann-system.tests2");

EthernetSoftwareIF::EthernetSoftwareIF() : udp_socket(-1)
	,_log(Logger::instance())
{}

EthernetSoftwareIF::~EthernetSoftwareIF() {
	#ifndef NCSIM
	// TODO calling end? but dtor may not throw!
	if (udp_socket != -1)
		close(udp_socket);
	#endif
}

bool EthernetSoftwareIF::init(const uint16_t uiPort)
{
	#ifdef NCSIM
	// simulation mode
        #ifndef FPGA_BOARD_BS_K7 // do these settings manually in testcase for Kintex7
	    _log(Logger::INFO) << "EthernetSoftwareIF::init: Use simulation mode";
		uint8_t c_auiLocalMAC[] = {0xD0, 0x35, 0x63, 0x31, 0xA3, 0x56};
	    eth_if *pEthIf = eth_if::getInstance();
        pEthIf->setIfType(ETH_IF_RGMII);
    	pEthIf->setIP(0xC0A8017F);
	    pEthIf->setSubnet(0xFFFFFF00);
    	pEthIf->setGateway(0xC0A801FE);
	    pEthIf->setMAC(c_auiLocalMAC);
        #endif

		this->udp_socket = this->socket_if.open(AF_INET, SOCK_DGRAM, 0);
	#else
		this->udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	#endif

	if ( this->udp_socket < 0 )
	{
		_log(Logger::ERROR) << "Cannot open socket";
		return false;
	}
	
	_log(Logger::DEBUG0) << "EthernetSoftwareIF::init: Successfully opened socket";
	
//	struct hostent     *h;
//	h = gethostbyname("192.168.1.1");
//	this->remoteServAddr.sin_family = h->h_addrtype;
//	memcpy((char *) &(this->remoteServAddr.sin_addr.s_addr), h->h_addr_list[0], h->h_length);

	this->remoteServAddr.sin_port = htons(uiPort);
	this->remoteServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	this->remoteServAddr.sin_family = AF_INET;

	// bind socket
	#ifdef NCSIM
		// simulation mode
		int status = this->socket_if.bind((const sockaddr*)&(this->remoteServAddr), sizeof(this->remoteServAddr));
	#else
		remoteServAddr.sin_port = htons(0); // bind to some arbitrary port
		int status = bind(this->udp_socket, (const sockaddr*)&(this->remoteServAddr), sizeof(this->remoteServAddr));
		//socklen_t tmp = sizeof(remoteServAddr);
		//getsockname(udp_socket, reinterpret_cast<sockaddr*>(&remoteServAddr), &tmp);
	#endif

	if ( status == -1 )
	{
		_log(Logger::ERROR) << "error: Failed to bind socket to port " << uiPort << " for reception." << Logger::flush;
		perror("EthernetSoftwareIF::init perror");

		#ifdef NCSIM
			// simulation mode
			this->socket_if.close();
		#else
			close(this->udp_socket);
		#endif

		this->udp_socket = -1;
		throw std::runtime_error("EthernetSoftwareIF::init failed");
		return false;
	}

	_log(Logger::DEBUG0) << "EthernetSoftwareIF::init: Successful binding of socket to remote port " << uiPort << " for reception";

	return true;
}

int EthernetSoftwareIF::sendUDP(const uint32_t uiTargetIP, const uint16_t uiPort, const void *pszData, const uint16_t uiDataBytes)
{
	_log(Logger::DEBUG0) << "EthernetSoftwareIF::sendUDP: uiTargetIP " << boost::asio::ip::address_v4(uiTargetIP).to_string()
						 << Logger::flush;
	_log(Logger::DEBUG0) << "EthernetSoftwareIF::sendUDP: uiPort " << uiPort << Logger::flush;

	#ifdef NCSIM
        // simulation mode
        #ifndef FPGA_BOARD_BS_K7
		if (uiTargetIP != 0xC0A80101)
		{
			_log(Logger::DEBUG0) << "EthernetSoftwareIF::sendUDP: trying to send a frame to IP address 0x" << std::hex <<uiTargetIP
			                     << ", but FPGA IP is fixed to 0xC0A80101. Forcing to 0xC0A80101.";
		}
		this->remoteServAddr.sin_addr.s_addr = htonl(0xC0A80101);
        #else
        this->remoteServAddr.sin_addr.s_addr = htonl(uiTargetIP);
        std::cout << "sendUDP: sending to IP address 0x" << std::hex << uiTargetIP << std::dec << std::endl;
        #endif
	#else
		this->remoteServAddr.sin_addr.s_addr = htonl(uiTargetIP);
	#endif

	this->remoteServAddr.sin_port = htons(uiPort);

    // for debugging only...
    #ifdef NCSIM
    if ((uiPort >= 1700) && ((uiPort <= 1703)))
    {
        _log(Logger::DEBUG0) << "Sending JTAG packet to port " << uiPort << " at simulation time " << (int)(sc_simulation_time()) << "ns";
    }
    #endif

	// convert to network byte order
	// ECM: no, this cannot be done in a general way => not all data is 32-bit aligned => the user has to do it!

	int rc = 0;

	#ifdef NCSIM
		rc = this->socket_if.sendto(pszData, uiDataBytes, (const struct sockaddr *)&(this->remoteServAddr), sizeof(this->remoteServAddr));
	#else
		rc = sendto(this->udp_socket, pszData, uiDataBytes, 0, (const struct sockaddr *)&(this->remoteServAddr), sizeof(this->remoteServAddr));
	#endif


	if ( rc < 0 )
	{
		_log(Logger::WARNING) << "EthernetSoftwareIF::sendUDP: cannot write data to socket (" << rc << ")" << Logger::flush;
		_log(Logger::WARNING) << "EthernetSoftwareIF::sendUDP: sin_port was "
							  << ntohs(this->remoteServAddr.sin_port) << Logger::flush;
		_log(Logger::WARNING) << "EthernetSoftwareIF::sendUDP: sin_addr.s_addr was "
							  << ntohl(this->remoteServAddr.sin_addr.s_addr) << Logger::flush;
		perror("EthernetSoftwareIF::sendUDP perror");
		return 0;
	}
	else if ( rc < uiDataBytes )
	{
      _log(Logger::WARNING) << "EthernetSoftwareIF::sendUDP: could not write whole data block into socket (" << rc << ")" << Logger::flush;
	  return 0;
	}
	
    return rc;

}

bool EthernetSoftwareIF::receiveUDP(rx_entry_t &rxdata)
{
	char data[1500];

	#ifdef NCSIM
		// simulation mode
		int len = sizeof(this->remoteServAddr);
		int bytecount = this->socket_if.recvfrom(data, sizeof(data), MSG_DONTWAIT, (sockaddr*)&(this->remoteServAddr), &len );
	#else
		socklen_t len = sizeof(this->remoteServAddr);
		int bytecount = recvfrom(this->udp_socket, data, sizeof(data), MSG_DONTWAIT, (sockaddr*)&(this->remoteServAddr), &len );
	#endif

	if (bytecount < 0)
	{
		#ifdef NCSIM
			_log(Logger::ERROR) << "EthernetSoftwareIF::receiveUDP: error during packet reception (error code: " << this->socket_if.lasterror() << ").";
		#else
			_log(Logger::ERROR) << "EthernetSoftwareIF::receiveUDP: error during packet reception.";
		#endif

		return false;
	}

	rxdata.uiSourceIP = htonl(this->remoteServAddr.sin_addr.s_addr);
	rxdata.uiSourcePort = htons(this->remoteServAddr.sin_port);
	rxdata.uiDestPort = REMOTE_SERVER_PORT;
	rxdata.pData = std::vector<char>(data, data + bytecount);

	return true;
}

bool EthernetSoftwareIF::hasReceivedUDP()
{
	#ifdef NCSIM
		// simulation mode
		unsigned long bytecount = 0;
		this->socket_if.ioctl(FIONREAD,&bytecount);
		return (bytecount > 0);
	#else
		// copied from test_ddr2.cpp::isReadable
		int timeOut = 0;
		fd_set socketReadSet;
		struct timeval tv;
		FD_ZERO(&socketReadSet);
		FD_SET(this->udp_socket,&socketReadSet);
		tv.tv_sec  = timeOut / 1000;
		tv.tv_usec = (timeOut % 1000) * 1000;

		if (select(this->udp_socket+1,&socketReadSet,0,0,&tv) == -1) 
		{
			return false;
		}
		return FD_ISSET(this->udp_socket,&socketReadSet) != 0;
	#endif
}


void EthernetSoftwareIF::pauseEthernet(double time)
{
	#ifdef NCSIM
		// simulation mode
		wait(time*20, SC_US);
	#else
		unsigned int milliseconds = (unsigned int)(time+0.5);
		// copied from JTAG lib
		#if defined(WIN32)
			Sleep(milliseconds);
		#elif defined(__sun__)
			poll(0, 0, milliseconds);
		#else
			usleep(milliseconds * 1000);
		#endif
	#endif
}


void EthernetSoftwareIF::end()
{
	#ifdef NCSIM
		// simulation mode
		this->socket_if.close();
	#else
		if (this->udp_socket >= 0)
			close(this->udp_socket);
	#endif
}


#endif
