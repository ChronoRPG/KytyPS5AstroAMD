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
};

[[nodiscard]] const CodegenOptions& GetCodegenOptions();
// Test hook: replaces the options for subsequent compilations. Not thread-safe; call it only
// while no shader is being compiled.
void SetCodegenOptions(const CodegenOptions& options);

} // namespace Libs::Graphics::ShaderRecompiler

#endif // EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_
