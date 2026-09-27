#include "graphics/host_gpu/vulkanCommon.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-private-field"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "common/assert.h"
#include "common/hangTrace.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/host_gpu/graphicContext.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace Libs::Graphics {

namespace {
// Streamed textures churn through a few create-info classes (e.g. 2048^2/4096^2 BC4/BC5/BC7
// with full mip chains, 2.8-22 MB each) at hundreds of images per second. The pool must
// hold roughly one retirement period of that churn or most creates miss: 128 MB kept only
// ~25 2048^2 BC7 images. KYTY_NATIVE_IMAGE_POOL=0 disables the pool (it is on by default);
// KYTY_NATIVE_IMAGE_POOL_MB (default 1024, at most an eighth of the device budget) and
// KYTY_NATIVE_IMAGE_POOL_COUNT (default 1024) bound the retained images.
uint64_t RetiredImageByteLimit(const GraphicContext& graphics) {
	static const uint64_t configured = [] {
		const auto* value = std::getenv("KYTY_NATIVE_IMAGE_POOL_MB");
		return (value != nullptr ? std::strtoull(value, nullptr, 10) : 1024ull) * 1024ull * 1024ull;
	}();
	static const uint64_t limit = [&graphics] {
		const auto budget = graphics.GetTotalMemoryBudget();
		return budget != 0 ? std::min(configured, budget / 8) : configured;
	}();
	return limit;
}

size_t RetiredImageCountLimit() {
	static const size_t limit = [] {
		const auto* value = std::getenv("KYTY_NATIVE_IMAGE_POOL_COUNT");
		return value != nullptr ? static_cast<size_t>(std::strtoull(value, nullptr, 10)) : size_t {1024};
	}();
	return limit;
}

bool NativeImagePoolEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_NATIVE_IMAGE_POOL");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

bool CanRecycleImage(const vk::ImageCreateInfo& info) {
	const auto allowed_flags = vk::ImageCreateFlagBits::eMutableFormat |
	                           vk::ImageCreateFlagBits::eExtendedUsage |
	                           vk::ImageCreateFlagBits::eBlockTexelViewCompatible |
	                           vk::ImageCreateFlagBits::e2DArrayCompatible;
	return info.pNext == nullptr && info.pQueueFamilyIndices == nullptr &&
	       info.queueFamilyIndexCount == 0 && info.sharingMode == vk::SharingMode::eExclusive &&
	       info.tiling == vk::ImageTiling::eOptimal &&
	       info.initialLayout == vk::ImageLayout::eUndefined && !(info.flags & ~allowed_flags);
}

void DestroyNativeImage(VmaAllocator allocator, vk::Image image, VmaAllocation allocation) {
	Profiler::ScopedFrameWait timing(Profiler::FrameWait::NativeImageDestroy);
	vmaDestroyImage(allocator, image, allocation);
}
} // namespace

bool GraphicContext::CreateAllocator() {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(instance == nullptr || physical_device == nullptr || device == nullptr ||
	        allocator != nullptr);

	VmaVulkanFunctions functions {};
	functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
	functions.vkGetDeviceProcAddr   = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo info {};
	info.instance         = instance;
	info.physicalDevice   = physical_device;
	info.device           = device;
	info.pVulkanFunctions = &functions;
	info.vulkanApiVersion = VULKAN_TARGET_API_VERSION;
	info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	if (memory_budget_ext_enabled) {
		info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	}

	const auto result = static_cast<vk::Result>(vmaCreateAllocator(&info, &allocator));
	if (result != vk::Result::eSuccess) {
		LOGF("vmaCreateAllocator failed: %s\n", vk::to_string(result).c_str());
		return false;
	}
	return true;
}

void GraphicContext::DestroyAllocator() {
	if (allocator == nullptr) {
		return;
	}
	ClearRetiredImages();
	vmaDestroyAllocator(allocator);
	allocator = nullptr;
}

void GraphicContext::ClearRetiredImages() {
	std::scoped_lock lock(m_retired_image_mutex);
	for (const auto& retired: m_retired_images) {
		DestroyNativeImage(allocator, retired.image, retired.allocation);
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::NativeImagePoolRemovedBytes,
	                          m_retired_image_bytes);
	m_retired_images.clear();
	m_retired_image_bytes = 0;
}

