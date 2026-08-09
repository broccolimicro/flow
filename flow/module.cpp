#include "module.h"

namespace clocked {

Type::Type(TypeName type, size_t width, int shift) {
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

Statement::Statement() {
	net = -1;
	blocking = true;
	type = ASSIGN;
}

Statement::Statement(int net, Expression expr, bool blocking) {
	this->type = ASSIGN;
	this->net = net;
	this->expr = expr;
	this->blocking = blocking;
}

Statement::Statement(bool elif, vector<Statement> stmts, Expression expr) {
	this->type = elif ? ELIF : IF;
	this->expr = expr;
	this->sub = stmts;
}

Statement::~Statement() {
}

Trigger::Trigger(Expression clk, vector<Statement> stmts) {
	this->clk = clk;
	this->stmts = stmts;
}

Trigger::~Trigger() {
}

Instance::Instance() {
}

Instance::Instance(string type, vector<Expression> ports) {
	this->type = type;
	this->ports = ports;
}

Instance::~Instance() {
}

Module::Module() {
	reset = -1;
	clk = -1;
}

Module::~Module() {
}

Operand Module::getClk() {
	return arithmetic::Operand::varOf(clk);
}

Operand Module::getReset() {
	return arithmetic::Operand::varOf(reset);
}

int Module::netIndex(string name) const {
	for (size_t i = 0; i < (size_t)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	return -1;
}

int Module::netIndex(string name, bool define) {
	for (size_t i = 0; i < (size_t)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	if (define) {
		size_t result = (size_t)nets.size();
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

size_t Module::pushNet(string name, Type type, Net::Purpose purpose) {
	size_t index = (size_t)nets.size();
	nets.push_back(Net(name, type, purpose));
	return index;
}

}
