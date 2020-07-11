// i2s_inout_usb_hi.v: usb-mode version of i2s with 50MHz clock
// this version runs sclk at the full 12MHz rate as in the WM8731 in USB mode
// 07-02-20 E. Brombaugh

`default_nettype none

module i2s_inout_usb_hi(
	input clk,								// System clock @ 50MHz
	input reset,							// System POR
	input bosr,								// base oversampling rate
	input outena,							// output enable
	input [3:0] sr,							// sample rate select
	input signed [15:0] l_in, r_in,			// parallel inputs
	output reg signed [15:0] l_out, r_out,	// parallel outputs
	inout sdout,							// I2S serial data out
	input sdin,								// I2S serial data in
	inout sclk,								// I2S serial clock
	inout lrclk,							// I2S Left/Right clock
	output reg [1:0] dr,					// decimation rate bits
	output reg valid,						// Sample rate enable output
	output i2s_diag							// diagnostic
);
	// 1/4 rate enable for all logic
	reg [1:0] clkdiv;
	reg clkena_r, clkena_f;
	always @(posedge clk)
		if(reset)
		begin
			clkdiv <= 2'b00;
			clkena_r <= 1'b0;
			clkena_f <= 1'b0;
		end
		else
		begin
			clkdiv <= clkdiv + 2'b01;
			if(clkdiv == 2'b00)
				clkena_r <= 1'b1;
			else
				clkena_r <= 1'b0;
			if(clkdiv == 2'b10)
				clkena_f <= 1'b1;
			else
				clkena_f <= 1'b0;
		end
		
	// look up sample rate divider
	// note - asymmetrical in/out rates unsupported
	reg [7:0] rate;
	always @(posedge clk)
		case({bosr, sr})
			5'b00000:
			begin
				// 48kHz divisor = 250
				rate <= 8'd124;
				dr <= 2'b00;
			end
			
			5'b11000:
			begin
				// 44.1kHz divisor = 272
				rate <= 8'd135;
				dr <= 2'b01;
			end
			
			5'b11111:
			begin
				// 88.2kHz divisor = 136
				rate <= 8'd67;
				dr <= 2'b10;
			end
			
			default:
			begin
				// all else is 48kHz divisor = 250
				rate <= 8'd124;
				dr <= 2'b00;
			end
		endcase
		
	// Sample rate generation
	reg [7:0] cnt;
	reg load;
	reg pre_lrclk;
	reg start_shift;
	always @(posedge clk)
		if(reset | ~outena)
		begin
			cnt <= rate;
			load <= 1'b0;
			pre_lrclk <= 1'b0;
		end
		else if(clkena_r)
		begin
			if(cnt)
			begin
				cnt <= cnt - 8'd1;
			end
			else
			begin
				cnt <= rate;
				pre_lrclk <= ~pre_lrclk;
			end
			
			load <= pre_lrclk & (cnt == 8'd1);
			start_shift <= (cnt == 8'd0);
		end
	
	// 16-bit shift enable
	reg [3:0] shfcnt;
	reg shfena;
	always @(posedge clk)
		if(clkena_r)
		begin
			if(start_shift)
			begin
				shfena <= 1'b1;
				shfcnt <= 4'hf;
			end
			else
			begin
				if(shfena)
				begin
					shfcnt <= shfcnt - 4'h1;
					shfena <= |shfcnt;
				end
			end
		end
		
	// Shift register advances on serial clock
	reg [32:0] sreg;
	always @(posedge clk)
		if(clkena_r)
		begin
			if(load)
			begin
				sreg <= {l_in,r_in};
				{l_out,r_out} <= sreg[31:0];
			end
			else if(shfena)
				sreg <= {sreg[30:0],sdin};
		end
		
	// 1/2 serial clock cycle delay on data & lrclk relative to sclk
	reg sdout_int, lrclk_int;
	always @(posedge clk)
		if(clkena_f)
		begin
			sdout_int <= sreg[31];
			lrclk_int <= pre_lrclk;
		end
	
	// qualify load signal w/ higher rate clock
	always @(posedge clk)
		valid <= load & clkena_r;
		
	// diagnostics
	assign i2s_diag = sreg[sdout_int];
		
	// sclk is 1/4 rate clock
	SB_IO #(
		.PIN_TYPE(6'b101001),
		.PULLUP(1'b1),
		.NEG_TRIGGER(1'b0),
		.IO_STANDARD("SB_LVCMOS")
	) usclk (
		.PACKAGE_PIN(sclk),
		.LATCH_INPUT_VALUE(1'b0),
		.CLOCK_ENABLE(1'b0),
		.INPUT_CLK(1'b0),
		.OUTPUT_CLK(1'b0),
		.OUTPUT_ENABLE(outena),
		.D_OUT_0(clkdiv[1]),
		.D_OUT_1(1'b0),
		.D_IN_0(),
		.D_IN_1()
	);

	// lrclk
	SB_IO #(
		.PIN_TYPE(6'b101001),
		.PULLUP(1'b1),
		.NEG_TRIGGER(1'b0),
		.IO_STANDARD("SB_LVCMOS")
	) ulrclk (
		.PACKAGE_PIN(lrclk),
		.LATCH_INPUT_VALUE(1'b0),
		.CLOCK_ENABLE(1'b0),
		.INPUT_CLK(1'b0),
		.OUTPUT_CLK(1'b0),
		.OUTPUT_ENABLE(outena),
		.D_OUT_0(lrclk_int),
		.D_OUT_1(1'b0),
		.D_IN_0(),
		.D_IN_1()
	);

	// sdout
	SB_IO #(
		.PIN_TYPE(6'b101001),
		.PULLUP(1'b1),
		.NEG_TRIGGER(1'b0),
		.IO_STANDARD("SB_LVCMOS")
	) usdout (
		.PACKAGE_PIN(sdout),
		.LATCH_INPUT_VALUE(1'b0),
		.CLOCK_ENABLE(1'b0),
		.INPUT_CLK(1'b0),
		.OUTPUT_CLK(1'b0),
		.OUTPUT_ENABLE(outena),
		.D_OUT_0(sdout_int),
		.D_OUT_1(1'b0),
		.D_IN_0(),
		.D_IN_1()
	);

endmodule