void GraphicContext::LogMemoryBudget() const {
	if (allocator == nullptr || physical_device == nullptr) {
		return;
	}

	const auto& properties = GetPhysicalDeviceMemoryProperties();
	VmaBudget   budgets[VK_MAX_MEMORY_HEAPS] {};
	vmaGetHeapBudgets(allocator, budgets);
	for (uint32_t i = 0; i < properties.memoryHeapCount; i++) {
		LOGF("VMA heap %u: usage=%" PRIu64 ", budget=%" PRIu64 ", allocation=%" PRIu64
		     ", blocks=%" PRIu64 "\n",
		     i, static_cast<uint64_t>(budgets[i].usage), static_cast<uint64_t>(budgets[i].budget),
		     static_cast<uint64_t>(budgets[i].statistics.allocationBytes),
		     static_cast<uint64_t>(budgets[i].statistics.blockBytes));
	}
}

uint64_t GraphicContext::GetDeviceMemoryUsage() const {
	if (!CanReportMemoryUsage() || allocator == nullptr) {
		return 0;
	}
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS] {};
	vmaGetHeapBudgets(allocator, budgets);
	const bool discrete =
	    physical_device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
	uint64_t usage = 0;
	for (uint32_t heap = 0; heap < physical_device_memory_properties.memoryHeapCount; heap++) {
		const bool device_local =
		    static_cast<bool>(physical_device_memory_properties.memoryHeaps[heap].flags &
		                      vk::MemoryHeapFlagBits::eDeviceLocal);
		if (!discrete || device_local) {
			usage += budgets[heap].usage;
		}
	}
	return usage;
}

uint64_t GraphicContext::GetTotalMemoryBudget() const {
	if (allocator == nullptr) {
		return 0;
	}
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS] {};
	vmaGetHeapBudgets(allocator, budgets);
	const bool discrete =
	    physical_device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
	uint64_t budget = 0;
	uint64_t local  = 0;
	uint64_t usage  = 0;
	for (uint32_t heap = 0; heap < physical_device_memory_properties.memoryHeapCount; heap++) {
		const auto& properties = physical_device_memory_properties.memoryHeaps[heap];
		const bool  device_local =
		    static_cast<bool>(properties.flags & vk::MemoryHeapFlagBits::eDeviceLocal);
		if (device_local) {
			local += properties.size;
		}
		if (!discrete || device_local) {
			budget += CanReportMemoryUsage() ? budgets[heap].budget : properties.size;
			usage += CanReportMemoryUsage() ? budgets[heap].usage : 0;
		}
	}
	if (discrete) {
		return budget - std::min<uint64_t>(budget / 8, 1024ull * 1024 * 1024);
	}
	constexpr uint64_t system_reserve = 8ull * 1024 * 1024 * 1024;
	const auto         available      = budget > usage ? budget - usage : uint64_t {0};
	return std::max(local, available > system_reserve ? available - system_reserve : uint64_t {0});
}

