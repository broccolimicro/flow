#include <bit>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <common/mapping.h>
#include <common/mock_netlist.h>
#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export_dot.h>
#include <interpret_flow/export_verilog.h>

#define EXPECT_SUBSTRING(source, substring) \
    EXPECT_NE((source).find(substring), std::string::npos) \
		<< "ERROR: Expected Verilog to contain substring \"" << (substring) << "\""
#define EXPECT_NO_SUBSTRING(source, substring) \
    EXPECT_EQ((source).find(substring), std::string::npos) \
		<< "ERROR: Expected Verilog to NOT contain substring \"" << (substring) << "\""

using std::filesystem::absolute;
using std::filesystem::current_path;

using arithmetic::Expression;
using arithmetic::Operation;
using arithmetic::Operand;
using namespace flow;

const size_t WIDTH = 16;
const std::filesystem::path TEST_DIR = absolute(current_path() / "tests");


parse_verilog::module_def synthesizeVerilogFromFunc(const Func &func) {
	string filenameWithoutExtension = (TEST_DIR / func.name).string();
	// Render flow diagram
	{
		string graphvizRaw = flow::export_func(func, false).to_string();

		string flow_filename = filenameWithoutExtension + ".dot";
		std::ofstream export_file(flow_filename);
		if (!export_file) {
			std::cerr << "ERROR: Failed to open file for dot export: "
				<< flow_filename << std::endl
				<< "ERROR: Try again from dir: <project_dir>/lib/flow" << std::endl;
			//TODO: we want soft failure, but this doesn't break or prevent file writing
		}
		export_file << graphvizRaw;
		//gvdot::render(filenameWithoutExtension + ".png", graphvizRaw);
	}

	// Test synthesis
	clocked::Module mod = synthesizeModuleFromFunc(func);
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
	string verilog_filename = filenameWithoutExtension + ".v";
	EXPECT_NO_THROW(write_verilog_file(verilog, verilog_filename));

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

TEST(ModuleSynthesis, Source) {
	Func func;
	func.name = "source";
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, Expression::intOf(1));  //TODO: send random int?

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= 1");
}

TEST(ModuleSynthesis, Sink) {
	Func func;
	func.name = "sink";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, Buffer) {
	Func func;
	func.name = "buffer";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL);
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L_data;");
}

TEST(ModuleSynthesis, Copy) {
	Func func;
	func.name = "copy";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R0 = func.pushNet("R0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func.pushNet("R1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R0, exprL);
	func.conds[branch0].req(R1, exprL);
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R0_state <= L_data;");
	EXPECT_SUBSTRING(verilog, "R1_state <= L_data;");
}

