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
// Filename          :   jtag_eth_socket.cpp
// Project Name      :   JTAG Library V2
// Description       :   Ethernet Socket wrappers
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 29 Aug 2013   |  initial version
// ----------------------------------------------------------------
#include "jtag_eth_socket.hpp"
#include "jtag_extern.h"
#include "jtag_stdint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#pragma comment(lib, "Ws2_32.lib")
#include <ws2tcpip.h>

#else

#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#endif

#if defined(_WIN32) && defined(_MSC_VER)
#if _MSC_VER < 1300
#error Ethernet is not supported with this Visual Studio version.
#endif
#endif

#ifndef _WIN32

bool jtag_lib_v2::eth_socket_sc::m_bInitialized = false;
jtag_lib_v2::eth_socket_sc::createEthSocket_t jtag_lib_v2::eth_socket_sc::createEthSocket = 0;
jtag_lib_v2::eth_socket_sc::destroyEthSocket_t jtag_lib_v2::eth_socket_sc::destroyEthSocket = 0;
jtag_lib_v2::eth_socket_sc::openEthSocket_t jtag_lib_v2::eth_socket_sc::openEthSocket = 0;
jtag_lib_v2::eth_socket_sc::closeEthSocket_t jtag_lib_v2::eth_socket_sc::closeEthSocket = 0;
jtag_lib_v2::eth_socket_sc::bindEthSocket_t jtag_lib_v2::eth_socket_sc::bindEthSocket = 0;
jtag_lib_v2::eth_socket_sc::sendToEthSocket_t jtag_lib_v2::eth_socket_sc::sendToEthSocket = 0;
jtag_lib_v2::eth_socket_sc::recvFromEthSocket_t jtag_lib_v2::eth_socket_sc::recvFromEthSocket = 0;
jtag_lib_v2::eth_socket_sc::lastErrorEthSocket_t jtag_lib_v2::eth_socket_sc::lastErrorEthSocket = 0;
jtag_lib_v2::eth_socket_sc::getBufferBytesEthSocket_t
	jtag_lib_v2::eth_socket_sc::getBufferBytesEthSocket = 0;
jtag_lib_v2::eth_socket_sc::waitEthSocket_t jtag_lib_v2::eth_socket_sc::waitEthSocket = 0;

jtag_lib_v2::eth_socket_sc::eth_socket_sc() : m_pEthSocket(0), m_iError(0) {}

jtag_lib_v2::eth_socket_sc::~eth_socket_sc()
{
	if (eth_socket_sc::m_bInitialized) {
		this->close();
		eth_socket_sc::destroyEthSocket(this->m_pEthSocket);
	}
}

void jtag_lib_v2::eth_socket_sc::close()
{
	this->m_iError = 0;
	if (eth_socket_sc::m_bInitialized)
		eth_socket_sc::closeEthSocket(this->m_pEthSocket);
}

int jtag_lib_v2::eth_socket_sc::connect(const struct sockaddr* /*name*/, int /*namelen*/)
{
	this->m_iError = ENOSYS;
	return -1;
}

int jtag_lib_v2::eth_socket_sc::bind(const struct sockaddr* name, int namelen)
{
	this->m_iError = 0;
	if (eth_socket_sc::m_bInitialized)
		return eth_socket_sc::bindEthSocket(this->m_pEthSocket, name, namelen);
	this->m_iError = EINVAL;
	return -1;
}

int jtag_lib_v2::eth_socket_sc::open(int af, int type, int protocol)
{
	this->m_iError = 0;
	if (eth_socket_sc::m_bInitialized) {
		if (this->m_pEthSocket) {
			this->close();
			eth_socket_sc::destroyEthSocket(this->m_pEthSocket);
		}
		this->m_pEthSocket = eth_socket_sc::createEthSocket();
		if (!this->m_pEthSocket) {
			this->m_iError = EINVAL;
			return -1;
		}
		return eth_socket_sc::openEthSocket(this->m_pEthSocket, af, type, protocol);
	}
	this->m_iError = EINVAL;
	return -1;
}

