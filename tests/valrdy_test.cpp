#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

using namespace flow;
using arithmetic::Expression;
using arithmetic::Operand;
using std::filesystem::absolute;
using std::filesystem::current_path;

const int WIDTH = 16;
const std::filesystem::path PROJECT_DIR = absolute(current_path() / "tests");


int exportFuncVerilog(const Func &func) {
	clocked::Module mod = synthesize_valrdy(func);
	parse_verilog::module_def mod_v = export_module(mod);
	string exported_verilog = mod_v.to_string();

	cout << exported_verilog << endl;
	string export_filename = (PROJECT_DIR / (func.name + ".v")).string();
	std::ofstream export_file(export_filename);
	if (!export_file) {
			std::cerr << "Failed to open file for verilog export: " << export_filename << std::endl;
			return 1;
	}
	export_file << exported_verilog;
	return 0;
}

TEST(ExportTest, Source) {
	Func func;
	func.name = "source";
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, Expression::intOf(1));  //TODO: send random int?

	EXPECT_EQ(exportFuncVerilog(func), 0);
}

TEST(ExportTest, Sink) {
	Func func;
	func.name = "sink";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].ack(L);

	EXPECT_EQ(exportFuncVerilog(func), 0);
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

	EXPECT_EQ(exportFuncVerilog(func), 0);
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

	EXPECT_EQ(exportFuncVerilog(func), 0);
}

TEST(ExportTest, Func) {
	Func func;
	func.name = "func";
	Operand L0 = func.pushNet("L0", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);

	//TODO: internal mem required??
	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL0 | exprL1);
	func.conds[branch0].ack({L0, L1});

	EXPECT_EQ(exportFuncVerilog(func), 0);
}

TEST(ExportTest, Merge) {
	Func func;
	func.name = "merge";
	Operand A = func.pushNet("A", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand B = func.pushNet("B", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprA(A);
	Expression exprB(B);
	Expression exprC(C);

	int branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(R, exprA);
	func.conds[branch0].ack({C, A});

	int branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(R, exprB);
	func.conds[branch1].ack({C, B});

	EXPECT_EQ(exportFuncVerilog(func), 0);
}

TEST(ExportTest, Split) {
	Func func;
	func.name = "split";
	Operand L = func.pushNet("L", Type(Type::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	Operand A = func.pushNet("A", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Operand B = func.pushNet("B", Type(Type::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);
	Expression exprC(C);

	int branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(A, exprL);
	func.conds[branch0].ack({C, L});

	int branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(B, exprL);
	func.conds[branch1].ack({C, L});

	EXPECT_EQ(exportFuncVerilog(func), 0);
}

/*
TEST(ExportTest, DSAdder) {
	Func func;
	func.name = "ds_adder";
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

	EXPECT_EQ(exportFuncVerilog(func), 0);
}

TEST(ExportTest, Add) {
	Func func;
	func.name = "add";
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

	EXPECT_EQ(exportFuncVerilog(func), 0);
}
*/
