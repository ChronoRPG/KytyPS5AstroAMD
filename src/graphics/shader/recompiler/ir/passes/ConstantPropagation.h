#pragma once

#include "graphics/shader/recompiler/ir/Block.h"

namespace Libs::Graphics::ShaderRecompiler::IR {

void ConstantPropagationPass(const BlockList& blocks, uint32_t wave_size = 64);
struct Program;
// KYTY_FOLD_LANE_MASKS=1 (default off; Senaxx 5189ea360): reads of the current lane's bit of a
// wave mask that is known per lane (a ballot of a predicate, a constant, or bitwise logic of those)
// become that predicate. Returns how many; 0 when the switch is off.
uint32_t FoldLaneMasks(Program& program);

} // namespace Libs::Graphics::ShaderRecompiler::IR
