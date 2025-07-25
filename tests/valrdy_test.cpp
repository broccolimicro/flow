#include <bit>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

#define EXPECT_SUBSTRING(source, substring) \
    EXPECT_NE((source).find(substring), std::string::npos) \
		<< "ERROR: Expected Verilog to contain substring \"" << (substring) << "\""
#define EXPECT_NO_SUBSTRING(source, substring) \
    EXPECT_EQ((source).find(substring), std::string::npos) \
		<< "ERROR: Expected Verilog to NOT contain substring \"" << (substring) << "\""

using namespace flow;
using arithmetic::Expression;
using arithmetic::Operand;
using std::filesystem::absolute;
using std::filesystem::current_path;

const int WIDTH = 16;
const std::filesystem::path TEST_DIR = absolute(current_path() / "tests");


parse_verilog::module_def synthesizeVerilogFromFunc(const Func &func) {
	clocked::Module mod = synthesize_valrdy(func);
	parse_verilog::module_def mod_v = export_module(mod);
	string verilog = mod_v.to_string();
	cout << verilog << endl;

	auto write_verilog_file = [](const string &data, const string &filename) {
		std::ofstream export_file(filename);
		if (!export_file) {
				std::cerr << "ERROR: Failed to open file for verilog export: "
					<< filename << std::endl
					<< "ERROR: Try again from dir: <project_dir>/lib/flow" << std::endl;
		}
		export_file << data;
	};
	string export_filename = (TEST_DIR / (func.name + ".v")).string();
	EXPECT_NO_THROW(write_verilog_file(verilog, export_filename));

	// Validate branches
	size_t branch_count = func.conds.size();
	int branch_reg_width = std::bit_width(branch_count) - 1;
	if (branch_count > 2) {
		EXPECT_SUBSTRING(verilog, "reg [" + std::to_string(branch_reg_width) + ":0] branch_id;");
	} else {
		EXPECT_SUBSTRING(verilog, "reg branch_id;");
	}
	for (int i = 0; i < branch_count; i++) {
		EXPECT_SUBSTRING(verilog, "branch_id <= " + std::to_string(i) + ";");
	}

	return mod_v;
}

TEST(ExportTest, Source) {
	Func func;
	func.name = "source";
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, Expression::intOf(1));  //TODO: send random int?

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= 1");
}

TEST(ExportTest, Sink) {
	Func func;
	func.name = "sink";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ExportTest, Buffer) {
	Func func;
	func.name = "buffer";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL);
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L_data;");
}

TEST(ExportTest, Copy) {
	Func func;
	func.name = "copy";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand R0 = func.pushNet("R0", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func.pushNet("R1", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R0, exprL);
	func.conds[branch0].req(R1, exprL);
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R0_state <= L_data;");
	EXPECT_SUBSTRING(verilog, "R1_state <= L_data;");
}

TEST(ExportTest, Func) {
	Func func;
	func.name = "func";
	Operand L0 = func.pushNet("L0", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL0 || exprL1);
	//TODO: HWAT??? bitwise vs non-bitwise operators actually invert in synthesis!! (see arith::Expr tests, this if expected behavior!?)
	//func.conds[branch0].mem(m_or, exprL0 || exprL1);
	func.conds[branch0].ack({L0, L1});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L0_data|L1_data;");
	EXPECT_NO_SUBSTRING(verilog, "R_state <= L0_data||L1_data;");
}

TEST(ExportTest, Merge) {
	Func func;
	func.name = "merge";
	Operand L0 = func.pushNet("L0", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);
	Expression exprC(C);

	int branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(R, exprL0);
	func.conds[branch0].ack({C, L0});

	int branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(R, exprL1);
	func.conds[branch1].ack({C, L1});
;
	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L0_data;");
	EXPECT_SUBSTRING(verilog, "R_state <= L1_data;");
}

TEST(ExportTest, Split) {
	Func func;
	func.name = "split";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	Operand R0 = func.pushNet("R0", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func.pushNet("R1", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);
	Expression exprC(C);

	int branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(R0, exprL);
	func.conds[branch0].ack({C, L});

	int branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(R1, exprL);
	func.conds[branch1].ack({C, L});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R0_state <= L_data;");
	EXPECT_SUBSTRING(verilog, "R1_state <= L_data;");
}

TEST(ExportTest, SAdder) {
	Func func;
	func.name = "s_adder";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand m = func.pushNet("m", Type(Type::FIXED, WIDTH), flow::Net::REG);
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);
	Expression exprm(m);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL + exprm);
	func.conds[branch0].mem(m, exprL);
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L_data+m_data;");
	EXPECT_SUBSTRING(verilog, "m_data <= L_data;");
}

