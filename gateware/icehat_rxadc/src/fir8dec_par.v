// fir8dec_par.v: 8x FIR decimator with separate I/Q I/O
// 07-04-20 E. Brombaugh

module fir8dec_par #(
	parameter isz = 16,					// input data size
	          osz = 16,					// output data size
	          psz = 8,					// pointer size
              csz = 16,                 // coeff data size
              clen = 246,               // coeff data length
              agrw = 3                  // accumulator growth
)
(
	input clk,								// System clock
	input reset,							// System POR
	input ena,								// New sample available on input
	input signed [isz-1:0] iin, qin,    	// Input data
	output reg valid,                   	// New output sample ready
	output reg signed [osz-1:0] iout,qout	// Output data - reverse order
);	
	
	//------------------------------
    // write address generator
	//------------------------------
    reg [psz-1:0] w_addr;
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            w_addr <= 0;
        end
        else
        begin
            if(ena == 1'b1)
            begin
                w_addr <= w_addr + 1;
            end
        end
    end
    
	//------------------------------
    // MAC control state machine
	//------------------------------
    `define sm_wait 2'b00
    `define sm_mac  2'b01
    `define sm_dmp  2'b10
    
    reg [1:0] state;
    reg coeff_end, mac_ena, dump;
    reg [psz-1:0] r_addr;
    reg [psz-1:0] c_addr;
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            state <= `sm_wait;
            mac_ena <= 1'b0;
            dump <= 1'b0;
            r_addr <= {psz+1{1'd0}};
            c_addr <= {psz{1'd0}};
            coeff_end <= 1'b0;
        end
        else
        begin
            case(state)
                `sm_wait :
                begin
                    // halt and hold
                    if(w_addr[2:0] == 3'b111)
                    begin
                        // start a MAC sequence every 8 samples
                        state <= `sm_mac;
                        mac_ena <= 1'b1;
                        r_addr <= w_addr;
                        c_addr <= {psz{1'd0}};
                        coeff_end <= 1'b0;
                    end
                end
                
                `sm_mac :
                begin
                    // Accumulate I
                    if(!coeff_end)
                    begin
                        // advance to next coeff
                        r_addr <= r_addr - 1;
                        c_addr <= c_addr + 1;
						coeff_end <= (c_addr == clen);
                    end
                    else
                    begin
                        // finish mac and advance to dump
                        state <= `sm_dmp;
                        mac_ena <= 1'b0;
                        dump <= 1'b1;
                    end
                end
                
                `sm_dmp :
                begin
                    // finish dump and return to wait
                    state <= `sm_wait;
                    dump <= 1'b0;
                end
                
                default :
                begin
                    state <= `sm_wait;
                    mac_ena <= 1'b0;
                    dump <= 1'b0;
                end
            endcase
        end
    end
    
	//------------------------------
    // input buffer memory
	//------------------------------
    reg [psz-1:0] r_addr_d;
	reg signed [2*isz-1:0] buf_mem [255:0];
	reg signed [isz-1:0] ird, qrd;
	always @(posedge clk) // Write memory.
	begin
        if(ena == 1'b1)
        begin
            buf_mem[w_addr] <= {iin,qin};
        end
	end
    
	always @(posedge clk) // Read memory.
	begin
        r_addr_d <= r_addr;
		{ird,qrd} <= buf_mem[r_addr_d];
	end
    
	//------------------------------
    // coeff ROM
	//------------------------------
    reg [psz-1:0] c_addr_d;
    reg signed [csz-1:0] coeff_rom[0:255];
    reg signed [csz-1:0] c_data /* synthesis syn_romstyle = "BRAM" */;
    initial
    begin
        $readmemh("../src/fir8dec_coeff.memh", coeff_rom);
    end
    always @(posedge clk)
    begin
        c_addr_d <= c_addr;
        c_data <= coeff_rom[c_addr_d];
    end
    
	//------------------------------
    // MACs
	//------------------------------
    reg [2:0] mac_ena_pipe;
    reg [2:0] dump_pipe;
    reg signed [csz+isz-1:0] imult, qmult;
    reg signed [csz+isz+agrw-1:0] iacc, qacc;
    wire signed [csz+isz+agrw-1:0] rnd_const = 1<<(csz+1);
    wire signed [osz-1:0] isat, qsat;
	
    // Saturate accum output
    sat #(.isz(agrw+osz-2), .osz(osz))
        u_isat(.in(iacc[csz+isz+agrw-1:csz+2]), .out(isat));
    sat #(.isz(agrw+osz-2), .osz(osz))
        u_qsat(.in(qacc[csz+isz+agrw-1:csz+2]), .out(qsat));
    
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            mac_ena_pipe <= 3'b000;
            dump_pipe <= 3'b000;
            imult <= {csz+isz{1'b0}};
            qmult <= {csz+isz{1'b0}};
            iacc <= rnd_const;
            qacc <= rnd_const;
            valid <= 1'b0;
			iout <= {osz{1'b0}};
			qout <= {osz{1'b0}};
        end
        else
        begin
            // shift pipes
            mac_ena_pipe <= {mac_ena_pipe[1:0],mac_ena};
            dump_pipe <= {dump_pipe[1:0],dump};

            // multipliers always run
            imult <= ird * c_data;
            qmult <= qrd * c_data;
            
            // accumulator
            if(mac_ena_pipe[1] == 1'b1)
            begin
                // two-term accumulate
                iacc <= iacc + {{agrw{imult[csz+isz-1]}},imult};
                qacc <= qacc + {{agrw{qmult[csz+isz-1]}},qmult};
            end
            else
            begin
                // clear to round constant
                iacc <= rnd_const;
                qacc <= rnd_const;
            end
            
            // output
            if(dump_pipe[1] == 1'b1)
            begin
                iout <= isat;
                qout <= qsat;
            end
            
            // valid
            valid <= dump_pipe[1];
        end
    end
endmodule
	
