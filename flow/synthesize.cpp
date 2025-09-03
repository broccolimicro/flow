#include <algorithm>
#include <functional>
#include <iterator>
#include <set>

#include <arithmetic/algorithm.h>
#include <common/mapping.h>
#include <common/math.h>
#include <interpret_arithmetic/export_verilog.h>

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
	return (o.func == Operation::OpType::TYPE_CALL)
		&& (!o.operands.empty())
		&& (o.operands[0].cnst.sval == "probe");
}

bool isBooleanOperation(const Operation::OpType &op) {
	return op == Operation::OpType::TYPE_BOOLEAN_AND
		|| op == Operation::OpType::TYPE_BOOLEAN_OR
		|| op == Operation::OpType::TYPE_BOOLEAN_XOR
		|| op == Operation::OpType::TYPE_BOOLEAN_NOT
		|| op == Operation::OpType::TYPE_EQUAL
		|| op == Operation::OpType::TYPE_NOT_EQUAL
		|| op == Operation::OpType::TYPE_LESS
		|| op == Operation::OpType::TYPE_GREATER
		|| op == Operation::OpType::TYPE_LESS_EQUAL
		|| op == Operation::OpType::TYPE_GREATER_EQUAL;
}


Expression synthesizeExpressionProbes(const Expression &e, const mapping &ChannelToValid, const mapping &ChannelToData) {
	cout << endl << endl << "<<<<<<<<<<<>>>>>>>>>>>" << endl;
	Expression result(e);

	//TODO: verify that we're only substituting channel references, not local vars (if mistakenly probed)
	//std::set<size_t> non_channel_vars;
	//std::set_difference(
	//		ChannelToData.begin(), ChannelToData.end(),
	//		ChannelToValid.begin(), ChannelToValid.end(),
	//		std::inserter(non_channel_vars, non_channel_vars.begin())
	//);

	auto emplaceProbe = [&](size_t parent_expr_operation_idx, size_t channel_idx) {
		cout << " ~ ~ emplace: e" << parent_expr_operation_idx << " <- v" << channel_idx << " ~ ~" << endl;
		Operation substitution(Operation::OpType::TYPE_IDENTITY, {Operand::varOf(channel_idx)});
		substitution.exprIndex = parent_expr_operation_idx;
		result.sub.setExpr(substitution);
	};

	size_t subexpr_count = e.sub.size();
	if (subexpr_count == 0) {
		return e;

	} else if (subexpr_count == 1) {

		const Operation &operation = *e.getExpr(0);
		if (!isProbeCall(operation)) { return e; }

		size_t channel_idx = 0;
		size_t channel_data = ChannelToData.map(channel_idx);
		size_t channel_valid = ChannelToValid.map(channel_idx);
		cout << "probe channel v" << channel_idx << " [v:" << channel_valid << ",d:" << channel_data << "]" << endl;
		emplaceProbe(0, channel_valid);

		cout << ">>>>>>>>>>>early<<<<<<<<<<<" << endl;
		result.minimize();
		return result;
	}

	cout << "Post-order DFS:" << endl;
	vector<Operation> child_probes;
	for (arithmetic::PostOrderDFSIterator operation_it(e.sub, {e.top}); !operation_it.done(); ++operation_it) {
		const Operation &operation = *operation_it;
		cout << "==> e" << operation.exprIndex;

		// Descend down to all probe() calls
		if (isProbeCall(operation)) {
			cout << endl;
			child_probes.push_back(operation);
			continue;
		}

		// New, lower "or" ceiling?
		if (operation.func == Operation::OpType::TYPE_BOOLEAN_OR) {
			cout << "!!" << endl;
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
				cout << "  \\___> " << child_operation_idx << endl;
				cout << "probe channel v" << channel_idx << " [v:" << channel_valid << ",d:" << channel_data << "]" << endl;
				cout << "       parent e" << parent_operation_idx << endl;

				emplaceProbe(child_operation_idx, channel_data);
				new_parent_operands.insert(new_parent_operands.begin(), Operand::varOf(channel_valid));
			}
			child_probes.clear();

			Operation parent_expr_only_when_valid(Operation::OpType::TYPE_BOOLEAN_AND, new_parent_operands);
			parent_expr_only_when_valid.exprIndex = parent_operation_idx;
			result.sub.setExpr(parent_expr_only_when_valid);
			continue;
		}

		//TODO: when backtracking, pop off ceiling
		//cout << valid_regions.back() << endl;
		cout << endl;
	}

	//TODO: extract base case para merge two near-identical substitutions 
	// Synthesize leftover probes not nested within any BOOLEAN_OR
	if (!child_probes.empty()) {
		cout << endl << "...nobody's kids?" << endl;
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
			cout << "  \\___> " << child_operation_idx << endl;
			cout << "probe channel v" << channel_idx << " [v:" << channel_valid << ",d:" << channel_data << "]" << endl;
			cout << "       parent e" << parent_operation_idx << endl;

			// Base case: top IS probe() call
			if (parent_operation_idx == child_operation_idx) {
				emplaceProbe(child_operation_idx, channel_valid);
				break;  //TODO: just return immediately?
			}

			emplaceProbe(child_operation_idx, channel_data);
			new_parent_operands.insert(new_parent_operands.begin(), Operand::varOf(channel_valid));
		}
		child_probes.clear();

		Operation parent_expr_only_when_valid(Operation::OpType::TYPE_BOOLEAN_AND, new_parent_operands);
		parent_expr_only_when_valid.exprIndex = parent_operation_idx;
		result.sub.setExpr(parent_expr_only_when_valid);
	}

	cout << endl << "><><<>><><<>><><<>><><" << endl;
	cout << "PRE-MINIMIZE:" << endl;
	cout << result.to_string();
	cout << ">>>>>>>>>>><<<<<<<<<<<" << endl;

	result.minimize();
	//result.tidy();
	return result;
}