int jtag_lib_v2::eth_socket_sc::sendto(
	const void* buf, int len, const struct sockaddr* name, int namelen)
{
	this->m_iError = 0;
	if (eth_socket_sc::m_bInitialized)
		return eth_socket_sc::sendToEthSocket(this->m_pEthSocket, buf, len, name, namelen);
	this->m_iError = EINVAL;
	return -1;
}

int jtag_lib_v2::eth_socket_sc::recvfrom(
	void* buf, int len, int flags, struct sockaddr* name, int* namelen)
{
	this->m_iError = 0;
	if (eth_socket_sc::m_bInitialized)
		return eth_socket_sc::recvFromEthSocket(this->m_pEthSocket, buf, len, flags, name, namelen);
	this->m_iError = EINVAL;
	return -1;
}

int jtag_lib_v2::eth_socket_sc::recvDataAvail()
{
	this->m_iError = 0;

	if (!eth_socket_sc::m_bInitialized) {
		this->m_iError = EINVAL;
		return -1;
	}

	uint8_t uiRetries = 0;
	uint32_t uiBytes = 0;
	do {
		uiBytes = eth_socket_sc::getBufferBytesEthSocket(this->m_pEthSocket);
		if (!uiBytes) {
			eth_socket_sc::waitEthSocket(10000);
			uiRetries++;
		}
	} while (!uiBytes && uiRetries < 50);

	return uiBytes;
}

int jtag_lib_v2::eth_socket_sc::lasterror()
{
	if (eth_socket_sc::m_bInitialized && !this->m_iError)
		return eth_socket_sc::lastErrorEthSocket(this->m_pEthSocket);
	else if (this->m_iError)
		return this->m_iError;
	return EINVAL;
}

int jtag_lib_v2::eth_socket_sc::resolveAddress(const char* szAddr, struct in_addr& sin_addr)
{
	/* don't resolve address for SystemC, just copy it */
	if (!::inet_aton(szAddr, &sin_addr))
		return -1;
	return 0;
}

bool jtag_lib_v2::eth_socket_sc::loadEthSocket()
{
	if (eth_socket_sc::m_bInitialized)
		return true;

	eth_socket_sc::createEthSocket =
		reinterpret_cast<createEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "createEthSocket"));
	eth_socket_sc::destroyEthSocket =
		reinterpret_cast<destroyEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "destroyEthSocket"));
	eth_socket_sc::openEthSocket =
		reinterpret_cast<openEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "openEthSocket"));
	eth_socket_sc::closeEthSocket =
		reinterpret_cast<closeEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "closeEthSocket"));
	eth_socket_sc::bindEthSocket =
		reinterpret_cast<bindEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "bindEthSocket"));
	eth_socket_sc::sendToEthSocket =
		reinterpret_cast<sendToEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "sendToEthSocket"));
	eth_socket_sc::recvFromEthSocket =
		reinterpret_cast<recvFromEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "recvFromEthSocket"));
	eth_socket_sc::lastErrorEthSocket = reinterpret_cast<lastErrorEthSocket_t>(
		::getProcFunction(RTLD_DEFAULT, "lastErrorEthSocket"));
	eth_socket_sc::getBufferBytesEthSocket = reinterpret_cast<getBufferBytesEthSocket_t>(
		::getProcFunction(RTLD_DEFAULT, "getBufferBytesEthSocket"));
	eth_socket_sc::waitEthSocket =
		reinterpret_cast<waitEthSocket_t>(::getProcFunction(RTLD_DEFAULT, "waitEthSocket"));

	if (!eth_socket_sc::createEthSocket || !eth_socket_sc::destroyEthSocket ||
		!eth_socket_sc::openEthSocket || !eth_socket_sc::closeEthSocket ||
		!eth_socket_sc::bindEthSocket || !eth_socket_sc::sendToEthSocket ||
		!eth_socket_sc::recvFromEthSocket || !eth_socket_sc::waitEthSocket ||
		!eth_socket_sc::getBufferBytesEthSocket || !eth_socket_sc::lastErrorEthSocket)
		return false;

	eth_socket_sc::m_bInitialized = true;
	return true;
}

