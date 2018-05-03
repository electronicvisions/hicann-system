/*! \file CseJtagAPI.h
	\brief Xilinx Jtag Communcation Interface
*/

/*
 * Copyright (c) 2011 Xilinx, Inc.  All rights reserved.
 *
 *                 XILINX CONFIDENTIAL PROPERTY
 * This   document  contains  proprietary information  which   is
 * protected by  copyright. All rights  are reserved. No  part of
 * this  document may be photocopied, reproduced or translated to
 * another  program  language  without  prior written  consent of
 * XILINX Inc., San Jose, CA. 95124
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _CseJtagAPI
#define _CseJtagAPI

#ifdef _WIN32
#ifndef DLLEXPORT
#define DLLEXPORT __declspec(dllexport)
#endif /*#ifndef DLLEXPORT*/
#ifndef DLLIMPORT
#define DLLIMPORT __declspec(dllimport)
#endif /*#ifndef DLLIMPORT*/
#else  /*#ifdef _WIN32*/
#ifndef DLLEXPORT
#define DLLEXPORT
#endif /*#ifndef DLLEXPORT*/
#ifndef DLLIMPORT
#define DLLIMPORT
#endif /*#ifndef DLLIMPORT*/
#endif /*#ifdef _WIN32*/

#ifndef CSEJTAG_DLL
#ifdef CSEJTAG_DLL_EXPORTS
#define CSEJTAG_DLL DLLEXPORT
#else /*#ifdef CSEJTAG_DLL_EXPORTS*/
#define CSEJTAG_DLL DLLIMPORT
#endif /*#ifdef CSEJTAG_DLL DLLEXPORT*/
#endif /*#ifndef CSEJTAG_DLL*/


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/
/*                                  TYPES                                  */
/***************************************************************************/

/**
32 bit handle structure.
When obtained from CseJtag_session_create(), the handle is divided into
fields:
<pre>
+----------+------------+----------+-------------+
| 31-24    | 23-16      | 15-8     | 7-0         |
| Reserved | Session ID | Reserved | Target Slot |
| 0x00     |            | 0x00     | 0x00        |
+----------+------------+----------+-------------+
</pre>
Currently the only implemented target slot is 0 (one target per session).
This may change in the future if needed.
 */
typedef unsigned int CSE_HANDLE;

/**
General return value from all functions.
Divided into following fields:
<pre>
+-----------+----------+-------------+
| 31        | 30-16    | 15-0        |
| Success=0 | Reserved | Result Code |
| Failed=1  | 0x0000   |             |
+-----------+----------+-------------+
</pre>
 */
typedef unsigned int CSE_RETURN;

/**
Success result.
Do not use this value directly.
Use macros IS_CSE_SUCCESS or IS_CSE_FAILURE to determine result.
 */
#define CSE_SUCCESS 0x00000000
/**
Failure result.
Do not use this value directly.
Use macros IS_CSE_SUCCESS or IS_CSE_FAILURE to determine result.
 */
#define CSE_FAILURE 0x80000000

/* RESULT CODES */
/** Result Code - Unknown Error. */
#define CSE_RESULT_CODE_UNKNOWN_ERROR 0x00000000
/** Result Code - Invalid Arguments. */
#define CSE_RESULT_CODE_INVALID_ARGUMENTS 0x00000001
/** Result Code - Invalid Handle. */
#define CSE_RESULT_CODE_INVALID_HANDLE 0x00000002
/** Result Code - Target Not Open. */
#define CSE_RESULT_CODE_TARGET_NOT_OPEN 0x00000003
/** Result Code - Target Not Locked. */
#define CSE_RESULT_CODE_TARGET_NOT_LOCKED 0x00000004
/** Result Code - Larger buffer argument is required */
#define CSE_RESULT_CODE_LARGER_BUFFER_ARG_REQUIRED 0x00000005

/** Result Code - Read/Write Error. */
#define CSE_RESULT_CODE_READ_WRITE_ERROR 0x00000007
/** Result Code - Target Already Locked. */
#define CSE_RESULT_CODE_ALREADY_LOCKED 0x00000008
/** Result Code - No Target Power. */
#define CSE_RESULT_CODE_NO_TARGET_POWER 0x00000009
/** Result Code - No Attached Cable. */
#define CSE_RESULT_CODE_NO_ATTACHED_CABLE 0x0000000A
/** Result Code - Invalid License. */
#define CSE_RESULT_CODE_INVALID_LICENSE 0x0000000B

/** Result Code - Invalid Target Option. */
#define CSE_RESULT_CODE_INVALID_TARGET_OPTION 0x00000010
/** Result Code - Invalid Target Speed. */
#define CSE_RESULT_CODE_INVALID_TARGET_SPEED 0x00000011

/** Decode success/failure and return result */
#define IS_CSE_SUCCESS(x) (((x) &0x80000000) == 0)
/** Decode success/failure and return result */
#define IS_CSE_FAILURE(x) (((x) &0x80000000) != 0)
/** Decode success/failure and return result */
#define CSE_RESULT_CODE(x) ((x) &0x0000ffff)

/** CSE Message type - Error.  */
#define CSE_MSG_ERROR 0x00000100
/** CSE Message type - Warning.  */
#define CSE_MSG_WARNING 0x00000200
/** CSE Message type - Status. */
#define CSE_MSG_STATUS 0x00000300
/** CSE Message type - Info. */
#define CSE_MSG_INFO 0x00000400
/** CSE Message type - Noise. */
#define CSE_MSG_NOISE 0x00000500
/** CSE Message type - Debug. */
#define CSE_MSG_DEBUG 0x00008000
/** CSE Message type macro */
#define IS_CSE_MSG_ERROR(x) ((x) == CSE_MSG_ERROR)
/** CSE Message type macro */
#define IS_CSE_MSG_WARNING(x) ((x) == CSE_MSG_WARNING)
/** CSE Message type macro */
#define IS_CSE_MSG_STATUS(x) ((x) == CSE_MSG_STATUS)
/** CSE Message type macro */
#define IS_CSE_MSG_INFO(x) ((x) == CSE_MSG_INFO)
/** CSE Message type macro */
#define IS_CSE_MSG_NOISE(x) ((x) == CSE_MSG_NOISE)
/** CSE Message type macro */
#define IS_CSE_MSG_DEBUG(x) (((x) &CSE_MSG_DEBUG) != 0)

/**
Messaging Router function pointer used to pass messages from the session
to the application.

@param handle   (IN)  Handle passed to the operation that called the Message Router function.
@param msgType  (IN)  CSE Message Type.
@param msg      (IN)  Message passed to application.
*/
typedef void(CALLBACK* CSE_MSG_RTR)(CSE_HANDLE handle, unsigned int msgType, const char* msg);

/** Status for Progress Callback for GUI Update meters */
enum CSE_PROGRESS_STATUS
{
	/** The current operation is starting. */
	CSE_START = 0,
	/** The current operation is providing a status update. */
	CSE_UPDATE,
	/** The current operation is ending. */
	CSE_END
};
/** Return value for Progress Callback for GUI Update meters */
enum CSE_PROGRESS_RETURN
{
	/** Return in Progress Callback to stop the current operation if allowed. */
	CSE_STOP = 0,
	/** Return in Progress Callback to continue the current operation. */
	CSE_CONTINUE
};
/**
Progress Callback function pointer for GUI Update meters.  To get the
percentage complete, divide the currentCount by the totalCount.

@param handle       (IN)  Handle passed to the operation that called the Progress Callback.
@param totalCount   (IN)  Total progress count.
@param currentCount (IN)  Current progress count.

@return CSE_CONINUE to continue the operation.
*/
typedef enum CSE_PROGRESS_RETURN(CALLBACK* CSE_PROGRESS_CB)(
	CSE_HANDLE handle,
	unsigned int totalCount,
	unsigned int currentCount,
	enum CSE_PROGRESS_STATUS status);

