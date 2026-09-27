#ifndef EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_
#define EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_

#include <cstdint>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler {

// How V_MAD_F32/V_MAC_F32/V_MADMK_F32/V_MADAK_F32 (unfused on PS5: the product is rounded before
// the add) are emitted, and which float arithmetic may not be contracted by the host compiler.
enum class MadMode : uint8_t {
	// Every MAD is an FMul plus an FAdd, and every guest FMul/FAdd/FSub is NoContraction: bit
	// exact everywhere, at the cost of one extra instruction per MAD.
	Exact,
	// Exact on the data flow that feeds position exports (plus Invariant on the position built-in),
	// fused FMA elsewhere: positions computed by different shaders (depth pre-pass and main pass)
	// match bit for bit, while pixel-shader math keeps the cheaper FMA.
	Position,
	// Every MAD is a fused FMA and the host may contract freely (the behaviour before MadMode).
	Fused,
};

// Which pixel shaders start with EXEC holding only the non-helper invocations (KYTY_PS_LIVE_EXEC).
enum class PsLiveExec : uint8_t {
	// Every invocation starts with its EXEC bit set, helper invocations included.
	Off,
	// Pixel shaders that contain DS_APPEND or DS_CONSUME.
	AppendConsume,
	// Every pixel shader.
	All,
};

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
	// KYTY_MAD_MODE=exact|position|fused, see MadMode.
	MadMode mad_mode = MadMode::Position;
	// KYTY_INTERP_MODES=0: interpolate every pixel input at the pixel center with the shader-wide
	// perspective (NoPerspective on all inputs when LINEAR_CENTER is enabled) instead of per input
	// from the I/J pair its V_INTERP_P2 reads use (centroid, sample, linear).
	bool interp_modes = true;
	// KYTY_SAMPLE_OFFSETS=0: ignore the texel offsets of IMAGE_SAMPLE*_O (the behaviour before
	// U50) instead of applying them.
	bool sample_offsets = true;
	// KYTY_SAMPLE_LOD_CLAMP=0: ignore the LOD clamp of IMAGE_SAMPLE*_CL (the behaviour before U50)
	// instead of applying it as the sample's minimum LOD.
	bool sample_lod_clamp = true;
	// KYTY_HOST_FTZ_INPUTS=1: when the module declares DenormFlushToZero 32, leave the denormal
	// inputs of rcp/rsq/sqrt/exp/log to the host instead of flushing them in the shader. Vulkan
	// only says such operands "may" be flushed, so this is opt-in for hosts that pass
	// CodegenTranscendentalDenormInputs with "host FTZ declared" (the RTX 3090 on driver 610.74
	// has no f32 flush-to-zero, so it never applies there).
	bool host_ftz_inputs = false;
	// KYTY_EXEC_SELECTS=0: keep every EXEC-masked VGPR merge Select(exec, new, old) instead of
	// replacing the ones whose old value no lane can observe (IR::EliminateExecSelects).
	bool exec_selects = true;
	// KYTY_PS_APPEND_LIVE_ELECTION=1: in a pixel shader, DS_APPEND/DS_CONSUME elect the lane that
	// performs the counter atomic among the non-helper invocations only. A pixel shader's EXEC
	// includes the helper invocations of partially covered quads, and Vulkan discards a helper
	// invocation's atomic and leaves its result undefined, so electing a helper broadcast an
	// undefined base to the whole subgroup (duplicate append indices; Astro Bot's GI ray-bundle
	// linked lists then form cycles). The added count stays popcount(EXEC), so the per-lane
	// indices a shader derives with V_MBCNT stay unique.
	bool ps_append_live_election = false;
	// KYTY_PS_LIVE_EXEC=1|all: pixel shaders that contain DS_APPEND/DS_CONSUME (1) or all pixel
	// shaders (all) start with EXEC holding only the non-helper invocations. On the PS5 the
	// initial EXEC is the pixel valid mask and S_WQM_B64 adds the helper lanes of partially
	// covered quads; with this off EXEC starts full, so the exact-mode EXEC a shader restores
	// before its stores and atomics still holds the helpers. A DS_APPEND then also counts and
	// indexes the helper lanes: no duplicates with KYTY_PS_APPEND_LIVE_ELECTION, but slots that
	// nothing writes (Astro Bot's mesh-particle emitter 0x4dd1f85484fc31f2 leaves unwritten
	// particle slots, its GI ray-bundle shaders unlinked list nodes).
	PsLiveExec ps_live_exec = PsLiveExec::Off;
	// KYTY_LOOP_GUARD=<n> with KYTY_LOOP_GUARD_SHADERS=<hash>[,<hash>...] (hexadecimal guest shader
	// hashes): a diagnostic for a GPU hang suspected in a shader loop. Every structured loop of a
	// listed shader counts iterations against one per-invocation budget; an invocation that has
	// run more than <n> iterations takes each loop's exit edge, and at return it adds one to the
	// last GDS dword, which the command processor reports at flips ("Loop guard"). It changes the
	// guarded shaders' results when it fires. Off unless both variables are set.
	uint32_t              loop_guard_budget = 0;
	std::vector<uint64_t> loop_guard_shaders;
	// KYTY_SRT_VARIANT_READS=1: a scalar read whose address is only known inside the shader (not a
	// valid runtime value: e.g. a BVH traversal's loop-carried instance pointer, or data the GPU
	// produces) is planned as a runtime read instead of a flat SRT slot. A flat slot is evaluated
	// once before the dispatch, so such a slot always fails to evaluate and the whole dispatch is
	// dropped. An S_BUFFER_LOAD through a V# read that way then reads through BDA, and a program with
	// another such descriptor (no BDA path) is dropped, as before. Reads whose address can be
	// evaluated before the dispatch keep their flat slots.
	bool srt_variant_reads = false;
};

// True when KYTY_LOOP_GUARD applies to the guest shader with this hash.
[[nodiscard]] bool LoopGuardApplies(uint64_t shader_hash);

[[nodiscard]] const CodegenOptions& GetCodegenOptions();
// Test hook: replaces the options for subsequent compilations. Not thread-safe; call it only
// while no shader is being compiled.
void SetCodegenOptions(const CodegenOptions& options);

} // namespace Libs::Graphics::ShaderRecompiler

#endif // EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENOPTIONS_H_
