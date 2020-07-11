// icehat_rxadc.v - top level for RXADC on icehat
// 07-02-20 E. Brombaugh

`default_nettype none

module icehat_rxadc #(
    parameter isz = 14,
              fsz = 26,
              dsz = 16
)
(
	// I2C port on PMOD P401 bodged to RPi connector
	inout	SDA,
			SCL,
	
    // serial on TP401, TP402 bodged to RPi connector
    inout RX, TX,
	
	// I2S master port on RPi connector in/out names from RPi perspective
	output I2S_CLK, I2S_FS, I2S_DI,
	input I2S_DO,
	
	// SPI slave port on RPi connector
	input SPI_CE0,
	input SPI_MOSI,
	output SPI_MISO,
	input SPI_SCLK,
	
	// SS pin is unused after configuration
	input GPIO25_SS,
	
	// unused TP403
	input TP403,
		
	// rxadc board interface on PMODs P403, P404
	input rxadc_clk,
	output rxadc_pd,
	input signed [isz-1:0] rxadc_dat,
	
	// PDM outputs on P401
	output lpdm, rpdm,
	
	// remaining P401 stuff
	output [3:0] P401,
	
	// LED - via drivers
	output RGB0, RGB1, RGB2
);

	//------------------------------
	// 50MHz RXADC clock is system clock
	//------------------------------
	wire clk;
	SB_GB clkbuf (
		.USER_SIGNAL_TO_GLOBAL_BUFFER(!rxadc_clk),
		.GLOBAL_BUFFER_OUTPUT(clk)
	);
	
	//------------------------------
	// reset generator waits > 10us
	//------------------------------
	reg [8:0] reset_cnt;
	reg reset;
	initial
        reset_cnt <= 9'h100;
    
	always @(posedge clk)
	begin
		if(reset_cnt != 9'h1ff)
        begin
            reset_cnt <= reset_cnt + 9'h001;
            reset <= 1'b1;
        end
        else
            reset <= 1'b0;
	end
    
	//------------------------------
	// Internal SPI slave port
	//------------------------------
	wire [31:0] wdat;
	reg [31:0] rdat;
	wire [6:0] addr;
	wire re, we, spi_slave_miso;
	spi_slave
		uspi(.clk(clk), .reset(reset),
			.spiclk(SPI_SCLK), .spimosi(SPI_MOSI),
			.spimiso(SPI_MISO), .spicsl(SPI_CE0),
			.we(we), .re(re), .wdat(wdat), .addr(addr), .rdat(rdat));
	
	//------------------------------
	// Writeable registers
	//------------------------------
	reg [31:0] spi_reg_01;
	reg [25:0] ddc_frq;
    reg dac_mux_sel;
    reg ddc_ns_ena;
    reg [2:0] ddc_cic_shf;
	always @(posedge clk)
		if(reset)
		begin
			spi_reg_01 <= 32'd87961155;	// 1kHz @ 48.828ksps
            ddc_frq <= 26'd1932735;		// 1.44MHz
			dac_mux_sel <= 1'b0;
			ddc_ns_ena <= 1'b0;
			ddc_cic_shf <= 3'b111;
		end
		else if(we)
			case(addr)
				7'h01: spi_reg_01 <= wdat;
				7'h10: ddc_frq <= wdat;
                7'h11: dac_mux_sel <= wdat;
                7'h12: ddc_ns_ena <= wdat;
                7'h13: ddc_cic_shf <= wdat;
			endcase
	
	//------------------------------
	// readback
	//------------------------------
	parameter DESIGN_ID = 32'hADC50001;
    wire [6:0] sathld;
	always @(*)
		case(addr)
			7'h00: rdat = DESIGN_ID;
			7'h01: rdat = spi_reg_01;
			7'h10: rdat = ddc_frq;
			7'h11: rdat = dac_mux_sel;
			7'h12: rdat = ddc_ns_ena;
			7'h13: rdat = ddc_cic_shf;
			7'h15: rdat = sathld;
			default: rdat = 32'd0;
		endcase

	//------------------------------
	// register ADC input data
	//------------------------------
	reg signed [isz-1:0] rxadc_dat_reg;
	always @(posedge clk)
		rxadc_dat_reg <= rxadc_dat;
	
        
//`define BYPASS_DDC
`ifdef BYPASS_DDC
	//------------------------------
	// DDC
	//------------------------------
    wire signed [dsz-1:0] ddc_i, ddc_q;
    wire ddc_v;
	wire [1:0] dr;
    ddc_14 #(
        .isz(isz),
        .fsz(fsz),
        .osz(dsz)
    )
    u_ddc(
        .clk(clk), .reset(reset),
        .in(rxadc_dat_reg),
		.dr(dr),
        .frq(ddc_frq),
        .ns_ena(ddc_ns_ena),
        .cic_shf(ddc_cic_shf),
        .sathld(sathld),
        .valid(ddc_v),
        .i_out(), .q_out()
    );

	//------------------------------
	// test oscillator
	//------------------------------
	wire signed [15:0] sine, cosine;
	sine_osc usine(
		.clk(clk),
		.reset(reset),
		.freq(spi_reg_01),
		.load(ddc_v),
		.sin(ddc_q),
		.cos(ddc_i)
	);
`else
	//------------------------------
	// DDC
	//------------------------------
    wire signed [dsz-1:0] ddc_i, ddc_q;
    wire ddc_v;
	wire [1:0] dr;
    ddc_14 #(
        .isz(isz),
        .fsz(fsz),
        .osz(dsz)
    )
    u_ddc(
        .clk(clk), .reset(reset),
        .in(rxadc_dat_reg),
		.dr(dr),
        .frq(ddc_frq),
        .ns_ena(ddc_ns_ena),
        .cic_shf(ddc_cic_shf),
        .sathld(sathld),
        .valid(ddc_v),
        .i_out(ddc_i), .q_out(ddc_q)
    );
