`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date:    10:56:23 09/03/2009 
// Design Name: 
// Module Name:    IboardCPLD 
// Project Name: 
// Target Devices: 
// Tool versions: 
// Description: 
//
// Dependencies: 
//
// Revision: 
// Revision 0.01 - File Created
// Additional Comments: 
//
//////////////////////////////////////////////////////////////////////////////////
module IboardCPLD(
    input CLK,
    input TMS33,
    output TDO33,
    input TCK33,
    input SYSSTART33,
    input ARESET_L33,
	 output ARESET_L,
	 input TDI33,
    output TDI,
    input TDO,
    output TCK,
    output TMS_SYSSTART,
    output LED1,
    output LED2,
    output LED3,
    output FREE1,
    output FREE2
    );

	reg running;
	reg running1;
	reg [8:0] count;
	
	assign TDI = TDI33;
	assign TDO33 = TDO;
	assign TCK = TCK33;
//	assign ARESET_L = ARESET_L33;
//	assign LED1 = ARESET_L33;
	assign ARESET_L = count[8];
	assign LED1 = count[8];
	assign LED2 = SYSSTART33;
	assign LED3 = TMS33;
	assign FREE1 = TCK33;
	assign FREE2 = TMS_SYSSTART;
		
	
	BUFG buf_clk (.I(CLK),.O(clk));
	BUFG buf_rst (.I(ARESET_L33),.O(a_reset_l));
		
	always@(posedge clk or negedge a_reset_l)
	begin
		if (a_reset_l == 1'b0)
		begin
			running <= 1'b0;
			running1 <= 1'b0;
		end else begin
			running <= SYSSTART33;
			running1 <= running;
		end
	end

`ifdef SIM
	initial begin
		count = 9'b0;
	end
`endif

	always@(posedge clk or negedge a_reset_l)
	begin
		if (a_reset_l == 1'b0)
		begin
			count <= 'b0;
		end else if (count[8] == 1'b0)
		begin
			count <= count + 1;
		end
	end
	
	assign TMS_SYSSTART = (~running1 && SYSSTART33) || TMS33;
		
		

		
			
endmodule