/** Max IR length supported in bits */
#define CSEJTAG_MAX_IR_LEN 256
/** Required Bypass buffer size */
#define CSEJTAG_BYPASSBUF_LEN ((CSEJTAG_MAX_IR_LEN / 8) + 1)
/** Required IDCODE buffer size */
#define CSE_IDCODEBUF_LEN 128
/** Required device name buffer size */
#define CSEJTAG_MAX_DEVICENAME_LEN 128
/** Required version buffer size */
#define CSEJTAG_VERSION_LEN 128
/** Maximum agregate path length */
#define CSEJTAG_PATH_LEN 1024
/** Required buffer length when passing buffer to functions not listed above */
#define CSEJTAG_INFO_BUFFER_LEN 1024

/** Status of lock returned from CseJtag_target_getLockStatus() */
enum CSEJTAG_TARGET_LOCK_STATUS
{
	/** No target opened. */
	CSEJTAG_NOTARGET,
	/** Target locked by another session. */
	CSEJTAG_LOCKED_OTHER,
	/** Target locked by this session. */
	CSEJTAG_LOCKED_ME,
	/** Target unlocked. */
	CSEJTAG_UNLOCKED,
	/** Target has unknown Lock Status. */
	CSEJTAG_UNKNOWN
};

/** Xilinx target type: Autodetect Parallel or USB. */
#define CSEJTAG_TARGET_AUTO "auto"
/** Xilinx target type: Parallel Cable. */
#define CSEJTAG_TARGET_PARALLEL "xilinx_parallel"
/** Xilinx target type: Platform USB Cable. */
#define CSEJTAG_TARGET_PLATFORMUSB "xilinx_platformusb"
/** Xilinx target type: SVF File Writer (old). */
#define CSEJTAG_TARGET_SVFFILE "xilinx_svffile"
/** Xilinx target type: SVF, XSVF, and STAPL File Writer. */
#define CSEJTAG_TARGET_FILE "xilinx_file"
/** Digilent target type: All Digilent Cables. */
#define CSEJTAG_TARGET_DIGILENT "digilent_plugin"

/**
Structure returned from CseJtag_target_getInfo() function.
Contains information about the plugin

@see CseJtag_target_getInfo
 */
struct CSEJTAG_TARGET_INFO
{
	/** Name used to open the target. */
	char target_name[CSEJTAG_INFO_BUFFER_LEN];
	/** Name of the plug-in library. */
	char plugin_name[CSEJTAG_INFO_BUFFER_LEN];
	/** Firmware version. */
	char fw_ver[CSEJTAG_INFO_BUFFER_LEN];
	/** Driver version. */
	char driver_ver[CSEJTAG_INFO_BUFFER_LEN];
	/** Plug-in version. */
	char plugin_ver[CSEJTAG_INFO_BUFFER_LEN];
	/** Plug-in vendor. */
	char vendor[CSEJTAG_INFO_BUFFER_LEN];
	/** TCK Fequency. */
	char frequency[CSEJTAG_INFO_BUFFER_LEN];
	/** Port. */
	char port[CSEJTAG_INFO_BUFFER_LEN];
	/** Full name of target. */
	char full_name[CSEJTAG_INFO_BUFFER_LEN];
	/** Target Unique ID.  For Xilinx Cables this is the ESN. */
	char target_uid[CSEJTAG_INFO_BUFFER_LEN];
	/** Other information. */
	char rawinfo[CSEJTAG_INFO_BUFFER_LEN];
	/** Target Flags. */
	unsigned int target_flags;
};
/** Bit mask for checking if firmware update is needed. */
#define CSEJTAG_FIRMWARE_UPDATE_NEEDED 0x00000001
/**
Use with CSEJTAG_TARGET_INFO target_flags to check if
a firmware update is needed.
 */
#define IS_CSEJTAG_FIRMWARE_UPDATE_NEEDED(x) (((x) &CSEJTAG_FIRMWARE_UPDATE_NEEDED) != 0)

/**
Bit mask for checking if the usb cable is connected to a high-speed (USB 2.0) port.

Use with CSEJTAG_TARGET_INFO target_flags.
*/
#define CSEJTAG_HIGH_SPEED_PORT 0x00000002
/**
Bit mask for checking if the connected usb cable is the XPress cable.

Use with CSEJTAG_TARGET_INFO target_flags.
*/
#define CSEJTAG_XPRESS_CABLE 0x00000004
/**
Bit mask for checking if the connected usb cable is the Platform Cable USB II cable.

Use with CSEJTAG_TARGET_INFO target_flags.
*/
#define CSEJTAG_USBII_CABLE 0x00000008

/** Shift Mode: Read only */
#define CSEJTAG_SHIFT_READ 0x01
/** Shift Mode: Write only */
#define CSEJTAG_SHIFT_WRITE 0x02
/** Shift Mode: Read/Write */
#define CSEJTAG_SHIFT_READWRITE 0x03
/** Shift Mode: Asynchronous */
#define CSEJTAG_SHIFT_ASYNC 0x04

/** Jtag TAP States defined for CseJtag_tap_navigate(). */
enum CSEJTAG_STATE
{
	/** TEST-LOGIC-RESET. */
	CSEJTAG_TEST_LOGIC_RESET,
	/** RUN-TEST-IDLE. */
	CSEJTAG_RUN_TEST_IDLE,
	/** SELECT-DR-SCAN. */
	CSEJTAG_SELECT_DR_SCAN,
	/** CAPTURE-DR. */
	CSEJTAG_CAPTURE_DR,
	/** SHIFT-DR. */
	CSEJTAG_SHIFT_DR,
	/** EXIT1-DR. */
	CSEJTAG_EXIT1_DR,
	/** PAUSE-DR. */
	CSEJTAG_PAUSE_DR,
	/** EXIT2-DR. */
	CSEJTAG_EXIT2_DR,
	/** UPDATE-DR. */
	CSEJTAG_UPDATE_DR,
	/** SELECT-IR-SCAN. */
	CSEJTAG_SELECT_IR_SCAN,
	/** CAPTURE-IR. */
	CSEJTAG_CAPTURE_IR,
	/** SHIFT-IR. */
	CSEJTAG_SHIFT_IR,
	/** EXIT1-IR. */
	CSEJTAG_EXIT1_IR,
	/** PAUSE-IR. */
	CSEJTAG_PAUSE_IR,
	/** EXIT2-IR. */
	CSEJTAG_EXIT2_IR,
	/** UPDATE-IR. */
	CSEJTAG_UPDATE_IR,
	/** Number of States. */
	CSEJTAG_STATE_SIZE
};

/** JTAG Scan modes for CseJtag_file_scanEndState(). */
enum CSEJTAG_SCANMODE
{
	CSEJTAG_SCAN_IR,
	CSEJTAG_SCAN_DR,
	CSEJTAG_SCANMODE_SIZE
};

/** Jtag Scan Algorithm for CseJtag_tap_autodetect(). */
#define CSEJTAG_SCAN_DEFAULT 0x00
/** Jtag Scan Algorithm for CseJtag_tap_autodetect(). */
#define CSEJTAG_SCAN_TLRSHIFT 0x01
/** Jtag Scan Algorithm for CseJtag_tap_autodetect(). */
#define CSEJTAG_SCAN_WALKING_ONES 0x02

/** Jtag TAP Pins defined for set/get/pulse Pin functions. */
enum CSEJTAG_PIN
{
	/** TDI Pin. */
	CSEJTAG_TDI,
	/** TDO Pin. */
	CSEJTAG_TDO,
	/** TMS Pin. */
	CSEJTAG_TMS,
	/** TCK Pin. */
	CSEJTAG_TCK,
	/** SRST Pin. */
	CSEJTAG_SRST
};

