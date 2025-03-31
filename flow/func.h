#pragma once

#include <arithmetic/expression.h>

#include <vector>
#include <string>
#include <stdio.h>

#include <ucs/variable.h>

using namespace std;
using namespace arithmetic;

namespace flow {

struct Type {
	Type();
	Type(int type, int width, int shift=0);
	~Type();

	enum {
		BITS = 0,
		FIXED = 1
	};

	int type;
	int width;
	int shift;
};

struct Port {
	Port();
	Port(int index, Type type);
	~Port();

	// index to the valid/request if an input channel
	// index to the ready/acknowledge/enable if an output channel
	int index;
	Type type;

	// Based on cond for input ports
	// Based on cond, from, and regs for output ports
	expression token;
	expression value;
};

struct Register {
	Register();
	Register(int index, Type type);
	~Register();

	int index;
	Type type;
	expression write;
	expression value;
};

struct Condition {
	Condition();
	Condition(int index, expression req, expression en);
	~Condition();

	int index;
	expression req;
	expression en;
};

struct Func {
	Func();
	~Func();

	string name;
	ucs::variable_set vars;

	vector<Port> from;
	vector<Port> to;
	vector<Register> regs;

	// Based only on from, ready(to), and regs
	vector<Condition> cond;

	int pushFrom(string name, Type type);
	int pushTo(string name, Type type);
	int pushReg(string name, Type type);
	int pushCond(expression req, expression en);

	Port &portAt(int index);
	Register &regAt(int index);
};

}
