///////////////////////////////////////////////////////////////////////////////
// $LastChangedDate: 2008-10-16 13:04:56 +0200 (Thu, 16 Oct 2008) $
// $LastChangedRevision: 1 $
// $LastChangedBy: henker $
////////////////////////////////////////////////////////////////
// Company		:	TU-Dresden
// Author		:	<Designer Name>
// E-Mail		:	<email>
//
// Date			:    	<create date>
// Last Change		:    	<last change date> 
// Module Name		:    	jtag_emulator
// Filename		: 	jtag_emulator.vhd
// Project Name		:   	<project name>
// Description		:    	Verilog Bench for SystemC Module jtag_emulator
//	
////////////////////////////////////////////////////////////////

module jtag_emulator (
	start_jtag_h,
	tdo,
	tck,
	tdi,
	tms	
)

//
// Note that the foreign attribute string value must be "SystemC".
//
  (* integer foreign      = "SystemC"; 
  *);
//
// Note that port names must match exactly port names as they appear in
//  sc_module class in SystemC; they must also match in order, mode and type.
//

	input 	start_jtag_h;
	input 	tdo;
	output	tck;
	output	tms;
	output	tdi;
	

endmodule

