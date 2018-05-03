//-----------------------------------------------------------------
//
// Copyright (c) 2015 TU-Dresden  All rights reserved.
//
// Unless otherwise stated, the software on this site is distributed
// in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. THERE IS NO WARRANTY FOR THE SOFTWARE,
// TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN OTHERWISE
// STATED IN WRITING THE COPYRIGHT HOLDERS PROVIDE THE SOFTWARE
// "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE ENTIRE
// RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH YOU.
// SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL
// NECESSARY SERVICING, REPAIR OR CORRECTION. IN NO EVENT UNLESS
// REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL ANY
// COPYRIGHT HOLDER, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY
// GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT
// OF THE USE OR INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT
// LIMITED TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES
// SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF THE SOFTWARE TO
// OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER HAS BEEN
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//-----------------------------------------------------------------

// Company           :   TU-Dresden
// Author            :   Stephan Hartmann
// E-Mail            :   hartmann@iee.et.tu-dresden.de
//
// Filename          :   jtag_eth_socket.hpp
// Project Name      :   JTAG Library V2
// Description       :   Ethernet Socket wrapper
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 29 Aug 2013   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_ETH_SOCKET_HPP__
#define __JTAG_ETH_SOCKET_HPP__

#include "jtag_stdint.h"

#ifdef _WIN32
#include <winsock2.h>

#define AF_UNIX 1

struct sockaddr_un
{
	short sun_family;
	char sun_path[108];
};
#else
#include <arpa/inet.h>
#endif

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

namespace jtag_lib_v2 {
class eth_socket_base
{
public:
	virtual void close() = 0;
	virtual int bind(const struct sockaddr*, int) = 0;
	virtual int connect(const struct sockaddr*, int) = 0;
	virtual int open(int, int, int) = 0;
	virtual int sendto(const void*, int, const struct sockaddr*, int) = 0;
	virtual int recvfrom(void*, int, int, struct sockaddr*, int*) = 0;
	virtual int recvDataAvail() = 0;
	virtual int lasterror() = 0;
	virtual int resolveAddress(const char*, struct in_addr&) = 0;
	static char* strerror(const int);
	virtual ~eth_socket_base() {}
};

class eth_socket_host : public eth_socket_base
{
private:
#ifdef _WIN32
	SOCKET m_iSocket;
	HANDLE m_iHandle;
	bool m_bInitialized;
	WSADATA m_wsaData;
	WSAOVERLAPPED m_wsaOverlapped;
	OVERLAPPED m_overlapped;
	bool m_bNamedPipe;
	bool initializeSocket();
#else
	int m_iSocket;
#endif

public:
	eth_socket_host();
	virtual ~eth_socket_host();

	virtual void close();
	virtual int bind(const struct sockaddr*, int);
	virtual int connect(const struct sockaddr*, int);
	virtual int open(int, int, int);
	virtual int sendto(const void*, int, const struct sockaddr*, int);
	virtual int recvfrom(void*, int, int, struct sockaddr*, int* namelen);
	virtual int recvDataAvail();
	virtual int lasterror();
	virtual int resolveAddress(const char*, struct in_addr&);
};

class eth_socket_sc : public eth_socket_base
{
private:
	typedef void* (*createEthSocket_t)();
	typedef void (*destroyEthSocket_t)(void*);
	typedef int (*openEthSocket_t)(void*, int, int, int);
	typedef int (*closeEthSocket_t)(void*);
	typedef int (*bindEthSocket_t)(void*, const struct sockaddr*, int);
	typedef int (*sendToEthSocket_t)(void*, const void*, int, const sockaddr*, int);
	typedef int (*recvFromEthSocket_t)(void*, void*, int, int, const sockaddr*, int*);
	typedef int (*lastErrorEthSocket_t)(void*);
	typedef void (*waitEthSocket_t)(unsigned long);
	typedef uint32_t (*getBufferBytesEthSocket_t)(void*);

	static bool m_bInitialized;
	void* m_pEthSocket;
	int m_iError;

	static createEthSocket_t createEthSocket;
	static destroyEthSocket_t destroyEthSocket;
	static openEthSocket_t openEthSocket;
	static closeEthSocket_t closeEthSocket;
	static bindEthSocket_t bindEthSocket;
	static sendToEthSocket_t sendToEthSocket;
	static recvFromEthSocket_t recvFromEthSocket;
	static lastErrorEthSocket_t lastErrorEthSocket;
	static waitEthSocket_t waitEthSocket;
	static getBufferBytesEthSocket_t getBufferBytesEthSocket;

public:
	eth_socket_sc();
	virtual ~eth_socket_sc();

	static bool loadEthSocket();

	virtual void close();
	virtual int bind(const struct sockaddr*, int);
	virtual int connect(const struct sockaddr*, int);
	virtual int open(int, int, int);
	virtual int sendto(const void*, int, const struct sockaddr*, int);
	virtual int recvfrom(void*, int, int, struct sockaddr*, int*);
	virtual int recvDataAvail();
	virtual int lasterror();
	virtual int resolveAddress(const char*, struct in_addr&);
};

} // namespace jtag_lib_v2

#endif // __JTAG_ETH_SOCKET_HPP__
