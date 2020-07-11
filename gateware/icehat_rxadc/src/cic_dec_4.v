// cic_dec_4.v: CIC Decimator - single with internal truncation before combs
// integrators split & pipelined to run faster than 40MHz
// 2017-03-17 E. Brombaugh

module cic_dec_4 #(
	parameter NUM_STAGES = 4,						// Stages of int / comb
	          STG_GSZ = 8,							// Bit growth per stage
	          ISZ = 10,								// Input word size
	          ASZ = (ISZ + (NUM_STAGES * STG_GSZ)),	// Integrator/Adder word size
	          LSZ = 23,                             // Integrator low section size
              HSZ = ASZ - LSZ,                      // Integrator high section size
              OSZ = ASZ	                            // Output word size
)
(
	input clk,						// System clock
	input reset,					// System POR
	input ena_out,					// Decimated output rate (2 clks wide)
	input signed [ISZ-1:0] x,	    // Input data
	output signed [OSZ-1:0] y,	    // Output data
	output valid					// Output Valid
);	
	// sign-extend input
	wire signed [ASZ-1:0] x_sx = {{ASZ-ISZ{x[ISZ-1]}},x};
    
    // stagger - split input into low / high sections
    wire signed [LSZ-1:0] x_sx_l = x_sx[LSZ-1:0];
    reg signed [HSZ-1:0] x_sx_h;
    always @(posedge clk)
        x_sx_h <= x_sx[ASZ-1:HSZ];
    
	// Integrators
	reg signed [LSZ:0] integrator_l[0:NUM_STAGES-1];
	reg signed [HSZ-1:0] integrator_h[0:NUM_STAGES-1];
	always @(posedge clk)
	begin
		if(reset == 1'b1)
		begin
			integrator_l[0] <= 0;
			integrator_h[0] <= 0;
		end
		else
		begin
			integrator_l[0] <= integrator_l[0][LSZ-1:0] + x_sx_l;
            integrator_h[0] <= integrator_h[0] + x_sx_h + integrator_l[0][LSZ];
		end
	end
	generate
		genvar i;
		for(i=1;i<NUM_STAGES;i=i+1)
		begin
			always @(posedge clk)
			begin
				if(reset == 1'b1)
                begin
					integrator_l[i] <= 0;
					integrator_h[i] <= 0;
                end
				else
                begin
					integrator_l[i] <= integrator_l[i][LSZ-1:0] + integrator_l[i-1][LSZ-1:0];
					integrator_h[i] <= integrator_h[i] + integrator_h[i-1] + integrator_l[i][LSZ];
                end
			end
		end
	endgenerate
        
    // destagger - combine low / high sections
    reg signed [LSZ-1:0] low_pipe;
    always @(posedge clk)
        low_pipe <= integrator_l[NUM_STAGES-1];
    wire signed [ASZ-1:0] integrator_out = {integrator_h[NUM_STAGES-1],low_pipe};
		
	// Combs
	reg [NUM_STAGES:0] comb_ena;
	reg signed [OSZ-1:0] comb_diff[0:NUM_STAGES];
	reg signed [OSZ-1:0] comb_dly[0:NUM_STAGES];
	always @(posedge clk)
	begin
		if(reset == 1'b1)
		begin
			comb_ena <= {NUM_STAGES+2{1'b0}};
			comb_diff[0] <= {OSZ{1'b0}};
			comb_dly[0] <= {OSZ{1'b0}};
		end
		else
        begin
            if(ena_out == 1'b1)
            begin
                comb_diff[0] <= integrator_out>>>(ASZ-OSZ);
                comb_dly[0] <= comb_diff[0];
            end
            comb_ena <= {comb_ena[NUM_STAGES:0],ena_out};
        end
	end
	generate
		genvar j;
		for(j=1;j<=NUM_STAGES;j=j+1)
		begin
			always @(posedge clk)
			begin
				if(reset == 1'b1)
				begin
					comb_diff[j] <= {OSZ{1'b0}};
					comb_dly[j] <= {OSZ{1'b0}};
				end
				else if(comb_ena[j-1] == 1'b1)
				begin
					comb_diff[j] <= comb_diff[j-1] - comb_dly[j-1];
					comb_dly[j] <= comb_diff[j];
				end
			end
		end
	endgenerate
	
	// assign output
	assign y = comb_diff[NUM_STAGES];
	assign valid = comb_ena[NUM_STAGES];
endmodule
