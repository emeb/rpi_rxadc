// tb_icehat_rxadc.v - testbench for rxadc on icehat
// 07-02-20 E. Brombaugh

`timescale 1ns/1ps
`default_nettype none

module tb_icehat_rxadc;
    reg rxadc_clk;
    reg reset;
	reg [6:0] txaddr;
	reg [6:0] codec_reg;
	reg [8:0] codec_dat;
	reg start;
	reg err;
	real phs, frq;
	wire SCL, SDA, ACK;
	wire I2S_CLK, I2S_FS, I2S_DI;
	reg SPI_CE0, SPI_MOSI, SPI_SCLK;
	wire SPI_MISO;
	wire rxadc_pd;
	reg signed [13:0] rxadc_dat;
	wire lpdm, rpdm;
	wire [3:0] P401;
	wire RGB0, RGB1, RGB2;
	
    // 50MHz clock source
    always
        #10 rxadc_clk = ~rxadc_clk;
    
    // reset
    initial
    begin
`ifdef icarus
  		$dumpfile("tb_icehat_rxadc.vcd");
		$dumpvars;
`endif
        
        // init regs
        rxadc_clk = 1'b0;
        reset = 1'b1;
        start = 1'b0;
        txaddr = 7'h00;
		codec_reg = 7'h00;
		codec_dat = 9'h000;
		phs = 0;
		frq = 6.2832*1.441e6/50e6;
		SPI_CE0 = 1'b0;
		SPI_MOSI = 1'b0;
		SPI_SCLK = 1'b0;
		rxadc_dat = 14'd0;
		
        // release reset
        #1000
        reset = 1'b0;
		
		// send a packet
		#9900
        txaddr = 7'h1A;
		codec_reg = 7'h08;	// Sample Control
		codec_dat = 9'h023;	// 48kHz USB
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
		
		// send another after 400us
		#399900
        txaddr = 7'h1A;
		codec_reg = 7'h06;	// Powerdown
		codec_dat = 9'h000;	// all on
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
        
		
		// send another after 400us
		#399900
        txaddr = 7'h1A;
		codec_reg = 7'h07;	// Digital Format
		codec_dat = 9'h040;	// master
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
        
		// send another after 400us
		#399900
        txaddr = 7'h1A;
		codec_reg = 7'h09;	// Active Control
		codec_dat = 9'h001;	// active
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
		
		// send another after 400us
		#1399900
        txaddr = 7'h1A;
		codec_reg = 7'h02;	// Left HP Vol
		codec_dat = 9'h17F;	// 
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
		
		// send another after 400us
		#1399900
        txaddr = 7'h1A;
		codec_reg = 7'h08;	// Sample Control
		codec_dat = 9'h023;	// 44.1kHz USB
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
		
		// send another after 400us
		#399900
        txaddr = 7'h1A;
		codec_reg = 7'h08;	// Sample Control
		codec_dat = 9'h03f;	// 88.2kHz USB
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
        
		// send another after 400us
		#399900
        txaddr = 7'h1B;
		codec_reg = 7'h00;
		codec_dat = 9'h000;
		#100
		start = 1'b1;
		#1000
		start = 1'b0;
        
`ifdef icarus
        // stop after 4ms
		#400000 $finish;
`endif
    end
	
	// I2C bus stimulus generator
	i2c_stim sg(
		.addr(txaddr),
		.data({codec_reg,codec_dat}),
		.start(start),
		.ACK(ACK),
		.SCL(SCL),
		.SDA(SDA)
	);
	
	// pullups in I2C signals
    pullup(SCL);
	pullup(SDA);
	
	// ADC data
	always @(negedge rxadc_clk)
	begin
		rxadc_dat <= 1600*$sin(phs);
		phs <= phs + frq;
	end
	
    // Unit under test
	icehat_rxadc uut(
		// I2C port on PMOD P401 bodged to RPi connector
		.SDA(SDA), .SCL(SCL),
		
		// serial on TP401, TP402 bodged to RPi connector
		.RX(), .TX(),
		
		// I2S master port on RPi connector in/out names from RPi perspective
		.I2S_CLK(I2S_CLK), .I2S_FS(I2S_FS), .I2S_DI(I2S_DI), .I2S_DO(I2S_DI),
		
		// SPI slave port on RPi connector
		.SPI_CE0(SPI_CE0), .SPI_MOSI(SPI_MOSI),
		.SPI_MISO(SPI_MISO), .SPI_SCLK(SPI_SCLK),
		
		// SS pin is unused after configuration
		.GPIO25_SS(),
		
		// unused TP403
		.TP403(),
			
		// rxadc board interface on PMODs P403, P404
		.rxadc_clk(rxadc_clk),
		.rxadc_pd(rxadc_pd),
		.rxadc_dat(rxadc_dat),
		
		// PDM outputs on P401
		.lpdm(lpdm), .rpdm(rpdm),
		
		// remaining P401 stuff
		.P401(P401),
		
		// LED - via drivers
		.RGB0(RGB0), .RGB1(RGB1), .RGB2(RGB2)
	);
endmodule

// stimulus generator
// note - this is ugly behavioral code with timing embedded
// it's not expected to be synthesized
module i2c_stim(
	input [6:0] addr,
	input [15:0] data,
	input start,
	output ACK,
	inout SCL,
	inout SDA
);
	reg rSCL;
	reg rSDA;
	reg rACK;
	
	// task to send a byte
	task sendbyte(
		input [7:0] byte
	);
	begin : sendblk
		reg [7:0] sr;
		
		// load shift reg and drive clock / data 8x
		sr = byte;
		repeat(8)
		begin
			rSDA = sr[7];
			#2500
			rSCL = 1'b1;
			#5000
			sr[7:0] = {sr[6:0],1'b0};
			rSCL = 1'b0;
			#2500
			rSCL = 1'b0;
		end
		
		// release SDA for ACK/NACK
		rSDA = 1'b1;
		rACK = 1'b1;
		#2500
		rSCL = 1'b1;
		#5000
		rSCL = 1'b1;		
	end
	endtask
	
	initial
	begin
		rSCL = 1'b1;
		rSDA = 1'b1;
		rACK = 1'b0;
	end
	
	// wait for start
	always @(posedge start)
	begin
		// start condition - SDA drops while SCL high
		rSDA = 1'b0;
		#2500
		
		// loop over all bits of the address & write bit
		rSCL = 1'b0;
		#2500
		sendbyte({addr,1'b0});
		
		// send low byte
		rSCL = 1'b0;
		#2500
		rACK = 1'b0;
		sendbyte(data[15:8]);
		
		// send high byte
		rSCL = 1'b0;
		#2500
		rACK = 1'b0;
		sendbyte(data[7:0]);
		
		// send stop condition SDA rises after SCL rises
		rSCL = 1'b0;
		#2500
		rACK = 1'b0;
		rSDA = 1'b0;
		#2500
		rSCL = 1'b1;		
		#2500
		rSDA = 1'b1;
	end
	
	// output drivers
	assign SCL = rSCL ? 1'bz : 1'b0;
	assign SDA = rSDA ? 1'bz : 1'b0;
	assign ACK = rACK;
endmodule
