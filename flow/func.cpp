#include <compare>
#include <stdexcept>

#include <arithmetic/expression.h>
#include <interpret_arithmetic/export.h>
#include <parse_expression/expression.h>

#include "func.h"

using arithmetic::Expression;
using arithmetic::Operand;

namespace flow {

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

Net::Net(string name, Type type, Purpose purpose) {
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

bool operator==(const Condition &c1, const Condition &c2) {
    if (c1.uid != c2.uid ||
        !areSame(c1.valid, c2.valid) ||
        c1.outs.size() != c2.outs.size() ||
        c1.regs.size() != c2.regs.size() ||
        c1.ins.size() != c2.ins.size()) {
        return false;
    }

    // Compare outs vector
    for (size_t i = 0; i < c1.outs.size(); ++i) {
        if (c1.outs[i].first != c2.outs[i].first ||
            !areSame(c1.outs[i].second, c2.outs[i].second)) {
            return false;
        }
    }

    // Compare regs vector
    for (size_t i = 0; i < c1.regs.size(); ++i) {
        if (c1.regs[i].first != c2.regs[i].first ||
            !areSame(c1.regs[i].second, c2.regs[i].second)) {
            return false;
        }
    }

    // Compare ins vector (simple integers)
    for (size_t i = 0; i < c1.ins.size(); ++i) {
        if (c1.ins[i] != c2.ins[i]) {
            return false;
        }
    }

    return true;
}

bool operator!=(const Condition &c1, const Condition &c2) {
    return !(c1 == c2);
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

Operand Func::pushNet(string name, Type type, Net::Purpose purpose) {
	int uid = (int)nets.size();
	nets.push_back(Net(name, type, purpose));
	return Operand::varOf(uid);
}

int Func::pushCond(Expression valid) {
	int uid = (int)nets.size();
	int index = (int)conds.size();
	conds.push_back(Condition(uid, valid));
	nets.push_back(Net("branch_" + ::to_string(index), Type(Type::TypeName::BITS, 1), Net::Purpose::COND));
	return index;
}

std::ostream& operator<<(std::ostream& os, const Func& func) {
	os << "Func: " << func.name << "\n";

	// Output nets
	os << "  Nets (" << func.nets.size() << "):\n";
	for (size_t i = 0; i < func.nets.size(); ++i) {
		const auto& net = func.nets[i];
		const char* purpose = "UNKNOWN";
		switch (net.purpose) {
			case Net::Purpose::NONE: purpose = "NONE"; break;
			case Net::Purpose::IN: purpose = "IN"; break;
			case Net::Purpose::OUT: purpose = "OUT"; break;
			case Net::Purpose::REG: purpose = "REG"; break;
			case Net::Purpose::COND: purpose = "COND"; break;
		}
		os << "    [" << i << "] " << net.name
			<< " (type: " << net.type
			<< ", purpose: " << purpose << ")\n";
	}

	// Output conditions
	os << "  Conditions (" << func.conds.size() << "):\n";
	for (size_t i = 0; i < func.conds.size(); ++i) {
		const auto& cond = func.conds[i];
		os << "    Condition " << i << " (uid: " << cond.uid << "):\n";
		os << "      Valid: " << cond.valid << "\n";

		// Output outputs
		if (!cond.outs.empty()) {
			os << "      Outputs:\n";
			for (const auto& out : cond.outs) {
				os << "        [" << out.first << "] = " << out.second << "\n";
			}
		}

		// Output registers
		if (!cond.regs.empty()) {
			os << "      Registers:\n";
			for (const auto& reg : cond.regs) {
				os << "        [" << reg.first << "] = " << reg.second << "\n";
			}
		}

		// Output inputs
		if (!cond.ins.empty()) {
			os << "      Inputs: ";
			for (size_t j = 0; j < cond.ins.size(); ++j) {
				if (j > 0) os << ", ";
				os << cond.ins[j];
			}
			os << "\n";
		}
	}

	return os;
}

bool operator==(const Func &f1, const Func &f2) {
	// Compare simple members
	if (f1.name != f2.name ||
			f1.nets.size() != f2.nets.size() ||
			f1.conds.size() != f2.conds.size()) {
		return false;
	}

	// Compare nets vector
	for (size_t i = 0; i < f1.nets.size(); ++i) {
		const auto& net1 = f1.nets[i];
		const auto& net2 = f2.nets[i];

		if (net1.name != net2.name ||
				net1.type != net2.type ||
				net1.purpose != net2.purpose) {
			return false;
		}
	}

	// Compare conditions vector
	for (size_t i = 0; i < f1.conds.size(); ++i) {
		if (!(f1.conds[i] == f2.conds[i])) {
			return false;
		}
	}

	return true;
}

bool operator!=(const Func &f1, const Func &f2) {
	return !(f1 == f2);
}

}
