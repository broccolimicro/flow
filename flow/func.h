#pragma once

#include <common/net.h>
#include <arithmetic/expression.h>

#include <stdio.h>
#include <string>
#include <vector>

using namespace std;
using arithmetic::Expression;
using arithmetic::Operand;

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

	auto operator<=>(const Type &t) const = default;
	friend std::ostream& operator<<(std::ostream& os, const Type& t);
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
	Condition(int uid, Expression valid);
	~Condition();

	// index into nets
	int uid;

	// expression of IN and REG -> is_valid(expr(valid, data)) evaluate encodings!
	Expression valid;

	// derive ready from outs
	// Output for each condition
	// expression of IN and REG -> expr(data)
	vector<pair<int, Expression> > outs;
	// Value to write for each condition
	// expression of IN and REG -> expr(data)
	vector<pair<int, Expression> > regs;
	vector<int> ins;

	void req(Operand out, Expression expr);
	void mem(Operand mem, Expression expr);
	void ack(Operand in);
	void ack(vector<Operand> in);

	auto operator<=>(const Condition &cond);
	friend std::ostream& operator<<(std::ostream& os, const Condition& cond);
};

bool operator==(const Condition &c1, const Condition &c2);
bool operator!=(const Condition &c1, const Condition &c2);


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

	int netIndex(string name) const;
	int netIndex(string name, bool define=false);
	string netAt(int uid) const;
	int netCount() const;

	Operand pushNet(string name, Type type=Type(Type::BITS, 1), int purpose=Net::NONE);
	int pushCond(Expression valid);
	friend std::ostream& operator<<(std::ostream& os, const Func& f);
};

bool operator==(const Func &f1, const Func &f2);
bool operator!=(const Func &f1, const Func &f2);
}
