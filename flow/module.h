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

	TypeName type;
	size_t width;
	int shift;

	Type(TypeName type=TypeName::FIXED, size_t width=1, int shift=0);
	~Type();

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

	string name;
	Type type;
	Purpose purpose;

	Net();
	Net(string name, Type type=Type(Type::TypeName::BITS, 1), Purpose purpose=Purpose::WIRE);
	~Net();

	auto operator<=>(const Net &n) const = default;
	friend std::ostream& operator<<(std::ostream& os, const Net& n);
};

struct Channel {
	enum Purpose {
		IN = 0,
		OUT = 1,
		REG = 2,
		COND = 3,
	};

	int valid;
	int ready;
	int data;

	Purpose purpose;

	Channel();
	~Channel();

	Operand getValid();
	Operand getReady();
	Operand getData();
};

struct Statement {
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

	// Assignment
	Statement();
	Statement(int net, Expression expr, bool blocking=false);

	// If/Else
	Statement(bool elif, vector<Statement> stmts=vector<Statement>(), Expression expr=Expression::boolOf(true));
	~Statement();
};

struct Trigger {
	Expression clk;
	vector<Statement> stmts;

	Trigger(Expression clk=Operand(true), vector<Statement> stmts=vector<Statement>());
	~Trigger();
};

struct Instance {
	string type;
	string name;
	vector<Expression> ports;

	string comment;

	Instance();
	Instance(string type, vector<Expression> ports=vector<Expression>());
	~Instance();
};

struct Module {
	string name;
	string comment;

	vector<Net> nets;
	vector<Channel> chans;
	int reset;
	int clk;

	vector<Statement> stmts;
	vector<Trigger> triggers;
	vector<Instance> inst;

	Module();
	~Module();

	Operand getClk();
	Operand getReset();

	int netIndex(string) const;
	int netIndex(string, bool define=false);
	string netAt(int uid) const;
	int netCount() const;

	size_t pushNet(string name, Type type=Type(Type::TypeName::BITS, 1), Net::Purpose purpose=Net::Purpose::WIRE);
};

}
