#include "func.h"

#include <parse_expression/expression.h>
#include <interpret_arithmetic/export.h>

#include <stdexcept>

namespace flow {

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
	purpose = NONE;
}

Net::Net(string name, Type type, int purpose) {
	this->name = name;
	this->type = type;
	this->purpose = purpose;
}

Net::~Net() {
}

Condition::Condition() {
	this->uid = -1;
}

Condition::Condition(int uid, Expression valid) {
	this->uid = uid;
	this->valid = valid;
}

Condition::~Condition() {
}

void Condition::req(Operand out, Expression expr) {
	if (not out.isVar()) {
		return;
	}
	outs.push_back({out.index, expr});
}

void Condition::ack(Operand in) {
	if (not in.isVar()) {
		return;
	}
	ins.push_back(in.index);
}

void Condition::ack(vector<Operand> in) {
	for (auto i = in.begin(); i != in.end(); i++) {
		if (not i->isVar()) {
			continue;
		}
		ins.push_back(i->index);
	}
}

Input::Input() {
	this->uid = -1;
}

Input::Input(int uid) {
	this->uid = uid;
}

Input::~Input() {
}

Func::Func() {
}

Func::~Func() {
}

int Func::netIndex(string name) const {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	return -1;
}

int Func::netIndex(string name, bool define) {
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

string Func::netAt(int uid) const {
	return nets[uid].name;
}

int Func::netCount() const {
	return (int)nets.size();
}

Operand Func::pushNet(string name, Type type, int purpose) {
	int uid = (int)nets.size();
	nets.push_back(Net(name, type, purpose));
	return Operand::varOf(uid);
}

int Func::pushCond(Expression valid) {
	int uid = (int)nets.size();
	int index = (int)conds.size();
	conds.push_back(Condition(uid, valid));
	nets.push_back(Net("case_" + ::to_string(index), Type(Type::BITS, 1), Net::COND));
	return index;
}

}
