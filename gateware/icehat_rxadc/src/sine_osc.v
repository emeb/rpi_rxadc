// sine_osc.v - 16-bit sinewave oscillator for testing
// 06-29-20 E. Brombaugh

`default_nettype none

module sine_osc(
		input clk,
		input reset,
		input [31:0]freq,
		input load,
		output reg signed [15:0] sin, cos
	);
	// phase accumulator
	reg [31:0] phs;
	always @(posedge clk)
		if(reset)
			phs <= 32'd0;
		else if(load)
			phs <= phs + freq;
	
	// sine LUT
	reg signed [15:0] sine_lut[255:0];
	initial
		$readmemh("../src/sine.hex", sine_lut, 0);
	
	wire [7:0] addr = phs[31:24];
		
	// sine shaping
	reg [7:0] sin_addr, cos_addr;
	always @(posedge clk)
	begin
		sin_addr <= addr;
		cos_addr <= addr + 8'h40;
		sin <= sine_lut[sin_addr];
		cos <= sine_lut[cos_addr];
	end
endmodule
