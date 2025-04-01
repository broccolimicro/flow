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

	// INPUTS:
	//   ready = active (expression of COND)       -> (~expr(valid) | expr(ready))
	//   value is not used
	// OUTPUTS:
	//   valid = active (expression of COND)       -> expr(valid & ready)
	//   data  = value  (expression of IN and REG) -> expr(data)
	// REGISTERS:
	//   write = active (expression of COND)       -> expr(valid & ready)
	//   data  = value  (expression of IN and REG) -> expr(data)
	// CONDITIONS:
	//   valid = active (expression of IN and REG) -> is_valid(expr(valid, data)) evaluate encodings!
	//   ready = value  (expression of OUT)        -> expr(ready)

	expression active;
	expression value;
};

struct Func {
	Func();
	~Func();

	string name;

	vector<Net> nets;

	int netIndex(string name, int region=0) const;
	int netIndex(string name, int region=0, bool define=false);
	pair<string, int> netAt(int uid) const;
	int netCount() const;

	int pushNet(string name, Type type=Type(Type::BITS, 1), int purpose=Net::NONE);
	int pushCond();
};

}
