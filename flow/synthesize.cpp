#include <algorithm>
#include <functional>
#include <iterator>
#include <set>

#include <arithmetic/algorithm.h>
#include <common/mapping.h>
#include <common/math.h>

#include "synthesize.h"

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


//TODO: push helpers to arithmetic
bool isProbeCall(const Operation &o) {
	return (o.func == Operation::OpType::CALL)
		&& (!o.operands.empty())
		&& (o.operands[0].cnst.sval == "probe");
}

bool isBooleanOperation(const Operation &operation) {
	switch (operation.func) {
		case Operation::OpType::BOOLEAN_AND:
		case Operation::OpType::BOOLEAN_OR:
		case Operation::OpType::BOOLEAN_XOR:
		case Operation::OpType::BOOLEAN_NOT:
		// Operators below don't _require_ boolean operands
		//case Operation::OpType::EQUAL:
		//case Operation::OpType::NOT_EQUAL:
		//case Operation::OpType::LESS:
		//case Operation::OpType::GREATER:
		//case Operation::OpType::LESS_EQUAL:
		//case Operation::OpType::GREATER_EQUAL:
			return true;
		default:
			return false;
	}
}


Expression synthesizeExpressionProbes(const Expression &e, const Mapping<size_t> &ChannelToValid, const Mapping<size_t> &ChannelToData) {
	//cout << endl << endl << "<<<<<<<<<<<>>>>>>>>>>>" << endl;
	Expression result(e);

	//TODO: verify that we're only substituting channel references, not local vars (if mistakenly probed)
	//std::set<size_t> non_channel_vars;
	//std::set_difference(
	//		ChannelToData.begin(), ChannelToData.end(),
	//		ChannelToValid.begin(), ChannelToValid.end(),
	//		std::inserter(non_channel_vars, non_channel_vars.begin())
	//);

	auto emplaceProbe = [&](size_t parent_expr_operation_idx, size_t channel_idx) {
		//cout << " ~ ~ emplace: e" << parent_expr_operation_idx << " <- v" << channel_idx << " ~ ~" << endl;
		Operation substitution(Operation::OpType::IDENTITY, {Operand::varOf(channel_idx)});
		substitution.exprIndex = parent_expr_operation_idx;
		result.sub.setExpr(substitution);
	};

	size_t subexpr_count = e.sub.size();
	if (subexpr_count == 0) {
		//cout << ">>>>>>>>>>> early, no probes <<<<<<<<<<<" << endl;
		return e;

	} else if (subexpr_count == 1) {

		const Operation &operation = *e.getExpr(0);
		if (!isProbeCall(operation)) { return e; }

		size_t channel_idx = 0;
		//size_t channel_data = ChannelToData.map(channel_idx);
		size_t channel_valid = ChannelToValid.map(channel_idx);
		//cout << "probe channel v" << channel_idx << " [v:" << channel_valid << ",d:" << channel_data << "]" << endl;
		emplaceProbe(0, channel_valid);

		//cout << ">>>>>>>>>>> early, expr is just 1 probe <<<<<<<<<<<" << endl;
		result.minimize();
		return result;
	}

	//cout << "Post-order DFS:" << endl;
	vector<Operation> child_probes;
	for (arithmetic::PostOrderDFSIterator operation_it(e.sub, {e.top}); !operation_it.done(); ++operation_it) {
		const Operation &operation = *operation_it;
		//cout << "==> e" << operation.exprIndex;

		// Descend down to all probe() calls
		if (isProbeCall(operation)) {
			//cout << endl;
			child_probes.push_back(operation);
			continue;
		}

		// New, lower "or" ceiling?
		if (operation.func == Operation::OpType::BOOLEAN_OR) {
			//cout << "!!" << endl;
			size_t parent_operation_idx = operation.exprIndex;
			Operation parent_operation = *e.getExpr(parent_operation_idx);

			Operand parent_copy = result.sub.pushExpr(parent_operation);
			vector<Operand> new_parent_operands = {parent_copy};  //Operand::exprOf(parent_operation_idx);

			// For each channel_data substitution, append an "&& channel_valid" atop this BOOLEAN_OR
			for (const Operation &probe_operation : child_probes) {
				size_t child_operation_idx = probe_operation.exprIndex;
				size_t channel_idx = probe_operation.operands[1].index;
				size_t channel_data = ChannelToData.map(channel_idx);
				size_t channel_valid = ChannelToValid.map(channel_idx);
				//cout << "  \\___> " << child_operation_idx << endl;
				//cout << "probe channel v" << channel_idx << " [v:" << channel_valid << ",d:" << channel_data << "]" << endl;
				//cout << "       parent e" << parent_operation_idx << endl;

				emplaceProbe(child_operation_idx, channel_data);
				new_parent_operands.insert(new_parent_operands.begin(), Operand::varOf(channel_valid));
			}
			child_probes.clear();

			Operation parent_expr_only_when_valid(Operation::OpType::BOOLEAN_AND, new_parent_operands);
			parent_expr_only_when_valid.exprIndex = parent_operation_idx;
			result.sub.setExpr(parent_expr_only_when_valid);
			continue;
		}

		//TODO: when backtracking, pop off ceiling
		//cout << valid_regions.back() << endl;
		//cout << endl;
	}

	//TODO: extract base case para merge two near-identical substitutions 
	// Synthesize leftover probes not nested within any BOOLEAN_OR
	if (!child_probes.empty()) {
		//cout << endl << "...nobody's kids?" << endl;
		size_t parent_operation_idx = e.top.index;  //TODO: e.sub.getExpr(e.top.index); ??
		Operation parent_operation = *e.getExpr(parent_operation_idx);

		Operand parent_copy = result.sub.pushExpr(parent_operation);
		vector<Operand> new_parent_operands = {parent_copy};  //Operand::exprOf(parent_operation_idx);

		// For each channel_data substitution, append an "&& channel_valid" at the top
		for (const Operation &probe_operation : child_probes) {
			size_t child_operation_idx = probe_operation.exprIndex;
			size_t channel_idx = probe_operation.operands[1].index;
			size_t channel_data = ChannelToData.map(channel_idx);
			size_t channel_valid = ChannelToValid.map(channel_idx);
			//cout << "  \\___> " << child_operation_idx << endl;
			//cout << "probe channel v" << channel_idx << " [v:" << channel_valid << ",d:" << channel_data << "]" << endl;
			//cout << "       parent e" << parent_operation_idx << endl;

			// Base case: top IS probe() call
			if (parent_operation_idx == child_operation_idx) {
				emplaceProbe(child_operation_idx, channel_valid);
				break;  //TODO: just return immediately?
			}

			emplaceProbe(child_operation_idx, channel_data);
			new_parent_operands.insert(new_parent_operands.begin(), Operand::varOf(channel_valid));
		}
		child_probes.clear();

		Operation parent_expr_only_when_valid(Operation::OpType::BOOLEAN_AND, new_parent_operands);
		parent_expr_only_when_valid.exprIndex = parent_operation_idx;
		result.sub.setExpr(parent_expr_only_when_valid);
	}

	//cout << endl << "><><<>><><<>><><<>><><" << endl;
	//cout << "PRE-MINIMIZE:" << endl;
	//cout << result.to_string();
	//cout << ">>>>>>>>>>><<<<<<<<<<<" << endl;

	result.minimize();
	//result.tidy();
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

set<size_t> getNetsInExpression(const Expression &e) {
	set<size_t> nets;
	for (const arithmetic::Operand &operand : e.exprIndex()) {
		if (operand.type == arithmetic::Operand::Type::VAR) {
			nets.insert(operand.index);
		}
	}
	return nets;
}

clocked::Module synthesizeModuleFromFunc(const Func &func, bool debug) {
	clocked::Module mod;
	mod.name = func.name;

	mod.clk = mod.pushNet("clk", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);
	mod.reset = mod.pushNet("reset", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);

	clocked::Trigger always(mod.getClk());
	always.stmts.push_back(clocked::Statement(false, {}, mod.getReset()));

	// Map flow nets to valid-ready channels
	Mapping<size_t> funcNetToChannelData(-1, true);
	Mapping<size_t> funcNetToChannelValid(-1, true);
	Mapping<size_t> funcNetToChannelReady(-1, true);
	//TODO: set<size_t> internalRegisters; ???

	for (size_t netIdx = 0; netIdx < func.nets.size(); netIdx++) {
		synthesizeChannel(mod, func.nets[netIdx], always.stmts[0]);

		// Map flow::Func nets to clocked::Channel nets
		funcNetToChannelData.set(netIdx, mod.chans[netIdx].data);
		funcNetToChannelValid.set(netIdx, mod.chans[netIdx].valid);
		funcNetToChannelReady.set(netIdx, mod.chans[netIdx].ready);
	}

	for (const auto &cond : func.conds) {
		clocked::Statement branch(true);

		set<size_t> branchOperands;
		// only when [input?] channels referenced in guard predicate are valid
		for (size_t net : getNetsInExpression(cond.valid)) {
			branchOperands.insert(funcNetToChannelValid.map(net));
		}

		// only when all input channels, who need acknowledgement, are valid
		for (int input : cond.ins) {
			branchOperands.insert(funcNetToChannelValid.map(input));
		}

		for (const auto &reg : cond.regs) {
			Expression internalRegAssignment(reg.second);
			internalRegAssignment.minimize();

			// only when [input?] channels referenced in internal-memory assignments are valid
			for (size_t net : getNetsInExpression(internalRegAssignment)) {
				branchOperands.insert(funcNetToChannelValid.map(net));
			}

			// Assign to internal-memory registers
			size_t mod_data_net = funcNetToChannelData.map(reg.first);
			internalRegAssignment.applyVars(funcNetToChannelData); 
			branch.sub.push_back(clocked::Statement(mod_data_net, internalRegAssignment));
		}

		for (const auto &output : cond.outs) {
			//only when [input?] channels referenced in requests to be sent are valid
			for (size_t net : getNetsInExpression(output.second)) {
				branchOperands.insert(funcNetToChannelValid.map(net));
			}

			// Assign to outputs
			size_t mod_data_net = funcNetToChannelData.map(output.first);
			Expression request = output.second;
			request.applyVars(funcNetToChannelData);
			request.minimize();
			branch.sub.push_back(clocked::Statement(mod_data_net, request));

			// only when all output channels are ready to be written to
			size_t mod_valid_net = funcNetToChannelValid.map(output.first);
			if (mod_valid_net != funcNetToChannelValid.undef) {  // flow::Net::REG don't have valid/ready signals over channel
				branch.sub.push_back(clocked::Statement(mod_valid_net, Expression::intOf(1)));

				size_t mod_ready_net = funcNetToChannelReady.map(output.first);
				if (mod_ready_net != funcNetToChannelReady.undef) {  // flow::Net::REG don't have valid/ready signals ovver channel
					branch.expr = branch.expr
						&& (!Expression::varOf(mod_valid_net) || Expression::varOf(mod_ready_net));
				}
			}
			// ...either they're open (!valid) or _will be_ open next cycle (ready)
		}

		Expression branch_valid = cond.valid;
		branch_valid.applyVars(funcNetToChannelData);
		//branch_valid = synthesizeExpressionProbes(branch_valid, funcNetToChannelValid, funcNetToChannelData);
		for (size_t mod_valid_net : branchOperands) {
			if (mod_valid_net != funcNetToChannelValid.undef) {  // flow::Net::REG don't have valid/ready signals over channel
				branch_valid = branch_valid && Expression::varOf(mod_valid_net);
			}
		}
		branch_valid.minimize();

		branch.expr = mod.chans[cond.uid].getValid() && branch.expr;
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
						size_t mod_ready_net = funcNetToChannelReady.map(cond.uid);
						chan_ready = chan_ready || Expression::varOf(mod_ready_net);
					}
				}
			}
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
