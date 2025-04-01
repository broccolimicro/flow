#include "synthesize.h"

#include <common/mapping.h>

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
		mod.nets[data].rules.push_back(clocked::Rule(operand(result.data, operand::variable)));
	} else if (net.purpose == flow::Net::REG) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::WIRE);
		result.ready = -1;
		result.data = mod.pushNet(net.name+"_data", synthesize_type(net.type), clocked::Net::REG);
	} else if (net.purpose == flow::Net::COND) {
		result.valid = mod.pushNet(net.name+"_valid", wire, clocked::Net::WIRE);
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
	vector<expression> condToValRdy;
	for (int i = 0; i < (int)func.nets.size(); i++) {
		if (func.nets[i].purpose == flow::Net::COND) {
			condToValid.set(i, result.chans[i].valid);
			condToReady.set(i, result.chans[i].ready);
			if (i >= (int)condToValRdy.size()) {
				condToValRdy.resize(i+1, expression());
			}
			condToValRdy[i] = result.chans[i].getValid() & result.chans[i].getReady();
		} else if (func.nets[i].purpose == flow::Net::IN or func.nets[i].purpose == flow::Net::REG) {
			inregToData.set(i, result.chans[i].data);
		} else if (func.nets[i].purpose == flow::Net::OUT) {
			outToReady.set(i, result.chans[i].ready);
		}
	}

	for (int i = 0; i < (int)func.nets.size(); i++) {
		if (func.nets[i].purpose == flow::Net::COND) {
			expression ready = func.nets[i].value;
			ready.apply(outToReady);
			clocked::Rule rule(~result.chans[i].getValid() | ready);
			result.nets[result.chans[i].ready].rules.push_back(rule);
		}
	}

	/*for (int i = 0; i < (int)func.nets.size(); i++) {
		if (func.nets[i].purpose == flow::Net::OUT) {
			Rule rule(func.nets[i].active); //.apply(...); map from func.nets to result.nets, looking at the validity signals and data signals
			result.nets[result.chans[i].valid].rules.push_back(rule);
		}
	}*/

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
