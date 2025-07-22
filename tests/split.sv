`timescale 1ns/1ps

module Split #(
	parameter WIDTH = 64
)(
	input  wire	clk,
	input  wire reset,

	input  wire [WIDTH-1:0] L_data,
	input  wire L_valid,
	output wire L_ready,

	input  wire C_data,
	input  wire C_valid,
	output wire C_ready,

	output wire [WIDTH-1:0] A_data,
	output wire A_valid,
	input  wire A_ready,

	output wire [WIDTH-1:0] B_data,
	output wire B_valid,
	input  wire B_ready
);

	reg [WIDTH-1:0] A_state;
	reg  A_valid_reg;
	assign A_data = A_state;
	assign A_valid = A_valid_reg;

	reg [WIDTH-1:0] B_state;
	reg  B_valid_reg;
	assign B_data = B_state;
	assign B_valid = B_valid_reg;

	reg branch_id;
	wire branch_0_ready;
	wire branch_1_ready;

	always @(posedge clk) begin
		if (reset) begin
			branch_id <= 0;

			A_state <= 0;
			A_valid_reg <= 0;

			B_state <= 0;
			B_valid_reg <= 0;

		end else if ((C_data == 0) && L_valid && C_valid && (!A_valid_reg || A_ready)) begin
			branch_id <= 0;
			A_state <= L_data;
			A_valid_reg <= 1;
			B_valid_reg <= 0;

		end else if ((C_data == 1) && L_valid && C_valid && (!B_valid_reg || B_ready)) begin
			branch_id <= 1;
			B_state <= L_data;
			B_valid_reg <= 1;
			A_valid_reg <= 0;

		end else begin
			if (A_ready) begin
				A_valid_reg <= 0;
			end
			if (B_ready) begin
				B_valid_reg <= 0;
			end
		end
	end

	assign branch_0_ready = (branch_id == 0) && L_valid && C_valid && (!A_valid_reg || A_ready);
	assign branch_1_ready = (branch_id == 1) && L_valid && C_valid && (!B_valid_reg || B_ready);

	assign L_ready = branch_0_ready || branch_1_ready;
	assign C_ready = branch_0_ready || branch_1_ready;
endmodule
