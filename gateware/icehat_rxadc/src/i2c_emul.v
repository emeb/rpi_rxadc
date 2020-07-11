// i2c_emul.v - i2c emulator
// 06-25-20 E. Brombaugh
// based on https://www.fpga4fun.com/I2C_2.html

`default_nettype none

module i2c_emul #(parameter I2C_ADR= 7'h00) (
	input clk,
	input reset,
	input SCL,
	inout SDA,
	output reg [15:0] data,
	output reg stb
);
	// We use two wires with a combinatorial loop to detect the start and stop conditions
	//  ... making sure these two wires don't get optimized away
	wire SCL_buf;
	wire SDA_buf;
	(* keep *) wire SDA_shadow    /* synthesis keep = 1 */;
	(* keep *) wire start_or_stop /* synthesis keep = 1 */;
	assign SDA_shadow = reset ? 1'b0 : (~SCL_buf | start_or_stop) ? SDA_buf : SDA_shadow;
	assign start_or_stop = reset | ( ~SCL_buf ? 1'b0 : (SDA_buf ^ SDA_shadow));
	
	reg incycle;
	always @(negedge SCL_buf or posedge start_or_stop)
		if(start_or_stop)
			incycle <= 1'b0;
		else if(~SDA_buf)
			incycle <= 1'b1;
	
	reg [3:0] bitcnt;  // counts the I2C bits from 7 downto 0, plus an ACK bit
	wire bit_DATA = ~bitcnt[3];  // the DATA bits are the first 8 bits sent
	wire bit_ACK = bitcnt[3];  // the ACK bit is the 9th bit sent
	reg data_phase;

	always @(negedge SCL_buf or negedge incycle)
		if(~incycle)
		begin
			bitcnt <= 4'h7;  // the bit 7 is received first
			data_phase <= 0;
		end
		else
		begin
			if(bit_ACK)
			begin
				bitcnt <= 4'h7;
				data_phase <= 1;
			end
			else
				bitcnt <= bitcnt - 4'h1;
		end
	
	wire adr_phase = ~data_phase;
	reg adr_match, op_read, got_ACK;
	
	// sample SDA on posedge since the I2C spec specifies as low as 0µs hold-time on negedge
	reg SDAr; 
	always @(posedge SCL_buf)
		SDAr<=SDA_buf;
	
	reg [7:0] mem;
	wire op_write = ~op_read;

	always @(negedge SCL_buf or negedge incycle)
		if(~incycle)
		begin
			got_ACK <= 0;
			adr_match <= 1;
			op_read <= 0;
		end
		else
		begin
			if(adr_phase & bitcnt==7 & SDAr!=I2C_ADR[6]) adr_match<=0;
			if(adr_phase & bitcnt==6 & SDAr!=I2C_ADR[5]) adr_match<=0;
			if(adr_phase & bitcnt==5 & SDAr!=I2C_ADR[4]) adr_match<=0;
			if(adr_phase & bitcnt==4 & SDAr!=I2C_ADR[3]) adr_match<=0;
			if(adr_phase & bitcnt==3 & SDAr!=I2C_ADR[2]) adr_match<=0;
			if(adr_phase & bitcnt==2 & SDAr!=I2C_ADR[1]) adr_match<=0;
			if(adr_phase & bitcnt==1 & SDAr!=I2C_ADR[0]) adr_match<=0;
			if(adr_phase & bitcnt==0) op_read <= SDAr;
			
			// we monitor the ACK to be able to free the bus when the master doesn't ACK during a read operation
			if(bit_ACK)
				got_ACK <= ~SDAr;

			if(adr_match & bit_DATA & data_phase & op_write)
				mem[bitcnt] <= SDAr;  // memory write
		end
	
	// generate ACK
	wire mem_bit_low = ~mem[bitcnt[2:0]];
	wire SDA_assert_low = adr_match & bit_DATA & data_phase & op_read & mem_bit_low & got_ACK;
	wire SDA_assert_ACK = adr_match & bit_ACK & (adr_phase | op_write);
	wire SDA_low = SDA_assert_low | SDA_assert_ACK;

	// instantiate I/O drivers
	SB_IO #(
		.PIN_TYPE(6'b101000),
		.PULLUP(1'b1),
		.NEG_TRIGGER(1'b0),
		.IO_STANDARD("SB_LVCMOS")
	) uscl(
		.PACKAGE_PIN(SCL),
		.LATCH_INPUT_VALUE(1'b0),
		.CLOCK_ENABLE(1'b1),
		.INPUT_CLK(clk),
		.OUTPUT_CLK(1'b0),
		.OUTPUT_ENABLE(1'b0),
		.D_OUT_0(1'b0),
		.D_OUT_1(1'b0),
		.D_IN_0(SCL_buf),
		.D_IN_1()
	);

	SB_IO #(
		.PIN_TYPE(6'b101000),
		.PULLUP(1'b1),
		.NEG_TRIGGER(1'b0),
		.IO_STANDARD("SB_LVCMOS")
	) usda (
		.PACKAGE_PIN(SDA),
		.LATCH_INPUT_VALUE(1'b0),
		.CLOCK_ENABLE(1'b1),
		.INPUT_CLK(clk),
		.OUTPUT_CLK(1'b0),
		.OUTPUT_ENABLE(SDA_low),
		.D_OUT_0(1'b0),
		.D_OUT_1(1'b0),
		.D_IN_0(SDA_buf),
		.D_IN_1()
	);

	// assign 16-bit data
	reg bytecnt;
	always @(negedge SCL_buf or negedge incycle)
		if(~incycle)
		begin
			bytecnt <= 1'b0;
			stb <= 1'b0;
		end
		else
		begin
			if(adr_match & data_phase & bit_ACK)
			begin
				if(bytecnt==1'b0)
				begin
					data[15:8] <= mem;
					bytecnt <=1'b1;
				end
				else
				begin
					data[7:0] <= mem;
					stb <= 1'b1;
				end
			end
		end
endmodule

