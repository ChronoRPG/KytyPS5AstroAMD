#pragma once

#include "graphics/shader/recompiler/ir/ShaderIR.h"

namespace Libs::Graphics::ShaderRecompiler::IR {

struct ExecSelectStats {
	// Selects whose condition a dominating per-invocation branch already decided.
	uint32_t branch_known = 0;
	// Select arms that were selects implied by (true arm) or implying (false arm) the condition.
	uint32_t nested_folds = 0;
	// Selects Select(c, t, f) whose value is only ever observed in lanes where c holds.
	uint32_t masked_uses = 0;
};

// An EXEC-masked VGPR write is Select(exec, new, old). This pass replaces such merges (any
// Select, in fact) by the arm every observer sees:
// - per_invocation_branches (one guest lane per host invocation, lane_count 1): the host takes
//   each conditional branch per invocation, so a block entered only through a branch edge knows
//   the branch condition (s_cbranch_execz regions know EXEC is set).
// - Every stage: Select(c, Select(e, t, f), z) with c => e reads t, Select(c, x, Select(e, t, f))
//   with e => c reads f (a VGPR written twice under one EXEC keeps only the first old value).
// - Every stage: when each use of Select(c, t, f) is masked by c (the true arm of a select or an
//   AND on c, or an access guarded by c) or feeds lane-local arithmetic whose own uses are, lanes
//   without c never observe the value, so t can replace it. A loop phi that only feeds the false
//   arms of such selects carries nothing observable (a greatest fixpoint over the selects). Other
//   phis, cross-lane operations, implicit-derivative samples and unguarded side effects count as
//   observations.
// Instructions the resource plan references stay untouched. The pass removes the code it made
// dead itself.
[[nodiscard]] ExecSelectStats EliminateExecSelects(Program& program, bool per_invocation_branches);

} // namespace Libs::Graphics::ShaderRecompiler::IR
