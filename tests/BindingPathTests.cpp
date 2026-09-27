// Unit tests for the pure parts of the texture/descriptor binding path: pipeline layout
// signatures (pipelineLayoutCache.h) and same-command-buffer descriptor set reuse
// (descriptorSetReuse.h).

#include "graphics/host_gpu/renderer/pipeline/descriptorSetReuse.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.h"

#include <array>
#include <cstdint>
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

template <typename Handle>
Handle FakeHandle(uintptr_t value) {
	return Handle(reinterpret_cast<typename Handle::CType>(value));
}

struct FakeWrites {
	std::array<vk::DescriptorBufferInfo, 2> buffers {};
	std::array<vk::DescriptorImageInfo, 2>  images {};
	std::array<vk::WriteDescriptorSet, 3>   writes {};

	FakeWrites() {
		buffers[0] = {FakeHandle<vk::Buffer>(0x1000), 0, 256};
		buffers[1] = {FakeHandle<vk::Buffer>(0x2000), 512, 64};
		images[0]  = {nullptr, FakeHandle<vk::ImageView>(0x3000),
		              vk::ImageLayout::eShaderReadOnlyOptimal};
		images[1]  = {FakeHandle<vk::Sampler>(0x4000), nullptr, vk::ImageLayout::eUndefined};
		writes[0].dstBinding      = 0;
		writes[0].descriptorCount = 2;
		writes[0].descriptorType  = vk::DescriptorType::eStorageBuffer;
		writes[0].pBufferInfo     = buffers.data();
		writes[1].dstBinding      = 52;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = vk::DescriptorType::eSampledImage;
		writes[1].pImageInfo      = &images[0];
		writes[2].dstBinding      = 95;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType  = vk::DescriptorType::eSampler;
		writes[2].pImageInfo      = &images[1];
	}
	FakeWrites(const FakeWrites& other): FakeWrites() {
		buffers = other.buffers;
		images  = other.images;
	}
};

void TestDescriptorSetReuse() {
	const auto layout       = FakeHandle<vk::DescriptorSetLayout>(0x5000);
	const auto other_layout = FakeHandle<vk::DescriptorSetLayout>(0x5100);
	const auto set          = FakeHandle<vk::DescriptorSet>(0x6000);
	DescriptorSetReuse reuse;
	FakeWrites         a;
	const auto         hash = DescriptorSetReuse::Hash(layout, a.writes);
	Check(!reuse.Find(7, layout, a.writes, hash), "an empty cache finds nothing");
	reuse.Insert(7, layout, a.writes, hash, set);
	Check(reuse.Find(7, layout, a.writes, hash) == set, "same tick, layout and contents reuse");

	FakeWrites copy(a);
	Check(DescriptorSetReuse::Hash(layout, copy.writes) == hash &&
	          reuse.Find(7, layout, copy.writes, hash) == set,
	      "equal contents in other arrays reuse");
	Check(!reuse.Find(8, layout, a.writes, hash), "another command buffer never reuses");
	Check(!reuse.Find(7, other_layout, a.writes, DescriptorSetReuse::Hash(other_layout, a.writes)),
	      "another layout never reuses");

	FakeWrites offset(a);
	offset.buffers[1].offset = 768;
	const auto offset_hash = DescriptorSetReuse::Hash(layout, offset.writes);
	Check(!reuse.Find(7, layout, offset.writes, offset_hash) &&
	          !reuse.Find(7, layout, offset.writes, hash),
	      "a different buffer offset never reuses");
	FakeWrites image_layout(a);
	image_layout.images[0].imageLayout = vk::ImageLayout::eGeneral;
	Check(!reuse.Find(7, layout, image_layout.writes, hash),
	      "a different image layout never reuses");
	FakeWrites view(a);
	view.images[0].imageView = FakeHandle<vk::ImageView>(0x3100);
	Check(!reuse.Find(7, layout, view.writes, hash), "a different view never reuses");
	FakeWrites sampler(a);
	sampler.images[1].sampler = FakeHandle<vk::Sampler>(0x4100);
	Check(!reuse.Find(7, layout, sampler.writes, hash), "a different sampler never reuses");
	FakeWrites binding(a);
	binding.writes[1].dstBinding = 53;
	Check(!reuse.Find(7, layout, binding.writes, hash), "a different binding never reuses");
	Check(!reuse.Find(7, layout, std::span(a.writes).first(2), hash),
	      "a shorter write list never reuses");

	// The cache kept copies: changing the caller's arrays after inserting changes nothing.
	FakeWrites mutated(a);
	reuse.Insert(9, layout, mutated.writes, hash, set);
	mutated.buffers[0].range = 1;
	FakeWrites original(a);
	Check(reuse.Find(9, layout, original.writes, hash) == set,
	      "entries own copies of the descriptor infos");

	FakeWrites texel(a);
	const auto view_handle = FakeHandle<vk::BufferView>(0x7000);
	texel.writes[0].pTexelBufferView = &view_handle;
	reuse.Insert(10, layout, texel.writes, hash, set);
	Check(!reuse.Find(10, layout, texel.writes, hash) && !reuse.Find(10, layout, a.writes, hash),
	      "unsupported writes are not cached");
}

