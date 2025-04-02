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

struct Net {
	Net();
	Net(string name, Type type=Type(Type::BITS, 1), int purpose=NONE);
	~Net();

	enum {
		NONE = 0,
		IN = 1,
		OUT = 2,
		REG = 3,
		COND = 4,
	};

	string name;
	Type type;
	int purpose;
};

struct Condition {
	Condition();
	Condition(int uid, expression valid);
	~Condition();

	// index into nets
	int uid;

	// expression of IN and REG -> is_valid(expr(valid, data)) evaluate encodings!
	expression valid;

	// derive ready from outs
	// Output for each condition
	// expression of IN and REG -> expr(data)
	vector<pair<int, expression> > outs;
	// Value to write for each condition
	// expression of IN and REG -> expr(data)
	vector<pair<int, expression> > regs;
	vector<int> ins;
};

struct Input {
	Input();
	Input(int uid);
	~Input();

	// index into nets
	int uid;

	// Which conditions to acknowledge on
	vector<int> ack;
};

struct Func {
	Func();
	~Func();

	string name;

	vector<Net> nets;

	vector<Condition> conds;

	int netIndex(string name, int region=0) const;
	int netIndex(string name, int region=0, bool define=false);
	pair<string, int> netAt(int uid) const;
	int netCount() const;

	int pushNet(string name, Type type=Type(Type::BITS, 1), int purpose=Net::NONE);
	int pushCond(expression valid);
};

}
