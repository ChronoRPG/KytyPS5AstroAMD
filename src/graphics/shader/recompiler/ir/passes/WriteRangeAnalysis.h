#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_WRITERANGEANALYSIS_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_WRITERANGEANALYSIS_H_

#include "graphics/shader/recompiler/ir/ShaderIR.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler::IR {

// Precise write ranges (KYTY_PRECISE_WRITE_RANGES). For every storage buffer the shader writes,
// records how each store/atomic address is computed from values whose bounds are known at compile
// time (constants, workgroup-local thread ids, lane ids) or when the dispatch/draw is recorded
// (user data, flattened SRT dwords, direct dispatch size). Any other source (memory loads, lane
// shuffles, float conversions, loop-carried values, ...) is unknown, and an unknown address keeps
// the whole binding written. Requires the specialized, bound program (after AllocateBindings), so
// the tape matches the emitted SPIR-V: stride/swizzle/ADD_TID come from the specialization and
// user-data leaves from the bound register list.
void AnalyzeBufferWriteRanges(Program& program, ShaderStageInputInfo input_info);

struct WriteRangeInputs {
	std::span<const uint32_t> user_data;
	std::span<const uint32_t> flattened_srt;
	std::array<uint32_t, 3>   groups {};
	bool                      has_groups = false;
};

// Byte range [begin, end) relative to the binding base.
struct WriteRangeSpan {
	uint64_t begin = 0;
	uint64_t end   = 0;

	bool operator==(const WriteRangeSpan& other) const = default;
};

// Evaluates a sealed WriteRangeProgram. Reusable; not thread-safe (one per recording thread).
class WriteRangeEvaluator {
public:
	static constexpr size_t MaxSpans = 8;

	// Evaluates every node once for these inputs.
	void Evaluate(const WriteRangeProgram& program, const WriteRangeInputs& inputs);

	// Written spans of `buffer` clipped to [0, size), sorted and merged (at most MaxSpans). Returns
	// false when some store of the buffer is unbounded: the caller must treat [0, size) as written.
	// An empty span list with true means no store can reach the binding.
	[[nodiscard]] bool Spans(const WriteRangeProgram& program, uint32_t buffer, uint64_t size,
	                         std::vector<WriteRangeSpan>& spans) const;

	// Diagnostic description of a buffer's accesses with their evaluated address intervals.
	[[nodiscard]] std::string Describe(const WriteRangeProgram& program, uint32_t buffer) const;

private:
	struct Interval {
		uint64_t lo = 0;
		uint64_t hi = UINT32_MAX;
	};
	[[nodiscard]] bool AccessSpan(const BufferWriteRange& buffer, const WriteRangeAccess& access,
	                              WriteRangeSpan& span) const;

	std::vector<Interval> m_values;
	const WriteRangeProgram* m_program = nullptr;
};

} // namespace Libs::Graphics::ShaderRecompiler::IR

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_WRITERANGEANALYSIS_H_ */