/** Jtag TAP Pins values for set/get Pin functions. */
#define CSEJTAG_PIN_LO 0
/** Jtag TAP Pins values for set/get Pin functions. */
#define CSEJTAG_PIN_HI 1

/***************************************************************************/
/*                            SESSION MANAGEMENT                           */
/***************************************************************************/

/**
Get API version information. versionString must be allocated to size
specified by CSEJTAG_VERSION_LEN by caller.

apiVersion contains the version when viewed in hexadecimal.  The version 9.2.03
has the apiVersion 0x00009203.  The version 10.1 has the apiVersion 0x00010100.

The apiVersion can be decoded as:<br>
MajorVersion = (((apiVersion >> 16) & 0x0000000F) * 10) + ((apiVersion >> 12) & 0x0000000F);<br>
MinorVersion = (apiVersion >> 8) & 0x0000000F;<br>
ServicePackVersion = (((apiVersion >> 4) & 0x0000000F) * 10) + (apiVersion & 0x0000000F);

@param apiVersion       (OUT) API Version number
@param versionString    (OUT) build number version string
@param versionLen       (IN)  Size of version buffer in bytes

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN CseJtag_session_getAPIVersion(
	unsigned int* apiVersion, char* versionString, unsigned int versionLen);


/**
Send a message to the message router function.
@see CseJtag_session_create()

@todo From bkfross: Investigate flags for debug messages to toggle categories
	  of debug messages on/off.

@param handle   (IN)  Session handle
@param msgType  (IN)  Message type
@param msg      (IN)  Standard C string message to pass

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN
CseJtag_session_sendMessage(CSE_HANDLE handle, unsigned int msgType, const char* msg);


/**
Create and initialize a session. This is the first call made to
CSE. The returned session handle allows you to open
and control target devices (such as JTAG cables). Passing NULL
to the messageRouterFn means simply route all messages to stdout.
The messageRouterFn should not be set to NULL if the application
uses a graphical user interface as this may result in all
messages being discarded.  The argv/argc arguments are used to specify
optional arguments by passing an array of c-strings and the array count.
This does not take in the same optional arguments as CseJtag_target_open().

CseJtag_session_create looks for files required to use CSE
(idcode.lst and idcode.acd) in the directory named data/cse in a
location two directory levels up relative to the library
libCseJtag.so/dll (e.g., It will look in "../../data/cse/").
The function will return CSE_FAILURE if the data directory or the
required files cannot be found.

On Windows, libCseJtag.dll is found by looking in the current
directory first, and then in the PATH variable.  On Linux, libCseJtag.so
is found by looking in the LD_LIBRARY_PATH variable.  This is
the same method in which the operating systems searches for
a library.

To specify a server when using the CseJtagClient library, add two
c-strings "-server" followed by "<hostname>" to the argv and increment
argc by 2. To specify a port, add the c_str "-port" followed
by "<portnumber>" to argv and increment argc by 2.  This will only
connect the application to a ChipScope Server.  Any other arguments
passed into argv will be discarded by CseJtag_session_create.

@todo Enhance handle management so handles are re-used instead of simply
	  incrementing by 1 each time. We will run out of session handles
	  after 65535 create session calls with the current
	  non-recycling system.


@param messageRouterFn (IN)  Message router function. NULL means just pass
							 messages to stdout
@param argc            (IN)  Number of arguments
@param argv            (IN)  Argument array to pass optional arguments
@param handle          (OUT) Session handle returned

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_session_create(
	CSE_MSG_RTR messageRouterFn, unsigned int argc, const char* argv[], CSE_HANDLE* handle);

/**
Destroy and free resources used by an existing session. This is the
opposite of CseJtag_createSession.

@param handle         (IN) Session handle

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_session_destroy(CSE_HANDLE handle);

/**
Return the number of known targets in the system. Each plugin library supports
at least one target but possibly more. For example, the Xilinx libCommProxyPlugin
handles xilinx_parallel and xilinx_platformusb cable connections. After
obtaining the total number of targets in the system, you may call
CseJtag_session_getTargetByIndex() and CseJtag_session_getTargetXMLSizeByIndex()
to get additional information about each target.

@param handle  (IN)  Session handle
@param count   (OUT) Count of known targets

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_session_getTargetCount(CSE_HANDLE handle, unsigned int* count);


/**
Returns the required xml buffer size for a given target. Call this function
prior to calling CseJtag_session_getTargetByIndex() with the computed
buffer size.

@param handle     (IN)  Session handle
@param targetIdx  (IN)  Index of the target (0..count-1)
@param xmlBufLen  (OUT) Size of xml buffer in bytes

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_session_getTargetXMLSizeByIndex(
	CSE_HANDLE handle, unsigned int targetIdx, unsigned int* xmlBufLen);

/**
Returns a target given its index. The target name is a unique string that
identifies the cable type. For example, xilinx_parallel identifies a parallel
cable. Prior to calling this function use CseJtag_session_getTargetCount() to
find the number of supported targets on this system. Make sure targetName and
pluginName are allocated of size CSEJTAG_PATH_LEN prior to calling.

isGUIVisible returns a 0 (false) or 1 (true) depending if a targetName should be
visible in a GUI.  This is only a suggestion that is provided in the plug-in xml.
It is useful for plug-ins that cannot read back data, such as file writers.

The XML description contains information to describe all target options for
automated application GUI dialog setup. An example of the XML format can be
found in:

< location of libCseJtag >/plugins/Xilinx/libCseCommProxyPlugin/libCseCommProxyPlugin.xml

Prior to calling this function, call CseJtag_session_getTargetXMLSizeByIndex()
to determine the required size of the xml buffer to be passed into xmlBufLen.

@param handle        (IN)  Session handle
@param targetIdx     (IN)  Index of the target (0..count-1)
@param xmlbuf        (OUT) Target information in XML
@param xmlBufLen     (IN)  Size of xml buffer in bytes
@param targetName    (OUT) Target Name
@param targetNameLen (IN)  Size of name buffer
@param pluginName    (OUT) Plugin associated with this target
@param pluginNameLen (IN)  Size of the name buffer
@param isGUIVisible  (OUT) 0=false; 1=true

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_session_getTargetByIndex(
	CSE_HANDLE handle,
	unsigned int targetIdx,
	char* xmlbuf,
	unsigned int xmlBufLen,
	char* targetName,
	unsigned int targetNameLen,
	char* pluginName,
	unsigned int pluginNameLen,
	unsigned int* isGUIVisible);


/***************************************************************************/
/*                            TARGET OPERATIONS                            */
/***************************************************************************/


