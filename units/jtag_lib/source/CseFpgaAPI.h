/*! \file CseFpgaAPI.h
	\brief
	 ANSI C API to ChipScope Runtime Engine FPGA functionality.
*/

/*
 * Copyright (c) 2010 Xilinx, Inc.  All rights reserved.
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


#ifndef CseFpgaAPI_h
#define CseFpgaAPI_h

#include "CseJtagAPI.h"

#ifndef CSE_DLL
#define CSE_DLL DLLEXPORT
#endif


#ifdef __cplusplus
extern "C" {
#endif //#ifdef __cplusplus

/**
Recommended length for output name text buffers.
 */
#define CSEFPGA_NAME_BUFFER_LEN 8192

/**
Recommended length for eFuse register buffers.
 */
#define CSEFPGA_REGISTER_BUFFER_LEN 64

/**
Status bits showing status after configuration.
 */
enum CSE_CONFIG_STATUS
{
	/**
	Specifies bit position for internal done status.
	Shows value of DONE bit in JTAG instruction register.
	Bit is set when device configuration is successful and
	option "verify_internal_done=false" was not used.
	Otherwise the bit is reset.
	 */
	CSE_INTERNAL_DONE_HIGH_STATUS = 0x0001,

	/**
	Specifies bit position for external done status.
	Bit is set when device package DONE pin is high, after configuration.
	Shows value of DONE bit in device configuration status word.
	If bitstream is not read enabled or "verify_external_done=true"
	is not set, the bit will be reset.
	 */
	CSE_EXTERNAL_DONE_HIGH_STATUS = 0x0002,

	/**
	Bit is set when device configuration CRC fails.
	Shows value of CRC_ERROR bit in device configuration status word.
	If bitstream is not read enabled or "verify_crc=true" is not set,
	is the bit will be reset.
	 */
	CSE_CRC_ERROR_STATUS = 0x0004,

	/**
	Bit is set when the bitstream is read enabled.
	 */
	CSE_BITSTREAM_READ_ENABLED = 0x0008,

	/**
	Bit is set when the bitstream is write enabled.
	 */
	CSE_BITSTREAM_WRITE_ENABLED = 0x0010,

	/**
	Specifies bit position for the configuration readback
	verification.  If the data matches then the bit is
	set high.  Bit is set when the option "verify_config_data=true"
	was used.  Otherwise the bit is reset.
	 */
	CSE_VERIFY_CONFIG_DATA_STATUS = 0x0020

};