void synthesizeChannel(clocked::Module &mod, const Net &net) {
	static const clocked::Type wire(clocked::Type::TypeName::BITS, 1);
	clocked::Channel channel;
	clocked::Block &always = mod.blocks.back();

	if (net.purpose == flow::Net::Purpose::IN) {
		channel.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::IN);
		channel.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::Purpose::OUT);
		channel.data = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::IN);

	} else if (net.purpose == flow::Net::Purpose::OUT) {
		int valid_wire = mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::OUT);
		int valid_reg = mod.pushNet(net.name+"_valid_reg", clocked::Type(clocked::Type::TypeName::FIXED, 1), clocked::Net::Purpose::REG);
		channel.valid = valid_reg;
		mod.assign.push_back(clocked::Assign(valid_wire, Expression::varOf(valid_reg), true));
		always.reset.push_back(clocked::Assign(channel.valid, Expression::intOf(0)));

		channel.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::Purpose::IN);
		clocked::Rule reset_valid_reg({
				clocked::Assign(channel.valid, Expression::intOf(0)),
		}, Expression::varOf(channel.ready));
		always._else.push_back(reset_valid_reg);

		int data = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::OUT);
		channel.data = mod.pushNet(net.name+"_state", synthesizeChannelType(net.type), clocked::Net::Purpose::REG);
		mod.assign.push_back(clocked::Assign(data, Expression::varOf(channel.data), true));
		always.reset.push_back(clocked::Assign(channel.data, Expression::intOf(0)));

		int debug_wire = mod.pushNet("__"+net.name+"_ok", wire, clocked::Net::Purpose::WIRE);
		mod.assign.push_back(clocked::Assign(debug_wire, arithmetic::ident(
						~Expression::varOf(channel.valid) | Expression::varOf(channel.ready)), true));

	} else if (net.purpose == flow::Net::Purpose::REG) {
		channel.valid = -1;  //mod.pushNet(net.name+"_valid", wire, clocked::Net::Purpose::WIRE);
		channel.ready = -1;  //TODO: these could be wires for debug or mere modelling in cocotb harness
		channel.data = mod.pushNet(net.name+"_data", synthesizeChannelType(net.type), clocked::Net::Purpose::REG);
		always.reset.push_back(clocked::Assign(channel.data, Expression::intOf(0)));

	//TODO: migrate out of synthesizeChannel(), into synthesizeModuleFromFunc(), if/when COND's are no longer detected in netlist?
	} else if (net.purpose == flow::Net::Purpose::COND) {
		channel.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::Purpose::WIRE);
		channel.data = -1;
	}
	mod.chans.push_back(channel);
}


