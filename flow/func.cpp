#include "func.h"

#include <parse_expression/expression.h>
#include <interpret_arithmetic/export.h>
#include <ucs/variable.h>

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

Func::Func() {
}

Func::~Func() {
}

Port::Port() {
}

Port::Port(int index, Type type) {
	this->index = index;
	this->type = type;
}

Port::~Port() {
}

Register::Register() {
}

Register::Register(int index, Type type) {
	this->index = index;
	this->type = type;
}

Register::~Register() {
}

Condition::Condition() {
	index = -1;
}

Condition::Condition(int index, expression req, expression en) {
	this->index = index;
	this->req = req;
	this->en = en;
}

Condition::~Condition() {
}

int Func::pushFrom(string name, Type type) {
	int index = vars.define(ucs::variable(name));
	from.push_back(Port(index, type));
	return index;
}

int Func::pushTo(string name, Type type) {
	int index = vars.define(ucs::variable(name));
	to.push_back(Port(index, type));
	return index;
}

int Func::pushReg(string name, Type type) {
	int index = vars.define(ucs::variable(name));
	regs.push_back(Register(index, type));
	return index;
}

int Func::pushCond(expression req, expression en) {
	int index = vars.define(ucs::variable("case_" + ::to_string(cond.size())));
	cond.push_back(Condition(index, req, en));
	return index;
}

Port &Func::portAt(int index) {
	for (int i = 0; i < (int)from.size(); i++) {
		if (from[i].index == index) {
			return from[i];
		}
	}

	for (int i = 0; i < (int)to.size(); i++) {
		if (to[i].index == index) {
			return to[i];
		}
	}
	throw std::out_of_range("port not found");
}

Register &Func::regAt(int index) {
	for (int i = 0; i < (int)regs.size(); i++) {
		if (regs[i].index == index) {
			return regs[i];
		}
	}

	throw std::out_of_range("reg not found");
}

}
