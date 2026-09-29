#include "synthesize.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>

#include <arithmetic/algorithm.h>
#include <common/mapping.h>
#include <common/math.h>

using arithmetic::Expression;
using arithmetic::Operation;

namespace flow {

clocked::Type synthesizeChannelType(const Type &type) {
	clocked::Type result;
	if (type.type == flow::Type::TypeName::BITS) {
		result.type = clocked::Type::TypeName::BITS;
	} else if (type.type == flow::Type::TypeName::FIXED) {
		result.type = clocked::Type::TypeName::FIXED;
	}
	result.width = type.width;
	result.shift = type.shift;
	return result;
}

void resetValidReg(clocked::Statement &block, const clocked::Channel &chan) {
	block.sub.push_back(
		clocked::Statement(false, {
			clocked::Statement(chan.valid, Expression::intOf(0)),
		}, Expression::varOf(chan.ready))
	);
}


void synthesizeChannel(clocked::Module &mod, const Net &net, clocked::Statement &resetBlock) {
	static const clocked::Type wire(clocked::Type::TypeName::BITS, 1);
	clocked::Channel channel;
	if (net.purpose == flow::Net::IN) {
		channel.purpose = clocked::Channel::IN;
		channel.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::IN);
		channel.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::Purpose::OUT);
		channel.data = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::IN);

	} else if (net.purpose == flow::Net::OUT) {
		channel.purpose = clocked::Channel::OUT;
		size_t valid_wire = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::OUT);
		size_t valid_reg = mod.pushNet(net.name+"_valid_reg", clocked::Type(clocked::Type::TypeName::FIXED, 1), clocked::Net::Purpose::REG);
		channel.valid = valid_reg;
		mod.stmts.push_back(clocked::Statement(valid_wire, Expression::varOf(valid_reg), true));
		resetBlock.sub.push_back(clocked::Statement(channel.valid, Expression::intOf(0)));

		channel.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::Purpose::IN);

		size_t data = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::OUT);
		channel.data = mod.pushNet(net.name+"_data_reg", synthesizeChannelType(net.type), clocked::Net::Purpose::REG);
		mod.stmts.push_back(clocked::Statement(data, Expression::varOf(channel.data), true));
		resetBlock.sub.push_back(clocked::Statement(channel.data, Expression::intOf(0)));
		
	} else if (net.purpose == flow::Net::REG) {
		channel.purpose = clocked::Channel::REG;
		channel.valid = -1;  //mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::WIRE);
		channel.ready = -1;  //TODO: these could be wires for debug or mere modelling in cocotb harness
		channel.data = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::REG);
		resetBlock.sub.push_back(clocked::Statement(channel.data, Expression::intOf(0)));

	//TODO: migrate out of synthesizeChannel(), into synthesizeModuleFromFunc(), if/when COND's are no longer detected in netlist?
	} else if (net.purpose == flow::Net::COND) {
		channel.purpose = clocked::Channel::COND;
		channel.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::WIRE);
		channel.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::Purpose::WIRE);
		channel.data = -1;
	}
	mod.chans.push_back(channel);
}

arithmetic::RuleSet buildRules() {
	using namespace arithmetic;

	Expression a = Expression::varOf(0);
	Expression b = Expression::varOf(1);
	Expression c = Expression::varOf(2);
	Expression d = Expression::varOf(3);
	Expression U = Expression::U();

	// All of the simplification rules for Val-Data
	return RuleSet({
		memberCall(construct("ValData", {a, b}), "probe", {}) > construct("ValData", {a, b}), 
		member(construct("ValData", {a, b}), "val") > a,
		member(construct("ValData", {a, b}), "data") > b,
		(isValid(construct("ValData", {a, b}))) > a,
		(~construct("ValData", {a, b})) > (~a),
		(construct("ValData", {a, b}) & construct("ValData", {c, d})) > (a&c),
		(construct("ValData", {a, b}) | construct("ValData", {c, d})) > (a|c),
		(construct("ValData", {a, b}) & c) > (a&c),
		(construct("ValData", {a, b}) | c) > (a|c),
		(a & construct("ValData", {c, d})) > (a&c),
		(a | construct("ValData", {c, d})) > (a|c),

		(isTrue(construct("ValData", {a, b}))) > (a&&b),
		(!construct("ValData", {a, b})) > construct("ValData", {a, !b}),
		(construct("ValData", {a, b}) && construct("ValData", {c, d})) > construct("ValData", {a&c, b&&d}),
		(construct("ValData", {a, b}) || construct("ValData", {c, d})) > construct("ValData", {(a&&b)|(c&&d)|(a&c), (a&&b)||(c&&d)}),
		(booleanXor(construct("ValData", {a, b}), construct("ValData", {c, d}))) > construct("ValData", {a&c, booleanXor(b, d)}),

		(construct("ValData", {a, b}) + construct("ValData", {c, d})) > construct("ValData", {a&c, b+d}),
		(construct("ValData", {a, b}) - construct("ValData", {c, d})) > construct("ValData", {a&c, b-d}),
		(construct("ValData", {a, b}) * construct("ValData", {c, d})) > construct("ValData", {a&c, b*d}),
		(construct("ValData", {a, b}) / construct("ValData", {c, d})) > construct("ValData", {a&c, b/d}),
		(construct("ValData", {a, b}) % construct("ValData", {c, d})) > construct("ValData", {a&c, b%d}),
		
		(construct("ValData", {a, b}) == construct("ValData", {c, d})) > construct("ValData", {a&c, b==d}),
		(construct("ValData", {a, b}) != construct("ValData", {c, d})) > construct("ValData", {a&c, b!=d}),
		(construct("ValData", {a, b}) < construct("ValData", {c, d})) > construct("ValData", {a&c, b<d}),
		(construct("ValData", {a, b}) > construct("ValData", {c, d})) > construct("ValData", {a&c, b>d}),
		(construct("ValData", {a, b}) <= construct("ValData", {c, d})) > construct("ValData", {a&c, b<=d}),
		(construct("ValData", {a, b}) >= construct("ValData", {c, d})) > construct("ValData", {a&c, b>=d}),
	});
}

