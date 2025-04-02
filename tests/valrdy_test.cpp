#include <gtest/gtest.h>
#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

using namespace flow;
using namespace arithmetic;

TEST(ExportTest, Source) {
	Func func;
	func.name = "source";
	int R = func.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond(true);
	
	func.conds[case0].outs.push_back({R, value(0)});

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Sink) {
	Func func;
	func.name = "sink";
	int L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	int case0 = func.pushCond(operand(L, operand::variable));
	
	func.conds[case0].ins.push_back(L);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Buffer) {
	Func func;
	func.name = "buffer";
	int L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	int R = func.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond(operand(L, operand::variable));
	
	func.conds[case0].outs.push_back({R, operand(L, operand::variable)});
	func.conds[case0].ins.push_back(L);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Copy) {
	Func func;
	func.name = "copy";
	int L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	int R0 = func.pushNet("R0", Type(Type::FIXED, 16), flow::Net::OUT);
	int R1 = func.pushNet("R1", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond(operand(L, operand::variable));
	
	func.conds[case0].outs.push_back({R0, operand(L, operand::variable)});
	func.conds[case0].outs.push_back({R1, operand(L, operand::variable)});
	func.conds[case0].ins.push_back(L);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Add) {
	Func func;
	func.name = "add";
	int A = func.pushNet("A", Type(Type::FIXED, 16), flow::Net::IN);
	int B = func.pushNet("B", Type(Type::FIXED, 16), flow::Net::IN);
	int Ci = func.pushNet("Ci", Type(Type::FIXED, 1), flow::Net::IN);
	int S = func.pushNet("S", Type(Type::FIXED, 17), flow::Net::OUT);
	int case0 = func.pushCond(operand(A, operand::variable)&operand(B, operand::variable)&operand(Ci, operand::variable));
	
	func.conds[case0].outs.push_back({S, operand(A, operand::variable)+operand(B, operand::variable)+operand(Ci, operand::variable)});
	func.conds[case0].ins.push_back(A);
	func.conds[case0].ins.push_back(B);
	func.conds[case0].ins.push_back(Ci);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Merge) {
	Func func;
	func.name = "merge";
	int A = func.pushNet("A", Type(Type::FIXED, 16), flow::Net::IN);
	int B = func.pushNet("B", Type(Type::FIXED, 16), flow::Net::IN);
	int C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	int R = func.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond((operand(C, operand::variable) == 0) & is_valid(operand(A, operand::variable)));
	int case1 = func.pushCond((operand(C, operand::variable) == 1) & is_valid(operand(B, operand::variable)));

	func.conds[case0].outs.push_back({R, operand(A, operand::variable)});
	func.conds[case0].ins.push_back(A);
	func.conds[case0].ins.push_back(C);

	func.conds[case1].outs.push_back({R, operand(B, operand::variable)});
	func.conds[case1].ins.push_back(B);
	func.conds[case1].ins.push_back(C);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}

TEST(ExportTest, Split) {
	Func func;
	func.name = "split";
	int L = func.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	int C = func.pushNet("C", Type(Type::FIXED, 1), flow::Net::IN);
	int A = func.pushNet("A", Type(Type::FIXED, 16), flow::Net::OUT);
	int B = func.pushNet("B", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = func.pushCond((operand(C, operand::variable) == 0) & is_valid(operand(L, operand::variable)));
	int case1 = func.pushCond((operand(C, operand::variable) == 1) & is_valid(operand(L, operand::variable)));

	func.conds[case0].outs.push_back({A, operand(L, operand::variable)});
	func.conds[case0].ins.push_back(L);
	func.conds[case0].ins.push_back(C);

	func.conds[case1].outs.push_back({B, operand(L, operand::variable)});
	func.conds[case1].ins.push_back(L);
	func.conds[case1].ins.push_back(C);

	clocked::Module mod = synthesize_valrdy(func);
	cout << export_module(mod).to_string() << endl;
}
