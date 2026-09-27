// KYTY_CP_REPEAT_TRACE hashing (repeatTrace.h): the register hashes ignore struct padding, and
// every component hash changes with its own inputs only. CPU only (no device, no guest memory).
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/drawPrep/drawPrep.h"
#include "graphics/host_gpu/renderer/drawPrep/repeatTrace.h"
#include "graphics/host_gpu/renderer/render.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

namespace {

using namespace Libs::Graphics;
using RepeatTrace::Component;

int g_failures = 0;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "FAILED: %s\n", text);
		g_failures++;
	}
}

struct SnapshotDeleter {
	void operator()(DrawPrep::RegisterSnapshot* snapshot) const {
		snapshot->~RegisterSnapshot();
		::operator delete(snapshot);
	}
};
using Snapshot = std::unique_ptr<DrawPrep::RegisterSnapshot, SnapshotDeleter>;

// Default-initialized in memory pre-filled with `garbage`: the padding keeps those bytes.
Snapshot MakeSnapshot(uint8_t garbage) {
	void* raw = ::operator new(sizeof(DrawPrep::RegisterSnapshot));
	std::memset(raw, garbage, sizeof(DrawPrep::RegisterSnapshot));
	Snapshot snapshot(new (raw) DrawPrep::RegisterSnapshot);
	auto& ctx = snapshot->context;
	ctx.SetRenderTargetMask(0xf);
	ctx.SetColorBase(0, {0x5000000000ull});
	HW::ColorInfo info;
	info.format = Prospero::ChannelLayout::k8_8_8_8;
	ctx.SetColorInfo(0, info);
	ctx.SetViewportScaleOffset(0, 960.0f, 960.0f, -540.0f, 540.0f, 0.5f, 0.5f);
	ctx.SetScreenScissor(0, 0, 1920, 1080);
	HW::BlendControl blend;
	blend.enable = true;
	ctx.SetBlendControl(0, blend);
	HW::DepthControl depth;
	depth.z_enable = true;
	depth.zfunc    = 3;
	ctx.SetDepthControl(depth);
	auto& shaders = snapshot->shaders;
	shaders.SetGsShaderBase(0x4000000100ull);
	shaders.SetPsShaderBase(0x4000000200ull);
	shaders.SetGsUserSgpr(0, 0x11111111u, HW::UserSgprType::Unknown);
	shaders.SetPsUserSgpr(0, 0x22222222u, HW::UserSgprType::Unknown);
	snapshot->user_config.SetPrimitiveType(Prospero::PrimitiveType::kTriList);
	return snapshot;
}

RepeatTrace::DrawRecord Hash(const DrawPrep::RegisterSnapshot& snapshot,
                             const DrawIndexArgs& args, const DrawPrep::PreparedDraw& prepared) {
	RepeatTrace::DrawRecord record;
	RepeatTrace::HashDraw(snapshot, &args, nullptr, true, prepared, record);
	return record;
}

// Components of `a` and `b` that differ, as a bit mask.
uint32_t Diff(const RepeatTrace::DrawRecord& a, const RepeatTrace::DrawRecord& b) {
	uint32_t mask = 0;
	for (uint32_t c = 0; c < Component::Count; c++) {
		mask |= a.h[c] != b.h[c] ? (1u << c) : 0u;
	}
	return mask;
}

constexpr uint32_t Bit(Component c) {
	return 1u << static_cast<uint32_t>(c);
}

DrawIndexArgs Args() {
	DrawIndexArgs args;
	args.index_count    = 36;
	args.index_addr     = reinterpret_cast<const void*>(0x6000000000ull);
	args.instance_count = 1;
	return args;
}

void TestPaddingIgnored() {
	const auto a        = MakeSnapshot(0x00);
	const auto b        = MakeSnapshot(0xcd);
	const auto prepared = std::make_unique<DrawPrep::PreparedDraw>();
	const auto ra       = Hash(*a, Args(), *prepared);
	const auto rb       = Hash(*b, Args(), *prepared);
	Check(ra.hashed == 1 && ra.prep_ok == 0, "a failed preparation still hashes the registers");
	Check(Diff(ra, rb) == 0, "equal registers hash equally whatever the padding bytes hold");
	Check(ra.h[Component::Programs] == 0 && ra.h[Component::VSharp] == 0,
	      "no preparation: no program or descriptor hashes");
}

