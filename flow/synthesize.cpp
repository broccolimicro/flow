#include "synthesize.h"

#include <arithmetic/algorithm.h>
#include <common/mapping.h>
#include <common/math.h>
#include <interpret_arithmetic/export_verilog.h>

namespace flow {

clocked::Type synthesize_type(const Type &type) {
	clocked::Type result;
	if (type.type == flow::Type::BITS) {
		result.type = clocked::Type::BITS;
	} else if (type.type == flow::Type::FIXED) {
		result.type = clocked::Type::FIXED;
	}
	result.width = type.width;
	result.shift = type.shift;
	return result;
}


void synthesize_chan(clocked::Module &mod, const Net &net) {
	static const clocked::Type wire(clocked::Type::BITS, 1);
	clocked::ValRdy result;
	clocked::Block &always = mod.blocks.back();

	if (net.purpose == flow::Net::IN) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::IN);
		result.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::OUT);
		result.data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::IN);

	} else if (net.purpose == flow::Net::OUT) {
		int valid_wire = mod.pushNet(net.name+"_valid", wire, clocked::Net::OUT);
		int valid_reg = mod.pushNet(net.name+"_valid_reg", clocked::Type(1, 1), clocked::Net::REG);
		result.valid = valid_reg;
		mod.assign.push_back(clocked::Assign(valid_wire, Expression::varOf(valid_reg), true));
		always.reset.push_back(clocked::Assign(result.valid, Expression::intOf(0)));

		result.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::IN);
		clocked::Rule reset_valid_reg({
				clocked::Assign(result.valid, Expression::intOf(0)),
		}, Expression::varOf(result.ready));
		always._else.push_back(reset_valid_reg);

		int data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::OUT);
		result.data = mod.pushNet(net.name+"_state", synthesize_type(net.type), clocked::Net::REG);
		mod.assign.push_back(clocked::Assign(data, Expression::varOf(result.data), true));
		always.reset.push_back(clocked::Assign(result.data, Expression::intOf(0)));

		int debug_wire = mod.pushNet("__"+net.name+"_ok", wire, clocked::Net::WIRE);
		mod.assign.push_back(clocked::Assign(debug_wire, arithmetic::ident(
						~Expression::varOf(result.valid) | Expression::varOf(result.ready)), true));

	} else if (net.purpose == flow::Net::REG) {
		result.valid = -1;  //mod.pushNet(net.name+"_valid", wire, clocked::Net::WIRE);
		result.ready = -1;  //TODO: these could be wires for debug or mere modelling in cocotb harness
		result.data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::REG);
		always.reset.push_back(clocked::Assign(result.data, Expression::intOf(0)));

	//TODO: migrate out of synthesize_chan(), into synthesize_valrdy(), if/when COND's are no longer detected in netlist?
	} else if (net.purpose == flow::Net::COND) {
		result.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::WIRE);
		result.data = -1;
	}
	mod.chans.push_back(result);
}


clocked::Module synthesize_valrdy(const Func &func) {
	clocked::Module result;
	result.name = func.name;

	result.clk = result.pushNet("clk", clocked::Type(clocked::Type::BITS, 1), clocked::Net::IN);
	result.reset = result.pushNet("reset", clocked::Type(clocked::Type::BITS, 1), clocked::Net::IN);

	clocked::Block setupBlock(Expression::varOf(result.clk));
	result.blocks.push_back(setupBlock);
	clocked::Block &always = result.blocks.back();

	// Create channels for each variable
	mapping funcNetToChannelData;
	for (int netIdx = 0; netIdx < (int)func.nets.size(); netIdx++) {
		synthesize_chan(result, func.nets[netIdx]);

		// Map flow::Func nets to channel-bound clocked::Nets
		if (func.nets[netIdx].purpose == flow::Net::IN or func.nets[netIdx].purpose == flow::Net::REG) {
			funcNetToChannelData.set(netIdx, result.chans[netIdx].data);
		}
	}

	// Track branch selection for conditional execution
	int branch_id_width = log2i(func.conds.size());
	int branch_id_reg = result.pushNet("branch_id", clocked::Type(1, branch_id_width), clocked::Net::REG);
	always.reset.push_back(clocked::Assign(branch_id_reg, Expression::intOf(0)));

	int branch_id = 0;
	for (auto condIt = func.conds.begin(); condIt != func.conds.end(); ++condIt) {
		clocked::Rule branch_rule;
		branch_rule.assign.push_back(clocked::Assign(branch_id_reg, Expression::intOf(branch_id)));

		Expression valid = condIt->valid;
		//valid.minimize();
		//valid = isValid(valid);
		valid.apply(funcNetToChannelData);

		// Assemble branch_ready expression
		Expression branch_ready = Expression::boolOf(true);
		for (auto condInputIt = condIt->ins.begin(); condInputIt != condIt->ins.end(); condInputIt++) {
			branch_ready = branch_ready & Expression::varOf(result.chans[*condInputIt].valid); 
		}
		for (auto condOutputIt = condIt->outs.begin(); condOutputIt != condIt->outs.end(); condOutputIt++) {
			branch_ready = branch_ready & (
					~Expression::varOf(result.chans[condOutputIt->first].valid)
					| Expression::varOf(result.chans[condOutputIt->first].ready));

			Expression request = condOutputIt->second;
			//request.minimize();
			//request = isValid(request);  //request.isValid()
			request.apply(funcNetToChannelData);

			branch_rule.assign.push_back(clocked::Assign(result.chans[condOutputIt->first].data, request));
			branch_rule.assign.push_back(clocked::Assign(result.chans[condOutputIt->first].valid, Expression::intOf(1)));
		}

		// Route internal-memory registers
		for (auto condRegIt = condIt->regs.begin(); condRegIt != condIt->regs.end(); condRegIt++) {
			Expression internalRegAssignment(condRegIt->second);
			internalRegAssignment.apply(funcNetToChannelData);
			//internalRegAssignment.minimize();

			branch_rule.assign.push_back(clocked::Assign(result.chans[condRegIt->first].data, internalRegAssignment));
		}

		branch_rule.guard = arithmetic::ident(valid) & branch_ready;
		always.rules.push_back(branch_rule);

		Expression branch_selector = arithmetic::ident(Expression::varOf(branch_id_reg) == Expression::intOf(branch_id));
		branch_ready = branch_selector & branch_ready;
		//branch_ready.minimize();
		result.assign.push_back(clocked::Assign(result.chans[condIt->uid].ready, branch_ready, true));

		branch_id++;
	}

	// Return ready signals on each channel
	for (int netIdx = 0; netIdx < (int)func.nets.size(); netIdx++) {
		if (func.nets[netIdx].purpose == flow::Net::IN) {

			//Expression chan_nvalid = Expression::boolOf(true);
			Expression chan_ready = Expression::boolOf(false);
			for (auto condIt = func.conds.begin(); condIt != func.conds.end(); condIt++) {
				for (auto condInputIt = condIt->ins.begin(); condInputIt != condIt->ins.end(); condInputIt++) {
					if (*condInputIt == netIdx) {
						//chan_nvalid = chan_nvalid & ~Expression::varOf(result.chans[condIt->uid].valid);
						chan_ready = chan_ready | Expression::varOf(result.chans[condIt->uid].ready);
					}
				}
			}

			Expression chan_ready_out = chan_ready; //chan_nvalid | chan_ready;
			//chan_ready_out.minimize();
			result.assign.push_back(clocked::Assign(result.chans[netIdx].ready, chan_ready_out, true));
		}
	}

	return result;
}

}
