#ifndef KYTY_RENDERER_OCCLUSION_H_
#define KYTY_RENDERER_OCCLUSION_H_

#include "graphics/host_gpu/vulkanCommon.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

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
	// Returns true when the caller must wait for this dump's publication before continuing
	// (default on, KYTY_OCCLUSION_SYNC_PROXY=0 disables; this end dump closes a depth-only proxy scope).
	[[nodiscard]] bool Dump(uint64_t address);
	[[nodiscard]] static bool SyncProxyDumps();
	// True while a dump has been recorded but not yet published to guest memory.
	[[nodiscard]] bool HasUnpublishedDumps() const noexcept {
		return m_published.load(std::memory_order_acquire) != m_issued;
	}
	[[nodiscard]] bool Active() const noexcept { return m_active; }
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
