#pragma once

#include "func.h"
#include <verilog/module.h>

namespace flow {

Mapping<int> mapToVerilog(const flow::Func &func, const clocked::Module &mod);

struct Implementation {
	const flow::Func *func;
	clocked::Module *mod;
	Mapping<int> funcToMod;

	Implementation();
	Implementation(const flow::Func *func, clocked::Module *mod);
	~Implementation();
};

struct Linker {
	virtual Implementation find(const Instance &inst) = 0;
};

clocked::Type synthesizeChannelType(const flow::Type &type);
void synthesizeChannel(clocked::Module &mod, const flow::Net &net, clocked::Statement &resetBlock);
clocked::Module synthesizeModuleFromFunc(const Func &func, Linker *linker=nullptr, bool debug=false);

}
