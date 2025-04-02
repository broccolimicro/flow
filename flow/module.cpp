#include "module.h"

namespace clocked {

Type::Type() {
	type = FIXED;
	width = 16;
	shift = 0;
}

Type::Type(int type, int width, int shift) {
	this->type = type;
	this->width = width;
	this->shift = shift;
}

Type::~Type() {
}

Net::Net() {
	this->purpose = WIRE;
}

Net::Net(string name, Type type, int purpose) {
	this->name = name;
	this->type = type;
	this->purpose = purpose;
}

Net::~Net() {
}

ValRdy::ValRdy() {
	valid = -1;
	ready = -1;
	data = -1;
}

ValRdy::~ValRdy() {
}

operand ValRdy::getValid() {
	return operand(valid, operand::variable);
}

operand ValRdy::getReady() {
	return operand(ready, operand::variable);
}

operand ValRdy::getData() {
	return operand(data, operand::variable);
}

Assign::Assign() {
	net = -1;
	expr = false;
	blocking = true;
}

Assign::Assign(int net, expression expr, bool blocking) {
	this->net = net;
	this->expr = expr;
	this->blocking = blocking;
}

Assign::~Assign() {
}

Rule::Rule(vector<Assign> assign, expression guard) {
	this->guard = guard;
	this->assign = assign;
}

Rule::~Rule() {
}

Block::Block(expression clk, vector<Rule> rules) {
	this->clk = clk;
	this->rules = rules;
}

Block::~Block() {
}

int Module::netIndex(string name, int region) const {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	return -1;
}

int Module::netIndex(string name, int region, bool define) {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	if (define) {
		int result = (int)nets.size();
		nets.push_back(Net(name));
		return result;
	}

	return -1;
}

pair<string, int> Module::netAt(int uid) const {
	return pair<string, int>(nets[uid].name, 0);
}

int Module::netCount() const {
	return (int)nets.size();
}

int Module::pushNet(string name, Type type, int purpose) {
	int index = (int)nets.size();
	nets.push_back(Net(name, type, purpose));
	return index;
}

}
