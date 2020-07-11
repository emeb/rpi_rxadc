// wm8731.v - wm8731 emulator
// 06-28-20 E. Brombaugh

`default_nettype none

module wm8731(
	// 12MHz clock
	input clk,
	input reset,
	
	// I2C control bus
	inout SCL, SDA,
	
	// I2S audio bus
	inout I2S_BCLK, I2S_FS, I2S_DOUT,
	input I2S_DIN,
	
	// parallel audio data
	input signed [15:0] l_in, r_in,
	output reg signed [15:0] l_out, r_out,
	output audio_stb,
	
	// control bits
	output [1:0] dr,
	
	// diagnostic
	output [15:0] i2c_data,
	output i2c_stb,
	output i2s_diag
);
	// I2C port
	wire [15:0] i2c_data;
	wire i2c_stb;
	wire tst;
    i2c_emul #(.I2C_ADR(7'h1B)) ui2c(
		.clk(clk),
        .reset(reset),
        .SCL(SCL),
        .SDA(SDA),
		.data(i2c_data),
		.stb(i2c_stb)
    );
	
	// writeable registers
	reg i2c_stb_d1, i2c_stb_d2;
	reg [6:0] lhpvol, rhpvol;
	reg pwroff, dacpd, adcpd, ms, usb, bosr, act;
	reg [3:0] sr;
	
	always @(posedge clk)
		if(reset)
		begin
			lhpvol <= 7'b1111001;
			rhpvol <= 7'b1111001;
			pwroff <= 1'b1;
			dacpd <= 1'b1;
			adcpd <= 1'b1;
			ms <= 1'b0;
			usb <= 1'b1;
			bosr <= 1'b0;
			sr <= 4'h6;		// default to 32kHz (24kHz actual) 
			act <= 1'b0;
		end
		else
		begin
			// reclock strobe for boundry crossing
			i2c_stb_d1 <= i2c_stb;
			i2c_stb_d2 <= i2c_stb_d1;
			
			// detect rising edge for writes
			if(i2c_stb_d1 && !i2c_stb_d2)
			begin
				case(i2c_data[15:9])
					7'h02:	// Left Headphone Volume
					begin
						lhpvol <= i2c_data[6:0];
						if(i2c_data[8])
							rhpvol <= i2c_data[6:0];
					end
					
					7'h03:	// Right Headphone Volume
					begin
						rhpvol <= i2c_data[6:0];
						if(i2c_data[8])
							lhpvol <= i2c_data[6:0];
					end
					
					7'h06:	// Power Down
					begin
						adcpd <= i2c_data[2];
						dacpd <= i2c_data[3];
						pwroff <= i2c_data[7];
					end
					
					7'h07: ms <= i2c_data[6];
					7'h08: {sr,bosr,usb} <= i2c_data[5:0];
					7'h09: act <= i2c_data[0];
				endcase
			end
		end
		
	// enable output
    reg outena;
    always @(posedge clk)
		outena <= !pwroff & !(dacpd & adcpd) & ms & act;
	
	// I2S port with 50MHz input clock
	wire valid;
	wire signed [15:0] l_rx, r_rx;
	i2s_inout_usb_hi ui2s(
		.clk(clk),						// System clock
		.reset(reset),					// System POR
		.bosr(bosr),					// Base Sample Rate
		.outena(outena),				// output enable
		.sr(sr),						// sample rate
		.l_in(l_in), .r_in(r_in),		// parallel inputs
		.l_out(l_rx), .r_out(r_rx),		// parallel outputs
		.sdout(I2S_DOUT),				// I2S serial data out
		.sdin(I2S_DIN),					// I2S serial data in
		.sclk(I2S_BCLK),				// I2S serial clock
		.lrclk(I2S_FS),					// I2S Left/Right clock
		.dr(dr),						// decimation rate bits
		.valid(valid),					// Sample rate enable output
		.i2s_diag(i2s_diag)				// i2s guts debugging
	);
	
	// output volume control
	reg [3:0] vol_pipe;
	reg [6:0] addr;
	reg [15:0] gain_lut[0:127];
	reg signed [31:0] scale;
	reg signed [15:0] gain, r_pre;
	
	// the gain LUT
	initial
		$readmemh("../src/gain_lut.memh", gain_lut, 0);
	
	// saturation
	wire [15:0] volsat;
    sat #(.isz(18),.osz(16))
        u_sat(.in(scale[31:14]),.out(volsat));
	
	// the pipeline
	always @(posedge clk)
	begin
		vol_pipe <= {vol_pipe[2:0],valid};
		gain <= gain_lut[addr];
		if(valid)
			addr <= lhpvol;
		else
			addr <= rhpvol;
			
		if(vol_pipe[1])
		begin
			scale <= l_rx * gain;
			r_pre <= volsat;
		end
		else
			scale <= r_rx * gain;
		
		if(vol_pipe[2])
		begin
			l_out <= volsat;
			r_out <= r_pre;
		end
	end
	
	assign audio_stb = vol_pipe[3];
endmodule

	
	
