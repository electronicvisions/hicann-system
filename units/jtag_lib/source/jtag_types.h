///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate: 2011-12-14 15:29:54 +0100 (Wed, 14 Dec 2011) $
// $LastChangedRevision: 123 $
// $LastChangedBy: henker $

#ifndef __JTAG_TYPES__
#define __JTAG_TYPES__


//////////////////////////////////////////////////////////////////////////
/// standard headers
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>


//////////////////////////////////////////////////////////////////////////
/// standard headers
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
/// c++ headers
#include <vector>
#include <exception>





///////////////////////////////////////////////////////////////////////////////
// no comment
#ifdef _MSC_VER
#pragma warning(disable : 4097) // typedef-name used as based class of (...)
#pragma warning(disable : 4099)	// "type name first seen using 'struct' now seen using 'class'"
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4121)	// allignment depends on pragma 'pack'
#pragma warning(disable : 4127)	// constant assignment
//#pragma warning(disable : 4200)	//'...'" warning, NULL field in struct
#pragma warning(disable : 4201)	// nameless struct/union
#pragma warning(disable : 4211)	// not standard conform redefinition of extern as static (issued falsly at 'static const' template members
#pragma warning(disable : 4213) // type conversion of L-value issued falsly at operator bool
#pragma warning(disable : 4231) // non standard extension : 'extern' before template instanciation
#pragma warning(disable : 4244) // converting type on return will shorten
#pragma warning(disable : 4250) // dominant derive, is only informational
//#pragma warning(disable : 4251)	// disable "class '...' needs to have dll-interface to be used by clients of class '...'", since the compiler may sometimes give this warning incorrectly.
#pragma warning(disable : 4256)	// constructor for class with virtual bases has '...'; calls may not be compatible with older versions of Visual C++
#pragma warning(disable : 4267)	// disable "argument conversion possible loss of data"
#pragma warning(disable : 4275)	// disable VC6 "exported class was derived from a class that was not exported"
#pragma warning(disable : 4284) // disable VC6 for -> operator
#pragma warning(disable : 4290)	// ... C++-specification of exception ignored
//   4305 - VC6, identifier type was converted to a smaller type
//   4309 - VC6, type conversion operation caused a constant to exceeded the space allocated for it
#pragma warning(disable : 4310)	// converting constant will shorten
#pragma warning(disable : 4355)	// "'this' : used in base member initializer list"
//   4389  '==' : signed/unsigned mismatch
#pragma warning(disable : 4503)	// VC6, decorated name was longer than the maximum the compiler allows (only affects debugger)
#pragma warning(disable : 4505)	// unreferenced local function has been removed
#pragma warning(disable : 4511)	// no copy constructor
#pragma warning(disable : 4512)	// no assign operator
#pragma warning(disable : 4514)	// unreferenced inline function has been removed
#if (_MSC_VER < 1200) // VC5 and earlier
#pragma warning(disable : 4521)	// multiple copy constructors/assignment operators specified,
#pragma warning(disable : 4522)	// with member templates are bogus...
#endif
#pragma warning(disable : 4571) // catch(...) blocks compiled with /EHs do not catch or re-throw structured exceptions
#pragma warning(disable : 4660) // template-class specialization '...' is already instantiated
#pragma warning(disable : 4663) // new syntax for explicit template specification (comes from Microsofts own c++ headers along with a bunch of sign warnings)
//   4675 - VC7.1, "change" in function overload resolution _might_ have altered program
#pragma warning(disable : 4702) // disable "unreachable code" warning for throw (known compiler bug)
#pragma warning(disable : 4706) // assignment within conditional
#pragma warning(disable : 4710)	// is no inline function
#pragma warning(disable : 4711)	// '..' choosen for inline extension
#pragma warning(disable : 4786)	// disable VC6 "identifier std::string exceeded maximum allowable length and was truncated" (only affects debugger)
#pragma warning(disable : 4800)	// disable VC6 "forcing value to bool 'true' or 'false'" (performance warning)
#pragma warning(disable : 4996)	// disable deprecated warnings

#endif