/**
Open a target device (usually a JTAG cable) and associate with this session.
Currently only one target can be opened and associated with a session at a time.
This limitation may change in the future.  The argv/argc arguments are used to specify
optional arguments by passing an array of c-strings and the array count.
This does not take in the same optional arguments as CseJtag_session_create().

Use the optional argument "AVOID_FIRMWARE_UPDATE=TRUE" to close the cable and
prevent a firmware update if one is available.  This allows you to provide the
user with an option of waiting for the firmware update or using a different target.
Returns CSE_SUCCESS when cable is closed.  You must use
"IS_CSEJTAG_FIRMWARE_UPDATE_NEEDED(info->target_flags)" to check if the cable was closed.

For the target CSEJTAG_TARGET_PLATFORMUSB, the port specifies which USB cable to use.
USB21 is the first cable connected, USB22 is the second cable connected, etc...  However,
Once the computer is restarted, if there is more than one cable connected, the port numbers
are not guaranteed to be the same and must be reconnected in the order desired.

The use of Xilinx Cables requires Jungo license strings for different cables and platforms.
They must be passed in using the format "license=<cabletype>:<platforms>:<licensestring>".
Only the license string may contain whitespace.  Cable type must be usb or par.  Platforms
can be a comma seperated list that contains all of the platforms which the license supports.
All of the licenses that the applications supports can be passed in.  Examples are contained
in the table below.

If targetName is CSEJTAG_TARGET_AUTO, the first Xilinx cable connection will
be automatically opened.  Other valid targetNames are listed in the table below:

<!-- (same as html table below)
---------------------------------------------------------------------------------------------------------------------------
|targetName                 | argc | argv |
---------------------------------------------------------------------------------------------------------------------------
|CSEJTAG_TARGET_AUTO        | 2+   | {"license=usb:nt:<license string>", | |
|      |  "license=par:nt:<license string>",                                                  | |
|      |  ...}                                                                                |
---------------------------------------------------------------------------------------------------------------------------
|CSEJTAG_TARGET_PARALLEL    | 3+   | {"port=LPT1 | LPT2 | LPT3", | |                           |
|  "frequency=5000000 | 2500000 | 1250000 | 625000 | 200000",                          | |
|      |  "license=par:nt:<license string>",                                                  | |
|      |  "license=par:lin:<license string>",                                                 | |
|      |  "license=par:nt64,lin64:<license string>"}                                          |
---------------------------------------------------------------------------------------------------------------------------
|CSEJTAG_TARGET_PLATFORMUSB | 3+   | {"port=USB2 (aliased to USB21) | USB21 | USB22 | USB23 | ...",
| |                           |      |  "frequency=12000000 | 6000000 | 3000000 | 1500000 | 750000",
| |                           |      |  "license=usb:nt:<license string>",
| |                           |      |  "license=usb:lin:<license string>",
| |                           |      |  "license=usb:nt64,lin64:<license string>"}
|
---------------------------------------------------------------------------------------------------------------------------
|CSEJTAG_TARGET_FILE        | 2    | {"file=<svf or stapl filename with full path>", | |
|      |  "mode=create |  append"}                                                            |
---------------------------------------------------------------------------------------------------------------------------
|CSEJTAG_TARGET_SVFFILE(old)| 1    | {"fname=<svf filename with full path>"} |
---------------------------------------------------------------------------------------------------------------------------
-->
<table border="1" cellpadding="2">
<tr>
<td>targetName</td><td>argc</td><td>argv</td>
</tr>
<tr>
<td>CSEJTAG_TARGET_AUTO</td>
<td>2+</td>
<td>{<br>"license=usb:nt:<license string>",<br>"license=par:nt:<license string>",<br>...<br>}</td>
</tr>
<tr>
<td>CSEJTAG_TARGET_PARALLEL</td>
<td>3+</td>
<td>{<br>"port=LPT1 | LPT2 | LPT3",<br>"frequency=5000000 | 2500000 | 1250000 | 625000 |
200000",<br>"license=par:nt:<license string>",<br>"license=par:lin:<license
string>",<br>"license=par:nt64,lin64:<license string>"<br>}</td>
</tr>
<tr>
<td>CSEJTAG_TARGET_PLATFORMUSB</td>
<td>3+</td>
<td>{<br>"port=USB2 (aliased to USB21) | USB21 | USB22 | USB23 | ...",<br>"frequency=12000000 |
6000000 | 3000000 | 1500000 | 750000",<br>"license=usb:nt:<license
string>",<br>"license=usb:lin:<license string>",<br>"license=usb:nt64,lin64:<license
string>"<br>}</td>
</tr>
<tr>
<td>CSEJTAG_TARGET_FILE</td>
<td>2</td>
<td>{<br>"file=<svf or stapl filename with full path>",<br>"mode=create |  append"<br>}</td>
<tr>
<td>CSEJTAG_TARGET_SVFFILE(old)</td>
<td>1</td>
<td>{<br>"fname=<svf filename with full path>"<br>}</td>
</tr>
</table>

@param handle       (IN) session handle
@param targetName   (IN) name of target to open, "auto" to autodetect
@param progressFunc (IN) Optional Progress callback function for GUI update meters
@param argc         (IN) number of arguments
@param argv         (IN) argument array to pass port and frequency
@param info         (OUT) Target status and version information, can be NULL

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_open(
	CSE_HANDLE handle,
	const char* targetName,
	CSE_PROGRESS_CB progressFunc,
	unsigned int argc,
	const char* argv[],
	struct CSEJTAG_TARGET_INFO* info);


/**
Close a target device.

@param handle  (IN) session handle

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_close(CSE_HANDLE handle);


/**
Test and return connection state.

@param handle      (IN)  handle to the session instance
@param isConnected (OUT) 1=true; 0=false

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_isConnected(CSE_HANDLE handle, unsigned int* isConnected);


/**
Attempt to obtain an exclusive lock on target. This function will
ONLY succeed if the target resource is currently unlocked. This
following table summarizes status and function return values for
the different cases.

<!-- (same as html table below)
+--------------------+-----------------------+-----------------+
| CURRENT LOCK OWNER | STATUS RETURNED       | FUNCTION RETURN |
+--------------------+-----------------------+-----------------+
| Nobody             | CSEJTAG_LOCKED_ME     | CSE_SUCCESS     |
| This session       | CSEJTAG_LOCKED_ME     | CSE_FAILURE     |
| Another session    | CSEJTAG_LOCKED_OTHER  | CSE_FAILURE     |
| Error determining  | CSEJTAG_UNKNOWN       | CSE_FAILURE     |
+--------------------+-----------------------+-----------------+
-->
<table border="1" cellpadding="2">
<tr>
<td>CURRENT LOCK OWNER</td><td>STATUS RETURNED</td><td>FUNCTION RETURN</td>
</tr>
<tr>
<td>Nobody</td><td>CSEJTAG_LOCKED_ME</td><td>CSE_SUCCESS</td>
</tr>
<tr>
<td>This session</td><td>CSEJTAG_LOCKED_ME</td><td>CSE_FAILURE</td>
</tr>
<tr>
<td>Another session</td><td>CSEJTAG_LOCKED_OTHER</td><td>CSE_FAILURE</td>
</tr>
<tr>
<td>Error determining</td><td>CSEJTAG_UNKNOWN</td><td>CSE_FAILURE</td>
</tr>
</table><br>
SIDE EFFECTS:
- On success, tap state is forced to TLR to guarantee known state on lock

@param handle  (IN)  session handle
@param msWait  (IN)  wait time in milliseconds before giving up (-1=wait forever)
@param status  (OUT) lock status return (same as getLockStatus)

@return CSE_SUCCESS on lock success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_lock(CSE_HANDLE handle, int msWait, enum CSEJTAG_TARGET_LOCK_STATUS* status);

/**
Release exclusive lock on target.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

SIDE EFFECTS:
	- TAP state is set to RTI at unlock to hand back in a known state.
	- This will flush the cable buffer if the target is CSEJTAG_TARGET_PLATFORMUSB.

@param handle  (IN) session handle

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_unlock(CSE_HANDLE handle);


/**
Get lock status for the target device.

@param handle  (IN)  Session handle
@param args    (OUT) Target lock status

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_getLockStatus(CSE_HANDLE handle, enum CSEJTAG_TARGET_LOCK_STATUS* status);


/**
Clean up cable locks. Only call as a last resort. This function
kills sharing semaphores. Currently only works with Xilinx
Parallel and Platform USB cables.

@param handle   (IN)  handle to the session instance (can be NULL)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_cleanLocks(CSE_HANDLE handle);


/**
Flush the target's buffer.

Useful for the CSEJTAG_TARGET_PLATFORMUSB target which buffers
all navigate, shift and pin functions.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle   (IN)  handle to the session instance

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_flush(CSE_HANDLE handle);


/**
Set a JTAG TAP pin's value.  This function only works with
CSEJTAG_TDI and CSEJTAG_TMS pins.  To change the CSEJTAG_TCK
pin, use CseJtag_target_pulsePin().

Warning: If using this function to change the TAP state,
CseJtag does not keep track of the TAP state.  Before using
any of the tap functions, first use CseJtag_tap_navigate() with
CSEJTAG_TEST_LOGIC_RESET to return to a known state.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle   (IN)  handle to the session instance
@param pin      (IN)  pin indentifier
@param value    (IN)  pin value (1=set, 0=clear)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_setPin(CSE_HANDLE handle, enum CSEJTAG_PIN pin, unsigned int value);


/**
Get a JTAG TAP pin's value.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle   (IN)  handle to the session instance
@param pin      (IN)  pin indentifier
@param value    (OUT)  pin value (1=set, 0=clear)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_getPin(CSE_HANDLE handle, enum CSEJTAG_PIN pin, unsigned int* value);


/**
Pulse a JTAG TAP pin.

Warning: If using this function to change the TAP state,
CseJtag does not keep track of the TAP state.  Before using
any of the tap functions, first use CseJtag_tap_navigate() with
CSEJTAG_TEST_LOGIC_RESET to return to a known state.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle   (IN)  handle to the session instance
@param pin      (IN)  pin indentifier
@param count    (IN)  Number of times to pulse the pin

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_pulsePin(CSE_HANDLE handle, enum CSEJTAG_PIN pin, unsigned int count);


/**
Wait for a specified amount of time.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle   (IN)  handle to the session instance
@param us       (IN)  Microseconds to wait

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_waitTime(CSE_HANDLE handle, unsigned int us);

/**
Send a command directly to the target command processor. This is
used to pass sideband communication directly to a target device when no
API call is available.

Note:
 - Outbuf must be allocated by caller. Target will copy data into outbuf
   but assumes outbuf has already been allocated. Call will fail if
   outbuflen is too small to hold results of function.

@param handle       (IN)     Session handle
@param cmd          (IN)     command c-string
@param inbuf        (IN)     pointer to input buffer
@param inbuflen     (IN)     length in bytes of input buffer
@param outbuf       (OUT)    output buffer (null if not used in command)
@param outbuflen    (IN/OUT) length of returned buffer
@param progressFunc (IN)     Optional Progress callback function for GUI update meters

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_target_sendCommand(
	CSE_HANDLE handle,
	const char* cmd,
	unsigned char* inbuf,
	unsigned int inbuflen,
	unsigned char* outbuf,
	unsigned int* outbuflen,
	CSE_PROGRESS_CB progressFunc);

/**
Send a parameter string to the target. Used to configure target options such as
frequency.

<!-- (same as html table below)
-----------------------------------------------------------------------------------------------------------------------------
|key                  | targetName                   | valid values        | description |
-----------------------------------------------------------------------------------------------------------------------------
|"frequency"          | CSEJTAG_TARGET_PLATFORMUSB   | see                 | Change the frequency of
Xilinx Cables.         | |                     | CSEJTAG_TARGET_PARALLEL      | CseJtag_target_open
|                                                |
-----------------------------------------------------------------------------------------------------------------------------
|"blink_led"          | CSEJTAG_TARGET_PLATFORMUSB   | "true" or "false"   | Blink's the LED on
Xilinx USB Cables when      | |                     |                              |
| set to true.                                   |
-----------------------------------------------------------------------------------------------------------------------------
|"skip_rti_on_shift"  | All Targets                  | "true" or "false"   | If set to true, this
will skip RUN_TEST_IDLE   | |                     |                              |
| when changing to a SHIFT state during calls to | |                     |
|                     | any CseJtag_tap_shift function.                |
-----------------------------------------------------------------------------------------------------------------------------
-->
<table border="1" cellpadding="2">
<tr>
<td>key</td><td>targetName</td><td>valid values</td><td>description</td>
</tr>
<tr>
<td>"frequency"</td>
<td>CSEJTAG_TARGET_PLATFORMUSB<br>CSEJTAG_TARGET_PARALLEL</td>
<td>see CseJtag_target_open()</td>
<td>Change the frequency of Xilinx Cables.</td>
</tr>
<tr>
<td>"blink_led"</td>
<td>CSEJTAG_TARGET_PLATFORMUSB</td>
<td>"true" or "false"</td>
<td>Blink's the LED on Xilinx USB Cables when set to true.</td>
</tr>
<tr>
<td>"skip_rti_on_shift"</td>
<td>All Targets</td>
<td>"true" or "false"</td>
<td>If set to true, this will skip RUN_TEST_IDLE when changing to a SHIFT state during calls to any
CseJtag_tap_shift function.</td>
</tr>
</table>

@param handle   (IN)  Session handle
@param key      (IN)  option name
@param value    (IN)  option value

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_setOption(CSE_HANDLE handle, const char* key, const char* value);

/**
Get a parameter string from the target. Used to obtain target options such as
ESN.

Note:
On entry, valueLen must be the size of the value buffer. On exit, valueLen is
set with the actual number of bytes returned.

<!-- (same as html table below)
---------------------------------------------------------------------------------------------------------------------------
|key                  | targetName                   | min valueLen        | description |
---------------------------------------------------------------------------------------------------------------------------
|"electronic_sn"      | CSEJTAG_TARGET_PLATFORMUSB   | 128                 | Get the ESN of Xilinx
Cables if available.   |
---------------------------------------------------------------------------------------------------------------------------
-->
<table border="1" cellpadding="2">
<tr>
<td>key</td><td>targetName</td><td>min valueLen</td><td>description</td>
</tr>
<tr>
<td>"electronic_sn"</td>
<td>CSEJTAG_TARGET_PLATFORMUSB</td>
<td>128</td>
<td>Get the ESN of Xilinx Cables if available.</td>
</tr>
</table>

@param handle    (IN)     Session handle
@param key       (IN)     option name
@param value     (OUT)    option value
@param valueLen  (IN/OUT) value buffer size

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_target_getOption(CSE_HANDLE handle, const char* key, char* value, unsigned int* valueLen);

/**
Grabs target version information.

A lock does not need to be obtained prior to calling this function.

@param handle  (IN)  Session handle
@param info    (OUT) Target status and version information

@return CSE_SUCCESS on success, Non-zero error code on failure
 */