/**
Retrieve device configuration register bits.

@param handle        (IN) Session handle.
@param deviceIndex   (IN) Index in the JTAG chain.
@param reg           (OUT) Buffer where register word will be put,
						   as bytes of data.
						   (bitCount/8 + ((bitCount%8)?1:0)) bytes
						   will be written.
@param bitCount      (IN)  Length in bits of register.
@param bitNameBuf    (OUT) Buffer to put one c-string with config reg bit
						   names.
						   The list is delimited by commas.
						   This in-parameter may be 0.
						   A most bitNameBufLen bytes will be written.

@param bitNameBufLen (IN) Length in bytes of buffer. May be zero.
						  Recommended length is CSEFPGA_NAME_BUFFER_LEN

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_getConfigReg(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	unsigned char* reg,
	unsigned int bitCount,
	char* bitNameBuf,
	unsigned int bitNameBufLen);


/**
Test if this sw library supports configuration of a device.

@param handle       (IN) Session handle.
@param idcode       (IN) JTAG idcode string (c string of '1' or '0').
@param result      (OUT) Zero for false, non-zero value for true.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN
CseFpga_isConfigSupported(CSE_HANDLE handle, const char* idcode, unsigned int* result);


/**
Test if a FPGA device is configured.
"Internal Done Bit" in JTAG Instruction Register (IR), is
used to determined if the FPGA is configured.

@param handle       (IN) Session handle.
@param deviceIndex  (IN) Index in the JTAG chain.
@param result      (OUT) Zero for false, non-zero value for true.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN
CseFpga_isConfigured(CSE_HANDLE handle, unsigned int deviceIndex, unsigned int* result);


/**
Configure a FPGA device with the contents of a bit, rbt or mcs file.

An nky file can be used to program the battery-backed RAM key.

argc/argv replaced the options argument.  argv is an array of c-strings.
Each c-string is a key=value pair.
Available options are:
\code
------------------------------------------------------------------------------------------------------------------------
| key                      | valid values    | Description |
------------------------------------------------------------------------------------------------------------------------
| reset_device             | true (default)  | CSE_RESET_DEVICE option is executed differently
depending on            | |                          | false           | device family.
| |                          |                 | For Virtex type devices, the regular shutdown
command sequence is used. | |                          |                 | For VirtexII type
devices, the JTAG JPROG_B command is used to reset    | |                          |
| the device.                                                             | |
|                 | If "shutdown_sequence=true" is also specified for a VirtexII device,    | |
|                 | the JPROG_B will be done before the config command shutdown sequence.   |
------------------------------------------------------------------------------------------------------------------------
| shutdown_sequence        | true            | Use regular Configuration commands to shut down
device.                 | |                          | false (default) | For Spartan-3 devices,
| |                          |                 | this option will have the same effect as
"reset_device=true".           |
------------------------------------------------------------------------------------------------------------------------
| verify_internal_done     | true (default)  | Read device internal done status after configuration,
| |                          | false           | by reading device JTAG instruction register.
|
------------------------------------------------------------------------------------------------------------------------
| verify_external_done     | true            | Read device external done status after configuration,
| |                          | false (default) | by reading device configuration status register.
| |                          |                 | If user ties package done package pins together,
| |                          |                 | the status depends on all devices which have done
package pins          | |                          |                 | tied together.
|
------------------------------------------------------------------------------------------------------------------------
| verify_crc               | true            | Read CRC status after configuration, by | |
| false (default) | reading device configuration status register.                           |
------------------------------------------------------------------------------------------------------------------------
| verify_config_data       | true            | Verify that the readback config data is the same as
the provided        | |                          | false (default) | config data.
|
------------------------------------------------------------------------------------------------------------------------
| use_assigned_config_data | true            | Use the config/mask data provided by
CseFpga_assignConfigDataToDevice() | |                          | false (default) | or
CseFpga_assignConfigDataFileToDevice()                               |
------------------------------------------------------------------------------------------------------------------------
\endcode

@param handle         (IN)  Session handle.
@param deviceIndex    (IN)  Device index in the JTAG chain.
@param filename       (IN)  '\0' terminated char string.
							File containing configuration data.
							Valid formats are "bit", "rbt", "mcs" and "nky".
							File extension needs to be the same as the format.
@param argc           (IN)  Number of arguments
@param argv           (IN)  Argument array to pass optional arguments
@param configStatus   (OUT) Bitfield where status of the configuration
							will be put.
@param progressFunc   (IN)  Function to show progress while shifting in
							configuration data into the device.
							Set to 0, if no function is provided.

@return CSE_SUCCESS on success, Non-zero error code on failure.
		Return value does not show if device configuration succeeded.
		Look at "*configStatus" for such information.
 */
CSE_DLL CSE_RETURN CseFpga_configureDeviceWithFile(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	const char* filename,
	unsigned int argc,
	const char* argv[],
	enum CSE_CONFIG_STATUS* configStatus,
	CSE_PROGRESS_CB progressFunc);


