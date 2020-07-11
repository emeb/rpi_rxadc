// tuner_2.v - real -> complex tuner V2
// 07-17-16 E. Brombaugh

module tuner_2 #(
    parameter dsz = 14,
              fsz = 26,
              psz = 12
)
(
    input clk, reset,
    input signed [dsz-1:0] in,
    input [fsz-1:0] frq,
    input ns_ena,
    output signed [dsz-1:0] i_out, q_out
);
    // phase accumulator
    reg [fsz-1:0] acc;
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            acc <= {fsz{1'b0}};
        end
        else
        begin
            acc <= acc + frq;
        end
    end

    // optional noise shaping
    reg [fsz-1:0] ns_acc;
    reg [psz-1:0] phs;
    wire [fsz-1:0] res = ns_acc[fsz-psz-1:0];
    always @(posedge clk)
    begin
        if(reset == 1'b1)
        begin
            ns_acc <= {fsz{1'b0}};
			phs <= 0;
        end
        else
        begin
            ns_acc <= acc + {{psz{res[fsz-psz-1]}},res};
			phs <= ns_ena ? ns_acc[fsz-1:fsz-psz] : acc[fsz-1:fsz-psz];
        end
    end

	//------------------------------
    // I slice instance
	//------------------------------
    tuner_slice_1k #(
        .dsz(dsz),
        .psz(psz)
    )
    u_slice_i(
        .clk(clk),
        .reset(reset),
        .shf_90(1'b1),
        .in(in),
        .phs(phs),
        .out(i_out)
    );
    
	//------------------------------
    // Q slice instance
	//------------------------------
    tuner_slice_1k #(
        .dsz(dsz),
        .psz(psz)
    )
    u_slice_q(
        .clk(clk),
        .reset(reset),
        .shf_90(1'b0),
        .in(in),
        .phs(phs),
        .out(q_out)
    );
    
endmodule