CSEJTAG_DLL CSE_RETURN CseJtag_target_getInfo(CSE_HANDLE handle, struct CSEJTAG_TARGET_INFO* info);


/***************************************************************************/
/*                          JTAG CHAIN OPERATIONS                          */
/***************************************************************************/

/**
Attempt to autodetect the current jtag chain. This function first obtains ID
codes for devices in the chain. Then devices with known ID codes have IR lengths
automatically assigned. Unrecognized devices must have their IR lengths assigned
manually. Algorithm selection is CSEJTAG_SCAN_DEFAULT, CSEJTAG_SCAN_TLRSHIFT or
CSEJTAG_SCAN_WALKING_ONES. Some non-IEEE standard parts will work with one
algorithm or the other but not both. Some non-IEEE standard parts will not work
with blind interrogation at all.

This call will fail if the JTAG chain could not be completely
detected and manual intervention is required.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

SIDE EFFECTS:
	- Calls CseJtag_tap_interrogateChain() to interrogate jtag chain idcodes
	- Calls CseJtag_tap_getDeviceCount() to set the device count
	- Calls CseJtag_tap_setDevice... to set information for each device
	  after matching idcode in the device database

@param  handle      (IN)  handle to the session instance
@param  algorithm   (IN)  RESERVED (pass 0)
@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_tap_autodetectChain(CSE_HANDLE handle, unsigned int algorithm);


/**
Blind scans the jtag chain to get IDCODES and chain length. Algorithm selection
is CSEJTAG_SCAN_DEFAULT, CSEJTAG_SCAN_TLRSHIFT, or CSEJTAG_SCAN_WALKING_ONES.
Some non-IEEE standard parts will work with one algorithm or the other but not
both. Some non-IEEE standard parts will not work with blind interrogation at
all.

CSEJTAG_SCAN_WALKING_ONES
- Set each device to bypass by shifting long stream of 1's into IR
- Shift DR pattern into TDI and wait for pattern on TDO. Number of
  shifts determines jtag chain length.
- Do CSEJTAG_SCAN_TLRSHIFT algorithm to get idcodes for each device.

CSEJTAG_SCAN_TLRSHIFT
- Move to TLR
- Shift out bits until all IDCODES (or BYPASS bits) are read


RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

SIDE EFFECTS:
	- Calls CseJtag_tap_getDeviceCount() to set the device count
	- Calls CseJtag_tap_setDevice... to set idcode for each device

@param  handle      (IN)  handle to the session instance
@param  algorithm   (IN)  RESERVED (pass 0)
@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_tap_interrogateChain(CSE_HANDLE handle, unsigned int algorithm);


/**
Return the number of jtag devices in the chain.

Note: Current Cse returns failure if jtag chain length
	  is invalid. Maybe we should rethink that and return success
	  and set the count to 0 instead. Function failure indicates
	  something extremely bad happened and that is not necessarily
	  the case.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle      (IN)  handle to the session instance
@param count       (OUT) number of devices in the chain

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_tap_getDeviceCount(CSE_HANDLE handle, unsigned int* count);


/**
Set the JTAG chain device count.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle      (IN)  handle to the session instance
@param count       (IN)  number of devices in the chain

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_tap_setDeviceCount(CSE_HANDLE handle, unsigned int count);


/**
Get the Bypass Instruction for a device.
The Bypass instruction defaults to all 1's and is shifted into each
device Instruction Register as needed to place device in bypass.
cmd_bypass[] buffer should be preallocated as unsigned char
*buf[CSEJTAG_BYPASSBUF_LEN].

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- Device count MUST have been set prior to calling this function with
	  CseJtag_tap_setDeviceCount()

@param handle       (IN)  handle to the session instance
@param deviceIndex  (IN)  device number (0..n-1) in the jtag chain
@param cmd_bypass   (OUT) bypass instruction string (c string of '1' or '0')
@param bypassbufLen (IN)  cmd_bypass buffer size in bytes (should be CSE_BYPASSBUF_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_tap_getBypassCmd(
	CSE_HANDLE handle, unsigned int deviceIndex, char* cmd_bypass, unsigned int bypassbufLen);


/**
<b>**NOT IMPLEMENTED YET**</b> Set the Bypass Instruction for a device.
The Bypass instruction defaults to all 1's and is shifted into each
device Instruction Register as needed to place device in bypass.
cmd_bypass[] buffer should be preallocated as unsigned char
*buf[CSEJTAG_BYPASSBUF_LEN].

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- Device count MUST have been set prior to calling this function with
	  CseJtag_tap_setDeviceCount()

@param handle       (IN)  handle to the session instance
@param deviceIndex  (IN)  device number (0..n-1) in the jtag chain
@param cmd_bypass   (IN)  bypass instruction string (c string of '1' or '0')

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_tap_setBypassCmd(CSE_HANDLE handle, unsigned int deviceIndex, char* cmd_bypass);


/**
Get the IR length of a device.  The IR length is used to determine the amount of
padding required to shift an instruction into a device register. Shift and
navigate functions WILL NOT WORK until all devices have the instruction register
lengths set up correctly. CseJtag_tap_autodetectChain() set IR lengths
automatically.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- Device count MUST have been set prior to calling this function with
	  CseJtag_tap_setDeviceCount()

@param handle      (IN)  handle to the session instance
@param deviceIndex (IN)  device number (0..n-1) in the jtag chain
@param irlen       (OUT) size in bits of the IR

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_tap_getIRLength(CSE_HANDLE handle, unsigned int deviceIndex, unsigned int* irlen);

/**
Set the IR length of a device. This MUST be called for each device in the chain.
The IR length is used to determine the amount of padding required to shift an
instruction into a device register. Shift and navigate functions WILL NOT WORK
until all devices have the instruction register lengths set up correctly.
CseJtag_tap_autodetectChain() set IR lengths automatically.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- Device count MUST have been set prior to calling this function with
	  CseJtag_tap_setDeviceCount()

@param handle      (IN)  handle to the session instance
@param deviceIndex (IN)  device number (0..n-1) in the jtag chain
@param irlen       (IN)  size in bits of the IR

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_tap_setIRLength(CSE_HANDLE handle, unsigned int deviceIndex, unsigned int irlen);

/**
Return the 32 bit device ID for a given device in the chain. If the device
does not support IDCODE, a 0 length string is returned as idcode.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- Device count MUST have been set prior to calling this function with
	  CseJtag_tap_setDeviceCount()

@param handle      (IN)  handle to the session instance
@param deviceIndex (IN)  device number to obtain ID code for (0..n-1)
@param idcode      (OUT) idcode string (c string of '1' or '0')
@param idcodeLen   (IN)  idocde buffer size in bytes (should be CSE_IDCODEBUF_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_tap_getDeviceIdcode(
	CSE_HANDLE handle, unsigned int deviceIndex, char* idcode, unsigned int idcodeLen);

/**
Sets the device idcode for a given device in the chain. Passing a null string
indicates the device does not support an idcode.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- Device count MUST have been set prior to calling this function with
	  CseJtag_tap_setDeviceCount()

@param handle      (IN)  handle to the session instance
@param deviceIndex (IN)  device index (0..n-1)
@param idcode      (IN)  idcode string (c string of '1' or '0')

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_tap_setDeviceIdcode(CSE_HANDLE handle, unsigned int deviceIndex, char* idcode);


/**
Navigate to a new tap state.

When the microseconds argument is non-zero, the target's buffer is flushed
and the operating system's sleep command is called for the specified amount.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle       (IN)  session or macro handle
@param newState     (IN)  state to navigate into
@param clockRepeat  (IN)  number of additional times to clock in the new state
@param microSeconds (IN)  number of microseconds to sleep after navigation and clock repeats

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN CseJtag_tap_navigate(
	CSE_HANDLE handle,
	enum CSEJTAG_STATE newState,
	unsigned int clockRepeat,
	unsigned int microSeconds);


/**
Shift a stream of bits in/out of the instruction register of the jtag chain.

The order in which the buffers are shifted in/out starts with the lsb bit
at index 0, which can obtained with the following: (buf[0] & 0x01).  The
2nd bit is: (buf[0] & 0x02), the 3rd bit is: (buf[0] & 0x04), ..., the 9th
bit is: (buf[1] & 0x01), the 10th bit is: (buf[1] & 0x02), and so on...

By default, this function will always navigate to RUN_TEST_IDLE before navigating
to SHIFT_IR, except when the state is already SHIFT_IR.  Even if the state is PAUSE_IR,
it will transition to RUN_TEST_IDLE first.  In order to skip RUN_TEST_IDLE and instead take
the shortest path to SHIFT_IR, use CseJtag_target_setOption(handle, "skip_rti_on_shift", "true").
Using CseJtag_target_setOption(handle, "skip_rti_on_shift", "false") will return to the
default behavior.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle       (IN) session or macro handle
@param shiftMode    (IN) CSEJTAG_SHIFT_READ, CSEJTAG_SHIFT_WRITE, CSEJTAG_SHIFT_READWRITE
@param exitState    (IN) state to end in (CSEJTAG_SHIFT_IR if no state change desired)
@param progressFunc (IN) Optional Progress callback function for GUI update meters
@param bitCount     (IN) number of bits to transfer
@param tdibuf       (IN) uchar array buffer of bits to shift in TDI and received from TDO
@param tdobuf       (IN/OUT) uchar array buffer to place bits received from TDO (may be same as
tdibuf)
@param tdimask      (IN) Optional TDI mask per svf spec, used for file output only (set to null if
unused)
@param tdomask      (IN) Optional TDO mask per svf spec, used for file output only (set to null if
unused)

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN CseJtag_tap_shiftChainIR(
	CSE_HANDLE handle,
	unsigned int shiftMode,
	enum CSEJTAG_STATE exitState,
	CSE_PROGRESS_CB progressFunc,
	unsigned int bitCount,
	const unsigned char* tdibuf,
	unsigned char* tdobuf,
	const unsigned char* tdimask,
	const unsigned char* tdomask);


/**
Shift a stream of bits in/out of the instruction register of a given device.
Performs the necessary header and trailer padding to communicate directly with
the given device. If device is -1, no padding will be applied.

ShiftDeviceIR puts all devices except the target into bypass by setting their
IR registers to the BYPASS instruction. This function should be called BEFORE calling
shiftDeviceDR() to ensure all chain devices except the target are put into bypass.

The order in which the buffers are shifted in/out starts with the lsb bit
at index 0, which can obtained with the following: (buf[0] & 0x01).  The
2nd bit is: (buf[0] & 0x02), the 3rd bit is: (buf[0] & 0x04), ..., the 9th
bit is: (buf[1] & 0x01), the 10th bit is: (buf[1] & 0x02), and so on...

By default, this function will always navigate to RUN_TEST_IDLE before navigating
to SHIFT_IR, except when the state is already SHIFT_IR.  Even if the state is PAUSE_IR,
it will transition to RUN_TEST_IDLE first.  In order to skip RUN_TEST_IDLE and instead take
the shortest path to SHIFT_IR, use CseJtag_target_setOption(handle, "skip_rti_on_shift", "true").
Using CseJtag_target_setOption(handle, "skip_rti_on_shift", "false") will return to the
default behavior.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- The number of bits shifted in to the device IR must be exactly equal
	  to the IR length of the device for shiftDeviceIR() or the call fails

@param handle       (IN) session or macro handle
@param deviceIndex  (IN) device number to communicate with in the chain
@param shiftMode    (IN) CSEJTAG_SHIFT_READ, CSEJTAG_SHIFT_WRITE, CSEJTAG_SHIFT_READWRITE
@param exitState    (IN) state to end in (CSEJTAG_SHIFT_IR if no state change desired)
@param progressFunc (IN) Optional Progress callback function for GUI update meters
@param bitCount     (IN) number of bits to transfer
@param tdibuf       (IN) uchar array buffer of bits to shift in TDI and received from TDO
@param tdobuf       (IN/OUT) uchar array buffer to place bits received from TDO (may be same as
tdibuf)
@param tdimask      (IN) Optional TDI mask per svf spec, used for file output only (set to null if
unused)
@param tdomask      (IN) Optional TDO mask per svf spec, used for file output only (set to null if
unused)

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN CseJtag_tap_shiftDeviceIR(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	unsigned int shiftMode,
	enum CSEJTAG_STATE exitState,
	CSE_PROGRESS_CB progressFunc,
	unsigned int bitCount,
	const unsigned char* tdibuf,
	unsigned char* tdobuf,
	const unsigned char* tdimask,
	const unsigned char* tdomask);


/**
Shift a stream of bits in or out of the data register of the jtag chain.

When using the CSEJTAG_SHIFT_READ shiftMode, you can specify a what constant
to shift in by setting tdibuf[0] to 0 or 1.  Normally, a CSEJTAG_SHIFT_READ is
translated into a CSEJTAG_SHIFT_READWRITE with the constant shifted in.
However, CSEJTAG_TARGET_PLATFORMUSB has a much faster read only feature
available when tdibuf[0] is set to 0.

The order in which the buffers are shifted in/out starts with the lsb bit
at index 0, which can obtained with the following: (buf[0] & 0x01).  The
2nd bit is: (buf[0] & 0x02), the 3rd bit is: (buf[0] & 0x04), ..., the 9th
bit is: (buf[1] & 0x01), the 10th bit is: (buf[1] & 0x02), and so on...

By default, this function will always navigate to RUN_TEST_IDLE before navigating
to SHIFT_DR, except when the state is already SHIFT_DR.  Even if the state is PAUSE_DR,
it will transition to RUN_TEST_IDLE first.  In order to skip RUN_TEST_IDLE and instead take
the shortest path to SHIFT_DR, use CseJtag_target_setOption(handle, "skip_rti_on_shift", "true").
Using CseJtag_target_setOption(handle, "skip_rti_on_shift", "false") will return to the
default behavior.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.

@param handle       (IN) session or macro handle
@param shiftMode    (IN) CSEJTAG_SHIFT_READ, CSEJTAG_SHIFT_WRITE, CSEJTAG_SHIFT_READWRITE
@param exitState    (IN) state to end in (CSEJTAG_SHIFT_DR if no state change desired)
@param progressFunc (IN) Optional Progress callback function for GUI update meters
@param bitCount     (IN) number of bits to transfer
@param tdibuf       (IN) uchar array buffer of bits to shift in TDI and received from TDO
@param tdobuf       (IN/OUT) uchar array buffer to place bits received from TDO (may be same as
tdibuf)
@param tdimask      (IN) Optional TDI mask per svf spec, used for file output only (set to null if
unused)
@param tdomask      (IN) Optional TDO mask per svf spec, used for file output only (set to null if
unused)

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN CseJtag_tap_shiftChainDR(
	CSE_HANDLE handle,
	unsigned int shiftMode,
	enum CSEJTAG_STATE exitState,
	CSE_PROGRESS_CB progressFunc,
	unsigned int bitCount,
	const unsigned char* tdibuf,
	unsigned char* tdobuf,
	const unsigned char* tdimask,
	const unsigned char* tdomask);

/**
Shift a stream of bits in or out of the data register of a given device. If
device is non-negative, this performs the necessary header and trailer padding
to communicate directly with the device.

When using the CSEJTAG_SHIFT_READ shiftMode, you can specify a what constant
to shift in by setting tdibuf[0] to 0 or 1.  Normally, a CSEJTAG_SHIFT_READ is
translated into a CSEJTAG_SHIFT_READWRITE with the constant shifted in.
However, CSEJTAG_TARGET_PLATFORMUSB has a much faster read only feature
available when tdibuf[0] is set to 0.

The order in which the buffers are shifted in/out starts with the lsb bit
at index 0, which can obtained with the following: (buf[0] & 0x01).  The
2nd bit is: (buf[0] & 0x02), the 3rd bit is: (buf[0] & 0x04), ..., the 9th
bit is: (buf[1] & 0x01), the 10th bit is: (buf[1] & 0x02), and so on...

By default, this function will always navigate to RUN_TEST_IDLE before navigating
to SHIFT_DR, except when the state is already SHIFT_DR.  Even if the state is PAUSE_DR,
it will transition to RUN_TEST_IDLE first.  In order to skip RUN_TEST_IDLE and instead take
the shortest path to SHIFT_DR, use CseJtag_target_setOption(handle, "skip_rti_on_shift", "true").
Using CseJtag_target_setOption(handle, "skip_rti_on_shift", "false") will return to the
default behavior.

RESTRICTIONS:
	- A lock MUST be obtained first with the CseJtag_target_lock() function before
	  calling this function.
	- For shiftDeviceDR(), all other devices should be put into BYPASS by calling
	  the shiftDeviceIR() function immediately before.

@param handle       (IN) session or macro handle
@param deviceIndex  (IN) device number to communicate with in the chain
@param shiftMode    (IN) CSEJTAG_SHIFT_READ, CSEJTAG_SHIFT_WRITE, CSEJTAG_SHIFT_READWRITE
@param exitState    (IN) state to end in (CSEJTAG_SHIFT_DR if no state change desired)
@param progressFunc (IN) Optional Progress callback function for GUI update meters
@param bitCount     (IN) number of bits to transfer
@param tdibuf       (IN) uchar array buffer of bits to shift in TDI and received from TDO
@param tdobuf       (IN/OUT) uchar array buffer to place bits received from TDO (may be same as
tdibuf)
@param tdimask      (IN) Optional TDI mask per svf spec, used for file output only (set to null if
unused)
@param tdomask      (IN) Optional TDO mask per svf spec, used for file output only (set to null if
unused)

@return CSE_SUCCESS on success, Non-zero error code on failure
 */