TEST(ExportTest, DSAdderFlat) {
	Func func;
	func.name = "ds_adder_flat";
	Operand Ad = func.pushNet("Ad", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand Ac = func.pushNet("Ac", Type(Type::FIXED, 1), flow::Net::IN);
	Operand Bd = func.pushNet("Bd", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand Bc = func.pushNet("Bc", Type(Type::FIXED, 1), flow::Net::IN);
	Operand Sd = func.pushNet("Sd", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Operand Sc = func.pushNet("Sc", Type(Type::FIXED, 1), flow::Net::OUT);
	Operand ci = func.pushNet("ci", Type(Type::FIXED, 1), flow::Net::REG);
	Expression exprAc(Ac);
	Expression exprAd(Ad);
	Expression exprBc(Bc);
	Expression exprBd(Bd);
	Expression exprci(ci);

	Expression s((exprAd + exprBd + exprci) % pow(2, WIDTH));
	Expression co((exprAd + exprBd + exprci) / pow(2, WIDTH));

	int branch0 = func.pushCond(~exprAc & ~exprBc);
	func.conds[branch0].req(Sd, s);
	func.conds[branch0].req(Sc, Operand::intOf(0));
	func.conds[branch0].mem(ci, co);
	func.conds[branch0].ack({Ac, Ad, Bc, Bd});

	int branch1 = func.pushCond(exprAc & ~exprBc);
	func.conds[branch1].req(Sd, s);
	func.conds[branch1].req(Sc, Operand::intOf(0));
	func.conds[branch1].mem(ci, co);
	func.conds[branch1].ack({Bc, Bd});

	int branch2 = func.pushCond(~exprAc & exprBc);
	func.conds[branch2].req(Sd, s);
	func.conds[branch2].req(Sc, Operand::intOf(0));
	func.conds[branch2].mem(ci, co);
	func.conds[branch2].ack({Ac, Ad});

	int branch3 = func.pushCond(exprAc & exprBc & (co != exprci));
	func.conds[branch3].req(Sd, s);
	func.conds[branch3].req(Sc, Operand::intOf(0));
	func.conds[branch3].mem(ci, co);

	int branch4 = func.pushCond(exprAc & exprBc & (co == exprci));
	func.conds[branch4].req(Sd, s);
	func.conds[branch4].req(Sc, Operand::intOf(1));
	func.conds[branch4].mem(ci, Operand::intOf(0));
	func.conds[branch0].ack({Ac, Ad, Bc, Bd});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "Sc_state <= 0;");
	EXPECT_SUBSTRING(verilog, "Sc_state <= 1;");
	EXPECT_SUBSTRING(verilog, "Sd_state <= (Ad_data+Bd_data+ci_data)%65536;");
	EXPECT_SUBSTRING(verilog, "ci_data <= (Ad_data+Bd_data+ci_data)/65536;");
}

TEST(ExportTest, Adder) {
	Func func;
	func.name = "adder";
	Operand A = func.pushNet("A", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand B = func.pushNet("B", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand Ci = func.pushNet("Ci", Type(Type::FIXED, 1), flow::Net::IN);
	Operand S = func.pushNet("S", Type(Type::FIXED, WIDTH + 1), flow::Net::OUT);
	Expression exprA(A);
	Expression exprB(B);
	Expression exprCi(Ci);

	int branch0 = func.pushCond(exprA & exprB & exprCi);
	func.conds[branch0].req(S, exprA + exprB + exprCi);
	func.conds[branch0].ack({A, B, Ci});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "S_state <= A_data+B_data+Ci_data;");
}
