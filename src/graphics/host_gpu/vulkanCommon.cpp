#include "graphics/host_gpu/vulkanCommon.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Libs::Graphics {

std::string VulkanToString(vk::Result value) {
	return vk::to_string(value);
}

std::string VulkanToString(vk::Format value) {
	return vk::to_string(value);
}

std::string VulkanToString(vk::ImageLayout value) {
	return vk::to_string(value);
}

std::string VulkanToString(vk::QueueFlags value) {
	return vk::to_string(value);
}

} // namespace Libs::Graphics