void TestComponentsAreIndependent() {
	const auto base     = MakeSnapshot(0x11);
	const auto prepared = std::make_unique<DrawPrep::PreparedDraw>();
	const auto r0       = Hash(*base, Args(), *prepared);

	auto changed = MakeSnapshot(0x77);
	changed->shaders.SetPsUserSgpr(0, 0x33333333u, HW::UserSgprType::Unknown);
	Check(Diff(r0, Hash(*changed, Args(), *prepared)) == Bit(Component::UserSgpr),
	      "a user SGPR changes only the usgpr hash");

	changed = MakeSnapshot(0x77);
	changed->context.SetColorBase(0, {0x5000100000ull});
	Check(Diff(r0, Hash(*changed, Args(), *prepared)) == Bit(Component::Targets),
	      "a render target address changes only the targets hash");

	changed = MakeSnapshot(0x77);
	changed->context.SetViewportScaleOffset(0, 480.0f, 480.0f, -270.0f, 270.0f, 0.5f, 0.5f);
	Check(Diff(r0, Hash(*changed, Args(), *prepared)) == Bit(Component::Viewport),
	      "a viewport changes only the viewport hash");

	changed = MakeSnapshot(0x77);
	HW::BlendControl blend;
	blend.enable         = true;
	blend.color_destblend = 1;
	changed->context.SetBlendControl(0, blend);
	Check(Diff(r0, Hash(*changed, Args(), *prepared)) == Bit(Component::State),
	      "blend state changes only the state hash");

	changed = MakeSnapshot(0x77);
	changed->shaders.SetPsShaderBase(0x4000000300ull);
	Check(Diff(r0, Hash(*changed, Args(), *prepared)) == Bit(Component::Shader),
	      "a shader address changes only the shader hash");

	auto args       = Args();
	args.index_addr = reinterpret_cast<const void*>(0x6000001000ull);
	Check(Diff(r0, Hash(*base, args, *prepared)) == Bit(Component::Args),
	      "an index address changes args but not argshape");
	args             = Args();
	args.index_count = 72;
	Check(Diff(r0, Hash(*base, args, *prepared)) ==
	          (Bit(Component::Args) | Bit(Component::ArgShape)),
	      "an index count changes args and argshape");
}

ShaderRecompiler::IR::DescriptorValue Descriptor(std::initializer_list<uint32_t> words) {
	ShaderRecompiler::IR::DescriptorValue value;
	value.dword_count = static_cast<uint32_t>(words.size());
	uint32_t i        = 0;
	for (const auto word: words) {
		value.dwords[i++] = word;
	}
	return value;
}

void TestPreparedResources() {
	const auto snapshot = MakeSnapshot(0x00);
	auto       prepared = std::make_unique<DrawPrep::PreparedDraw>();
	prepared->ok        = true;
	auto& resources     = prepared->vertex_prep.resources;
	// V#: base 0x00012000 (payload reads fail without guest memory: size-only payload hash).
	resources.buffers.push_back(Descriptor({0x00012000u, 0x00100000u, 64u, 0x00027facu}));
	resources.images.push_back(Descriptor({0x00001234u, 0x00000050u, 0, 0, 0, 0, 0, 0}));
	resources.samplers.push_back(Descriptor({1u, 2u, 3u, 4u}));
	resources.flattened_srt = {7u, 8u, 9u};
	resources.user_data     = {0x11111111u};
	const auto r0           = Hash(*snapshot, Args(), *prepared);
	Check(r0.prep_ok == 1 && r0.n_vsharp == 1 && r0.n_tsharp == 1 && r0.n_ssharp == 1 &&
	          r0.srt_words == 3,
	      "prepared resource counts");

	auto moved                  = std::make_unique<DrawPrep::PreparedDraw>();
	moved->ok                   = true;
	moved->vertex_prep.resources = resources;
	moved->vertex_prep.resources.buffers[0].dwords[0] = 0x00013000u;
	Check(Diff(r0, Hash(*snapshot, Args(), *moved)) == Bit(Component::VSharp),
	      "a V# base address changes vsharp but not vshape or payload");

	moved->vertex_prep.resources                    = resources;
	moved->vertex_prep.resources.buffers[0].dwords[2] = 128u;
	Check(Diff(r0, Hash(*snapshot, Args(), *moved)) ==
	          (Bit(Component::VSharp) | Bit(Component::VShape) | Bit(Component::Payload)),
	      "a V# record count changes vsharp, vshape and the payload size");

	moved->vertex_prep.resources                   = resources;
	moved->vertex_prep.resources.images[0].dwords[0] = 0x00001235u;
	Check(Diff(r0, Hash(*snapshot, Args(), *moved)) == Bit(Component::TSharp),
	      "a T# changes only tsharp");

	moved->vertex_prep.resources                = resources;
	moved->vertex_prep.resources.flattened_srt = {7u, 8u, 10u};
	Check(Diff(r0, Hash(*snapshot, Args(), *moved)) == Bit(Component::Srt),
	      "the flattened SRT changes only srt");
}

} // namespace

int main() {
	TestPaddingIgnored();
	TestComponentsAreIndependent();
	TestPreparedResources();
	if (g_failures != 0) {
		std::fprintf(stderr, "repeat trace tests: %d failures\n", g_failures);
		return 1;
	}
	std::printf("repeat trace tests passed\n");
	return 0;
}
