#include <gtest/gtest.h>
#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

using namespace flow;
using namespace arithmetic;

TEST(ExportTest, Buffer) {
	Func buffer;
	buffer.name = "buffer";
	int L = buffer.pushNet("L", Type(Type::FIXED, 16), flow::Net::IN);
	int R = buffer.pushNet("R", Type(Type::FIXED, 16), flow::Net::OUT);
	int case0 = buffer.pushCond();
	
	buffer.nets[case0].active = operand(L, operand::variable);
	buffer.nets[case0].value  = operand(R, operand::variable);

	// If we send on R, then we receive on L
	buffer.nets[L].active = operand(case0, operand::variable);
	buffer.nets[L].value = true;

	// We should forward the value on L to R
	buffer.nets[R].active = operand(case0, operand::variable);
	buffer.nets[R].value = operand(L, operand::variable);

	clocked::Module mod = synthesize_valrdy(buffer);

	cout << export_module(mod).to_string() << endl;
}


