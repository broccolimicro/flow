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

Mapping<int> mapToVerilog(const flow::Func &func, const clocked::Module &mod) {
	Mapping<int> result(-1, false);
	for (int i = 0; i < (int)mod.chans.size(); i++) {
		int uid = func.netIndex(mod.chans[i].name);
		if (uid >= 0) {
			result.set(uid, i);
		}
	}
	return result;
}

Implementation::Implementation() {
	func = nullptr;
	mod = nullptr;
}

Implementation::Implementation(const flow::Func *func, clocked::Module *mod) {
	this->func = func;
	this->mod = mod;
	if (func != nullptr and mod != nullptr) {
		this->funcToMod = mapToVerilog(*func, *mod);
	}
}

Implementation::~Implementation() {
}

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
			clocked::Statement(chan.validReg, Expression::intOf(0)),
		}, Expression::varOf(chan.ready))
	);
}

void synthesizeChannel(clocked::Module &mod, const Net &net, clocked::Statement &resetBlock) {
	static const clocked::Type wireType(clocked::Type::TypeName::BITS, 1);
	static const clocked::Type boolType(clocked::Type::TypeName::FIXED, 1);

	size_t uid = mod.chans.size();
	mod.chans.push_back(clocked::Channel());

	clocked::Channel channel(net.name);
	if (net.purpose == flow::Net::IN) {
		channel.purpose = clocked::Channel::IN;
		channel.valid = mod.pushNet(net.name+"_valid", wireType, clocked::Net::Purpose::IN, uid);
		channel.ready = mod.pushNet(net.name+"_ready", wireType, clocked::Net::Purpose::OUT, uid);
		channel.data  = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::IN, uid);
	} else if (net.purpose == flow::Net::OUT) {
		channel.purpose = clocked::Channel::OUT;
		channel.valid    = mod.pushNet(net.name+"_valid", wireType, clocked::Net::Purpose::OUT, uid);
		channel.validReg = mod.pushNet(net.name+"_valid_reg", boolType, clocked::Net::Purpose::REG, uid);
		channel.ready    = mod.pushNet(net.name+"_ready", wireType, clocked::Net::Purpose::IN, uid);
		channel.data     = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::OUT, uid);
		channel.dataReg  = mod.pushNet(net.name+"_data_reg", synthesizeChannelType(net.type), clocked::Net::Purpose::REG, uid);

		mod.stmts.push_back(clocked::Statement(channel.valid, Expression::varOf(channel.validReg), true));
		mod.stmts.push_back(clocked::Statement(channel.data, Expression::varOf(channel.dataReg), true));
		resetBlock.sub.push_back(clocked::Statement(channel.validReg, Expression::intOf(0)));
		resetBlock.sub.push_back(clocked::Statement(channel.dataReg, Expression::intOf(0)));
	} else if (net.purpose == flow::Net::REG) {
		channel.purpose = clocked::Channel::REG;
		channel.dataReg = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::REG, uid);
		channel.data    = channel.dataReg;

		// TODO(steven.kneiser) valid and ready could be wires for debug or mere modelling in cocotb harness
		// channel.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::WIRE);

		resetBlock.sub.push_back(clocked::Statement(channel.dataReg, Expression::intOf(0)));
	} else if (net.purpose == flow::Net::WIRE) {
		channel.purpose = clocked::Channel::WIRE;
		channel.data  = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::WIRE, uid);

		// TODO(steven.kneiser) these could be wires for debug or mere modelling in cocotb harness
		// channel.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::WIRE);
		// TODO(steven.kneiser) migrate out of synthesizeChannel(), into
		// synthesizeModuleFromFunc(), if/when COND's are no longer detected in netlist?
	} else if (net.purpose == flow::Net::COND) {
		channel.purpose = clocked::Channel::COND;
		channel.valid = mod.pushNet(net.name+"_valid", wireType, clocked::Net::Purpose::WIRE, uid);
		channel.ready = mod.pushNet(net.name+"_ready", wireType, clocked::Net::Purpose::WIRE, uid);
		channel.data  = -1;
	}
	mod.chans[uid] = channel;
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
		memberCall(construct("Bundle", {a, b}), "probe", {}) > construct("Bundle", {a, b}), 
		member(construct("Bundle", {a, b}), "valid") > a,
		member(construct("Bundle", {a, b}), "data") > b,
		(isValid(construct("Bundle", {a, b}))) > a,
		(~construct("Bundle", {a, b})) > (~a),
		(construct("Bundle", {a, b}) & construct("Bundle", {c, d})) > (a&c),
		(construct("Bundle", {a, b}) | construct("Bundle", {c, d})) > (a|c),
		(construct("Bundle", {a, b}) & c) > (a&c),
		(construct("Bundle", {a, b}) | c) > (a|c),
		(a & construct("Bundle", {c, d})) > (a&c),
		(a | construct("Bundle", {c, d})) > (a|c),

		(isTrue(construct("Bundle", {a, b}))) > (a&&b),
		(!construct("Bundle", {a, b})) > construct("Bundle", {a, !b}),
		(construct("Bundle", {a, b}) && construct("Bundle", {c, d})) > construct("Bundle", {a&c, b&&d}),
		(construct("Bundle", {a, b}) || construct("Bundle", {c, d})) > construct("Bundle", {(a&&b)|(c&&d)|(a&c), (a&&b)||(c&&d)}),
		(booleanXor(construct("Bundle", {a, b}), construct("Bundle", {c, d}))) > construct("Bundle", {a&c, booleanXor(b, d)}),

		(construct("Bundle", {a, b}) + construct("Bundle", {c, d})) > construct("Bundle", {a&c, b+d}),
		(construct("Bundle", {a, b}) - construct("Bundle", {c, d})) > construct("Bundle", {a&c, b-d}),
		(construct("Bundle", {a, b}) * construct("Bundle", {c, d})) > construct("Bundle", {a&c, b*d}),
		(construct("Bundle", {a, b}) / construct("Bundle", {c, d})) > construct("Bundle", {a&c, b/d}),
		(construct("Bundle", {a, b}) % construct("Bundle", {c, d})) > construct("Bundle", {a&c, b%d}),

		(isNegative(construct("Bundle", {a, b}))) > construct("Bundle", {a, isNegative(b)}),
		(construct("Bundle", {a, b}) == construct("Bundle", {c, d})) > construct("Bundle", {a&c, b==d}),
		(construct("Bundle", {a, b}) != construct("Bundle", {c, d})) > construct("Bundle", {a&c, b!=d}),
		(construct("Bundle", {a, b}) < construct("Bundle", {c, d})) > construct("Bundle", {a&c, b<d}),
		(construct("Bundle", {a, b}) > construct("Bundle", {c, d})) > construct("Bundle", {a&c, b>d}),
		(construct("Bundle", {a, b}) <= construct("Bundle", {c, d})) > construct("Bundle", {a&c, b<=d}),
		(construct("Bundle", {a, b}) >= construct("Bundle", {c, d})) > construct("Bundle", {a&c, b>=d}),
	});
}