CSEJTAG_DLL CSE_RETURN CseJtag_tap_shiftDeviceDR(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	unsigned int shiftMode,
	enum CSEJTAG_STATE exitState,
	CSE_PROGRESS_CB progressFunc,
	unsigned int bitCount,
	const unsigned char* tdibuf,
	unsigned char* tdobuf,
	const unsigned char* tdimask,
	const unsigned char* tdomask);


/***************************************************************************/
/*                      JTAG FILE OUTPUT OPERATIONS                        */
/***************************************************************************/

/**
True if output is sent to a file; false (0) otherwise

@param handle   (IN)  handle to the session instance
@param isFile   (OUT) 0=cable; 1=file

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_file_isFileOutput(CSE_HANDLE handle, unsigned int* isFile);


/**
Write comment to SVF or STAPL file.

@param handle   (IN)  handle to the session instance
@param text     (IN)  text comment

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_file_writeComment(CSE_HANDLE handle, const char* text);


/**
Write header to SVF or STAPL file.

@param handle   (IN)  handle to the session instance
@param text     (IN)  text header

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_file_writeHeader(CSE_HANDLE handle, const char* text);


/***************************************************************************/
/*                      JTAG DEVICE DATABASE                               */
/***************************************************************************/

/**
Add device records to the memory based lookup table. The lookup table is keyed
by IDCODE. Normally this lookup table is populated at session startup with the
default idcode.lst and idcode.acd file. This function allows additional entries
to be added to the table.

FILE FORMAT DETAILS:
- Same structure as the idcode.lst text file.

@param   handle    (IN) handle to the session instance
@param   filename  (IN) cstring with filename from the where the data was read
@param   buf       (IN) char array of text in idcode.lst format
@param   bufLen    (IN) size of the deviceData buffer

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_db_addDeviceData(
	CSE_HANDLE handle, const char* filename, const char* buf, unsigned int bufLen);


/**
Device lookup by IDCODE. Sets deviceName to the device name string. Sets irlen
to the irlength in bits. Sets cmd_bypass to the sequence needed to place
device in bypass (generally all 1's).

- allocate deviceName buffer of size CSEJTAG_MAX_DEVICENAME_LEN
- allocate cmd_bypass buffer of size CSEJTAG_BYPASSBUF_LEN

@param   handle        (IN)  handle to the session instance
@param   idcode        (IN) idcode string (c string of '1' or '0')
@param   deviceName    (OUT) cstring device name
@param   deviceNameLen (IN)  buffer size in bytes (should be CSEJTAG_MAX_DEVICENAME_LEN)
@param   irlen         (OUT) ir length of device
@param   cmd_bypass    (OUT) bypass instruction
@param   bypassbufLen  (IN)  cmd_bypass buffer size in bytes (should be CSE_BYPASSBUF_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_db_lookupDevice(
	CSE_HANDLE handle,
	const char* idcode,
	char* deviceName,
	unsigned int deviceNameLen,
	unsigned int* irlen,
	char* cmd_bypass,
	unsigned int bypassbufLen);


/**
Device name lookup by IDCODE.

Make sure to allocate a large enough deviceName buffer.
DeviceName[CSEJTAG_MAX_DEVICENAME_LEN] is adequate for all known
device names in the database.

@param handle         (IN)  handle to the session instance
@param idcode         (IN)  idcode string (c string of '1' or '0')
@param deviceName     (OUT) device name buffer
@param deviceNameLen  (IN)  buffer size in bytes (should be CSEJTAG_MAX_DEVICENAME_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_db_getDeviceNameForIdcode(
	CSE_HANDLE handle, const char* idcode, char* deviceName, unsigned int deviceNameLen);


/**
Gets IR length of device based on device file of specified ID code.

@param handle  (IN)  handle to the session instance
@param idcode  (IN)  idcode string (c string of '1' or '0')
@param irlen   (OUT) size in bits of the IR

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN
CseJtag_db_getIRLengthForIdcode(CSE_HANDLE handle, const char* idcode, unsigned int* irlen);

/**
Gets Bypass instruction of device based on device file of specified ID code.

Bypass instruction is the same number of bits as the irlength for the device.
cmd_bypass[] buffer should be preallocated as unsigned char
*buf[CSEJTAG_BYPASSBUF_LEN].

@param handle        (IN)  handle to the session instance
@param idcode        (IN)  idcode string (c string of '1' or '0')
@param cmd_bypass    (OUT) bypass instruction
@param bypassbufLen  (IN)  cmd_bypass buffer size in bytes (should be CSE_BYPASSBUF_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_db_getBypassForIdcode(
	CSE_HANDLE handle, const char* idcode, char* cmd_bypass, unsigned int bypassbufLen);


/**
Extract fields from a bsdl buffer.

cmd_bypass[] buffer should be preallocated as unsigned char
*buf[CSEJTAG_BYPASSBUF_LEN].

@param handle         (IN)  handle to the session instance
@param filename       (IN)  filename of the local bsdl file for debugging only
@param buf            (IN)  buffer representing complete BSDL file
@param bufLen         (IN)  size of buffer
@param deviceName     (OUT) name of the device (NULL if error reading)
@param deviceNameLen  (IN)  buffer size in bytes (should be CSEJTAG_MAX_DEVICENAME_LEN)
@param irlength       (OUT) ir length of device (-1 if error reading)
@param idcode         (OUT) idcode string (c string of '1' or '0')
@param idcodeLen      (IN)  idocde buffer size in bytes (should be CSE_IDCODEBUF_LEN)
@param cmd_bypass     (OUT) bypass command (default all 1s)
@param bypassbufLen   (IN)  cmd_bypass buffer size in bytes (should be CSE_BYPASSBUF_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_db_parseBSDL(
	CSE_HANDLE handle,
	const char* filename,
	const char* buf,
	unsigned int bufLen,
	char* deviceName,
	unsigned int deviceNameLen,
	unsigned int* irlength,
	char* idcode,
	unsigned int idcodeLen,
	char* cmd_bypass,
	unsigned int bypassbufLen);


/**
Extract fields from a bsdl file.

This function is a helper function on the client system only.
It generates a buffer from the filename and passes the buffer
to CseJtag_db_parseBSDL().

cmd_bypass[] buffer should be preallocated as unsigned char
*buf[CSEJTAG_BYPASSBUF_LEN].

@param handle          (IN)  handle to the session instance
@param filename        (IN)  filename of the local bsdl file
@param deviceName      (OUT) name of the device
@param deviceNameLen   (IN)  buffer size in bytes (should be CSEJTAG_MAX_DEVICENAME_LEN)
@param irlength        (OUT) ir length of device (-1 if error reading)
@param idcode          (OUT) idcode string (c string of '1' or '0')
@param idcodeLen       (IN)  idocde buffer size in bytes (should be CSE_IDCODEBUF_LEN)
@param cmd_bypass      (OUT) bypass command (default all 1s)
@param bypassbufLen    (IN)  cmd_bypass buffer size in bytes (should be CSE_BYPASSBUF_LEN)

@return CSE_SUCCESS on success, Non-zero error code on failure
*/
CSEJTAG_DLL CSE_RETURN CseJtag_db_parseBSDLFile(
	CSE_HANDLE handle,
	const char* filename,
	char* deviceName,
	unsigned int deviceNameLen,
	unsigned int* irlength,
	char* idcode,
	unsigned int idcodeLen,
	char* cmd_bypass,
	unsigned int bypassbufLen);


#ifdef __cplusplus
}
#endif

#endif /* ifndef _CseJtagAPI */
