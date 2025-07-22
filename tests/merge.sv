`timescale 1ns/1ps

module Merge #(
	parameter WIDTH = 64
)(
	 input wire clk,
	 input wire reset,

	// Input: A:16
	 input wire [WIDTH-1:0] A_data,
	 input wire A_valid,
	output wire A_ready,

	// Input: B:16
	 input wire [WIDTH-1:0] B_data,
	 input wire B_valid,
	output wire B_ready,

	// Input: C
	 input wire C_data,
	 input wire C_valid,
	output wire C_ready,

	// Output: R:16
	output wire [WIDTH-1:0] R_data,
	output wire R_valid,
	 input wire R_ready
);
	
	// outputs
	reg [WIDTH-1:0] R_state;
	reg R_valid_reg;
	assign R_data = R_state;
	assign R_valid = R_valid_reg;
	
	// branches: 0 to N
	reg branch_id;
	wire branch_0_ready;
	wire branch_1_ready;

	always @(posedge clk) begin
		if (reset) begin
			branch_id <= 0;

			R_state <= 0;
			R_valid_reg <= 0;

		end else if ((C_data == 0) && A_valid && C_valid && (!R_valid || R_ready)) begin
			branch_id <= 0;

			R_state <= A_data;
			R_valid_reg <= 1;

		end else if ((C_data == 1) && B_valid && C_valid && (!R_valid || R_ready)) begin
			branch_id <= 1;

			R_state <= B_data;
			R_valid_reg <= 1;

		//TODO: when C_data is Z?
		end else begin
			if (R_ready) begin
				R_valid_reg <= 0;
			end
		end
	end
	
	assign branch_0_ready = (branch_id == 0) && A_valid && C_valid && (!R_valid || R_ready);
	assign branch_1_ready = (branch_id == 1) && B_valid && C_valid && (!R_valid || R_ready);

	assign A_ready = branch_0_ready;
	assign B_ready = branch_1_ready;
	assign C_ready = branch_0_ready || branch_1_ready;
endmodule