#else

bool jtag_lib_v2::eth_socket_host::initializeSocket()
{
	const WORD wVersionRequested = MAKEWORD(2, 2);
	const int iResult = ::WSAStartup(wVersionRequested, &this->m_wsaData);
	if (iResult)
		return false;

	if (LOBYTE(this->m_wsaData.wVersion) != 2 || HIBYTE(this->m_wsaData.wVersion) != 2) {
		::WSASetLastError(WSAVERNOTSUPPORTED);
		::WSACleanup();
		return false;
	}

	this->m_bInitialized = true;

	return true;
}

#endif

jtag_lib_v2::eth_socket_host::eth_socket_host()
	:
#ifdef _WIN32
	  m_iSocket(INVALID_SOCKET),
	  m_iHandle(INVALID_HANDLE_VALUE),
	  m_bInitialized(false),
	  m_bNamedPipe(false)
#else
	  m_iSocket(-1)
#endif
{
#ifdef _WIN32
	::ZeroMemory((PVOID) & this->m_wsaOverlapped, sizeof(WSAOVERLAPPED));
	this->m_wsaOverlapped.hEvent = WSA_INVALID_EVENT;
#endif
}

jtag_lib_v2::eth_socket_host::~eth_socket_host()
{
	this->close();
#ifdef _WIN32
	if (this->m_bInitialized)
		::WSACleanup();
#endif
}

void jtag_lib_v2::eth_socket_host::close()
{
#ifdef _WIN32
	if (!this->m_bInitialized)
		return;
	if (this->m_bNamedPipe) {
		if (this->m_iHandle != INVALID_HANDLE_VALUE)
			::CloseHandle(this->m_iHandle);
		if (this->m_overlapped.hEvent != 0)
			::CloseHandle(this->m_overlapped.hEvent);
	} else {
		if (this->m_iSocket != INVALID_SOCKET)
			::closesocket(this->m_iSocket);
		if (this->m_wsaOverlapped.hEvent != WSA_INVALID_EVENT)
			::WSACloseEvent(this->m_wsaOverlapped.hEvent);
	}
	this->m_wsaOverlapped.hEvent = WSA_INVALID_EVENT;
	this->m_overlapped.hEvent = 0;
	this->m_iSocket = INVALID_SOCKET;
	this->m_iHandle = INVALID_HANDLE_VALUE;
	this->m_bNamedPipe = false;
#else
	if (this->m_iSocket >= 0)
		::close(this->m_iSocket);
	this->m_iSocket = -1;
#endif
}

int jtag_lib_v2::eth_socket_host::bind(const struct sockaddr* name, int namelen)
{
	return ::bind(this->m_iSocket, name, namelen);
}

