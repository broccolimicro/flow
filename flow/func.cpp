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

Func::Func() {
}

Func::~Func() {
}

int Func::netIndex(string name, int region) const {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			return i;
		}
	}

	return -1;
}

int Func::netIndex(string name, int region, bool define) {
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

pair<string, int> Func::netAt(int uid) const {
	return pair<string, int>(nets[uid].name, 0);
}

int Func::netCount() const {
	return (int)nets.size();
}

int Func::pushNet(string name, Type type, int purpose) {
	int index = (int)nets.size();
	nets.push_back(Net(name, type, purpose));
	return index;
}

int Func::pushCond() {
	static int count = 0;

	int index = (int)nets.size();
	nets.push_back(Net("case_" + ::to_string(count++), Type(Type::BITS, 1), Net::COND));
	return index;
}

}
