#pragma once

#include "func.h"
#include "module.h"

namespace flow {

clocked::Type synthesize_type(const flow::Type &type);
void synthesize_chan(clocked::Module &mod, const flow::Net &net);
clocked::Module synthesizeModuleFromFunc(const Func &func, bool debug=false);
arithmetic::Expression synthesizeExpressionProbes(const arithmetic::Expression &e, const Mapping<size_t> &ChannelToValid, const Mapping<size_t> &ChannelToData);

}
