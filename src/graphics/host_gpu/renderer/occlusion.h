#ifndef KYTY_RENDERER_OCCLUSION_H_
#define KYTY_RENDERER_OCCLUSION_H_

#include "graphics/host_gpu/vulkanCommon.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace Libs::Graphics {
class Buffer;
class RenderContext;

// GPU-resident cumulative counter. A guest dump is reduced by the GPU into a private
// host-visible slot and published into guest memory by a completion callback once that GPU
// work has finished, so results (including the ready bit) appear no earlier than on hardware.
// Publishing from the host keeps the dump pages CPU-owned: the game keeps EOP labels on the
// same pages, and GPU-owned dump pages made every label write fault and drain the GPU.
class OcclusionCounter {
public:
	explicit OcclusionCounter(RenderContext& context);
	~OcclusionCounter();
	static bool Enabled();
	void Prepare(uint32_t control); // outside rendering
	void Begin();                  // after beginning guest rendering
	void End();                    // before ending guest rendering
	void Accumulate();             // after ending rendering; flush only when pool is full
	// Visibility-proxy handling (an end dump closing a depth-only scope, whose result Astro Bot
	// reads right after the next end-of-pipe label). KYTY_OCCLUSION_PROXY_MODE:
	//   defer-label (default): the CP continues; its next label write is deferred until this
	//                          dump's tick has completed and its result has been published.
	//   sync:                  the CP waits for the publication (BufferWait), as before.
	// KYTY_OCCLUSION_SYNC_PROXY=0 disables proxy handling (plain asynchronous publication).
	enum class ProxyMode { Off, Sync, DeferLabel };
	[[nodiscard]] static ProxyMode GetProxyMode();
	// Returns true when this dump is a proxy end dump the caller must order (see ProxyMode).
	[[nodiscard]] bool Dump(uint64_t address);
	// Proxy detection enabled (any mode but Off).
	[[nodiscard]] static bool SyncProxyDumps();
	// Publications run on the completion (priority) runner instead of the GPU thread's pending
	// operations: required whenever labels may be deferred, so a deferred label, registered later
	// on the same FIFO runner, is written only after the result it announces.
	[[nodiscard]] static bool PriorityPublication();
	// True while a dump has been recorded but not yet published to guest memory.
	[[nodiscard]] bool HasUnpublishedDumps() const noexcept {
		return m_published.load(std::memory_order_acquire) != m_issued;
	}
	[[nodiscard]] bool Active() const noexcept { return m_active; }
	// KYTY_OCCLUSION_GATE (default on, needs KYTY_GPU_OCCLUSION=1). The guest reads only
	// end - begin differences of the cumulative counter, taken by interleaved dump pairs (begin at
	// A, A % 16 == 0; end at A + 8). Samples of rendering instances begun while no pair is open
	// cannot reach any such difference, so they are not counted. Every dump ends rendering, so
	// an instance never straddles a pair boundary. Unexpected dump patterns disable the gate for
	// the rest of the process (always-on counting, as without the gate).
	[[nodiscard]] static bool GateEnabled();
	// True when a rendering instance begun now with DB_COUNT_CONTROL `control` would be counted.
	[[nodiscard]] bool WouldCount(uint32_t control) const noexcept;
	// Hang-trace diagnostics: the depth target of the latest counted scope, reported with the
	// next dump.
	void NoteScope(uint64_t depth_address, uint32_t width, uint32_t height, uint32_t colors,
	               bool has_depth, uint32_t depth_format) {
		m_last_scope = {depth_address, width, height, colors, has_depth, depth_format};
	}
private:
	static constexpr uint32_t PublishSlots    = 1024;
	static constexpr uint64_t PublishSlotSize = 256;
	void Initialize();
	void FlushPending();
	void Dispatch(uint32_t mode, vk::Buffer output, uint64_t offset, uint64_t range);
	[[nodiscard]] static bool ControlCounts(uint32_t control) noexcept {
		return (control & 1u) == 0 && (control & 0xf00u) != 0;
	}
	[[nodiscard]] bool GateOpen() const noexcept {
		return !GateEnabled() || m_gate_broken || !m_open_pairs.empty();
	}
	void UpdateOpenPairs(uint64_t address);
	void BreakGate(const char* reason, uint64_t address);
	static constexpr size_t MaxOpenPairs = 64;
	std::vector<uint64_t> m_open_pairs; // begin addresses of dump pairs awaiting their end
	bool m_gate_broken = false;
	static constexpr uint32_t QueryCapacity = 1024;
	RenderContext& m_context;
	vk::QueryPool m_pool;
	vk::DescriptorSetLayout m_descriptors;
	vk::PipelineLayout m_layout;
	vk::Pipeline m_pipeline;
	std::unique_ptr<Buffer> m_counter;
	std::unique_ptr<Buffer> m_result;
	std::unique_ptr<Buffer> m_publish;
	std::array<uint64_t, PublishSlots> m_slot_ticks {};
	uint64_t m_issued = 0;
	std::atomic<uint64_t> m_published {0};
	bool m_prepared = false;
	bool m_active = false;
	uint32_t m_pending = 0;
	uint32_t m_scopes_since_dump = 0; // hang-trace diagnostics only
	struct ScopeInfo {
		uint64_t depth_address = 0;
		uint32_t width         = 0;
		uint32_t height        = 0;
		uint32_t colors        = 0;
		bool     has_depth     = false;
		uint32_t depth_format  = 0;
	};
	ScopeInfo m_last_scope {}; // hang-trace diagnostics only
};
}
#endif