bool GraphicContext::CreateImage(const vk::ImageCreateInfo& image_info, VulkanImage& image) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || image.image != nullptr || image.allocation != nullptr);

	const bool recycle = NativeImagePoolEnabled() && CanRecycleImage(image_info);
	if (recycle) {
		if (CanReportMemoryUsage() && GetDeviceMemoryUsage() >= GetTotalMemoryBudget()) {
			ClearRetiredImages();
		}
		std::scoped_lock lock(m_retired_image_mutex);
		for (size_t i = m_retired_images.size(); i > 0; --i) {
			const auto& retired = m_retired_images[i - 1];
			if (retired.create == image_info) {
				image.image = retired.image;
				image.allocation = retired.allocation;
				EXIT_IF(retired.bytes > m_retired_image_bytes);
				m_retired_image_bytes -= retired.bytes;
				Profiler::CountFrameEvent(Profiler::FrameEvent::NativeImagePoolRemovedBytes,
				                          retired.bytes);
				m_retired_images.erase(m_retired_images.begin() + static_cast<std::ptrdiff_t>(i - 1));
				break;
			}
		}
		Profiler::CountFrameEvent(image.image != nullptr ? Profiler::FrameEvent::NativeImagePoolHits
		                                                 : Profiler::FrameEvent::NativeImagePoolMisses);
	}
	const bool pool_hit = image.image != nullptr;
	if (image.image == nullptr) {
		VmaAllocationCreateInfo alloc_info {};
		alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		auto allocate = [&] {
			Profiler::ScopedFrameWait timing(Profiler::FrameWait::NativeImageCreate);
			vk::Image::CType native_image = VK_NULL_HANDLE;
			VmaAllocation allocation = nullptr;
			const auto result = static_cast<vk::Result>(vmaCreateImage(
			    allocator, static_cast<const vk::ImageCreateInfo::NativeType*>(image_info),
			    &alloc_info, &native_image, &allocation, nullptr));
			if (result == vk::Result::eSuccess) {
				image.image = native_image;
				image.allocation = allocation;
			}
			return result;
		};
		auto result = allocate();
		if (result == vk::Result::eErrorOutOfDeviceMemory && NativeImagePoolEnabled()) {
			// Retained objects are optional. Release them before one allocation retry.
			ClearRetiredImages();
			result = allocate();
		}
		if (result != vk::Result::eSuccess) {
			LogMemoryBudget();
			return false;
		}
	}

	if (HangTrace::Enabled()) {
		VmaAllocationInfo allocation_info {};
		vmaGetAllocationInfo(allocator, image.allocation, &allocation_info);
		HangTrace::RecordNativeImage(true, pool_hit, static_cast<uint32_t>(image_info.format),
		                             image_info.extent.width, image_info.extent.height,
		                             image_info.mipLevels, static_cast<uint32_t>(image_info.usage),
		                             static_cast<uint64_t>(allocation_info.size));
	}

	image.format     = image_info.format;
	image.image_type = image_info.imageType;
	image.extent     = image_info.extent;
	image.layers     = image_info.arrayLayers;
	image.mip_levels = image_info.mipLevels;
	image.samples    = static_cast<uint32_t>(image_info.samples);
	image.usage      = image_info.usage;
	image.flags      = image_info.flags;
	image.state      = {.layout = image_info.initialLayout};
	image.subresource_states.clear();
	image.pool_eligible = recycle;
	image.pool_create_info = recycle ? image_info : vk::ImageCreateInfo {};

	return true;
}

void GraphicContext::DeleteImage(VulkanImage& image) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(allocator == nullptr || image.image == nullptr || image.allocation == nullptr);

	// This is the existing destruction boundary: Image's views are already destroyed,
	// and TextureCache's deferred callback has waited for native completion/publication.
	// Only native storage is retained; no guest address, content-validity or view survives.
	if (HangTrace::Enabled()) {
		VmaAllocationInfo allocation_info {};
		vmaGetAllocationInfo(allocator, image.allocation, &allocation_info);
		HangTrace::RecordNativeImage(false, false, static_cast<uint32_t>(image.format),
		                             image.extent.width, image.extent.height, image.mip_levels,
		                             static_cast<uint32_t>(image.usage),
		                             static_cast<uint64_t>(allocation_info.size));
	}
	bool retained = false;
	if (image.pool_eligible && NativeImagePoolEnabled()) {
		const bool pressure = CanReportMemoryUsage() && GetDeviceMemoryUsage() >= GetTotalMemoryBudget();
		if (pressure) {
			ClearRetiredImages();
		} else {
			VmaAllocationInfo allocation_info {};
			vmaGetAllocationInfo(allocator, image.allocation, &allocation_info);
			const auto bytes       = static_cast<uint64_t>(allocation_info.size);
			const auto byte_limit  = RetiredImageByteLimit(*this);
			const auto count_limit = RetiredImageCountLimit();
			if (bytes <= byte_limit && count_limit != 0) {
				std::scoped_lock lock(m_retired_image_mutex);
				while (!m_retired_images.empty() &&
				       (m_retired_images.size() >= count_limit ||
				        bytes > byte_limit - m_retired_image_bytes)) {
					const auto oldest = m_retired_images.front();
					DestroyNativeImage(allocator, oldest.image, oldest.allocation);
					m_retired_image_bytes -= oldest.bytes;
					Profiler::CountFrameEvent(Profiler::FrameEvent::NativeImagePoolRemovedBytes,
					                          oldest.bytes);
					m_retired_images.erase(m_retired_images.begin());
				}
				m_retired_images.push_back({image.pool_create_info, image.image, image.allocation, bytes});
				m_retired_image_bytes += bytes;
				Profiler::CountFrameEvent(Profiler::FrameEvent::NativeImagePoolRetires);
				Profiler::CountFrameEvent(Profiler::FrameEvent::NativeImagePoolAddedBytes, bytes);
				retained = true;
			}
		}
	}
	if (!retained) {
		DestroyNativeImage(allocator, image.image, image.allocation);
	}
	image.image      = nullptr;
	image.allocation = nullptr;
	image.pool_eligible = false;
}

} // namespace Libs::Graphics
