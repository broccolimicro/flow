#pragma once

#include <arithmetic/expression.h>

#include <vector>
#include <string>
#include <stdio.h>

#include <ucs/variable.h>

#include "func.h"

using namespace std;
using namespace arithmetic;

namespace flow {

struct Arc {
	Arc();
	~Arc();

	int from;
	int fromPort;
	int to;
	int toPort;
};

struct Graph {
	Graph();
	~Graph();

	vector<Func> funcs;
	vector<Type> types;

	vector<int> nodes;
	vector<Arc> arcs;
};

}
