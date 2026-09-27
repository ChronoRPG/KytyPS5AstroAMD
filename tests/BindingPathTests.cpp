// Unit tests for the pure parts of the texture/descriptor binding path: pipeline layout
// signatures (pipelineLayoutCache.h).

#include "graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

// pipelineLayoutCache.cpp references the default dynamic dispatcher; the tests never call Vulkan.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {

using namespace Libs::Graphics;

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (!condition) {
		std::printf("FAILED: %s\n", what);
		++g_failures;
	}
}

vk::DescriptorSetLayoutBinding Binding(uint32_t binding, vk::DescriptorType type, uint32_t count,
                                       vk::ShaderStageFlags stages) {
	vk::DescriptorSetLayoutBinding result {};
	result.binding         = binding;
	result.descriptorType  = type;
	result.descriptorCount = count;
	result.stageFlags      = stages;
	return result;
}

void TestLayoutSignatures() {
	const auto vs = vk::ShaderStageFlags {vk::ShaderStageFlagBits::eVertex};
	const auto fs = vk::ShaderStageFlags {vk::ShaderStageFlagBits::eFragment};
	const auto push = vs | fs;
	const std::vector<vk::DescriptorSetLayoutBinding> a {
	    Binding(0, vk::DescriptorType::eStorageBuffer, 2, vs),
	    Binding(52, vk::DescriptorType::eSampledImage, 3, fs),
	    Binding(95, vk::DescriptorType::eSampler, 1, fs),
	};
	const std::vector<vk::DescriptorSetLayoutBinding> reordered {a[2], a[0], a[1]};
	const auto sa = MakePipelineLayoutSignature(a, push, 128, 32);
	const auto sr = MakePipelineLayoutSignature(reordered, push, 128, 32);
	Check(sa == sr, "binding order does not change the signature");
	Check(HashPipelineLayoutSignature(sa) == HashPipelineLayoutSignature(sr),
	      "equal signatures hash equally");
	Check(sa.push_descriptors, "6 descriptors fit 32 push descriptors");

	auto different_count = a;
	different_count[1].descriptorCount = 4;
	Check(!(MakePipelineLayoutSignature(different_count, push, 128, 32) == sa),
	      "descriptor count is part of the signature");
	auto different_type = a;
	different_type[1].descriptorType = vk::DescriptorType::eStorageImage;
	Check(!(MakePipelineLayoutSignature(different_type, push, 128, 32) == sa),
	      "descriptor type is part of the signature");
	auto different_stage = a;
	different_stage[0].stageFlags = fs;
	Check(!(MakePipelineLayoutSignature(different_stage, push, 128, 32) == sa),
	      "binding stages are part of the signature");
	auto different_binding = a;
	different_binding[2].binding = 96;
	Check(!(MakePipelineLayoutSignature(different_binding, push, 128, 32) == sa),
	      "binding numbers are part of the signature");
	Check(!(MakePipelineLayoutSignature(a, vs, 128, 32) == sa),
	      "push-constant stages are part of the signature");
	Check(!(MakePipelineLayoutSignature(a, push, 64, 32) == sa),
	      "push-constant size is part of the signature");
	const auto set_path = MakePipelineLayoutSignature(a, push, 128, 5);
	Check(!set_path.push_descriptors && !(set_path == sa),
	      "layouts beyond maxPushDescriptors use descriptor sets");
	Check(MakePipelineLayoutSignature(a, push, 128, 6).push_descriptors,
	      "exactly maxPushDescriptors still pushes");
	Check(MakePipelineLayoutSignature({}, push, 128, 32).bindings.empty(),
	      "an empty binding list is a valid signature");
}

} // namespace

int main() {
	TestLayoutSignatures();
	if (g_failures != 0) {
		std::printf("binding path tests: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("binding path tests passed\n");
	return EXIT_SUCCESS;
}