/**
Configure a FPGA device with the contents of a bit, rbt or mcs file.

An nky file can be used to program the battery-backed RAM key.

argc/argv replaced the options argument.  argv is an array of c-strings.
Each c-string is a key=value pair.
Available options are:
\code
------------------------------------------------------------------------------------------------------------------------
| key                      | valid values    | Description |
------------------------------------------------------------------------------------------------------------------------
| reset_device             | true (default)  | CSE_RESET_DEVICE option is executed differently
depending on            | |                          | false           | device family.
| |                          |                 | For Virtex type devices, the regular shutdown
command sequence is used. | |                          |                 | For VirtexII type
devices, the JTAG JPROG_B command is used to reset    | |                          |
| the device.                                                             | |
|                 | If "shutdown_sequence=true" is also specified for a VirtexII device,    | |
|                 | the JPROG_B will be done before the config command shutdown sequence.   |
------------------------------------------------------------------------------------------------------------------------
| shutdown_sequence        | true            | Use regular Configuration commands to shut down
device.                 | |                          | false (default) | For Spartan-3 devices,
| |                          |                 | this option will have the same effect as
"reset_device=true".           |
------------------------------------------------------------------------------------------------------------------------
| verify_internal_done     | true (default)  | Read device internal done status after configuration,
| |                          | false           | by reading device JTAG instruction register.
|
------------------------------------------------------------------------------------------------------------------------
| verify_external_done     | true            | Read device external done status after configuration,
| |                          | false (default) | by reading device configuration status register.
| |                          |                 | If user ties package done package pins together,
| |                          |                 | the status depends on all devices which have done
package pins          | |                          |                 | tied together.
|
------------------------------------------------------------------------------------------------------------------------
| verify_crc               | true            | Read CRC status after configuration, by | |
| false (default) | reading device configuration status register.                           |
------------------------------------------------------------------------------------------------------------------------
| verify_config_data       | true            | Verify that the readback config data is the same as
the provided        | |                          | false (default) | config data.
|
------------------------------------------------------------------------------------------------------------------------
| use_assigned_config_data | true            | Use the config/mask data provided by
CseFpga_assignConfigDataToDevice() | |                          | false (default) | or
CseFpga_assignConfigDataFileToDevice()                               |
------------------------------------------------------------------------------------------------------------------------
\endcode

@param handle           (IN)  Session handle.
@param deviceIndex      (IN)  Device index in the JTAG chain.
@param format           (IN)  '\0' terminated char string.
							  Valid formats are "bit", "rbt", "mcs" and "nky".
@param argc             (IN)  Number of arguments
@param argv             (IN)  Argument array to pass optional arguments
@param fileData         (IN)  Contents of configuration file in an array of bytes.
							  "bit" file must be read in "binary mode".
							  Other formats may be read in "binary mode" or
							  "text mode".
							  Do not remove Windows/unix end-of-line characters
							  from fileData.
							  Note! Caller owns this data.
@param fileDataByteLen  (IN)  Length of fileData byte array.
@param configStatus     (OUT) Bitfield where status of the configuration
							  will be put.
@param progressFunc     (IN)  Function to show progress while shifting in
							  configuration data into the device.
							  Set to 0, if no function is provided.

@return CSE_SUCCESS on success, Non-zero error code on failure.
		Return value does not show if device configuration succeeded.
		Look at "*configStatus" for such information.
 */
CSE_DLL CSE_RETURN CseFpga_configureDevice(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	const char* format,
	unsigned int argc,
	const char* argv[],
	unsigned char* fileData,
	unsigned int fileDataByteLen,
	enum CSE_CONFIG_STATUS* configStatus,
	CSE_PROGRESS_CB progressFunc);


