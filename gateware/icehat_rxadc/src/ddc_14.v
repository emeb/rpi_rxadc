// ddc_14.v - decimating downconverter
// 03-05-17 E. Brombaugh
//
// 14-bit IF input
// 16-bit Quadrature baseband output
// Decimated by 2048 from 50MSPS -> 24.414kHz

`default_nettype none

module ddc_14 #(
    parameter isz = 14, // input size
              fsz = 26, // frequency word size
              osz = 16, // output size
              gsz = 8   // bit-growth per stage (log2 of dec rate)
)
(
    input clk, reset,
    input signed [isz-1:0] in,
	input [1:0] dr,
	input [fsz-1:0] frq,
    input [2:0] cic_shf,
    input ns_ena,
    output reg [6:0] sathld,
    output valid,
    output signed [osz-1:0] i_out, q_out
);
	//------------------------------
    // look up sample rate
	//------------------------------
	reg [gsz-1:0] drate;
	always @(posedge clk)
		case(dr)
			2'b00: drate = 8'd124;		// 48k
			2'b01: drate = 8'd135;		// 44.1k
			2'b10: drate = 8'd67;		// 88.2k
			default: drate = 8'd249;	// 48k
		endcase

	//------------------------------
    // clock divider - CICs run at 1/256
	//------------------------------
    reg [gsz-1:0] dcnt;
    reg ena_cic;
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            dcnt <= 0;
            ena_cic <= 1'b0;
        end
        else
        begin
			if(dcnt == 0)
			begin
				dcnt <= drate;
				ena_cic <= 1'b1;
			end
			else
			begin
				dcnt <= dcnt - 1;
				ena_cic <= 1'b0;
			end
       end
    end
    
	//------------------------------
    // tuner instance
	//------------------------------
    wire signed [isz-1:0] tuner_i, tuner_q;
    tuner_2 #(
        .dsz(isz),
        .fsz(fsz)
    )
    u_tuner(
        .clk(clk),
        .reset(reset),
        .in(in),
        .frq(frq),
        .ns_ena(ns_ena),
        .i_out(tuner_i),
        .q_out(tuner_q)
    );
    
    //------------------------------
    // CICs with internal trunc @ Comb input
    //------------------------------
    parameter cicsz = 21;   // 14 + (11/2) = 19.5
    
    //------------------------------
    // I cic decimator instance
	//------------------------------
    wire cic_v;
    wire signed [cicsz-1:0] cic_i;
    cic_dec_4 #(
        .NUM_STAGES(4),		// Stages of int / comb
	    .STG_GSZ(gsz),		// Bit growth per stage
	    .ISZ(isz),          // input word size
        .OSZ(cicsz)         // output word size
    )
    u_cic_i(
        .clk(clk),				// System clock
        .reset(reset),			// System POR
        .ena_out(ena_cic),		// Decimated output rate (2 clks wide)
        .x(tuner_i),	        // Input data
        .y(cic_i),	            // Output data
        .valid(cic_v)			// Output Valid
    );	

	//------------------------------
    // Q cic decimator instance
	//------------------------------
    wire signed [cicsz-1:0] cic_q;
    cic_dec_4 #(
        .NUM_STAGES(4),		// Stages of int / comb
	    .STG_GSZ(gsz),		// Bit growth per stage
	    .ISZ(isz),          // input word size
        .OSZ(cicsz)         // output word size
    )
    u_cic_q(
        .clk(clk),				// System clock
        .reset(reset),			// System POR
        .ena_out(ena_cic),		// Decimated output rate (2 clks wide)
        .x(tuner_q),	        // Input data
        .y(cic_q),	            // Output data
        .valid()			    // Output Valid (unused)
    );	

	//------------------------------
    // trim cic output to 16 bits with left shift for weak signals
	//------------------------------
    reg signed [cicsz+6:0] cic_i_shf, cic_q_shf;
    reg signed [osz-1:0] cic_i_trim, cic_q_trim;
    wire signed [osz-1:0] cic_i_sat, cic_q_sat;
    wire isatflg, qsatflg;
    reg satval;
    reg [6:0] satcnt, satsum;
    // Saturate shifted cic output
    sat_flag #(.isz(osz+7), .osz(osz))
        u_sat_i(.in(cic_i_shf[cicsz+6:cicsz-osz]), .out(cic_i_sat), .flag(isatflg));
    sat_flag #(.isz(osz+7), .osz(osz))
        u_sat_q(.in(cic_q_shf[cicsz+6:cicsz-osz]), .out(cic_q_sat), .flag(qsatflg));
    always @(posedge clk)
    begin
        // pipe regs on CIC shift
        cic_i_shf <= cic_i<<<cic_shf;
        cic_q_shf <= cic_q<<<cic_shf;
        cic_i_trim <= cic_i_sat;
        cic_q_trim <= cic_q_sat;
        
        // saturation stats
        if(reset == 1'b1)
        begin
            satcnt <= 0;
			satval <= 0;
            satsum <= 0;
            sathld <= 0;
        end
        else
        begin
 			satcnt <= satcnt + 1;
			satval <= isatflg|qsatflg;
            satsum <= satsum + {6'd0,satval};
            
            if(satcnt == 7'h7f)
            begin
                sathld <= satsum;
                satsum <= 0;
            end
        end
    end

// uncomment to use original muxed FIR
//`define MUXED_FIR
`ifdef MUXED_FIR
	// original muxed I/Q FIR
    //------------------------------
    // Mux CIC outputs into single stream
 	//------------------------------
    reg cic_v_d, cic_iq_v;
    reg signed [osz-1:0] cic_iq_mux;
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            cic_v_d <= 1'b0;
            cic_iq_v <= 1'b0;
            cic_iq_mux <= {osz{1'b0}};
        end
        else
        begin
            cic_v_d <= cic_v;
            if((cic_v_d == 1'b0) & (cic_v == 1'b1))
            begin
                cic_iq_v <= 1'b1;
                cic_iq_mux <= cic_i_trim;
            end
            else if((cic_v_d == 1'b1) & (cic_v == 1'b0))
            begin
                cic_iq_v <= 1'b1;
                cic_iq_mux <= cic_q_trim;
            end
            else
            begin
                cic_iq_v <= 1'b0;
            end
        end
    end
    
 	//------------------------------
    // 8x FIR decimator instance
	//------------------------------
    wire fir_v;
    wire signed [osz-1:0] fir_qi;
    fir8dec #(
        .isz(osz),          // input data size
	    .osz(osz)           // output data size
    )
    u_fir(
        .clk(clk),              // System clock
        .reset(reset),          // System POR
        .ena(cic_iq_v),         // New sample available on input
        .iq_in(cic_iq_mux),     // Input data
        .valid(fir_v),          // New output sample ready
        .qi_out(fir_qi)         // Decimated Output data
    );	
	
	//------------------------------
    // demux outputs (fir output order is reversed)
	//------------------------------
    reg p_valid;
	reg signed [osz-1:0] q_hld;
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            p_valid <= 1'b0;
            valid <= 1'b0;
            q_hld <= {osz{1'b0}};
            i_out <= {osz{1'b0}};
            q_out <= {osz{1'b0}};
        end
        else
        begin
            p_valid <= fir_v;
			
			// q comes out first so delay it 1 cycle
            if((p_valid == 1'b0) & (fir_v == 1'b1))
                q_hld <= fir_qi;
			
			// i is second so pulse valid
            if((p_valid == 1'b1) & (fir_v == 1'b1))
            begin
				i_out <= fir_qi;
				q_out <= q_hld;
                valid <= 1'b1;
			end
            else
                valid <= 1'b0;
        end
    end
`else
	// new FIR with separate I & Q
	fir8dec_par #(
        .isz(osz),          // input data size
	    .osz(osz)           // output data size
    )
	u_fir(
		.clk(clk),								// System clock
		.reset(reset),							// System POR
		.ena(cic_v),							// New sample available on input
		.iin(cic_i_trim), .qin(cic_q_trim),    	// Input data
		.valid(valid),                   		// New output sample ready
		.iout(i_out), .qout(q_out)				// Output data
	);
`endif
endmodule