TEST(ModuleSynthesis, Func) {
	Func func;
	func.name = "func";
	Operand L0 = func.pushNet("L0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
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

TEST(ModuleSynthesis, Split) {
	Func func;
	func.name = "split";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Operand R0 = func.pushNet("R0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func.pushNet("R1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
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

TEST(ModuleSynthesis, Merge) {
	Func func;
	func.name = "merge";
	Operand L0 = func.pushNet("L0", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func.pushNet("L1", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand C = func.pushNet("C", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);
	Expression exprC(C);

	int branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(R, exprL0);
	func.conds[branch0].ack({C, L0});

	int branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(R, exprL1);
	func.conds[branch1].ack({C, L1});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L0_data;");
	EXPECT_SUBSTRING(verilog, "R_state <= L1_data;");
}

TEST(ModuleSynthesis, StreamingAdder) {
	Func func;
	func.name = "s_adder";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand m = func.pushNet("m", Type(Type::TypeName::FIXED, WIDTH), flow::Net::REG);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
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

TEST(ModuleSynthesis, DSAdderFlat) {
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

	Expression expr_s((expr_Ad + expr_Bd + expr_ci) % pow(2, WIDTH));
	Expression expr_co((expr_Ad + expr_Bd + expr_ci) / pow(2, WIDTH));

	int branch0 = func.pushCond(~expr_Ac & ~expr_Bc);
	func.conds[branch0].req(Sd, expr_s);
	func.conds[branch0].req(Sc, Expression::intOf(0));
	func.conds[branch0].mem(ci, expr_co);
	func.conds[branch0].ack({Ac, Ad, Bc, Bd});

	int branch1 = func.pushCond(expr_Ac & ~expr_Bc);
	func.conds[branch1].req(Sd, expr_s);
	func.conds[branch1].req(Sc, Expression::intOf(0));
	func.conds[branch1].mem(ci, expr_co);
	func.conds[branch1].ack({Bc, Bd});

	int branch2 = func.pushCond(~expr_Ac & expr_Bc);
	func.conds[branch2].req(Sd, expr_s);
	func.conds[branch2].req(Sc, Expression::intOf(0));
	func.conds[branch2].mem(ci, expr_co);
	func.conds[branch2].ack({Ac, Ad});

	int branch3 = func.pushCond(expr_Ac & expr_Bc & (expr_co != expr_ci));
	func.conds[branch3].req(Sd, expr_s);
	func.conds[branch3].req(Sc, Expression::intOf(0));
	func.conds[branch3].mem(ci, expr_co);

	int branch4 = func.pushCond(expr_Ac & expr_Bc & (expr_co == expr_ci));
	func.conds[branch4].req(Sd, expr_s);
	func.conds[branch4].req(Sc, Expression::intOf(1));
	func.conds[branch4].mem(ci, Expression::intOf(0));
	func.conds[branch4].ack({Ac, Ad, Bc, Bd});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "Sc_state <= 0;");
	EXPECT_SUBSTRING(verilog, "Sc_state <= 1;");
	EXPECT_SUBSTRING(verilog, "Sd_state <= (Ad_data+Bd_data+ci_data)%65536;");
	EXPECT_SUBSTRING(verilog, "ci_data <= (Ad_data+Bd_data+ci_data)/65536;");
}

/*
TEST(ModuleSynthesis, FullAdder) {
	Func func;
	func.name = "full_adder";
	Operand A = func.pushNet("A", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand B = func.pushNet("B", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand S = func.pushNet("S", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand Ci = func.pushNet("Ci", Type(Type::TypeName::FIXED, 1), flow::Net::IN);
	Operand Co = func.pushNet("Co", Type(Type::TypeName::FIXED, 1), flow::Net::OUT);
	Expression exprA(A);
	Expression exprB(B);
	Expression exprCi(Ci);

	int branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(S, bitwiseXor(bitwiseXor(exprA, exprB), exprCi));
	func.conds[branch0].req(Co, (exprA && exprB) + (exprCi && bitwiseXor(exprA, exprB)));
	func.conds[branch0].ack({A, B, Ci});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
	EXPECT_SUBSTRING(verilog, "S_state <= A_data+B_data+Ci_data;");
}
*/

//auto get_channel_probe = [](arithmetic::Operand &operand) {
//	vector<arithmetic::Operand> probe_args = { arithmetic::Operand::stringOf("probe"), operand };
//	return Expression(arithmetic::Operation::CALL, probe_args);
//};





TEST(ModuleSynthesis, ChannelProbes) {
	MockNetlist fn;
	Operand A = Operand::varOf(fn.netIndex("A", true));
	Operand B = Operand::varOf(fn.netIndex("B", true));
	Operand x = Operand::varOf(fn.netIndex("x", true));  // non-channel local var/reg
	Expression expr_A(A);
	Expression expr_B(B);
	Expression expr_x(x);

	MockNetlist mod;
	mod.netIndex("_random_offset", true);  // For verification, ensure fn->mod aren't 1:1
	Operand A_valid = Operand::varOf(mod.netIndex("A_valid", true));
	Operand B_valid = Operand::varOf(mod.netIndex("B_valid", true));
	Operand A_data = Operand::varOf(mod.netIndex("A_data", true));
	Operand B_data = Operand::varOf(mod.netIndex("B_data", true));
	Operand x_data = Operand::varOf(mod.netIndex("x", true));  // non-channel local var
	Expression expr_A_valid(A_valid);
	Expression expr_B_valid(B_valid);
	Expression expr_A_data(A_data);
	Expression expr_B_data(B_data);
	Expression expr_x_data(x_data);

	Mapping<int> ChannelValid(-1, true), ChannelData(-1, true);
	ChannelValid.set(A.index, A_valid.index);
	ChannelValid.set(B.index, B_valid.index);
	ChannelData.set(A.index, A_data.index);
	ChannelData.set(B.index, B_data.index);
	ChannelData.set(x.index, x_data.index);

	Expression probe_A = arithmetic::call("probe", {A});
	Expression probe_B = arithmetic::call("probe", {B});

	// Base case
	Expression valid_before = probe_A;
	Expression valid_target = expr_A_valid;
	valid_target.minimize();
	Expression valid_after = synthesizeExpressionProbes(valid_before, ChannelValid, ChannelData);

	EXPECT_TRUE(areSame(valid_after, valid_target))
		<< endl << "BEFORE: " << valid_before
		<< endl << "AFTER: " << valid_after
		<< endl << "TARGET: " << valid_target;

	Expression zero = Expression::intOf(0);
	Expression data_before = (probe_A == zero);
	Expression data_target = expr_A_valid & (expr_A_data == zero);
	data_target.minimize();
	Expression data_after = synthesizeExpressionProbes(data_before, ChannelValid, ChannelData);

	EXPECT_TRUE(areSame(data_after, data_target))
		<< endl << "BEFORE: " << data_before
		<< endl << "AFTER: " << data_after
		<< endl << "TARGET: " << data_target;


	//// Masked Addition
	//// "Keep A’s lower byte[-ish] if it’s real, then increment."
	// (A & 0xF7) + 1
	Expression f7 = Expression::intOf(0xf7);
	Expression one = Expression::intOf(1);
	Expression ma_before = (probe_A && f7) + one;
	Expression ma_target = (expr_A_valid & (expr_A_data && f7)) + one;
	ma_target.minimize();
	Expression ma_after = synthesizeExpressionProbes(ma_before, ChannelValid, ChannelData);

	EXPECT_TRUE(areSame(ma_after, ma_target))
		<< endl << "BEFORE: " << ma_before
		<< endl << "AFTER: " << ma_after
		<< endl << "TARGET: " << ma_target;

	//// Bitwise Inversion
	//// "Negate only the known."
	// ~(A | B)
	Expression bi_before = ~(probe_A | probe_B);
	Expression bi_target = ~(expr_A_valid & expr_B_valid & (expr_A_data | expr_B_data));
	bi_target.minimize();
	Expression bi_after = synthesizeExpressionProbes(bi_before, ChannelValid, ChannelData);

	EXPECT_TRUE(areSame(bi_after, bi_target))
		<< endl << "BEFORE: " << bi_before
		<< endl << "AFTER: " << bi_after
		<< endl << "TARGET: " << bi_target;

	//// Deep Boolean, Top Arithmetic
	//// "The logic part is fragile; Z is solid."
	// ((A < 3) && (B > 2)) + B
	Expression dbta_before = ((probe_A < 3) && (probe_B > 2)) + probe_B;
	//Expression dbta_target = ((expr_A_valid && (expr_A_data < 3)) && (expr_B_valid && (expr_B_data > 2))) + expr_B_valid;
	Expression dbta_target = expr_A_valid & expr_B_valid & (((expr_A_data < 3) && (expr_B_data > 2)) + expr_B_data);
	dbta_target.minimize();
	Expression dbta_after = synthesizeExpressionProbes(dbta_before, ChannelValid, ChannelData);

	EXPECT_TRUE(areSame(dbta_after, dbta_target))
		<< endl << "BEFORE: " << dbta_before
		<< endl << "AFTER: " << dbta_after
		<< endl << "TARGET: " << dbta_target;

	//TODO:
	//// Indexing Time
	//// "The array is stable, but not your index math."
	// arr[B + 2]
	//Expression it_before = expr_x[probe_A + 2];  //arithmetic::evaluate()
	//Expression it_target = expr_x[(expr_A_valid && expr_A_data) + 2];
	//it_target.minimize();
	//Expression it_after = synthesizeExpressionProbes(it_before, ChannelValid, ChannelData);

	//EXPECT_TRUE(areSame(it_after, it_target))
	//	<< endl << "BEFORE: " << it_before
	//	<< endl << "TARGET: " << it_target
	//	<< endl << "AFTER: " << it_after;

	//// Return of the DAG
	//// "Compute once, reuse — but only if it was valid."
	// T = (A + B);
	// U = (T * 2) + (T >> 1);
}


//TEST(ModuleSynthesis, ChannelProbes2) {
//	MockNetlist fn;
//	Operand A = Operand::varOf(fn.netIndex("A", true));
//	Operand B = Operand::varOf(fn.netIndex("B", true));
//	Operand C = Operand::varOf(fn.netIndex("C", true));
//	Operand x = Operand::varOf(fn.netIndex("x", true));
//	Expression expr_A(A);
//	Expression expr_B(B);
//	Expression expr_C(C);
//	Expression expr_x(x);
//
//	MockNetlist mod;
//	mod.netIndex("_random_offset", true);  // For verification, ensure fn->mod aren't 1:1
//	Operand A_valid = Operand::varOf(mod.netIndex("A_valid", true));
//	Operand B_valid = Operand::varOf(mod.netIndex("B_valid", true));
//	Operand C_valid = Operand::varOf(mod.netIndex("C_valid", true));
//	Operand A_data = Operand::varOf(mod.netIndex("A_data", true));
//	Operand B_data = Operand::varOf(mod.netIndex("B_data", true));
//	Operand C_data = Operand::varOf(mod.netIndex("C_data", true));
//	Operand x_data = Operand::varOf(mod.netIndex("x", true));  //TODO: x_data?
//	Expression expr_A_valid(A_valid);
//	Expression expr_B_valid(B_valid);
//	Expression expr_C_valid(C_valid);
//	Expression expr_A_data(A_data);
//	Expression expr_B_data(B_data);
//	Expression expr_C_data(C_data);
//	Expression expr_x_data(A_data);
//
//	mapping ChannelValid, ChannelData;
//	ChannelValid.set(A.index, A_valid.index);
//	ChannelValid.set(B.index, B_valid.index);
//	ChannelValid.set(C.index, C_valid.index);
//	ChannelData.set(A.index, A_data.index);
//	ChannelData.set(B.index, B_data.index);
//	ChannelData.set(C.index, C_data.index);
//	ChannelData.set(x.index, x_data.index);
//
//	Expression probe_A = arithmetic::call("probe", {A});
//	Expression probe_B = arithmetic::call("probe", {B});
//	Expression probe_C = arithmetic::call("probe", {C});
//	Expression three = Expression::intOf(3);
//	Expression six = Expression::intOf(6);
//
//	Expression target = ((expr_B_valid || (expr_A_valid && (expr_A_data == three)) || (expr_x_data != six))) && expr_C_valid;
//	Expression before = (probe_B || (probe_A == three) || (expr_x != six)) && probe_C;
//	Expression after = synthesizeExpressionProbes(before, ChannelValid, ChannelData);
//
//	target.minimize();
//	EXPECT_TRUE(areSame(after, target))
//		<< endl << "BEFORE: " << before
//		<< endl << "TARGET: " << target
//		<< endl << "AFTER: " << after;
//}


