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

const size_t WIDTH = 4;
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
	parse_verilog::module_def mod_v = parse_verilog::export_module(mod);
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

	return mod_v;
}

TEST(ModuleSynthesis, Source) {
	Func func;
	func.name = "source";
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, Expression::intOf(1));  //TODO: send random int?

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, Sink) {
	Func func;
	func.name = "sink";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, Buffer) {
	Func func;
	func.name = "buffer";
	Operand L = func.pushNet("L", Type(Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func.pushNet("R", Type(Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);

	size_t branch0 = func.pushCond(Expression::boolOf(true));
	func.conds[branch0].req(R, exprL);
	func.conds[branch0].ack(L);

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, Copy) {
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

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, Func) {
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

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, Split) {
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

	string verilog = synthesizeVerilogFromFunc(func).to_string();
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

	size_t branch0 = func.pushCond(exprC == Expression::intOf(0));
	func.conds[branch0].req(R, exprL0);
	func.conds[branch0].ack({C, L0});

	size_t branch1 = func.pushCond(exprC == Expression::intOf(1));
	func.conds[branch1].req(R, exprL1);
	func.conds[branch1].ack({C, L1});

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, StreamingAdder) {
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

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

TEST(ModuleSynthesis, SerialAdder) {
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

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

auto get_channel_probe = [](arithmetic::Operand &operand) {
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
}

TEST(ModuleSynthesis, FullAdder) {
	using arithmetic::booleanXor;

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

	string verilog = synthesizeVerilogFromFunc(func).to_string();
}

