#ifndef EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_
#define EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_

#include <cstdint>

namespace Libs::Graphics::ShaderRecompiler {

// Switches for code-generation changes that must stay revertible at runtime. Every field is read
// from its environment variable once (first use); tests may replace the whole set. Programs are
// cached in memory only and the driver pipeline cache is keyed by the SPIR-V code, so changing a
// switch between runs needs no cache invalidation.
struct CodegenOptions {
	// KYTY_MOVREL_RANGE=0: keep V_MOVRELS/V_MOVRELD select chains over every VGPR above the base
	// instead of folding the compares that the M0 value set proves false.
	bool movrel_range = true;
	// KYTY_FAST_FMINMAX=0: emulate f32 min/max/min3/max3/med3 with two bit classifications per
	// min/max instead of one compare-and-select plus a single two-zeros test.
	bool fast_float_min_max = true;
	// KYTY_FAST_PKRTZ=0: convert V_CVT_PKRTZ_F16_F32 halves with the original select chain instead
	// of the shorter integer formulation.
	bool fast_pkrtz = true;
	// KYTY_SINGLE_F2I_SATURATION=0: repeat the float-to-int NaN/range saturation in the emitter even
	// though the translator's saturated conversion already guarantees an in-range operand.
	bool single_f2i_saturation = true;
	// KYTY_LOD_STATS_GATE=0: record GET_LOD_STATS feedback for every sample, including images whose
	// T# has no mip-statistics counter, and issue the finest-level AtomicUMin unconditionally.
	bool lod_stats_gate = true;
	// KYTY_ROBUST_BUFFER_LOADS=0: bounds-check every plain dword storage-buffer load in the shader
	// even when the device's robustBufferAccess2 already returns zero for out-of-range dwords.
	bool robust_buffer_loads = true;
};

[[nodiscard]] const CodegenOptions& GetCodegenOptions();
// Test hook: replaces the options for subsequent compilations. Not thread-safe; call it only
// while no shader is being compiled.
void SetCodegenOptions(const CodegenOptions& options);

} // namespace Libs::Graphics::ShaderRecompiler

#endif // EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_
