// pdm_dac.v - pulse-density modulation DAC
// 06-29-20 E. Brombaugh

module pdm_dac(
	// 12MHz clock
	input clk,
	input reset,
	input load,
	input signed [15:0] lin, rin,
	output lpdm, rpdm
);
	// grab the audio inputs and convert to offset binary
	reg [15:0] lob, rob;
	always @(posedge clk)
	begin
		lob <= lin ^ 16'h8000;
		rob <= rin ^ 16'h8000;
	end
	
	// sigma delta - no interpolation yet
	reg [16:0] lsdacc, rsdacc;
	always @(posedge clk)
	begin
		lsdacc <= {1'b0,lsdacc[15:0]} + {1'b0,lob};
		rsdacc <= {1'b0,rsdacc[15:0]} + {1'b0,rob};
	end
	
	// hookup outputs
	assign lpdm = lsdacc[16];
	assign rpdm = rsdacc[16];
endmodule