void minimizeTypes(const clocked::Module &mod, arithmetic::OperationSet expr) {
	using namespace arithmetic;

	for (const auto &idx : expr.exprIndex()) {
		arithmetic::Operation op = *expr.getExpr(idx.index);

		bool modified = false;
		if (op.func == arithmetic::Operation::CALL) {
			if (op.operands.empty()
				or not op.operands[0].isConst()
				or op.operands[0].cnst.type != Value::LABEL) {
				continue;
			}

			std::string term = op.operands[0].cnst.sval;

			// built-in functions
			if (term == "rand") {
				op.operands[0].cnst.sval = "random";
				modified = true;
			}
		} else if (op.func == arithmetic::Operation::VALIDITY) {
			if (op.operands.empty()) {
				continue;
			}

			if (op.operands[0].isVar()) {
				clocked::Type type = mod.nets[op.operands[0].index].type;

				if (type.type == clocked::Type::BITS and type.width == 1) {
					op.func = arithmetic::Operation::IDENTITY;
					modified = true;
				} else {
					op.func = arithmetic::Operation::IDENTITY;
					op.operands.clear();
					op.operands.push_back(Operand::vdd());
					modified = true;
				}
			}
		} else if (op.func == arithmetic::Operation::TRUTHINESS) {
			if (op.operands.empty()) {
				continue;
			}

			if (op.operands[0].isVar()) {
				clocked::Type type = mod.nets[op.operands[0].index].type;

				if ((type.type == clocked::Type::BITS and type.width == 1)
					or (type.type == clocked::Type::FIXED and type.width == 1)) {
					op.func = arithmetic::Operation::IDENTITY;
					modified = true;
				} else {
					op.func = arithmetic::Operation::NOT_EQUAL;
					op.operands.push_back(Operand::intOf(0));
					modified = true;
				}
			}
		} else if (op.func == arithmetic::Operation::CAST) {
			if (op.operands.size() < 2u
				or not op.operands[0].isConst()
				or op.operands[0].cnst.type != Value::LABEL) {
				continue;
			}

			std::string term = op.operands[0].cnst.sval;

			if (op.operands[1].isVar()) {
				clocked::Type type = mod.nets[op.operands[1].index].type;

				if (term == "wire") {
					if (type.type == clocked::Type::BITS and type.width == 1) {
						op.func = arithmetic::Operation::IDENTITY;
						op.operands.erase(op.operands.begin());
						modified = true;
					} else {
						op.func = arithmetic::Operation::IDENTITY;
						op.operands.clear();
						op.operands.push_back(Operand::vdd());
						modified = true;
					}
				} else if (term == "bool") {
					if ((type.type == clocked::Type::BITS and type.width == 1)
						or (type.type == clocked::Type::FIXED and type.width == 1)) {
						op.func = arithmetic::Operation::IDENTITY;
						op.operands.erase(op.operands.begin());
						modified = true;
					} else {
						op.func = arithmetic::Operation::NOT_EQUAL;
						op.operands.erase(op.operands.begin());
						op.operands.push_back(Operand::intOf(0));
						modified = true;
					}
				}
			}
		}

		if (modified) {
			expr.setExpr(op);
		}
	}
}

