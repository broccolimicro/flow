#include <gtest/gtest.h>
#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

using namespace flow;
using arithmetic::Expression;
using arithmetic::Operand;

TEST(ExportTest, Source) {
	Func func;
	func.name = "source";
	Operand R = func.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond(Operand(true));
	
	func.conds[case0].req(R, Operand(0));

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Sink) {
	Func func;
	func.name = "sink";
	Operand L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	int case0 = func.pushCond(L);
	
	func.conds[case0].ack(L);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Buffer) {
	Func func;
	func.name = "buffer";
	Operand L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond(L);
	
	func.conds[case0].req(R, L);
	func.conds[case0].ack(L);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Copy) {
	Func func;
	func.name = "copy";
	Operand L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	Operand R0 = func.pushNet("R0", Type(Type::FIXED, 16), flow::Net::OUT);
	Operand R1 = func.pushNet("R1", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond(L);
	
	func.conds[case0].req(R0, L);
	func.conds[case0].req(R1, L);
	func.conds[case0].ack(L);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Add) {
	Func func;
	func.name = "add";
	Operand A = func.pushNet("A", Type(Type::FIXED, 16), flow::Net::IN);
	Operand B = func.pushNet("B", Type(Type::FIXED, 16), flow::Net::IN);
	Operand Ci = func.pushNet("Ci", Type(Type::FIXED, 1), flow::Net::IN);
	Operand S = func.pushNet("S", Type(Type::FIXED, 17), flow::Net::OUT);
	int case0 = func.pushCond(Expression(A) & B & Ci);
	
	func.conds[case0].req(S, Expression(A) + B + Ci);
	func.conds[case0].ack({A, B, Ci});

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Merge) {
	Func func;
	func.name = "merge";
	Operand A = func.pushNet("A", Type(Type::FIXED, 16), flow::Net::IN);
	Operand B = func.pushNet("B", Type(Type::FIXED, 16), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond((Expression(C) == 0) & isValid(A));
	int case1 = func.pushCond((Expression(C) == 1) & isValid(B));

	func.conds[case0].req(R, A);
	func.conds[case0].ack({C, A});

	func.conds[case1].req(R, B);
	func.conds[case1].ack({C, B});

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Split) {
	Func func;
	func.name = "split";
	Operand L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	Operand A = func.pushNet("A", Type(Type::FIXED, 16), flow::Net::OUT);
	Operand B = func.pushNet("B", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond((C == 0) & isValid(L));
	int case1 = func.pushCond((C == 1) & isValid(L));

	func.conds[case0].req(A, L);
	func.conds[case0].ack({C, L});

	func.conds[case1].req(B, L);
	func.conds[case1].ack({C, L});

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, DSAdder) {
	Func func;
	func.name = "digit-serial adder";
	int N = 4;  // Adder width

	Operand Ad = func.pushNet("Ad", Type(Type::FIXED, N), flow::Net::IN);
	Operand Ac = func.pushNet("Ac", Type(Type::FIXED, 1), flow::Net::IN);
	Operand Bd = func.pushNet("Bd", Type(Type::FIXED, N), flow::Net::IN);
	Operand Bc = func.pushNet("Bc", Type(Type::FIXED, 1), flow::Net::IN);
	Operand Sd  = func.pushNet("Sd",  Type(Type::FIXED, N), flow::Net::OUT);
	Operand Sc = func.pushNet("Sc", Type(Type::FIXED, 1), flow::Net::OUT);
	Operand ci = func.pushNet("ci", Type(Type::FIXED, 1), flow::Net::REG);
	Expression s((Expression(Ad) + Bd + ci) % pow(2, N));
	Expression co((Expression(Ad) + Bd + ci) / pow(2, N));

	int case0 = func.pushCond(~Expression(Ac) & ~Bc);
	func.conds[case0].req(Sd, s);
	func.conds[case0].req(Sc, Operand::intOf(0));
	func.conds[case0].mem(ci, co);
	func.conds[case0].ack({Ac, Ad, Bc, Bd});

	int case1 = func.pushCond(Expression(Ac) & ~Bc);
	func.conds[case1].req(Sd, s);
	func.conds[case1].req(Sc, Operand::intOf(0));
	func.conds[case1].mem(ci, co);
	func.conds[case1].ack({Bc, Bd});

	int case2 = func.pushCond(~Expression(Ac) & Bc);
	func.conds[case2].req(Sd, s);
	func.conds[case2].req(Sc, Operand::intOf(0));
	func.conds[case2].mem(ci, co);
	func.conds[case2].ack({Ac, Ad});

	int case3 = func.pushCond(Expression(Ac) & Bc & (co != ci));
	func.conds[case3].req(Sd, s);
	func.conds[case3].req(Sc, Operand::intOf(0));
	func.conds[case3].mem(ci, co);

	int case4 = func.pushCond(Expression(Ac) & Bc & (co == ci));
	func.conds[case4].req(Sd, s);
	func.conds[case4].req(Sc, Operand::intOf(1));
	func.conds[case4].mem(ci, Operand::intOf(0));
	func.conds[case0].ack({Ac, Ad, Bc, Bd});

	EXPECT_EQ(func.conds.size(), 5);

	//clocked::Module mod = synthesize_valrdy(func);
	//cout << export_module(mod).to_string() << endl;
}