///////////////////////////////////////////////////////////////////////////////
///
#ifdef __ICL
#pragma warning(disable :  279) // controlling expression is constant
#pragma warning(disable :  304) // access control not specified
#pragma warning(disable :  383) // value copied to temporary, reference to temporary used
#pragma warning(disable :  424) // extra ";" ignored
#pragma warning(disable :  444) // destructor for base class is not virtual
#pragma warning(disable :  810) // conversion from ... may lose significant bits
#pragma warning(disable :  869) // parameter was never referenced
#pragma warning(disable :  909) // exception specification ignored
#pragma warning(disable :  981) // operands are evaluated in unspecified order
#pragma warning(disable : 1418) // external function definition with no prior declaration 
#pragma warning(disable : 1572) // floating-point equality and inequality comparisons are unreliable
#pragma warning(disable : 1418) // external function definition with no prior declaration
#pragma warning(disable : 1419) // external declaration in primary source file

// possible problems
// it also seems 373 cannot be disabled
#pragma warning(disable :  373) // "copy constructor of copy forbidden classes" is inaccessible (which is actually exact what we intended)
#pragma warning(disable :  522) // function redeclared "inline" after being called

///////////////////////////////////////////////////////////////////////////////
/// Problems with ICL which is not possible to disable remark 373
/// which is spamming idiotic stuff
#define ICL_DUMMY_COPYCONSTRUCTOR(a)	a(const a&) {}
#define ICL_EMPTY_COPYCONSTRUCTOR(a)	a(const a&);
#else
#define ICL_DUMMY_COPYCONSTRUCTOR(a)
#define ICL_EMPTY_COPYCONSTRUCTOR(a)
#endif


///////////////////////////////////////////////////////////////////////////////
///
#if defined(__BORLANDC__)
// Borland
// Shut up the following irritating warnings
#pragma warn -8022
//   8022 - A virtual function in a base class is usually overridden by a
//          declaration in a derived class.
//          In this case, a declaration with the same name but different
//          argument types makes the virtual functions inaccessible to further
//          derived classes
#pragma warn -8008
//   8008 - Condition is always true.
//          Whenever the compiler encounters a constant comparison that (due to
//          the nature of the value being compared) is always true or false, it
//          issues this warning and evaluates the condition at compile time.
#pragma warn -8060
//   8060 - Possibly incorrect assignment.
//          This warning is generated when the compiler encounters an assignment
//          operator as the main operator of a conditional expression (part of
//          an if, while, or do-while statement). This is usually a
//          typographical error for the equality operator.
#pragma warn -8066
//   8066 - Unreachable code.
//          A break, continue, goto, or return statement was not followed by a
//          label or the end of a loop or function. The compiler checks while,
//          do, and for loops with a constant test condition, and attempts to
//          recognize loops that can't fall through.

#endif


//#if defined(_MSC_VER) && (_MSC_VER >= 1400 )
#if defined(_MSC_VER) && (_MSC_VER >= 5400 )
// Microsoft visual studio, version 2005 and higher.
#define strcasecmp			stricmp
#define strncasecmp			strnicmp
#define snprintf			_snprintf_s
#define snscanf				_snscanf_s
#define sscanf				sscanf_s
#define vsnprintf			_vsnprintf_s
#define vswprintf			_vsnwprintf_s
#elif defined(_MSC_VER) || defined(__BORLANDC__) || defined(__DMC__)
// Microsoft visual studio
#define strcasecmp			stricmp
#define strncasecmp			strnicmp
#define snprintf			_snprintf
#define snscanf				_snscanf
#define vsnprintf			_vsnprintf
#define vswprintf			_vsnwprintf
#endif//defined(_MSC_VER) || defined(__BORLANDC__)



///////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER)
///////////////////////////////////////////////////////////////////////////

