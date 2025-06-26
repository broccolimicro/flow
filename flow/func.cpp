#include "func.h"

#include <parse_expression/expression.h>
#include <interpret_arithmetic/export.h>
#include <arithmetic/expression.h>

#include <stdexcept>
#include <compare>

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

std::ostream& operator<<(std::ostream& os, const Type& type) {
    return os << "(type = " << type.type
              << ", width = " << type.width
              << ", shift = " << type.shift << ")";
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

std::ostream& operator<<(std::ostream& os, const Condition& cond) {
	os << "Condition(uid: " << cond.uid << ", "
		<< "valid: {" << endl << cond.valid.to_string() << endl << "}";

	os << endl << "==>  ins: [";
	for (size_t i = 0; i < cond.ins.size(); ++i) {
		if (i > 0) { os << ", "; }
		os << cond.ins[i];
	}
	os << "]" << endl;

	os << endl << "==>  outs: [" << endl;
	for (size_t i = 0; i < cond.outs.size(); ++i) {
		if (i > 0) { os << "," << endl; }
		os << "<" << cond.outs[i].first << ">{" << endl << cond.outs[i].second.to_string() << endl << "}";
	}
	os << "]" << endl;

	os << endl << "==>  regs: [" << endl;
	for (size_t i = 0; i < cond.regs.size(); ++i) {
		if (i > 0) { os << "," << endl; }
		os << "<" << cond.regs[i].first << ">{" << endl << cond.regs[i].second.to_string() << endl << "}";
	}
	os << "]";
	return os;
}

auto Condition::operator<=>(const Condition &other) const {
	if (auto cmp = uid <=> other.uid; cmp != 0) return cmp;
	if (auto cmp = areSame(valid, other.valid); !cmp) return std::strong_ordering::less; // assume areSame returns bool
	if (auto cmp = ins <=> other.ins; cmp != 0) return cmp;

	if (auto cmp = outs.size() <=> other.outs.size(); cmp != 0) return cmp;
	for (size_t i = 0; i < outs.size(); ++i) {
		if (auto cmp = outs[i].first <=> other.outs[i].first; cmp != 0) return cmp;
		if (!areSame(outs[i].second, other.outs[i].second)) return std::strong_ordering::less;
	}

	if (auto cmp = regs.size() <=> other.regs.size(); cmp != 0) return cmp;
	for (size_t i = 0; i < regs.size(); ++i) {
		if (auto cmp = regs[i].first <=> other.regs[i].first; cmp != 0) return cmp;
		if (!areSame(regs[i].second, other.regs[i].second)) return std::strong_ordering::less;
	}

	return std::strong_ordering::equal;
}

void Condition::req(Operand out, Expression expr) {
	if (not out.isVar()) {
		return;
	}
	outs.push_back({out.index, expr});
}

void Condition::mem(Operand mem, Expression expr) {
	if (not mem.isVar()) {
		return;
	}
	regs.push_back({mem.index, expr});
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
