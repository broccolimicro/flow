#pragma once

#include "func.h"
#include "module.h"

namespace flow {

clocked::Type synthesize_type(const Type &type);
void synthesize_chan(clocked::Module &mod, const Net &net);
clocked::Module synthesize_valrdy(const Func &func);

}
