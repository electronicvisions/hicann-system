//-----------------------------------------------------------------
//
// Copyright (c) 2014 TU-Dresden  All rights reserved.
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
// Filename          :   jtag_extern.h
// Project Name      :   JTAG Library V2
// Description       :   Helper functions for dynamic shared library
//                       loading
//-----------------------------------------------------------------
// Version   |  Author    |  Date         |  Comment
// ----------------------------------------------------------------
// 0.1       | hartmann   | 19 Jun 2012   |  initial version
// ----------------------------------------------------------------

#ifndef __JTAG_EXTERN_H__
#define __JTAG_EXTERN_H__

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <malloc.h>
#include <stdlib.h>
#include <windows.h>

#define DLL_EXT TEXT("dll")

#define strdup _strdup

#else // !ifdef _WIN32

#include <dlfcn.h>

typedef char TCHAR;
typedef void* HMODULE;
typedef int (*FARPROC)(void);

#define DLL_EXT "so"
#define TEXT(x) x

#endif // ifdef _WIN32

#if defined(_MSC_VER) && defined(_UNICODE)
#pragma warning(disable : 4996)
#endif

inline FARPROC getProcFunction(HMODULE hLib, const char* szFuncName)
{
#ifdef _WIN32
	return GetProcAddress(hLib, szFuncName);
#else  // !ifdef _WIN32
	union
	{
		void* pVPtr;
		FARPROC pFPtr;
	} uConv;
	uConv.pVPtr = dlsym(hLib, szFuncName);
	return uConv.pFPtr;
#endif // ifdef _WIN32
}

inline HMODULE loadLibrary(const TCHAR* szFileName)
{
#ifdef _WIN32
	return LoadLibrary(szFileName);
#else  // !ifdef _WIN32
	return dlopen(szFileName, RTLD_LAZY);
#endif // ifdef _WIN32
}

inline int freeLibrary(HMODULE hModule)
{
#ifdef _WIN32
	if (FreeLibrary(hModule))
		return 0;
	return 1;
#else  // !ifdef _WIN32
	return dlclose(hModule);
#endif // ifdef _WIN32
}

inline char* getErrorMessage()
{
#ifdef _WIN32
	LPTSTR lpMessage;
	static char* szMessage = NULL;
	DWORD dwError = GetLastError();
	if (szMessage) {
		free(szMessage);
		szMessage = NULL;
	}

	if (!dwError)
		return szMessage;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPTSTR>(&lpMessage), 0, NULL);
#ifdef _UNICODE
	size_t uiSize = wcstombs(NULL, lpMessage, 0);
	szMessage = static_cast<char*>(malloc(uiSize + 1));
	wcstombs(szMessage, lpMessage, uiSize + 1);
#else
	szMessage = strdup(lpMessage);
#endif // ifdef _UNICODE
	LocalFree(lpMessage);
	return szMessage;
#else
	return dlerror();
#endif // ifdef _WIN32
}

#endif // ifdef __JTAG_EXTERN_H__