std::string newInstanceName(const clocked::Module *mod, std::string prefix) {
	size_t i = 0;
	while (true) {
		std::string name = prefix + ::to_string(i);
		bool found = false;
		for (const auto &inst : mod->inst) {
			if (inst.name == name) {
				found = true;
				break;
			}
		}
		if (not found) {
			return name;
		}
		i++;
	}
}

std::string newNetName(const clocked::Module *mod, std::string prefix) {
	size_t i = 0;
	while (true) {
		std::string name = prefix + ::to_string(i);
		bool found = false;
		for (const auto &net : mod->nets) {
			if (net.name == name) {
				found = true;
				break;
			}
		}
		if (not found) {
			return name;
		}
		i++;
	}
}

Mapping<arithmetic::Operand> minimizeTypes(clocked::Module *mod, Linker *linker, arithmetic::OperationSet expr, std::vector<arithmetic::Operand> top) {
	using namespace arithmetic;

	static const arithmetic::RuleSet defaultRules = arithmetic::rewriteCanonical() + arithmetic::rewriteSimple();
	static const arithmetic::RuleSet rewriteRules = defaultRules + buildRules();
	auto typeMinimizer = std::bind(minimizeTypes, &mod, linker, std::placeholders::_1, std::placeholders::_2);


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
			} else {
				// TODO(edward.bingham) Need a function to propagate type information through an expression
				vector<Expression> flowPorts;
				for (size_t i = 1; i < op.operands.size(); i++) {
					flowPorts.push_back(arithmetic::subExpr(expr, op.operands[i]));
				}
				Implementation impl = linker->find(Instance(term, flowPorts));

				if (impl.mod == nullptr or impl.func == nullptr) {
					error("", "unable to link flow to verilog for \'" + term + "\'", __FILE__, __LINE__);
					continue;
				}

				Mapping<size_t> connect;
				std::vector<Expression> clockedPorts;
				for (size_t i = 0; i < impl.mod->nets.size(); i++) {
					bool isInput = impl.mod->nets[i].purpose == clocked::Net::IN;
					bool isOutput = impl.mod->nets[i].purpose == clocked::Net::OUT;

					if (isInput or isOutput) {
						int clockedChan = impl.mod->nets[i].chan;
						std::string netName = impl.mod->nets[i].name;
						if (clockedChan >= 0) {
							netName = impl.mod->chans[clockedChan].name;
						}

						int flowNet = -1;
						int flowPort = -1;
						for (size_t j = 0; j < impl.func->nets.size(); j++) {
							if (impl.func->nets[j].purpose == flow::Net::IN or
								impl.func->nets[j].purpose == flow::Net::OUT) {
								flowPort++;
							}
							if (impl.func->nets[j].name == netName) {
								flowNet = j;
								break;
							}
						}

						if (flowNet < 0) {
							error("", "unable to link port in verilog instantiation of flow function \'" + netName + "\'", __FILE__, __LINE__);
							clockedPorts.push_back(Expression::undef());
							continue;
						}

						Expression flowOp = Expression::undef();
						if (flowPort >= 0 and flowPort < flowPorts.size()) {
							flowOp = flowPorts[flowPort];
						}
						if (isOutput) {
							size_t net = mod->pushNet(
								newNetName(mod, "net"),
								impl.mod->nets[i].type,
								clocked::Net::Purpose::WIRE);

							connect.set(i, net);
							clockedPorts.push_back(Expression::varOf(net));
						} else if (clockedChan < 0) {
							clockedPorts.push_back(flowOp);
						} else if (i == impl.mod->chans[clockedChan].valid) {
							clockedPorts.push_back(arithmetic::member(flowOp, "valid"));
						} else if (i == impl.mod->chans[clockedChan].ready) {
							clockedPorts.push_back(arithmetic::member(flowOp, "ready"));
						} else if (i == impl.mod->chans[clockedChan].data) {
							clockedPorts.push_back(arithmetic::member(flowOp, "data"));
						} else {
							internal("", "improperly linked net \'" + netName + "\' in clocked module", __FILE__, __LINE__);
							clockedPorts.push_back(flowOp);
						}

						//cout << "before port " << clockedPorts.size()-1 << " " << clockedPorts.back().to_string(true, *mod) << endl;
						clockedPorts.back().minimize(rewriteRules);
						//cout << "after port " << clockedPorts.size()-1 << " " << clockedPorts.back().to_string(true, *mod) << endl;
					}
				}

				mod->inst.push_back(clocked::Instance(term, clockedPorts));
				//cout << "pushing instance to module: " << term << "(";
				//for (int k = 0; k < (int)clockedPorts.size(); k++) {
				//	cout << clockedPorts[k].to_string(true) << ", ";
				//}
				//cout << ")" << endl;

				mod->inst.back().name = newInstanceName(mod, "inst");

				std::vector<size_t> funcOuts;
				for (size_t j = 0; j < impl.func->nets.size(); j++) {
					if (impl.func->nets[j].purpose == flow::Net::OUT) {
						funcOuts.push_back(j);
					}
				}

				if (funcOuts.empty()) {
					op.func = arithmetic::Operation::STRUCT;
					op.operands.clear();
					op.operands.push_back(arithmetic::Operand::labelOf("Bundle"));
					op.operands.push_back(arithmetic::Operand::vdd());
					op.operands.push_back(arithmetic::Operand::undef());
				} else if (funcOuts.size() == 1) {
					op.func = arithmetic::Operation::STRUCT;
					op.operands.clear();
					op.operands.push_back(arithmetic::Operand::labelOf("Bundle"));
					op.operands.push_back(impl.mod->chans[funcOuts[0]].hasValid() ?
						arithmetic::Operand::varOf(connect.map(impl.mod->chans[funcOuts[0]].valid)) :
						arithmetic::Operand::vdd());
					op.operands.push_back(arithmetic::Operand::varOf(connect.map(impl.mod->chans[funcOuts[0]].data)));
				} else {
					op.func = arithmetic::Operation::ARRAY;
					op.operands.clear();
					for (size_t out : funcOuts) {
						op.operands.push_back(expr.pushExpr(arithmetic::Operation(
							arithmetic::Operation::STRUCT, {
								arithmetic::Operand::labelOf("Bundle"),
								(impl.mod->chans[out].hasValid() ?
									arithmetic::Operand::varOf(connect.map(impl.mod->chans[out].valid)) :
									arithmetic::Operand::vdd()),
								arithmetic::Operand::varOf(connect.map(impl.mod->chans[out].data))
							})));
					}
				}

				modified = true;
			}
		} else if (op.func == arithmetic::Operation::VALIDITY) {
			if (op.operands.empty()) {
				continue;
			}

			if (op.operands[0].isVar()) {
				clocked::Type type = mod->nets[op.operands[0].index].type;

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
				clocked::Type type = mod->nets[op.operands[0].index].type;

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
				clocked::Type type = mod->nets[op.operands[1].index].type;

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

	return Mapping<arithmetic::Operand>();
}

clocked::Module synthesizeModuleFromFunc(const Func &func, Linker *linker, bool debug) {
	static const arithmetic::RuleSet defaultRules = arithmetic::rewriteCanonical() + arithmetic::rewriteSimple();
	static const arithmetic::RuleSet rewriteRules = defaultRules + buildRules();

	clocked::Module mod(func.name);

	mod.clk = mod.pushNet("clk", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);
	mod.reset = mod.pushNet("reset", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);

	clocked::Trigger always(mod.getClk());
	always.stmts.push_back(clocked::Statement(false, {}, mod.getReset()));

	auto typeMinimizer = std::bind(minimizeTypes, &mod, linker, std::placeholders::_1, std::placeholders::_2);

	std::vector<size_t> nets;
	std::vector<Expression> structs;
	Expression constStruct = arithmetic::construct("Bundle", {
		Expression::vdd(),
		Expression::U(),
	});
	for (size_t netIdx = 0; netIdx < func.nets.size(); netIdx++) {
		synthesizeChannel(mod, func.nets[netIdx], always.stmts[0]);

		// Map flow::Func nets to clocked::Channel nets
		nets.push_back(netIdx);
		structs.push_back(
			arithmetic::construct("Bundle", {
				(mod.chans.back().hasValid() ?
					Expression::varOf(mod.chans.back().valid) :
					Expression::vdd()),
				Expression::varOf(mod.chans.back().data)
			})
		);
	}

	for (const auto &inst : func.inst) {
		Implementation impl = linker->find(inst);
		if (impl.mod == nullptr or impl.func == nullptr) {
			error("", "unable to link flow to verilog for \'" + inst.type + "\'", __FILE__, __LINE__);
			continue;
		}

		std::vector<Expression> clockedPorts;
		for (size_t i = 0; i < impl.mod->nets.size(); i++) {
			bool isInput = impl.mod->nets[i].purpose == clocked::Net::IN;
			bool isOutput = impl.mod->nets[i].purpose == clocked::Net::OUT;

			if (isInput or isOutput) {
				int clockedChan = impl.mod->nets[i].chan;
				std::string netName = impl.mod->nets[i].name;
				if (clockedChan >= 0) {
					netName = impl.mod->chans[clockedChan].name;
				}

				int flowNet = -1;
				int flowPort = -1;
				for (size_t j = 0; j < impl.func->nets.size(); j++) {
					if (impl.func->nets[j].purpose == flow::Net::IN or
						impl.func->nets[j].purpose == flow::Net::OUT) {
						flowPort++;
					}
					if (impl.func->nets[j].name == netName) {
						flowNet = j;
						break;
					}
				}

				if (flowNet < 0) {
					error("", "unable to link port in verilog instantiation of flow function \'" + netName + "\'", __FILE__, __LINE__);
					clockedPorts.push_back(Expression::undef());
					continue;
				}

				Expression flowOp = Expression::undef();
				if (flowPort >= 0 and flowPort < inst.ports.size()) {
					flowOp = inst.ports[flowPort];
				}
				flowOp.substituteConst(constStruct);
				flowOp.substitute(nets, structs);
				if (clockedChan < 0) {
					clockedPorts.push_back(flowOp);
				} else if (i == impl.mod->chans[clockedChan].valid) {
					clockedPorts.push_back(arithmetic::member(flowOp, "valid"));
				} else if (i == impl.mod->chans[clockedChan].ready) {
					clockedPorts.push_back(arithmetic::member(flowOp, "ready"));
				} else if (i == impl.mod->chans[clockedChan].data) {
					clockedPorts.push_back(arithmetic::member(flowOp, "data"));
				} else {
					internal("", "improperly linked net \'" + netName + "\' in clocked module", __FILE__, __LINE__);
					clockedPorts.push_back(flowOp);
				}

				//cout << "before port " << clockedPorts.size()-1 << " " << clockedPorts.back().to_string(true, *mod) << endl;
				clockedPorts.back().minimize(rewriteRules);
				//cout << "after port " << clockedPorts.size()-1 << " " << clockedPorts.back().to_string(true, *mod) << endl;
			}
		}

		mod.inst.push_back(clocked::Instance(inst.type, clockedPorts));
		mod.inst.back().name = inst.name;
		mod.inst.back().comment = inst.comment;
	}

	for (const auto &cond : func.conds) {
		clocked::Statement branch(true);

		for (const auto &reg : cond.regs) {
			Expression regExpr(reg.second);
			regExpr.substituteConst(constStruct);
			regExpr.substitute(nets, structs);
			regExpr = member(regExpr, "data");

			regExpr.minimize(rewriteRules, typeMinimizer);
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
			cout << "out from: " << request.to_string(true) << endl;
			request.substituteConst(constStruct);
			request.substitute(nets, structs);
			request = member(request, "data");
			cout << "out from: " << request.to_string(true) << endl;

			request.minimize(rewriteRules, typeMinimizer);
			cout << "out to: " << request.to_string(true) << endl;
			// TODO(edward.bingham) unwrap the structure, select the data
			branch.sub.push_back(
				clocked::Statement(mod.chans[output.first].dataReg, request));

			// only when all output channels are ready to be written to
			// flow::Net::REG don't have valid/ready signals over channel
			if (mod.chans[output.first].hasValid()) {
				branch.sub.push_back(clocked::Statement(
					mod.chans[output.first].validReg, Expression::vdd()));

				// flow::Net::REG don't have valid/ready signals over channel
				if (mod.chans[output.first].hasReady()) {
					branch.expr = branch.expr
						&& (!mod.chans[output.first].getValidReg() || mod.chans[output.first].getReady());
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

		branch_valid.minimize(rewriteRules, typeMinimizer);

		branch.expr = mod.chans[cond.uid].getValid() && branch.expr;
		branch.expr.minimize(defaultRules, typeMinimizer);


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
			chan_ready.minimize(defaultRules, typeMinimizer);
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
