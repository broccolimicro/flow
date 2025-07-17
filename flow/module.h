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

struct Statement {
	// Assignment
	Statement(int net, Expression expr, bool blocking=false);

	// If/Else
	Statement(Expression expr=Expression::boolOf(true), vector<Statement> stmts=vector<Statement>());
	~Statment();

	enum {
		ASSIGN = 0,
		BLOCK = 1
	};

	int type;

	int net;
	bool blocking;
	
	Expression expr;
	vector<Statement> sub;
};

struct Trigger {
	Trigger(Expression clk=Operand(true), vector<Statement> stmts=vector<Statement>());
	~Trigger();

	Expression clk;
	vector<Statement> stmts;
};

struct Module {
	string name;
	vector<Net> nets;
	vector<ValRdy> chans;
	int reset;
	int clk;

	vector<Statement> stmts;
	vector<Trigger> triggers;

	int netIndex(string) const;
	int netIndex(string, bool define=false);
	string netAt(int uid) const;
	int netCount() const;

	int pushNet(string name, Type type=Type(Type::BITS, 1), int purpose=Net::WIRE);
};

}
