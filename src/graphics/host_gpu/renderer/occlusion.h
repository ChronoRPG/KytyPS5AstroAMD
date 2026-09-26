#ifndef KYTY_RENDERER_OCCLUSION_H_
#define KYTY_RENDERER_OCCLUSION_H_

#include "graphics/host_gpu/vulkanCommon.h"
#include <memory>

namespace Libs::Graphics {
class Buffer;
class RenderContext;

// GPU-resident cumulative counter. Guest dumps use normal buffer-cache ownership
// and readback, including the ready bit; no host callback publishes an early result.
class OcclusionCounter {
public:
	explicit OcclusionCounter(RenderContext& context);
	~OcclusionCounter();
	static bool Enabled();
	void Prepare(uint32_t control); // outside rendering
	void Begin();                  // after beginning guest rendering
	void End();                    // before ending guest rendering
	void Accumulate();             // after ending rendering; flush only when pool is full
	void Dump(uint64_t address);
private:
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
	bool m_prepared = false;
	bool m_active = false;
	uint32_t m_pending = 0;
};
}
#endif