`endif

	//------------------------------
	// Strap ADC Powedown to enable
	//------------------------------
	assign rxadc_pd = 1'b0;
    
	//------------------------------
	// WM8731 emulation
	//------------------------------
	wire signed [dsz-1:0] l_out, r_out;
	wire audio_stb;
	wire [15:0] i2c_data;
	wire i2c_stb;
	wire i2s_diag;
	wm8731 ucodec(
		.clk(clk),
		.reset(reset),
		
		// I2C control bus
		.SCL(SCL), .SDA(SDA),
		
		// I2S audio bus
		.I2S_BCLK(I2S_CLK),
		.I2S_FS(I2S_FS),
		.I2S_DOUT(I2S_DI),
		.I2S_DIN(I2S_DO),
		
		// parallel audio data
		.l_in(ddc_i), .r_in(ddc_q),
		.l_out(l_out), .r_out(r_out),
		.audio_stb(audio_stb),
		
		// control
		.dr(dr),
		
		// diagnostic
		.i2c_data(i2c_data),
		.i2c_stb(i2c_stb),
		.i2s_diag(i2s_diag)
	);
	
	//------------------------------
	// mux DAC source
	//------------------------------
	wire signed [15:0] ldac = dac_mux_sel ? l_out : ddc_i;
	wire signed [15:0] rdac = dac_mux_sel ? r_out : ddc_q;
	wire dac_load = dac_mux_sel ? audio_stb : ddc_v;
	
	//------------------------------
	// PDM DAC generation
	//------------------------------
	pdm_dac udac(
		.clk(clk),
		.reset(reset),
		.load(dac_load),
		.lin(ldac), .rin(rdac),
		.lpdm(lpdm), .rpdm(rpdm)
	);
	
	//------------------------------
	// test outputs
	// P401 assignments are:
	//            lpdm  P3 1 2 P2  SDA
	//            rpdm P48 3 4 P47 SCL
	//  nmute  P401[0] P46 5 6 P45 P401[1]
	//         P401[2] P44 7 8 P43 P401[3]
	//------------------------------
	assign P401 = {audio_stb,ddc_v,2'b01};

	//------------------------------
	// RGB LED Driver
	//------------------------------
	SB_RGBA_DRV #(
		.CURRENT_MODE("0b1"),
		.RGB0_CURRENT("0b000111"),
		.RGB1_CURRENT("0b000111"),
		.RGB2_CURRENT("0b000111")
	) RGBA_DRIVER (
		.CURREN(1'b1),
		.RGBLEDEN(1'b1),
		.RGB0PWM(dac_mux_sel),	// Actually RBG2 pin 39 -> D404 Blue
		.RGB1PWM(dr[1]),		//          RGB1 pin 40 -> D403 Green 
		.RGB2PWM(dr[0]),		// Actually RGB0 pin 41 -> D402 Red
		.RGB0(RGB0),
		.RGB1(RGB1),
		.RGB2(RGB2)
	);
endmodule
