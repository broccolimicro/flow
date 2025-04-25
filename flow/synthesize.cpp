#include "synthesize.h"

#include <common/mapping.h>
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
	if (net.purpose == flow::Net::IN) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::IN);
		result.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::OUT);
		result.data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::IN);
	} else if (net.purpose == flow::Net::OUT) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::OUT);
		result.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::IN);
		int data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::OUT);
		result.data = mod.pushNet(net.name+"_state", synthesize_type(net.type), clocked::Net::REG);
		mod.assign.push_back(clocked::Assign(data, Operand::varOf(result.data)));
	} else if (net.purpose == flow::Net::REG) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::WIRE);
		result.ready = -1;
		result.data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::REG);
	} else if (net.purpose == flow::Net::COND) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::REG);
		result.ready = mod.pushNet(net.name+"_ready", wire, clocked::Net::WIRE);
		result.data = -1;
	}
	mod.chans.push_back(result);
}

clocked::Module synthesize_valrdy(const Func &func) {
	clocked::Module result;
	result.name = func.name;

	result.reset = result.pushNet("reset", clocked::Type(clocked::Type::BITS, 1), clocked::Net::IN);
	result.clk = result.pushNet("clk", clocked::Type(clocked::Type::BITS, 1), clocked::Net::IN);

	// create variables for each channel
	for (int i = 0; i < (int)func.nets.size(); i++) {
		synthesize_chan(result, func.nets[i]);
	}

	// Create variable mappings
	mapping condToValid;
	mapping condToReady;
	mapping inregToData;
	mapping outToReady;
	vector<Expression> condToValRdy;
	for (int i = 0; i < (int)func.nets.size(); i++) {
		if (func.nets[i].purpose == flow::Net::COND) {
			condToValid.set(i, result.chans[i].valid);
			condToReady.set(i, result.chans[i].ready);
			if (i >= (int)condToValRdy.size()) {
				condToValRdy.resize(i+1, Expression());
			}
			condToValRdy[i] = result.chans[i].getValid() & result.chans[i].getReady();
		} else if (func.nets[i].purpose == flow::Net::IN or func.nets[i].purpose == flow::Net::REG) {
			inregToData.set(i, result.chans[i].data);
		} else if (func.nets[i].purpose == flow::Net::OUT) {
			outToReady.set(i, result.chans[i].ready);
		}
	}

	for (auto i = func.conds.begin(); i != func.conds.end(); i++) {
		Expression ready = Operand(true);
		for (auto j = i->outs.begin(); j != i->outs.end(); j++) {
			ready = ready & Operand::varOf(result.chans[j->first].ready);
		}
		ready = ~result.chans[i->uid].getValid() | ready;
		ready.minimize();
		result.assign.push_back(
			clocked::Assign(result.chans[i->uid].ready, ready, true));

		Expression valid = i->valid;
		valid.apply(inregToData);
		valid = isValid(valid);
		valid.minimize();

		vector<clocked::Assign> assign;
		vector<clocked::Assign> reset;
		for (auto j = i->outs.begin(); j != i->outs.end(); j++) {
			Expression data = j->second;
			data.apply(inregToData);
			reset.push_back(clocked::Assign(result.chans[j->first].data, Operand::intOf(0)));
			assign.push_back(clocked::Assign(result.chans[j->first].data, data));
		}
		reset.push_back(clocked::Assign(result.chans[i->uid].valid, Operand(false)));
		assign.push_back(clocked::Assign(result.chans[i->uid].valid, Operand(true)));

		result.blocks.push_back(
			clocked::Block(Operand::varOf(result.clk), {
				clocked::Rule(assign,
					valid & result.chans[i->uid].getReady()),
				clocked::Rule({
					clocked::Assign(result.chans[i->uid].valid, Operand(false))
				}, result.chans[i->uid].getReady()),
			}));
		result.blocks.back().reset = reset;
	}

	for (int i = 0; i < (int)func.nets.size(); i++) {
		if (func.nets[i].purpose == flow::Net::OUT) {
			Expression valid = Operand(false);
			for (auto j = func.conds.begin(); j != func.conds.end(); j++) {
				for (auto k = j->outs.begin(); k != j->outs.end(); k++) {
					if (k->first == i) {
						valid = valid | Operand::varOf(result.chans[j->uid].valid);
					}
				}
			}
			valid.minimize();

			result.assign.push_back(
				clocked::Assign(result.chans[i].valid, valid, true));
		} else if (func.nets[i].purpose == flow::Net::IN) {
			Expression ready = Operand(false);
			Expression nvalid = Operand(true);
			for (auto j = func.conds.begin(); j != func.conds.end(); j++) {
				for (auto k = j->ins.begin(); k != j->ins.end(); k++) {
					if (*k == i) {
						nvalid = nvalid & ~Operand::varOf(result.chans[j->uid].valid);
						ready = ready | Operand::varOf(result.chans[j->uid].ready);
					}
				}
			}
			Expression ready_out = nvalid | ready;
			cout << ready_out << endl;
			ready_out.minimize();
			cout << ready_out << endl;

			result.assign.push_back(
				clocked::Assign(result.chans[i].ready, ready_out, true));
		}
	}

	/*for (int i = 0; i < (int)from.size(); i++) {
		string port = vars.nodes[from[i].index].to_string();
		expression ready = from[i].token;
		for (int i = 0; i < (int)cond.size(); i++) {
			// TODO(edward.bingham) need cond[i].ready
			ready.replace(cond[i].index, cond[i].ready);
		}

		fprintf(fptr, "\tassign %s_ready = %s;\n", port.c_str(), export_expression(~from[i].token | ready, vars).to_string().c_str());
	}

	arithmetic::expression ready = true;
	for (int i = 0; i < (int)to.size(); i++) {
		string port = vars.nodes[to[i].index].to_string();

		printf("\talways @(posedge clk) begin\n");
		printf("\t\tif (reset) begin\n");
		printf("\t\t\t%s_send <= 0;\n", port.c_str());
		printf("\t\t\t%s_state <= 0;\n", port.c_str());
		printf("\t\tend else if (!%s_valid || %s_ready) begin\n", port.c_str(), port.c_str());
		printf("\t\t\tif (%s) begin\n", export_expression(is_valid(to[i].token), vars).to_string().c_str());
		printf("\t\t\t\t%s_state <= %s;\n", port.c_str(), export_expression(to[i].value, vars).to_string().c_str());
		printf("\t\t\t\t%s_send <= 1;\n", port.c_str());
		printf("\t\t\tend else begin\n");
		printf("\t\t\t\t%s_send <= 0;\n", port.c_str());
		printf("\t\t\tend\n");
		printf("\t\tend\n");
		printf("\tend\n\n");
	}

	fprintf(fptr, "endmodule\n");*/

	return result;
}

}