/**
Assign config data or config mask data to a specific device.  CseFpga_configureDevice()
or CseFpga_configureDeviceWithFile() must use the "use_assigned_config_data=true"
option to use this config data.

@param handle           (IN)  Session handle.
@param deviceIndex      (IN)  Device index in the JTAG chain.
@param format           (IN)  '\0' terminated char string.
							  Valid choices are "bit", "rbt", "mcs", and "msk".
@param fileData         (IN)  Contents of configuration file in an array of bytes.
							  "bit" file must be read in "binary mode".
							  Other formats may be read in "binary mode" or
							  "text mode".
							  Do not remove Windows/unix end-of-line characters
							  from fileData.
							  Note! Caller owns this data.
@param fileDataByteLen  (IN)  Length of fileData byte array.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_assignConfigDataToDevice(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	const char* format,
	unsigned char* fileData,
	unsigned int fileDataByteLen);


/**
Assign config data or config mask data to a specific device.  CseFpga_configureDevice()
or CseFpga_configureDeviceWithFile() must use the "use_assigned_config_data=true"
option to use this config data.

@param handle           (IN)  Session handle.
@param deviceIndex      (IN)  Device index in the JTAG chain.
@param filename         (IN)  '\0' terminated char string.
							  File containing configuration data.
							  Valid choices are "bit", "rbt", "mcs", and "msk".
							  File extension needs to be the same as the format.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_assignConfigDataFileToDevice(
	CSE_HANDLE handle, unsigned int deviceIndex, const char* filename);


/**
Remove the assign config data or config mask data from a specific device.

@param handle           (IN)  Session handle.
@param deviceIndex      (IN)  Device index in the JTAG chain.
@param format           (IN)  '\0' terminated char string.
							  Valid choices are "bit", "rbt", "mcs", and "msk".
							  If format is NULL or length is 0, all assigned
							  data will be removed.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN
CseFpga_removeConfigDataFromDevice(CSE_HANDLE handle, unsigned int deviceIndex, const char* format);


/**
Read the specified device register.

@param handle           (IN)  Session handle.
@param deviceIndex      (IN)  Device index in the JTAG chain.
@param regName          (IN)  Name of the register to read
@param argc             (IN)  Number of arguments
@param argv             (IN)  Argument array to pass optional arguments
@param regValue         (OUT) Binary array containing register value
@param regValueLen      (IN)  Provide the current array size
							  Recommended length is CSEFPGA_REGISTER_BUFFER_LEN
@param bitCount         (OUT) Length in bits of register.
@param bitNameBuf       (OUT) Buffer to put one c-string with control reg bit
							  names.
							  The list is delimited by commas.
							  This in-parameter may be 0.
							  A most bitNameBufLen bytes will be written.
@param bitNameBufLen    (IN) Length in bytes of buffer. May be zero.
							 Recommended length is CSEFPGA_NAME_BUFFER_LEN

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_getDeviceReg(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	const char* regName,
	unsigned int argc,
	const char* argv[],
	unsigned char* regValue,
	unsigned int regValueLen,
	unsigned int* bitCount,
	char* bitNameBuf,
	unsigned int bitNameBufLen);


/**
Retrieve device instruction register bits.

@param handle        (IN)  Session handle.
@param deviceIndex   (IN)  Index in the JTAG chain.
@param reg           (OUT) Buffer where register word will be put,
						   as bytes of data.
						   (bitCount/8 + ((bitCount%8)?1:0)) bytes
						   will be written.
@param bitCount      (IN)  Length in bits of register.
@param bitNameBuf    (OUT) Buffer to put one c-string with config reg
						   bit names.
						   The list is delimited by commas.
						   This in-parameter may be 0.
						   One c-string with config reg bit names.
						   At most bitNameBufLen bytes will be
						   written.
@param bitNameBufLen (IN)  Length in bytes of buffer. May be zero.
						   Recommended length is CSEFPGA_NAME_BUFFER_LEN

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_getInstructionReg(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	unsigned char* reg,
	unsigned int bitCount,
	char* bitNameBuf,
	unsigned int bitNameBufLen);


/**
Read fpga design USERCODE.

@param handle      (IN) Session handle.
@param deviceIndex (IN) Index in the JTAG chain.
@param usercode    (OUT) Fpga design usercode.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN
CseFpga_getUsercode(CSE_HANDLE handle, unsigned int deviceIndex, unsigned int* usercode);


/**
Gets user chain count for device.

@param handle          (IN) Session handle.
@param idcode          (IN) JTAG idcode string (c string of '1' or '0')
@param userChainCount (OUT) User chain count.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN
CseFpga_getUserChainCount(CSE_HANDLE handle, const char* idcode, unsigned int* userChainCount);


/**
Test if System Monitor is supported for device type.

@param handle              (IN)  Session handle.
@param idcode              (IN)  JTAG idcode string (c string of '1' or '0')
@param result              (OUT) Zero for false, non-zero value for true.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN
CseFpga_isSysMonSupported(CSE_HANDLE handle, const char* idcode, unsigned int* result);


/**
A sequence of system monitor register read(s)/write(s) to Virtex5
systemMonitor registers.

@param handle       (IN) Session handle.
@param deviceIndex  (IN) Device index within the JTAG chain.
@param address      (IN) Array of register addresses.
@param data     (IN/OUT) Array of register data.
@param setMode      (IN) Array of set flags.
						 Each flag occupies an unsigned int.
						 Zero indicates get.
						 Non-zero indicates set.
@param commandCount (IN) Number of commands. Gives the size of the arrays.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_runSysMonCommandSequence(
	CSE_HANDLE handle,
	unsigned int deviceIndex,
	unsigned int* address,
	unsigned int* data,
	unsigned int* setMode,
	unsigned int commandCount);
/**
Read a Virtex5 systemMonitor register.

@param handle      (IN) Session handle.
@param deviceIndex (IN) Device index within the JTAG chain.
@param address     (IN) Register address.
@param data       (OUT) Register data.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_getSysMonReg(
	CSE_HANDLE handle, unsigned int deviceIndex, unsigned int address, unsigned int* data);

/**
Write a Virtex5 systemMonitor register.

@param handle      (IN) Session handle.
@param deviceIndex (IN) Device index within the JTAG chain.
@param address     (IN) Register address.
@param data        (IN) Register data.

@return CSE_SUCCESS on success, Non-zero error code on failure.
 */
CSE_DLL CSE_RETURN CseFpga_setSysMonReg(
	CSE_HANDLE handle, unsigned int deviceIndex, unsigned int address, unsigned int data);


#ifdef __cplusplus
}
#endif //#ifdef __cplusplus

#endif /* ifndef CseFpgaAPI_h */