//TODO: do we ever miss a lone ".top" expression with no ".sub" body? temp bug we need to watch in arith lib
//IDEA: DRY principle - encapsulate shared statements like <expr>.minimize() calls here?
set<int> getNetsInExpression(const Expression &e) { //, std::function<bool(const Operand &)> filter = [](const Operand &o){return true;}) {
	set<int> nets;
	for (auto operationIt = e.sub.elems.elems.begin(); operationIt != e.sub.elems.elems.end(); operationIt++) {
		for (auto operandIt = operationIt->operands.begin(); operandIt != operationIt->operands.end(); operandIt++) {
			if (operandIt->type == arithmetic::Operand::Type::VAR) { // && filter(operandIt)) {
				// issue when REG var, not IN?
				nets.insert(operandIt->index);
			}
		}
	}
	//TODO: descend recursively into exprs
	return nets;
}


clocked::Module synthesizeModuleFromFunc(const Func &func) {
	clocked::Module mod;
	mod.name = func.name;

	mod.clk = mod.pushNet("clk", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);
	mod.reset = mod.pushNet("reset", clocked::Type(clocked::Type::TypeName::BITS, 1), clocked::Net::Purpose::IN);

	clocked::Block setupBlock(Expression::varOf(mod.clk));
	mod.blocks.push_back(setupBlock);
	clocked::Block &always = mod.blocks.back();

	// Map flow nets to valid-ready channels
	mapping funcNetToChannelData;
	mapping funcNetToChannelValid;
	mapping funcNetToChannelReady;
	//TODO: set<int> internalRegisters; ???

	for (int netIdx = 0; netIdx < (int)func.nets.size(); netIdx++) {
		synthesizeChannel(mod, func.nets[netIdx]);

		// Map flow::Func nets to clocked::Channel nets
		funcNetToChannelData.set(netIdx, mod.chans[netIdx].data);
		funcNetToChannelValid.set(netIdx, mod.chans[netIdx].valid);
		funcNetToChannelReady.set(netIdx, mod.chans[netIdx].ready);
	}

	// Track branch selection for conditional execution
	int branch_id_width = log2i(func.conds.size());
	int branch_id_reg = mod.pushNet("branch_id",
		clocked::Type(clocked::Type::TypeName::FIXED, branch_id_width),
		clocked::Net::Purpose::REG);
	always.reset.push_back(clocked::Assign(branch_id_reg, Expression::intOf(0)));

	int branch_id = 0;
	for (auto condIt = func.conds.begin(); condIt != func.conds.end(); ++condIt) {
		clocked::Rule branch_rule;
		branch_rule.assign.push_back(clocked::Assign(branch_id_reg, Expression::intOf(branch_id)));

		Expression predicate = condIt->valid;
		//predicate = isValid(predicate);  //TODO: ensure this is used for condition predicates & not body expressions during synthesis (& that our synthesizeExpressionProbes probe tests include this assumption)
		predicate = synthesizeExpressionProbes(predicate, funcNetToChannelValid, funcNetToChannelData);
		//predicate.minimize();

		//
		// Assemble branch_ready expression for only when all conditions are met
		//
		//TODO: arithmetic::ident() instead?? only true by empty default?
		Expression branch_ready = Expression::boolOf(true); 
		set<int> clocked_nets_that_branch_needs_to_be_valid;

		// only when [input?] channels referenced in guard predicate are valid
		for (int net : getNetsInExpression(predicate)) {
			//int func_net = funcNetToChannelData.unmap(net);  //decode mapping applied beforehand
			int mod_valid_net = funcNetToChannelValid.map(net);
			clocked_nets_that_branch_needs_to_be_valid.insert(mod_valid_net);
			//cout << "==(var in predicate expr)> " << mod_valid_net << endl;
		}

		// only when all input channels, who need acknowledgement, are valid
		for (auto condInputIt = condIt->ins.begin(); condInputIt != condIt->ins.end(); condInputIt++) {
			int mod_valid_net = funcNetToChannelValid.map(*condInputIt);
			clocked_nets_that_branch_needs_to_be_valid.insert(mod_valid_net); //TODO: must ack's be valid?
			//cout << "==(var to ack)> " << mod_valid_net << endl;
		}

		for (auto condRegIt = condIt->regs.begin(); condRegIt != condIt->regs.end(); condRegIt++) {
			Expression internalRegAssignment(condRegIt->second);
			//internalRegAssignment.minimize();

			// only when [input?] channels referenced in internal-memory assignments are valid
			for (int net : getNetsInExpression(internalRegAssignment)) {
				int mod_valid_net = funcNetToChannelValid.map(net);
				clocked_nets_that_branch_needs_to_be_valid.insert(mod_valid_net);
				//cout << "==(var in mem expr)> " << mod_valid_net << endl;
			}

			// Assign to internal-memory registers
			int mod_data_net = funcNetToChannelData.map(condRegIt->first);
			internalRegAssignment.apply(funcNetToChannelData); 
			branch_rule.assign.push_back(clocked::Assign(mod_data_net, internalRegAssignment));
		}

		for (auto condOutputIt = condIt->outs.begin(); condOutputIt != condIt->outs.end(); condOutputIt++) {
			Expression request = condOutputIt->second;
			//request.minimize();

			//only when [input?] channels referenced in requests to be sent are valid
			for (int net : getNetsInExpression(request)) {
				int mod_valid_net = funcNetToChannelValid.map(net);
				clocked_nets_that_branch_needs_to_be_valid.insert(mod_valid_net);
				//cout << "==(var in req expr)> " << mod_valid_net << endl;
			}

			int mod_data_net = funcNetToChannelData.map(condOutputIt->first);

			// Assign to outputs
			request.apply(funcNetToChannelData);
			branch_rule.assign.push_back(clocked::Assign(mod_data_net, request));

			// only when all output channels are ready to be written to
			int mod_valid_net = funcNetToChannelValid.map(condOutputIt->first);
			if (mod_valid_net != -1) {  // flow::Net::Purpose::REG don't have valid/ready signals over channel
				branch_rule.assign.push_back(clocked::Assign(mod_valid_net, Expression::intOf(1)));

				int mod_ready_net = funcNetToChannelReady.map(condOutputIt->first);
				if (mod_ready_net != -1) {  // flow::Net::Purpose::REG don't have valid/ready signals over channel
					branch_ready = branch_ready & (
							~Expression::varOf(mod_valid_net)
							| Expression::varOf(mod_ready_net));
				}
			}
			// ...either they're open (!valid) or _will be_ open next cycle (ready)
		}

		//branch_ready.minimize();
		for (int mod_valid_net : clocked_nets_that_branch_needs_to_be_valid) {
			if (mod_valid_net != -1) {  // flow::Net::Purpose::REG don't have valid/ready signals over channel
				branch_ready = branch_ready & Expression::varOf(mod_valid_net);
			}
		}

		branch_rule.guard = arithmetic::ident(predicate) & branch_ready;
		branch_rule.guard.minimize();
		always.rules.push_back(branch_rule);

		// Ensure only this branch executes until transaction is complete
		Expression branch_selector = arithmetic::ident(Expression::varOf(branch_id_reg) == Expression::intOf(branch_id));
		branch_ready = branch_selector & branch_ready;
		branch_ready.minimize();
		mod.assign.push_back(clocked::Assign(mod.chans[condIt->uid].ready, branch_ready, true));

		branch_id++;
	}

	// Return ready signals for each channel
	for (int netIdx = 0; netIdx < (int)func.nets.size(); netIdx++) {
		if (func.nets[netIdx].purpose == flow::Net::Purpose::IN) {

			//Expression chan_nvalid = Expression::boolOf(true);
			//TODO: arithmetic::ident() instead?? only false by empty default?
			Expression chan_ready = Expression::boolOf(false);
			for (auto condIt = func.conds.begin(); condIt != func.conds.end(); condIt++) {
				for (auto condInputIt = condIt->ins.begin(); condInputIt != condIt->ins.end(); condInputIt++) {
					if (*condInputIt == netIdx) {
						//int mod_valid_net = funcNetToChannelValid.map(condIt->uid);
						//chan_nvalid = chan_nvalid & ~Expression::varOf(mod.chans[condIt->uid].valid);
						int mod_ready_net = funcNetToChannelReady.map(condIt->uid);
						chan_ready = chan_ready | Expression::varOf(mod_ready_net);
					}
				}
			}

			Expression chan_ready_out = chan_ready; //chan_nvalid | chan_ready;
			chan_ready_out.minimize();
			mod.assign.push_back(clocked::Assign(mod.chans[netIdx].ready, chan_ready_out, true));
		}
	}

	return mod;
}

}
