#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_DESCRIPTORSETREUSE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_DESCRIPTORSETREUSE_H_

#include "graphics/host_gpu/vulkanCommon.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Libs::Graphics {

// Descriptor sets written earlier in the same command buffer, by layout and exact contents
// (KYTY_DESCRIPTOR_SET_REUSE). Used for renderer layouts beyond maxPushDescriptors, which
// otherwise take a fresh set from the descriptor heap and vkUpdateDescriptorSets every commit.
//
// An entry is valid only for the command buffer (scheduler tick) that wrote it:
//  - every object a descriptor refers to (image views, buffer-cache buffers, the stream buffer,
//    samplers) is destroyed only by deferred operations that run after the tick that could use
//    it has completed, so while a tick is recorded equal handles denote the same live objects;
//  - the heap resets a pool only after the tick that retired it completed, and a set written
//    during tick T comes from a pool retired at T or later, so the set is intact until T ends;
//  - stream-buffer ranges (flattened SRT, shader data) are not overwritten during the tick that
//    allocated them;
//  - a written set is never updated again, so binding it again is binding the same contents.
class DescriptorSetReuse {
public:
	[[nodiscard]] static uint64_t Hash(vk::DescriptorSetLayout                  layout,
	                                   std::span<const vk::WriteDescriptorSet> writes);
	// A set with this layout and exactly these writes (binding, element, type, count and every
	// buffer/image/sampler info) recorded during `tick`, or null.
	[[nodiscard]] vk::DescriptorSet Find(uint64_t tick, vk::DescriptorSetLayout layout,
	                                     std::span<const vk::WriteDescriptorSet> writes,
	                                     uint64_t                                hash) const;
	void Insert(uint64_t tick, vk::DescriptorSetLayout layout,
	            std::span<const vk::WriteDescriptorSet> writes, uint64_t hash,
	            vk::DescriptorSet set);

private:
	struct Entry {
		uint64_t                              tick   = 0;
		uint64_t                              hash   = 0;
		vk::DescriptorSetLayout               layout = nullptr;
		vk::DescriptorSet                     set    = nullptr;
		std::vector<vk::WriteDescriptorSet>   writes; // info pointers cleared
		std::vector<vk::DescriptorBufferInfo> buffers;
		std::vector<vk::DescriptorImageInfo>  images;
	};
	static constexpr uint32_t Slots = 64;

	[[nodiscard]] static bool Matches(const Entry& entry, uint64_t tick,
	                                  vk::DescriptorSetLayout                  layout,
	                                  std::span<const vk::WriteDescriptorSet> writes,
	                                  uint64_t                                 hash);

	std::array<Entry, Slots> m_entries {};
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_DESCRIPTORSETREUSE_H_
