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
	enum TypeName : size_t {
		BITS = 0,
		FIXED = 1,
	};

	Type(TypeName type=TypeName::FIXED, size_t width=1, int shift=0);
	~Type();

	TypeName type;
	size_t width;
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

struct Statement {
	// Assignment
	Statement();
	Statement(int net, Expression expr, bool blocking=false);

	// If/Else
	Statement(bool elif, vector<Statement> stmts=vector<Statement>(), Expression expr=Expression::boolOf(true));
	~Statement();

	enum StatementType {
		ASSIGN = 0,
		IF = 1,
		ELIF = 2,
	};

	StatementType type;

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
	vector<Channel> chans;
	int reset;
	int clk;

	vector<Statement> stmts;
	vector<Trigger> triggers;

	int netIndex(string) const;
	int netIndex(string, bool define=false);
	string netAt(int uid) const;
	int netCount() const;

	size_t pushNet(string name, Type type=Type(Type::TypeName::BITS, 1), Net::Purpose purpose=Net::Purpose::WIRE);
};

}
