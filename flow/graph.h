#pragma once

#include <arithmetic/expression.h>

#include <vector>

using namespace std;

namespace flow {

struct Operator {
	Operator();
	~Operator();

	struct Port {
		int index;
		// Only receives or sends if data is valid
		// For output ports, the data is copied to the output.
		// For input ports, validity is the only part that matters.
		arithmetic::expression data;
	};

	vector<Port> from;
	vector<Port> to;
};

struct Value {
	Value();
	~Value();

	enum {
		BITS = 0,
		FIXED = 1
	};

	int type;
	int width;
	int shift;
};

struct Graph {
	Graph();
	~Graph();

	vector<Operator> operators;
	vector<Value> values;
};

}
