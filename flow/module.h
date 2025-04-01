#pragma once

#include <arithmetic/expression.h>

#include <vector>
#include <string>
#include <stdio.h>

using namespace std;
using namespace arithmetic;

namespace clocked {

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

struct Rule {
	Rule();
	Rule(expression assign, expression guard=true);
	~Rule();

	expression guard;
	expression assign;
};

struct Net {
	Net();
	Net(string name, Type type=Type(Type::BITS, 1), int purpose=WIRE);
	~Net();

	enum {
		WIRE = 0,
		IN = 1,
		OUT = 2,
		REG = 3,
	};

	string name;
	Type type;
	int purpose;
	vector<Rule> rules;
};

struct ValRdy {
	ValRdy();
	~ValRdy();

	int valid;
	int ready;
	int data;

	operand getValid();
	operand getReady();
	operand getData();
};

struct Module {
	string name;
	vector<Net> nets;
	vector<ValRdy> chans;
	int reset;
	int clk;

	int netIndex(string name, int region=0) const;
	int netIndex(string name, int region=0, bool define=false);
	pair<string, int> netAt(int uid) const;
	int netCount() const;

	int pushNet(string name, Type type=Type(Type::BITS, 1), int purpose=Net::WIRE);
};

}
