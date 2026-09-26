#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DCCCLEAR_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DCCCLEAR_H_

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <cstdint>
#include <memory>

namespace Libs::Graphics {

class Buffer;
class CommandScheduler;
class Image;
struct GraphicContext;

// TextureCache owns eligibility and guest-memory tracking. This helper only records native work.
class DccClearHelper final {
public:
	DccClearHelper(GraphicContext& graphics, CommandScheduler& scheduler);
	~DccClearHelper();
	KYTY_CLASS_NO_COPY(DccClearHelper);

	[[nodiscard]] bool Supports(const Image& image, uint64_t metadata_size) const;
	// The image and canonical metadata buffer must remain alive through this scheduler tick.
	// Returns no acceptance result: both metadata validation and conditional writes run on the GPU.
	void Record(Image& image, vk::Buffer metadata, uint64_t metadata_offset,
	            uint64_t metadata_size, bool alpha_msb);

private:
	struct Push {
		uint32_t metadata_base_words;
		uint32_t metadata_words;
		uint32_t width;
		uint32_t height;
		uint32_t clear_groups;
		uint32_t alpha_msb;
	};

	static constexpr uint32_t WorkgroupSize = 128;
	static constexpr uint64_t MaxMetadataSize = 1024 * 1024;
	static constexpr uint64_t ScratchSize = 32;

	GraphicContext& m_graphics;
	CommandScheduler& m_scheduler;
	bool m_supported = false;
	vk::DescriptorSetLayout m_descriptor_layout = nullptr;
	vk::PipelineLayout m_pipeline_layout = nullptr;
	vk::Pipeline m_validate_pipeline = nullptr;
	vk::Pipeline m_clear_pipeline = nullptr;
	// Reuse is ordered by GPU barriers; no CPU recycling or completion callback is needed.
	std::unique_ptr<Buffer> m_scratch;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DCCCLEAR_H_
