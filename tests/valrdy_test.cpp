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
	int case0 = func.pushCond(A & B & Ci);
	
	func.conds[case0].req(S, A + B + Ci);
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
	int case0 = func.pushCond((C == 0) & isValid(A));
	int case1 = func.pushCond((C == 1) & isValid(B));

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
