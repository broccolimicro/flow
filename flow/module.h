#pragma once

#include <common/net.h>
#include <arithmetic/expression.h>

#include <vector>
#include <string>
#include <stdio.h>

using namespace std;
using arithmetic::Expression;
using arithmetic::Operand;

namespace clocked {

struct Type {
	enum TypeName : int {
		BITS = 0,
		FIXED = 1
	};

	Type(TypeName type=TypeName::FIXED, int width=1, int shift=0);
	~Type();

	TypeName type;
	int width;
	int shift;

	auto operator<=>(const Type &t) const = default;
	friend std::ostream& operator<<(std::ostream& os, const Type& t);
};

struct Net {
	enum Purpose {
		WIRE = 0,
		IN = 1,
		OUT = 2,
		REG = 3,
	};

	Net();
	Net(string name, Type type=Type(Type::TypeName::BITS, 1), Purpose purpose=Purpose::WIRE);
	~Net();

	string name;
	Type type;
	Purpose purpose;

	auto operator<=>(const Net &n) const = default;
	friend std::ostream& operator<<(std::ostream& os, const Net& n);
};

struct Channel {
	Channel();
	~Channel();

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
	bool isChained = false; // True for chained else-if sequences & false for parallel ifs
};

struct Block {
	Block(Expression clk=Operand(true), vector<Rule> rules=vector<Rule>());
	~Block();

	Expression clk;
	vector<Assign> reset;
	vector<Rule> rules;
	vector<Rule> _else;  //TODO: support proper nesting, even if trailing branch is sufficient for our templates
};

struct Module {
	string name;
	vector<Net> nets;
	vector<Channel> chans;
	int reset;
	int clk;

	vector<Assign> assign;
	vector<Block> blocks;

	int netIndex(string) const;
	int netIndex(string, bool define=false);
	string netAt(int uid) const;
	int netCount() const;

	int pushNet(string name, Type type=Type(Type::TypeName::BITS, 1), Net::Purpose purpose=Net::Purpose::WIRE);
};

}
