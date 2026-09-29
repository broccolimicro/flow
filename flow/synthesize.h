#pragma once

#include "func.h"
#include <verilog/module.h>

namespace flow {

clocked::Type synthesizeChannelType(const flow::Type &type);
void synthesizeChannel(clocked::Module &mod, const flow::Net &net, clocked::Statement &resetBlock);
clocked::Module synthesizeModuleFromFunc(const Func &func, bool debug=false);

}
