#include <bit>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <common/mapping.h>
#include <common/mock_netlist.h>
#include <flow/func.h>
#include <verilog/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export_dot.h>
#include <interpret_verilog/export_verilog.h>

namespace fs = std::filesystem;

using arithmetic::Expression;
using arithmetic::Operation;
using arithmetic::Operand;
using namespace flow;

const size_t WIDTH = 4;
const fs::path TEST_DIR = fs::absolute(fs::current_path() / "tests");

void writeFile(std::string filename, std::string data) {
	std::ofstream fout(filename);
	if (not fout) {
		std::cerr << "ERROR: Failed to open file for verilog export: " << filename << std::endl;
		std::cerr << "ERROR: Try again from dir: <project_dir>/lib/flow" << std::endl;
		return;
	}

	fout << data;
}

void drawFlowDiagram(const Func &func) {
	string filestem = (TEST_DIR / func.name).string();
	string graphvizRaw = flow::export_func(func, false).to_string();

	writeFile(filestem + ".dot", graphvizRaw);
	//gvdot::render(filestem + ".png", graphvizRaw);
}

void expectEq(std::string expected, std::string actual) {
	auto diff = lineDiff(expected, actual);
	EXPECT_TRUE(isMatch(diff)) << diff;
}

struct FuncSet {
	vector<flow::Func> funcs;
};

struct ModSet {
	vector<clocked::Module> mods;
};

struct TestLinker : flow::Linker {
	FuncSet &lib;
	ModSet &rtl;

	TestLinker(FuncSet &lib, ModSet &rtl) : lib(lib), rtl(rtl) {
	}
	~TestLinker() {}

	flow::Implementation find(const flow::Instance &inst) override {
		flow::Implementation result;
		for (flow::Func &func : lib.funcs) {
			if (func.name == inst.type) {
				result.func = &func;
			}
		}
		for (clocked::Module &mod : rtl.mods) {
			if (mod.name == inst.type) {
				result.mod = &mod;
			}
		}

		if (result.mod == nullptr) {
			rtl.mods.push_back(clocked::Module());
			result.mod = &rtl.mods.back();
		}
		
		if (result.func != nullptr and result.mod != nullptr) {
			result.funcToMod = mapToVerilog(*result.func, *result.mod);
		}
		return result;
	}
};

