#pragma once

#include <arithmetic/expression.h>

#include <vector>
#include <string>
#include <stdio.h>

using namespace std;
using arithmetic::Expression;
using arithmetic::Operand;

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
};

struct ValRdy {
	ValRdy();
	~ValRdy();

	int valid;
	int ready;
	int data;

	Operand getValid();
	Operand getReady();
	Operand getData();
};

struct Assign {
	Assign();
	Assign(int net, Expression expr, bool blocking=false);
	~Assign();

	int net;
	Expression expr;
	bool blocking;
};

struct Rule {
	Rule(vector<Assign> assign=vector<Assign>(), Expression guard=Operand(true));
	~Rule();

	Expression guard;
	vector<Assign> assign;
};

struct Block {
	Block(Expression clk=Operand(true), vector<Rule> rules=vector<Rule>());
	~Block();

	Expression clk;
	vector<Assign> reset;
	vector<Rule> rules;
};

struct Module {
	string name;
	vector<Net> nets;
	vector<ValRdy> chans;
	int reset;
	int clk;

	vector<Assign> assign;
	vector<Block> blocks;

	int netIndex(string name, int region=0) const;
	int netIndex(string name, int region=0, bool define=false);
	pair<string, int> netAt(int uid) const;
	int netCount() const;

	int pushNet(string name, Type type=Type(Type::BITS, 1), int purpose=Net::WIRE);
};

}
