#include "module.h"

namespace clocked {

Type::Type(TypeName type, int width, int shift) {
	this->type = type;
	this->width = width;
	this->shift = shift;
}

Type::~Type() {
}

std::ostream& operator<<(std::ostream& os, const Type& type) {
    return os << "(type = " << type.type
              << ", width = " << type.width
              << ", shift = " << type.shift << ")";
}

Net::Net() {
	this->purpose = Purpose::WIRE;
}

Net::Net(string name, clocked::Type type, Purpose purpose) {
	this->name = name;
	this->type = type;
	this->purpose = purpose;
}

Net::~Net() {
}

std::ostream& operator<<(std::ostream& os, const Net& net) {
    return os << "(name = " << net.name
              << ", type = " << net.type
              << ", purpose = " << net.purpose << ")";
}

Channel::Channel() {
	valid = -1;
	ready = -1;
	data = -1;
}

Channel::~Channel() {
}

Operand Channel::getValid() {
	return Operand::varOf(valid);
}

Operand Channel::getReady() {
	return Operand::varOf(ready);
}

Operand Channel::getData() {
	return Operand::varOf(data);
}

Assign::Assign() {
	net = -1;
	blocking = true;
}

Assign::Assign(int net, Expression expr, bool blocking) {
	this->net = net;
	this->expr = expr;
	this->blocking = blocking;
}

Assign::~Assign() {
}

Rule::Rule(vector<Assign> assign, Expression guard) {
	this->guard = guard;
	this->assign = assign;
}

Rule::~Rule() {
}

Block::Block(Expression clk, vector<Rule> rules) {
	this->clk = clk;
	this->rules = rules;
}

Block::~Block() {
}

int Module::netIndex(string name) const {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	return -1;
}

int Module::netIndex(string name, bool define) {
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

string Module::netAt(int uid) const {
	return nets[uid].name;
}

int Module::netCount() const {
	return (int)nets.size();
}

int Module::pushNet(string name, Type type, Net::Purpose purpose) {
	int index = (int)nets.size();
	nets.push_back(Net(name, type, purpose));
	return index;
}

}