TEST(ModuleSynthesis, Source) {
	string expected = R"""(`timescale 1ns/1ps

module source (
	input wire clk,
	input wire reset,
	output wire R_valid,
	input wire R_ready,
	output wire [3:0] R_data
);
	reg R_valid_reg;
	reg [3:0] R_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	assign R_valid = R_valid_reg;
	assign R_data = R_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(R_ready||!R_valid_reg);
	always @(posedge clk) begin
		if (reset) begin
			R_valid_reg <= 0;
			R_data_reg <= 0;
		end else if (branch_0_valid&&(R_ready||!R_valid_reg)) begin
			R_data_reg <= 1;
			R_valid_reg <= 1;
		end else begin
			if (R_ready) begin
				R_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "source";
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, Expression::intOf(1));  //TODO: send random int?

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Sink) {
	string expected = R"""(`timescale 1ns/1ps

module sink (
	input wire clk,
	input wire reset,
	input wire L_valid,
	output wire L_ready,
	input wire [3:0] L_data
);
	wire branch_0_valid;
	wire branch_0_ready;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid;
	assign L_ready = branch_0_ready;
endmodule)""";

	Func func;
	func.name = "sink";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].ack(L);

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Buffer) {
	string expected = R"""(`timescale 1ns/1ps

module buffer (
	input wire clk,
	input wire reset,
	input wire L_valid,
	output wire L_ready,
	input wire [3:0] L_data,
	output wire R_valid,
	input wire R_ready,
	output wire [3:0] R_data
);
	reg R_valid_reg;
	reg [3:0] R_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	assign R_valid = R_valid_reg;
	assign R_data = R_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(R_ready||!R_valid_reg);
	assign L_ready = branch_0_ready;
	always @(posedge clk) begin
		if (reset) begin
			R_valid_reg <= 0;
			R_data_reg <= 0;
		end else if (branch_0_valid&&(R_ready||!R_valid_reg)) begin
			R_data_reg <= L_data;
			R_valid_reg <= 1;
		end else begin
			if (R_ready) begin
				R_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "buffer";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL);
	func.conds[branch0].ack(L);

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Copy) {
	string expected = R"""(`timescale 1ns/1ps

module copy (
	input wire clk,
	input wire reset,
	input wire L_valid,
	output wire L_ready,
	input wire [3:0] L_data,
	output wire R0_valid,
	input wire R0_ready,
	output wire [3:0] R0_data,
	output wire R1_valid,
	input wire R1_ready,
	output wire [3:0] R1_data
);
	reg R0_valid_reg;
	reg [3:0] R0_data_reg;
	reg R1_valid_reg;
	reg [3:0] R1_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	assign R0_valid = R0_valid_reg;
	assign R0_data = R0_data_reg;
	assign R1_valid = R1_valid_reg;
	assign R1_data = R1_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(R0_ready||!R0_valid_reg)&&(R1_ready||!R1_valid_reg);
	assign L_ready = branch_0_ready;
	always @(posedge clk) begin
		if (reset) begin
			R0_valid_reg <= 0;
			R0_data_reg <= 0;
			R1_valid_reg <= 0;
			R1_data_reg <= 0;
		end else if (branch_0_valid&&(R0_ready||!R0_valid_reg)&&(R1_ready||!R1_valid_reg)) begin
			R0_data_reg <= L_data;
			R0_valid_reg <= 1;
			R1_data_reg <= L_data;
			R1_valid_reg <= 1;
		end else begin
			if (R0_ready) begin
				R0_valid_reg <= 0;
			end
			if (R1_ready) begin
				R1_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "copy";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R0 = func.pushNet("R0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func.pushNet("R1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R0, exprL);
	func.conds[branch0].req(R1, exprL);
	func.conds[branch0].ack(L);

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Func) {
	string expected = R"""(`timescale 1ns/1ps

module func (
	input wire clk,
	input wire reset,
	input wire L0_valid,
	output wire L0_ready,
	input wire [3:0] L0_data,
	input wire L1_valid,
	output wire L1_ready,
	input wire [3:0] L1_data,
	output wire R_valid,
	input wire R_ready,
	output wire [3:0] R_data
);
	reg R_valid_reg;
	reg [3:0] R_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	assign R_valid = R_valid_reg;
	assign R_data = R_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(R_ready||!R_valid_reg);
	assign L0_ready = branch_0_ready;
	assign L1_ready = branch_0_ready;
	always @(posedge clk) begin
		if (reset) begin
			R_valid_reg <= 0;
			R_data_reg <= 0;
		end else if (branch_0_valid&&(R_ready||!R_valid_reg)) begin
			R_data_reg <= L0_valid&&L0_data||L1_valid&&L1_data;
			R_valid_reg <= 1;
		end else begin
			if (R_ready) begin
				R_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "func";
	Operand L0 = func.pushNet("L0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL0 || exprL1);
	//TODO: HWAT??? bitwise vs non-bitwise operators actually invert in synthesis!! (see arith::Expr tests, this if expected behavior!?)
	//func.conds[branch0].mem(m_or, exprL0 || exprL1);
	func.conds[branch0].ack({L0, L1});

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Split) {
	string expected = R"""(`timescale 1ns/1ps

module split (
	input wire clk,
	input wire reset,
	input wire L_valid,
	output wire L_ready,
	input wire [3:0] L_data,
	input wire C_valid,
	output wire C_ready,
	input wire C_data,
	output wire R0_valid,
	input wire R0_ready,
	output wire [3:0] R0_data,
	output wire R1_valid,
	input wire R1_ready,
	output wire [3:0] R1_data
);
	reg R0_valid_reg;
	reg [3:0] R0_data_reg;
	reg R1_valid_reg;
	reg [3:0] R1_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	wire branch_1_valid;
	wire branch_1_ready;
	assign R0_valid = R0_valid_reg;
	assign R0_data = R0_data_reg;
	assign R1_valid = R1_valid_reg;
	assign R1_data = R1_data_reg;
	assign branch_0_valid = C_valid&&C_data==0;
	assign branch_0_ready = branch_0_valid&&(R0_ready||!R0_valid_reg);
	assign branch_1_valid = C_valid&&C_data==1;
	assign branch_1_ready = branch_1_valid&&(R1_ready||!R1_valid_reg);
	assign L_ready = branch_0_ready||branch_1_ready;
	assign C_ready = branch_0_ready||branch_1_ready;
	always @(posedge clk) begin
		if (reset) begin
			R0_valid_reg <= 0;
			R0_data_reg <= 0;
			R1_valid_reg <= 0;
			R1_data_reg <= 0;
		end else if (branch_0_valid&&(R0_ready||!R0_valid_reg)) begin
			R0_data_reg <= L_data;
			R0_valid_reg <= 1;
			if (R1_ready) begin
				R1_valid_reg <= 0;
			end
		end else if (branch_1_valid&&(R1_ready||!R1_valid_reg)) begin
			R1_data_reg <= L_data;
			R1_valid_reg <= 1;
			if (R0_ready) begin
				R0_valid_reg <= 0;
			end
		end else begin
			if (R0_ready) begin
				R0_valid_reg <= 0;
			end
			if (R1_ready) begin
				R1_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "split";
	Expression L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Expression C = func.pushNet("C", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression R0 = func.pushNet("R0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression R1 = func.pushNet("R1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);

	size_t branch0 = func.pushCond(C == Expression::intOf(0));
	func.conds[branch0].req(R0.top, L);
	func.conds[branch0].ack({C.top, L.top});

	size_t branch1 = func.pushCond(C == Expression::intOf(1));
	func.conds[branch1].req(R1.top, L);
	func.conds[branch1].ack({C.top, L.top});

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Merge) {
	string expected = R"""(`timescale 1ns/1ps

module merge (
	input wire clk,
	input wire reset,
	input wire L0_valid,
	output wire L0_ready,
	input wire [3:0] L0_data,
	input wire L1_valid,
	output wire L1_ready,
	input wire [3:0] L1_data,
	input wire C_valid,
	output wire C_ready,
	input wire C_data,
	output wire R_valid,
	input wire R_ready,
	output wire [3:0] R_data
);
	reg R_valid_reg;
	reg [3:0] R_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	wire branch_1_valid;
	wire branch_1_ready;
	assign R_valid = R_valid_reg;
	assign R_data = R_data_reg;
	assign branch_0_valid = C_valid&&C_data==0;
	assign branch_0_ready = branch_0_valid&&(R_ready||!R_valid_reg);
	assign branch_1_valid = C_valid&&C_data==1;
	assign branch_1_ready = branch_1_valid&&(R_ready||!R_valid_reg);
	assign L0_ready = branch_0_ready;
	assign L1_ready = branch_1_ready;
	assign C_ready = branch_0_ready||branch_1_ready;
	always @(posedge clk) begin
		if (reset) begin
			R_valid_reg <= 0;
			R_data_reg <= 0;
		end else if (branch_0_valid&&(R_ready||!R_valid_reg)) begin
			R_data_reg <= L0_data;
			R_valid_reg <= 1;
		end else if (branch_1_valid&&(R_ready||!R_valid_reg)) begin
			R_data_reg <= L1_data;
			R_valid_reg <= 1;
		end else begin
			if (R_ready) begin
				R_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "merge";
	Operand L0 = func.pushNet("L0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);
	Expression exprC(C);

	size_t branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(R, exprL0);
	func.conds[branch0].ack({C, L0});

	size_t branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(R, exprL1);
	func.conds[branch1].ack({C, L1});

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, StreamingAdder) {
	string expected = R"""(`timescale 1ns/1ps

module s_adder (
	input wire clk,
	input wire reset,
	input wire L_valid,
	output wire L_ready,
	input wire [3:0] L_data,
	output wire R_valid,
	input wire R_ready,
	output wire [3:0] R_data
);
	reg [3:0] m_data;
	reg R_valid_reg;
	reg [3:0] R_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	assign R_valid = R_valid_reg;
	assign R_data = R_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(R_ready||!R_valid_reg);
	assign L_ready = branch_0_ready;
	always @(posedge clk) begin
		if (reset) begin
			m_data <= 0;
			R_valid_reg <= 0;
			R_data_reg <= 0;
		end else if (branch_0_valid&&(R_ready||!R_valid_reg)) begin
			m_data <= L_data;
			R_data_reg <= L_data+m_data;
			R_valid_reg <= 1;
		end else begin
			if (R_ready) begin
				R_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "s_adder";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand m = func.pushNet("m", Type(Type::TypeName::FIXED, WIDTH), flow::Net::REG);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);
	Expression exprm(m);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL + exprm);
	func.conds[branch0].mem(m, exprL);
	func.conds[branch0].ack(L);

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, SerialAdder) {
	string expected = R"""(`timescale 1ns/1ps

module serial_adder (
	input wire clk,
	input wire reset,
	input wire Ad_valid,
	output wire Ad_ready,
	input wire [3:0] Ad_data,
	input wire Ac_valid,
	output wire Ac_ready,
	input wire Ac_data,
	input wire Bd_valid,
	output wire Bd_ready,
	input wire [3:0] Bd_data,
	input wire Bc_valid,
	output wire Bc_ready,
	input wire Bc_data,
	output wire Sd_valid,
	input wire Sd_ready,
	output wire [3:0] Sd_data,
	output wire Sc_valid,
	input wire Sc_ready,
	output wire Sc_data
);
	reg Sd_valid_reg;
	reg [3:0] Sd_data_reg;
	reg Sc_valid_reg;
	reg Sc_data_reg;
	reg ci_data;
	wire branch_0_valid;
	wire branch_0_ready;
	wire branch_1_valid;
	wire branch_1_ready;
	wire branch_2_valid;
	wire branch_2_ready;
	wire branch_3_valid;
	wire branch_3_ready;
	wire branch_4_valid;
	wire branch_4_ready;
	assign Sd_valid = Sd_valid_reg;
	assign Sd_data = Sd_data_reg;
	assign Sc_valid = Sc_valid_reg;
	assign Sc_data = Sc_data_reg;
	assign branch_0_valid = (Ac_valid&&!Ac_data)&(Bc_valid&&!Bc_data);
	assign branch_0_ready = branch_0_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg);
	assign branch_1_valid = (Ac_valid&&Ac_data)&(Bc_valid&&!Bc_data);
	assign branch_1_ready = branch_1_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg);
	assign branch_2_valid = (Ac_valid&&!Ac_data)&(Bc_valid&&Bc_data);
	assign branch_2_ready = branch_2_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg);
	assign branch_3_valid = (Ac_valid&&Ac_data)&(Bc_valid&&Bc_data)&(Ad_valid&Bd_valid&&(Ad_data+Bd_data+ci_data)/16!=ci_data);
	assign branch_3_ready = branch_3_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg);
	assign branch_4_valid = (Ac_valid&&Ac_data)&(Bc_valid&&Bc_data)&(Ad_valid&Bd_valid&&(Ad_data+Bd_data+ci_data)/16==ci_data);
	assign branch_4_ready = branch_4_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg);
	assign Ad_ready = branch_0_ready||branch_2_ready||branch_4_ready;
	assign Ac_ready = branch_0_ready||branch_2_ready||branch_4_ready;
	assign Bd_ready = branch_0_ready||branch_1_ready||branch_4_ready;
	assign Bc_ready = branch_0_ready||branch_1_ready||branch_4_ready;
	always @(posedge clk) begin
		if (reset) begin
			Sd_valid_reg <= 0;
			Sd_data_reg <= 0;
			Sc_valid_reg <= 0;
			Sc_data_reg <= 0;
			ci_data <= 0;
		end else if (branch_0_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg)) begin
			ci_data <= (Ad_data+Bd_data+ci_data)/16;
			Sd_data_reg <= (Ad_data+Bd_data+ci_data)%16;
			Sd_valid_reg <= 1;
			Sc_data_reg <= 1'b0;
			Sc_valid_reg <= 1;
		end else if (branch_1_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg)) begin
			ci_data <= (Ad_data+Bd_data+ci_data)/16;
			Sd_data_reg <= (Ad_data+Bd_data+ci_data)%16;
			Sd_valid_reg <= 1;
			Sc_data_reg <= 1'b0;
			Sc_valid_reg <= 1;
		end else if (branch_2_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg)) begin
			ci_data <= (Ad_data+Bd_data+ci_data)/16;
			Sd_data_reg <= (Ad_data+Bd_data+ci_data)%16;
			Sd_valid_reg <= 1;
			Sc_data_reg <= 1'b0;
			Sc_valid_reg <= 1;
		end else if (branch_3_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg)) begin
			ci_data <= (Ad_data+Bd_data+ci_data)/16;
			Sd_data_reg <= (Ad_data+Bd_data+ci_data)%16;
			Sd_valid_reg <= 1;
			Sc_data_reg <= 1'b0;
			Sc_valid_reg <= 1;
		end else if (branch_4_valid&&(Sd_ready||!Sd_valid_reg)&&(Sc_ready||!Sc_valid_reg)) begin
			ci_data <= 0;
			Sd_data_reg <= (Ad_data+Bd_data+ci_data)%16;
			Sd_valid_reg <= 1;
			Sc_data_reg <= 1'b1;
			Sc_valid_reg <= 1;
		end else begin
			if (Sd_ready) begin
				Sd_valid_reg <= 0;
			end
			if (Sc_ready) begin
				Sc_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "serial_adder";
	Operand Ad = func.pushNet("Ad", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Ac = func.pushNet("Ac", Type(Type::TypeName::FIXED, 1),			flow::Net::IN);
	Operand Bd = func.pushNet("Bd", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Bc = func.pushNet("Bc", Type(Type::TypeName::FIXED, 1),			flow::Net::IN);
	Operand Sd = func.pushNet("Sd", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand Sc = func.pushNet("Sc", Type(Type::TypeName::FIXED, 1),			flow::Net::OUT);
	Operand ci = func.pushNet("ci", Type(Type::TypeName::FIXED, 1),			flow::Net::REG);
	Expression expr_Ac(Ac);
	Expression expr_Ad(Ad);
	Expression expr_Bc(Bc);
	Expression expr_Bd(Bd);
	Expression expr_ci(ci);

	Expression expr_s((expr_Ad + expr_Bd + expr_ci) % pow(2, WIDTH));
	Expression expr_co((expr_Ad + expr_Bd + expr_ci) / pow(2, WIDTH));

	size_t branch0 = func.pushCond(!expr_Ac && !expr_Bc);
	func.conds[branch0].req(Sd, expr_s);
	func.conds[branch0].req(Sc, Expression::boolOf(false));
	func.conds[branch0].mem(ci, expr_co);
	func.conds[branch0].ack({Ac, Ad, Bc, Bd});

	size_t branch1 = func.pushCond(expr_Ac && !expr_Bc);
	func.conds[branch1].req(Sd, expr_s);
	func.conds[branch1].req(Sc, Expression::boolOf(false));
	func.conds[branch1].mem(ci, expr_co);
	func.conds[branch1].ack({Bc, Bd});

	size_t branch2 = func.pushCond(!expr_Ac && expr_Bc);
	func.conds[branch2].req(Sd, expr_s);
	func.conds[branch2].req(Sc, Expression::boolOf(false));
	func.conds[branch2].mem(ci, expr_co);
	func.conds[branch2].ack({Ac, Ad});

	size_t branch3 = func.pushCond(expr_Ac && expr_Bc && (expr_co != expr_ci));
	func.conds[branch3].req(Sd, expr_s);
	func.conds[branch3].req(Sc, Expression::boolOf(false));
	func.conds[branch3].mem(ci, expr_co);

	size_t branch4 = func.pushCond(expr_Ac && expr_Bc && (expr_co == expr_ci));
	func.conds[branch4].req(Sd, expr_s);
	func.conds[branch4].req(Sc, Expression::boolOf(true));
	func.conds[branch4].mem(ci, Expression::intOf(0));
	func.conds[branch4].ack({Ac, Ad, Bc, Bd});

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

/*auto get_channel_probe = [](arithmetic::Operand &operand) {
	return arithmetic::memberCall(operand, "probe", {});
};

TEST(ModuleSynthesis, Probes) {
	Func func;
	func.name = "ds_adder_flat";
	Operand Ad = func.pushNet("Ad", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Ac = func.pushNet("Ac", Type(Type::TypeName::FIXED, 1),			flow::Net::IN);
	Operand Bd = func.pushNet("Bd", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Bc = func.pushNet("Bc", Type(Type::TypeName::FIXED, 1),			flow::Net::IN);
	Operand Sd = func.pushNet("Sd", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand Sc = func.pushNet("Sc", Type(Type::TypeName::FIXED, 1),			flow::Net::OUT);
	Operand ci = func.pushNet("ci", Type(Type::TypeName::FIXED, 1),			flow::Net::REG);
	Expression expr_Ac(Ac);
	Expression expr_Ad(Ad);
	Expression expr_Bc(Bc);
	Expression expr_Bd(Bd);
	Expression expr_ci(ci);

	Expression probe_Ac = get_channel_probe(Ac);
	Expression probe_Bc = get_channel_probe(Bc);
	Expression probe_Ad = get_channel_probe(Ad);
	Expression probe_Bd = get_channel_probe(Bd);
	//Expression probe_Bd(arithmetic::Operation::CALL, {probe, Bd});

	Expression expr_s((probe_Ad + probe_Bd + expr_ci) % pow(2, WIDTH));
	Expression expr_co((probe_Ad + probe_Bd + expr_ci) / pow(2, WIDTH));

	size_t branch0 = func.pushCond(!probe_Ac && !probe_Bc);
	func.conds[branch0].req(Sd, expr_s);
	func.conds[branch0].req(Sc, Expression::boolOf(false));
	func.conds[branch0].mem(ci, expr_co);
	func.conds[branch0].ack({Ac, Ad, Bc, Bd});

	size_t branch1 = func.pushCond(probe_Ac && !probe_Bc);
	func.conds[branch1].req(Sd, expr_s);
	func.conds[branch1].req(Sc, Expression::boolOf(false));
	func.conds[branch1].mem(ci, expr_co);
	func.conds[branch1].ack({Bc, Bd});

	size_t branch2 = func.pushCond(!probe_Ac && probe_Bc);
	func.conds[branch2].req(Sd, expr_s);
	func.conds[branch2].req(Sc, Expression::boolOf(false));
	func.conds[branch2].mem(ci, expr_co);
	func.conds[branch2].ack({Ac, Ad});

	size_t branch3 = func.pushCond(probe_Ac && probe_Bc && (expr_co != expr_ci));
	func.conds[branch3].req(Sd, expr_s);
	func.conds[branch3].req(Sc, Expression::boolOf(false));
	func.conds[branch3].mem(ci, expr_co);

	size_t branch4 = func.pushCond(probe_Ac && probe_Bc && (expr_co == expr_ci));
	func.conds[branch4].req(Sd, expr_s);
	func.conds[branch4].req(Sc, Expression::boolOf(true));
	func.conds[branch4].mem(ci, Expression::intOf(0));
	func.conds[branch4].ack({Ac, Ad, Bc, Bd});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}*/

TEST(ModuleSynthesis, FullAdder) {
	using arithmetic::booleanXor;

	string expected = R"""(`timescale 1ns/1ps

module full_adder (
	input wire clk,
	input wire reset,
	input wire A_valid,
	output wire A_ready,
	input wire A_data,
	input wire B_valid,
	output wire B_ready,
	input wire B_data,
	output wire S_valid,
	input wire S_ready,
	output wire S_data,
	input wire Ci_valid,
	output wire Ci_ready,
	input wire Ci_data,
	output wire Co_valid,
	input wire Co_ready,
	output wire Co_data
);
	reg S_valid_reg;
	reg S_data_reg;
	reg Co_valid_reg;
	reg Co_data_reg;
	wire branch_0_valid;
	wire branch_0_ready;
	assign S_valid = S_valid_reg;
	assign S_data = S_data_reg;
	assign Co_valid = Co_valid_reg;
	assign Co_data = Co_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(S_ready||!S_valid_reg)&&(Co_ready||!Co_valid_reg);
	assign A_ready = branch_0_ready;
	assign B_ready = branch_0_ready;
	assign Ci_ready = branch_0_ready;
	always @(posedge clk) begin
		if (reset) begin
			S_valid_reg <= 0;
			S_data_reg <= 0;
			Co_valid_reg <= 0;
			Co_data_reg <= 0;
		end else if (branch_0_valid&&(S_ready||!S_valid_reg)&&(Co_ready||!Co_valid_reg)) begin
			S_data_reg <= (A_data&&!B_data||!A_data&&B_data)&&!Ci_data||!(A_data&&!B_data||!A_data&&B_data)&&Ci_data;
			S_valid_reg <= 1;
			Co_data_reg <= A_data&&B_data&&A_valid&B_valid||(A_data&&Ci_data&&A_valid&Ci_valid)|(B_data&&Ci_data&&B_valid&Ci_valid)|A_valid&B_valid&Ci_valid&&(A_data&&Ci_data&&A_valid&Ci_valid||B_data&&Ci_data&&B_valid&Ci_valid);
			Co_valid_reg <= 1;
		end else begin
			if (S_ready) begin
				S_valid_reg <= 0;
			end
			if (Co_ready) begin
				Co_valid_reg <= 0;
			end
		end
	end
endmodule)""";

	Func func;
	func.name = "full_adder";
	Expression A = func.pushNet("A", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression B = func.pushNet("B", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression S = func.pushNet("S", Type(Type::TypeName::FIXED, 1), flow::Net::OUT);
	Expression Ci = func.pushNet("Ci", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression Co = func.pushNet("Co", Type(Type::TypeName::FIXED, 1), flow::Net::OUT);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(S.top, booleanXor(booleanXor(A, B), Ci));
	func.conds[branch0].req(Co.top, (A && B) || (A && Ci) || (B && Ci));
	func.conds[branch0].ack({A.top, B.top, Ci.top});

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

TEST(ModuleSynthesis, Instance) {
	string expected = R"""(`timescale 1ns/1ps

module instance_test (
	input wire clk,
	input wire reset,
	input wire A_valid,
	output wire A_ready,
	input wire A_data,
	input wire B_valid,
	output wire B_ready,
	input wire B_data,
	output wire S_valid,
	input wire S_ready,
	output wire S_data,
	input wire Ci_valid,
	output wire Ci_ready,
	input wire Ci_data,
	output wire Co_valid,
	input wire Co_ready,
	output wire Co_data
);
	reg S_valid_reg;
	reg S_data_reg;
	reg Co_valid_reg;
	reg Co_data_reg;
	wire s_data;
	wire co_data;
	wire branch_0_valid;
	wire branch_0_ready;
	wire thing;
	assign S_valid = S_valid_reg;
	assign S_data = S_data_reg;
	assign Co_valid = Co_valid_reg;
	assign Co_data = Co_data_reg;
	assign branch_0_valid = 1'b1;
	assign branch_0_ready = branch_0_valid&&(S_ready||!S_valid_reg)&&(Co_ready||!Co_valid_reg);
	assign A_ready = branch_0_ready;
	assign B_ready = branch_0_ready;
	assign Ci_ready = branch_0_ready;
	always @(posedge clk) begin
		if (reset) begin
			S_valid_reg <= 0;
			S_data_reg <= 0;
			Co_valid_reg <= 0;
			Co_data_reg <= 0;
		end else if (branch_0_valid&&(S_ready||!S_valid_reg)&&(Co_ready||!Co_valid_reg)) begin
			S_data_reg <= thing;
			S_valid_reg <= 1;
			Co_data_reg <= co_data;
			Co_valid_reg <= 1;
		end else begin
			if (S_ready) begin
				S_valid_reg <= 0;
			end
			if (Co_ready) begin
				Co_valid_reg <= 0;
			end
		end
	end
	add adder(A_data, B_data, Ci_data, s_data, co_data);
	myFunc func(s_data, thing);
endmodule)""";

	Func func;
	func.name = "instance_test";
	Expression A = func.pushNet("A", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression B = func.pushNet("B", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression S = func.pushNet("S", Type(Type::TypeName::FIXED, 1), flow::Net::OUT);
	Expression Ci = func.pushNet("Ci", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Expression Co = func.pushNet("Co", Type(Type::TypeName::FIXED, 1), flow::Net::OUT);
	Expression s = func.pushNet("s", Type(Type::TypeName::FIXED, 1), flow::Net::WIRE);
	Expression co = func.pushNet("co", Type(Type::TypeName::FIXED, 1), flow::Net::WIRE);
	

	func.inst.push_back(flow::Instance("add", {A, B, Ci, s, co}));
	func.inst.back().name = "adder";
	func.inst.back().comment = "this is an adder";

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(S.top, arithmetic::call("myFunc", {s}));
	func.conds[branch0].req(Co.top, co);
	func.conds[branch0].ack({A.top, B.top, Ci.top});

	FuncSet lib;
	ModSet rtl;
	lib.funcs.push_back(func);
	TestLinker linker(lib, rtl);
	clocked::Module mod = synthesizeModuleFromFunc(func, &linker);

	expectEq(expected, parse_verilog::export_module(mod).to_string());
}