// A set whose last write is a buffer (as the flattened-SRT and shader-data stream ranges are),
// with that buffer's offset a multiple of 256 (the stream buffer's alignment).
struct StreamLastWrites {
	vk::DescriptorImageInfo               image {};
	vk::DescriptorBufferInfo              stream {};
	std::array<vk::WriteDescriptorSet, 2> writes {};

	explicit StreamLastWrites(uint64_t offset) {
		image  = {nullptr, FakeHandle<vk::ImageView>(0x3000),
		          vk::ImageLayout::eShaderReadOnlyOptimal};
		stream = {FakeHandle<vk::Buffer>(0x8000), offset, 96};
		writes[0].dstBinding      = 52;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType  = vk::DescriptorType::eSampledImage;
		writes[0].pImageInfo      = &image;
		writes[1].dstBinding      = 120;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType  = vk::DescriptorType::eStorageBuffer;
		writes[1].pBufferInfo     = &stream;
	}
};

// KYTY_DESCRIPTOR_SET_REUSE_AUDIT: what the audit counts, and the slot collision it measures.
void TestDescriptorSetReuseAudit() {
	const auto layout = FakeHandle<vk::DescriptorSetLayout>(0x5000);
	const auto set_a  = FakeHandle<vk::DescriptorSet>(0x6000);
	const auto set_b  = FakeHandle<vk::DescriptorSet>(0x6100);
	const StreamLastWrites a(0x1000);
	const StreamLastWrites b(0x1100);

	// Hash's last two mixes (offset, then range) leave the slot bits (hash % 64) independent of
	// offset bits 8 and up: sets that differ only there share a slot and evict each other.
	bool same_slot = true;
	for (uint64_t k = 1; k <= 16; k++) {
		const StreamLastWrites other(0x100 * k);
		same_slot &= DescriptorSetReuse::Hash(layout, other.writes) % 64 ==
		             DescriptorSetReuse::Hash(layout, a.writes) % 64;
	}
	Check(same_slot, "stream offsets 256 apart share Hash's slot");
	DescriptorSetReuse reuse;
	const auto hash_a = DescriptorSetReuse::Hash(layout, a.writes);
	const auto hash_b = DescriptorSetReuse::Hash(layout, b.writes);
	reuse.Insert(1, layout, a.writes, hash_a, set_a);
	reuse.Insert(1, layout, b.writes, hash_b, set_b);
	Check(!reuse.Find(1, layout, a.writes, hash_a) && reuse.Find(1, layout, b.writes, hash_b) == set_b,
	      "alternating sets in one slot evict each other");

	// The digest separates them; the audit sees every repeat within a command buffer.
	const auto digest_a = DescriptorSetReuse::Digest(layout, a.writes);
	const auto digest_b = DescriptorSetReuse::Digest(layout, b.writes);
	Check(digest_a != digest_b && digest_a % 64 != digest_b % 64,
	      "the digest gives the two sets different slots");
	const StreamLastWrites a_again(0x1000);
	Check(DescriptorSetReuse::Digest(layout, a_again.writes) == digest_a,
	      "equal contents give equal digests");
	uint32_t repeats = 0;
	uint32_t slot_hits = 0;
	for (const auto digest: {digest_a, digest_b, digest_a, digest_b}) {
		const auto result = reuse.Audit(1, digest);
		repeats += result.repeat ? 1u : 0u;
		slot_hits += result.digest_slot_hit ? 1u : 0u;
	}
	Check(repeats == 2 && slot_hits == 2, "A, B, A, B: two repeats, both in the digest slots");
	const auto next_tick = reuse.Audit(2, digest_a);
	Check(!next_tick.repeat && !next_tick.digest_slot_hit, "a new command buffer starts empty");
}

} // namespace

int main() {
	TestLayoutSignatures();
	TestDescriptorSetReuse();
	TestDescriptorSetReuseAudit();
	if (g_failures != 0) {
		std::printf("binding path tests: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("binding path tests passed\n");
	return EXIT_SUCCESS;
}