int jtag_lib_v2::eth_socket_host::connect(const struct sockaddr* name, int namelen)
{
#ifdef _WIN32
	if (this->m_bNamedPipe) {
		if (namelen < sizeof(struct sockaddr_un)) {
			::SetLastError(ERROR_INVALID_PARAMETER);
			return -1;
		}
		const struct sockaddr_un* pSockAddr = (const struct sockaddr_un*) name;
		TCHAR szPath[UNIX_PATH_MAX + 1];
#ifdef _UNICODE
		::mbstowcs(szPath, pSockAddr->sun_path, UNIX_PATH_MAX + 1);
#else
		::strncpy(szPath, pSockAddr->sun_path, UNIX_PATH_MAX + 1);
#endif
		this->m_iHandle = ::CreateFile(
			szPath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
		if (this->m_iHandle == INVALID_HANDLE_VALUE) {
			this->m_iHandle = INVALID_HANDLE_VALUE;
			return -1;
		}

		return 0;
	}

	return ::WSAConnect(this->m_iSocket, name, namelen, 0, 0, 0, 0);
#else
	return ::connect(this->m_iSocket, name, namelen);
#endif
}

int jtag_lib_v2::eth_socket_host::open(int af, int type, int protocol)
{
#ifdef _WIN32
	if (af == AF_UNIX) {
		this->m_bNamedPipe = true;
		this->m_bInitialized = true;
		this->m_overlapped.hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
		if (this->m_overlapped.hEvent == 0)
			return -1;
	} else {
		if (!this->m_bInitialized) {
			if (!this->initializeSocket())
				return -1;
		}

		this->m_wsaOverlapped.hEvent = ::WSACreateEvent();
		if (this->m_wsaOverlapped.hEvent == WSA_INVALID_EVENT)
			return -1;

		this->m_iSocket = ::WSASocket(af, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);
		if (this->m_iSocket == INVALID_SOCKET)
			return -1;
	}

	return 0;
#else
	this->m_iSocket = ::socket(af, type, protocol);
	return this->m_iSocket;
#endif
}

int jtag_lib_v2::eth_socket_host::sendto(
	const void* buf, int len, const struct sockaddr* name, int /*namelen*/)
{
#ifdef _WIN32
	if (this->m_bNamedPipe) {
		DWORD dwWritten = 0;
		BOOL bResult = ::WriteFile(this->m_iHandle, buf, len, 0, &this->m_overlapped);
		int iError = ::GetLastError();
		if (!bResult && iError != ERROR_IO_PENDING) {
			::SetLastError(iError);
			return -1;
		}
		while (!::GetOverlappedResult(this->m_iHandle, &this->m_overlapped, &dwWritten, FALSE)) {
			iError = ::GetLastError();
			if (iError != ERROR_IO_INCOMPLETE && iError != ERROR_HANDLE_EOF) {
				::SetLastError(iError);
				return -1;
			}
		}

		return (int) dwWritten;
	}

	WSABUF wsaBuf;
	wsaBuf.len = len;
	wsaBuf.buf = (char*) buf;

	unsigned long ulBytesSent;
	int iResult = ::WSASendTo(
		this->m_iSocket, &wsaBuf, 1, &ulBytesSent, 0, name, namelen, &this->m_wsaOverlapped, 0);
	int iError = ::WSAGetLastError();
	if (iResult == SOCKET_ERROR && iError != WSA_IO_PENDING) {
		::WSASetLastError(iError);
		return -1;
	}
	iResult = ::WSAWaitForMultipleEvents(1, &this->m_wsaOverlapped.hEvent, TRUE, INFINITE, TRUE);
	if (iResult == WSA_WAIT_FAILED)
		return -1;

	DWORD dwFlags;
	iResult = ::WSAGetOverlappedResult(
		this->m_iSocket, &this->m_wsaOverlapped, &ulBytesSent, FALSE, &dwFlags);
	if (!iResult)
		return -1;

	return ulBytesSent;
#else
	return ::sendto(this->m_iSocket, buf, len, 0, name, sizeof(sockaddr_in));
#endif
}

int jtag_lib_v2::eth_socket_host::recvfrom(
	void* buf, int len, int flags, struct sockaddr* name, int* namelen)
{
#ifdef _WIN32
	if (this->m_bNamedPipe) {
		DWORD dwRead = 0;
		BOOL bResult = ::ReadFile(this->m_iHandle, buf, len, 0, &this->m_overlapped);
		int iError = ::GetLastError();
		if (!bResult && iError != ERROR_IO_PENDING) {
			::SetLastError(iError);
			return -1;
		}

		while (!::GetOverlappedResult(this->m_iHandle, &this->m_overlapped, &dwRead, FALSE)) {
			iError = ::GetLastError();
			if (iError != ERROR_IO_INCOMPLETE && iError != ERROR_HANDLE_EOF) {
				::SetLastError(iError);
				return -1;
			}
		}

		return (int) dwRead;
	}

	WSABUF wsaBuf;
	wsaBuf.len = len;
	wsaBuf.buf = static_cast<char*>(buf);

	DWORD dwBytesRecv;
	int iResult = ::WSARecvFrom(
		this->m_iSocket, &wsaBuf, 1, &dwBytesRecv, 0, name, namelen, &this->m_wsaOverlapped, 0);
	if (iResult) {
		int iError = this->lasterror();
		if (iError != WSA_IO_PENDING) {
			::WSASetLastError(iError);
			return -1;
		}
		iResult =
			::WSAWaitForMultipleEvents(1, &this->m_wsaOverlapped.hEvent, TRUE, INFINITE, TRUE);
		if (iResult == WSA_WAIT_FAILED)
			return -1;

		DWORD dwFlags;
		iResult = ::WSAGetOverlappedResult(
			this->m_iSocket, &this->m_wsaOverlapped, &dwBytesRecv, FALSE, &dwFlags);
		if (!iResult)
			return -1;

		return dwBytesRecv;
	}

	return dwBytesRecv;
#else
	return ::recvfrom(
		this->m_iSocket, buf, len, flags, name, reinterpret_cast<socklen_t*>(namelen));
#endif
}

int jtag_lib_v2::eth_socket_host::recvDataAvail()
{
#ifdef _WIN32
	if (this->m_bNamedPipe) {
		DWORD dwResult = ::WaitForSingleObject(this->m_overlapped.hEvent, 1000);
		if (dwResult == WAIT_TIMEOUT)
			return 0;
		if (dwResult == WAIT_FAILED)
			return -1;
		return 1;
	}
#endif

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(this->m_iSocket, &fds);
	struct timeval tv = {1, 0};
	return ::select(this->m_iSocket + 1, &fds, 0, 0, &tv);
}

int jtag_lib_v2::eth_socket_host::lasterror()
{
#ifdef _WIN32
	if (this->m_bNamedPipe)
		return ::GetLastError();
	else
		return ::WSAGetLastError();
#else
	return errno;
#endif
}

int jtag_lib_v2::eth_socket_host::resolveAddress(const char* szAddr, struct in_addr& sin_addr)
{
#ifdef _WIN32
	if (!this->m_bInitialized) {
		if (!this->initializeSocket())
			return this->lasterror();
	}
#endif

	struct addrinfo hints;
	::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	struct addrinfo* pServInfo;
	int iResult = ::getaddrinfo(szAddr, 0, &hints, &pServInfo);
	if (iResult)
		return iResult;

	struct sockaddr_in* pAddr = reinterpret_cast<struct sockaddr_in*>(pServInfo->ai_addr);
	sin_addr = pAddr->sin_addr;

	::freeaddrinfo(pServInfo);

	return 0;
}

char* jtag_lib_v2::eth_socket_base::strerror(const int iError)
{
#ifdef _WIN32
	LPTSTR lpMessage;
	static char* szMessage = 0;
	if (szMessage) {
		::free(szMessage);
		szMessage = 0;
	}

	if (!iError)
		return szMessage;

	if (!::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
			0, iError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPTSTR>(&lpMessage), 0, 0))
		return 0;

#ifdef _UNICODE
	size_t uiSize = ::wcstombs(0, lpMessage, 0);
	szMessage = static_cast<char*>(malloc(uiSize + 1));
	if (!szMessage)
		return 0;
	::wcstombs(szMessage, lpMessage, uiSize + 1);
#else
	szMessage = ::strdup(lpMessage);
#endif // ifdef _UNICODE
	::LocalFree(lpMessage);
	return szMessage;
#else
	return ::strerror(iError);
#endif // ifdef _WIN32
}