clocked::Module synthesizeModuleFromFunc(const Func &func, bool debug) {
	auto rules = buildRules();

	clocked::Module mod;
	mod.name = func.name;

	mod.clk = mod.pushNet("clk", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);
	mod.reset = mod.pushNet("reset", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);

	clocked::Trigger always(mod.getClk());
	always.stmts.push_back(clocked::Statement(false, {}, mod.getReset()));

	std::vector<size_t> nets;
	std::vector<Expression> structs;
	Expression constStruct = arithmetic::construct("ValData", {
		Expression::vdd(),
		Expression::U(),
	});
	for (size_t netIdx = 0; netIdx < func.nets.size(); netIdx++) {
		synthesizeChannel(mod, func.nets[netIdx], always.stmts[0]);

		// Map flow::Func nets to clocked::Channel nets
		nets.push_back(netIdx);
		structs.push_back(
			arithmetic::construct("ValData", {
				(mod.chans.back().hasValid() ?
					Expression::varOf(mod.chans.back().valid) :
					Expression::vdd()),
				Expression::varOf(mod.chans.back().data)
			})
		);
	}

	for (const auto &cond : func.conds) {
		clocked::Statement branch(true);

		for (const auto &reg : cond.regs) {
			Expression regExpr(reg.second);
			//cout << "reg from: " << regExpr.to_string(true) << endl;
			regExpr.substituteConst(constStruct);
			regExpr.substitute(nets, structs);
			regExpr = member(regExpr, "data");
			//cout << "reg from: " << regExpr.to_string(true) << endl;

			regExpr.minimize(rules);
			//cout << "reg from: " << regExpr.to_string(true) << endl;
			regExpr.minimize();
			minimizeTypes(mod, regExpr);
			regExpr.minimize();
			//cout << "reg to: " << regExpr.to_string(true) << endl;
			// TODO(edward.bingham) unwrap the structure, select the data
			branch.sub.push_back(
				clocked::Statement(mod.chans[reg.first].data, regExpr));
		}

		vector<int> branchOuts;
		for (const auto &output : cond.outs) {
			// TODO(edward.bingham) This assumes that there is a one to one mapping
			// between channels in the clocked module channels and the flow func
			// nets.
			branchOuts.push_back(output.first);

			// Assign to outputs
			Expression request(output.second);
			//cout << "out from: " << request.to_string(true) << endl;
			request.substituteConst(constStruct);
			request.substitute(nets, structs);
			request = member(request, "data");
			//cout << "out from: " << request.to_string(true) << endl;

			request.minimize(rules);
			//cout << "out from: " << request.to_string(true) << endl;
			request.minimize();
			minimizeTypes(mod, request);
			request.minimize();
			//cout << "out to: " << request.to_string(true) << endl;
			// TODO(edward.bingham) unwrap the structure, select the data
			branch.sub.push_back(
				clocked::Statement(mod.chans[output.first].data, request));

			// only when all output channels are ready to be written to
			// flow::Net::REG don't have valid/ready signals over channel
			if (mod.chans[output.first].hasValid()) {
				branch.sub.push_back(clocked::Statement(
					mod.chans[output.first].valid, Expression::vdd()));

				// flow::Net::REG don't have valid/ready signals over channel
				if (mod.chans[output.first].hasReady()) {
					branch.expr = branch.expr
						&& (!mod.chans[output.first].getValid() || mod.chans[output.first].getReady());
				}
			}
			// ...either they're open (!valid) or _will be_ open next cycle (ready)
		}

		for (int chanIdx = 0; chanIdx < (int)mod.chans.size(); chanIdx++) {
			if (mod.chans[chanIdx].purpose == clocked::Channel::OUT
				and find(branchOuts.begin(), branchOuts.end(), chanIdx) == branchOuts.end()) {
				resetValidReg(branch, mod.chans[chanIdx]);
			}
		}

		Expression branch_valid(arithmetic::isTrue(cond.valid));
		branch_valid.substituteConst(constStruct);
		branch_valid.substitute(nets, structs);

		branch_valid.minimize(rules);
		branch_valid.minimize();
		minimizeTypes(mod, branch_valid);
		branch_valid.minimize();

		branch.expr = mod.chans[cond.uid].getValid() && branch.expr;
		branch.expr.minimize();
		minimizeTypes(mod, branch.expr);
		branch.expr.minimize();


		always.stmts.push_back(branch);

		// Ensure only this branch executes until transaction is complete
		mod.stmts.push_back(clocked::Statement(mod.chans[cond.uid].valid, branch_valid, true));
		mod.stmts.push_back(clocked::Statement(mod.chans[cond.uid].ready, branch.expr, true));
	}

	// Return ready signals for each channel
	for (size_t netIdx = 0; netIdx < func.nets.size(); netIdx++) {
		if (func.nets[netIdx].purpose == flow::Net::IN) {
			Expression chan_ready = Expression::boolOf(false);
			for (const auto &cond : func.conds) {
				for (int input : cond.ins) {
					if (input == (int)netIdx) {
						chan_ready = chan_ready || mod.chans[cond.uid].getReady();
					}
				}
			}
			chan_ready.minimize();
			minimizeTypes(mod, chan_ready);
			chan_ready.minimize();
			mod.stmts.push_back(clocked::Statement(mod.chans[netIdx].ready, chan_ready, true));
		}
	}

	// When there's no input to process, we still need to reset the channels.
	clocked::Statement last(true, {}, Operand(true));
	for (const auto &chan : mod.chans) {
		if (chan.purpose == clocked::Channel::OUT) {
			resetValidReg(last, chan);
		}
	}
	always.stmts.push_back(last);

	// roll up the if statement so we don't have empty blocks
	while (not always.stmts.empty() and always.stmts.back().sub.empty()) {
		always.stmts.pop_back();
	}

	if (not always.stmts.empty()) {
		mod.triggers.push_back(always);
	}

	return mod;
}

}
