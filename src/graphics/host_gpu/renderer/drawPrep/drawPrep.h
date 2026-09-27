#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_DRAWPREP_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_DRAWPREP_H_

#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/drawPrep/readSet.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"
#include "graphics/host_gpu/renderer/render.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

// Draw-prep S5/S6: prepare-then-commit for direct draws.
//
// KYTY_DRAW_PREP=off (default): nothing here runs; the command processor executes draws exactly
// as before.
// KYTY_DRAW_PREP=inline (S5): each direct draw snapshots the registers, is prepared on the
// command processor (GPU) thread from that snapshot with every guest read recorded, and is then
// committed: the command buffer reads the snapshot, the certificate is checked where the serial
// path would call GetGraphicsPrograms, and the prepared programs are used when it holds (the
// serial preparation runs otherwise). This is the whole protocol without threads.
// KYTY_DRAW_PREP=parallel (S6): the command processor keeps parsing while DrawPrep workers
// prepare the published draws of a window; packets that could change what a later draw reads
// (fences) first commit the whole window in guest order.
//
// KYTY_DRAW_PREP_VERIFY=1|exit: after every committed preparation the serial preparation runs
// on copies and the outputs are compared (logged and counted; "exit" stops on a difference).
// KYTY_DRAW_PREP_CERT=value (default)|log: see Validate() in drawPrep.cpp.
namespace Libs::Graphics {

class RenderContext;
struct ShaderVertexInputInfo;
struct ShaderPixelInputInfo;

namespace DrawPrep {

enum class Mode : uint8_t { Off, Inline, Parallel };
enum class CertMode : uint8_t { Value, Log };

[[nodiscard]] Mode     GetMode();
[[nodiscard]] int      VerifyMode(); // 0 off, 1 count/log, 2 exit on difference
[[nodiscard]] CertMode GetCertMode();
// The command processor's per-packet hook (window fences and the S0 histogram) is needed.
[[nodiscard]] bool PacketHookEnabled();

// The register state a draw reads, copied when the draw packet is parsed.
struct RegisterSnapshot {
	HW::Context    context;
	HW::UserConfig user_config;
	HW::Shader     shaders;
};

enum class Failure : uint8_t {
	None,
	Ineligible,
	Unclean,
	Backing,
	Overflow,
	Inconsistent,
	Uncertified,
	NotPublished,
	ShaderMap,
	CertUnclean,
	CertChanged,
	CoherenceLog,
	Mismatch,
};

// One draw's speculative preparation and its certificate. Reused across draws (vectors keep
// their capacity).
struct PreparedDraw {
	bool                                                ok      = false;
	Failure                                             failure = Failure::None;
	bool                                                pixel_active = false;
	std::array<Prospero::ColorComponentMapping, 8>      target_export_mapping {};
	PipelineCache::GraphicsPrograms                     programs;
	ShaderVertexInputInfo                               vertex_info;
	ShaderPixelInputInfo                                pixel_info;
	PipelineCache::StagePrep                            vertex_prep;
	PipelineCache::StagePrep                            pixel_prep;
	ReadSet                                             reads;
	uint64_t                                            coherence_generation  = 0;
	uint64_t                                            shader_map_generation = 0;
};

// The pure preparation of one draw from a register snapshot. `exact`: the caller is the GPU
// thread (exact clean predicate for reads); otherwise a DrawPrep worker. Never touches the
// texture/buffer caches, the scheduler or Vulkan, and never faults on GPU-owned memory.
void Prepare(PipelineCache& pipeline_cache, const RegisterSnapshot& registers, bool exact,
             PreparedDraw& prepared);

// GPU thread, at the point where the serial path would prepare the programs: whether the
// prepared outputs equal what the serial preparation would produce now. Counts the committed
// draw or the fallback reason.
[[nodiscard]] bool Validate(PreparedDraw& prepared, bool pixel_active,
                            std::span<const Prospero::ColorComponentMapping, 8> target_export_mapping);

// KYTY_DRAW_PREP_VERIFY: compares committed outputs with a serial preparation's. Returns true
// when equal; otherwise counts, logs and (exit mode) stops.
bool VerifyCommitted(const PipelineCache::GraphicsPrograms& programs,
                     const ShaderVertexInputInfo& vertex_info, const ShaderPixelInputInfo& pixel_info,
                     const PipelineCache::GraphicsStagePreps& preps,
                     const PipelineCache::GraphicsPrograms& serial_programs,
                     const ShaderVertexInputInfo&            serial_vertex_info,
                     const ShaderPixelInputInfo&             serial_pixel_info,
                     const PipelineCache::GraphicsStagePreps& serial_preps);

// PM4 packet classes for the preparation window.
enum class PacketClass : uint8_t {
	WindowSafe, // only CP register/state writes, markers, control flow: no drain needed
	Draw,       // direct draw
	Fence,      // anything else: commit the window first
};
[[nodiscard]] PacketClass ClassifyPacket(uint32_t header, const uint32_t* body,
                                         uint32_t remaining_dw);

enum class DrawKind : uint8_t { Index, Auto };

// Owned by the graphics command processor; used only on the GPU thread.
class Engine {
public:
	explicit Engine(RenderContext& renderer);
	~Engine();
	Engine(const Engine&)            = delete;
	Engine& operator=(const Engine&) = delete;

	// Takes a direct draw whose arguments the command processor has fully resolved. Returns
	// false when the caller must execute the draw serially now (the engine has committed every
	// earlier draw first).
	[[nodiscard]] bool Submit(uint64_t submit_id, const DrawIndexArgs* index_args,
	                          const DrawAutoArgs* auto_args, const HW::Context& context,
	                          const HW::UserConfig& user_config, const HW::Shader& shaders);
	// Commits every pending draw in submission order.
	void Drain();
	[[nodiscard]] bool Pending() const noexcept { return m_head != m_tail; }

	// Per-packet hook of the command processor (before the packet's handler runs).
	void OnPacket(PacketClass packet_class);

private:
	struct Slot;
	struct Shared;

	void Commit(Slot& slot);
	void CommitHead();
	void NoteFence();

	RenderContext&                     m_renderer;
	Mode                               m_mode;
	std::vector<std::unique_ptr<Slot>> m_slots;
	uint64_t                           m_head = 0; // next slot to commit
	uint64_t                           m_tail = 0; // next slot to publish
	uint64_t                           m_draws_since_fence = 0;
	std::shared_ptr<Shared>            m_shared;
};

} // namespace DrawPrep
} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_DRAWPREP_H_