#define JTAG_DLL_EXPORT __declspec( dllexport )
#define JTAG_DLL_IMPORT __declspec( dllimport )
///////////////////////////////////////////////////////////
#if defined(_BUILD_JTAG_DLL)
///////////////////////////////////////////////////////////
#define JTAG_DLL_DECL JTAG_DLL_EXPORT
///////////////////////////////////////////////////////////
#elif defined(_USE_JTAG_DLL)
///////////////////////////////////////////////////////////
#define JTAG_DLL_DECL JTAG_DLL_IMPORT
///////////////////////////////////////////////////////////
#else
///////////////////////////////////////////////////////////
#define JTAG_DLL_DECL
///////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
#else//!defined(_MSC_VER)
///////////////////////////////////////////////////////////////////////////
#define JTAG_DLL_EXPORT
#define JTAG_DLL_IMPORT
#define JTAG_DLL_DECL
///////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////
/// contains jtag library and necessary types/enums.
/// header works standalone (e.g. with static/dynamic libraries).
namespace jtag_lib {
///////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
/// portable 64-bit integers
//////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__DMC__)
typedef __int64				int64;
typedef signed __int64		sint64;
typedef unsigned __int64	uint64;
#define LLCONST(a)			(a##i64)
#define ULLCONST(a)			(a##ui64)
#define CONST64(a)			(a##i64)

#if defined(_MSC_VER)
#define LLFORMAT			"I64"
#elif defined(__BORLANDC__)
#define LLFORMAT			"L"
#endif
#else //elif HAVE_LONG_LONG
typedef long long			int64;
typedef signed long long	sint64;
typedef unsigned long long	uint64;
#if defined(__64BIT__)
#define LLCONST(a)			(a##l)
#define ULLCONST(a)			(a##ul)
#define CONST64(a)			(a##l)
#else
#define LLCONST(a)			(a##ll)
#define ULLCONST(a)			(a##ull)
#define CONST64(a)			(a##ll)
#endif
#define LLFORMAT			"ll"
#endif

#ifndef INT64_MIN
#define INT64_MIN  (LLCONST(-9223372036854775807)-1)
#endif
#ifndef INT64_MAX
#define INT64_MAX  (LLCONST(9223372036854775807))
#endif
#ifndef UINT64_MAX
#define UINT64_MAX (ULLCONST(18446744073709551615))
#endif


///////////////////////////////////////////////////////////////////////////
/// jtag interfaces types.
enum jtag_interface_t
{
	JTAG_LPT,
	JTAG_USB,
	JTAG_XILV6,
	JTAG_DIGILENT,
	JTAG_ETH,
	JTAG_MAX
};

///////////////////////////////////////////////////////////////////////////
/// jtag transfer types.
enum jtag_transfer_type
{
	TDO_DEFAULT=0x00,
	TDO_READ   =0x01,

	TD___      =0x00,
	TD__O      =0x01,
	TD_I_      =0x02,
	TD_IO      =0x03,
	TDS__      =0x04,
	TDS_O      =0x05,
	TDSI_      =0x06,
	TDSIO      =0x07
};



///////////////////////////////////////////////////////////////////////////////
/// structs for delay values.
/// helpers to select overloaded functions.
struct JTAG_DLL_DECL us_delay
{
	unsigned long value;
	explicit us_delay(unsigned long v=0)
		: value(v)
	{}
};
struct JTAG_DLL_DECL ms_delay : public us_delay
{
	explicit ms_delay(unsigned long v=0)
		: us_delay(1000*v)
	{}
};

///////////////////////////////////////////////////////////////////////////////
/// structs for status values.
struct JTAG_DLL_DECL jtag_status_t
{
	jtag_interface_t jif;
	bool             opened;
	bool             valid;
	bool             bulkmode;
	unsigned long    rdbits;
	unsigned long    wrbits;
	unsigned long    delay;
	unsigned long    speed;
	const char*      name;
	const char*      device;
};

///////////////////////////////////////////////////////////////////////////////
/// structs to build an ipv4 number
struct JTAG_DLL_DECL ip_number
{
	unsigned int ip;
	ip_number(unsigned char v3, unsigned char v2, unsigned char v1, unsigned char v0)
		: ip( v3<<0x18 | v2<<0x10 | v1<<0x08 | v0)
	{}

	unsigned int value() const    { return this->ip; }
	operator unsigned int() const { return this->ip; }
};


///////////////////////////////////////////////////////////////////////////////
/// log function redirect
typedef int (*log_func)(char const*);


///////////////////////////////////////////////////////////////////////////////
/// jtag exception
struct JTAG_DLL_DECL jtag_exception : public std::exception
{
	char const* msg;
	jtag_exception(char const* m=NULL)
		: msg(m?m:"jtag exception")
	{}
	virtual ~jtag_exception()  throw()
	{}
	virtual char const* what() const throw()
	{
		return this->msg;
	}
};




///////////////////////////////////////////////////////////////////////////
} // end namespace jtag_lib
///////////////////////////////////////////////////////////////////////////


#endif//__JTAG_TYPES__
