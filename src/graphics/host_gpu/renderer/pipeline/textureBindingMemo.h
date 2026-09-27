#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_TEXTUREBINDINGMEMO_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_TEXTUREBINDINGMEMO_H_

#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/host_gpu/renderer/cache/textureCache.h"
#include "graphics/host_gpu/renderer/image/image.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/shader/recompiler/ir/ShaderIR.h"

#include <array>
#include <cstdint>
#include <memory>

namespace Libs::Graphics {

struct TextureBinding;

// Texture binding identity memo (KYTY_TEXTURE_BINDING_MEMO, default on; =0 restores the full
// resolution for every binding).
//
// RenderExecutor::ResolveTexture turns (T# dwords, shader image resource) into a texture-cache
// image and a description, and RebindImages turns that into the sampled view. Both repeat for
// every bound texture of every draw although the answer rarely changes. This memo remembers, per
// exact key (the eight T# dwords plus every ImageResource field the resolution reads), the final
// description, the image FindImage returned and the view FindTexture returned.
//
// When a memoized answer is exact:
//  - FindImage. Only answers of the first-page lookup are recorded: after the slow resolution,
//    FindImageWithSameBacking(final description) must return the same image and FindImage must
//    not have rebased the view (mip/slice-of-a-larger-image answers are never recorded; DCC
//    descriptions are never recorded because MaterializeDccClear inspects guest metadata on every
//    lookup). That lookup reads only the first page's owner list, the registered flag and
//    SameBacking fields, which are fixed for an image's lifetime. The owner lists and registered
//    flags change only in TextureCache::RegisterImage/UnregisterImage, which bump
//    TextureCache::m_binding_generation (every creation, free, expansion, overlap resolution,
//    depth recreate, garbage/pressure collection and unmap goes through them). An entry is used only
//    while that generation is unchanged, so FindImage would return the recorded image again.
//  - FindImage side effects. SyncAliasFromOwner must be a no-op: either the image is its alias
//    owner (checked live) or no other registered image has the same backing range, extent and
//    sample count (checked once when recording, over the first-page owners; such partners exist
//    only through registration). The tick/LRU touch is performed exactly as FindImage does. A
//    stencil association (depth_id, checked live) or a pending rebind (checked live) takes the
//    slow path.
//  - FindTexture (sampled bindings only; storage bindings always take the slow path because they
//    mark the image GPU-written). RefreshImage must be a no-op: the image is not CPU-dirty,
//    maybe-dirty or buffer-modified and its whole range is write-watched (TrackImage no-op), and it
//    has no stencil plane to refresh. Views are never destroyed or replaced while their image lives
//    and FindView returns the first view matching the normalized info, so the recorded view of
//    the same image id (slot generation included) is what FindView returns.
// Null descriptors resolve to the texture cache's permanent null images (never registered, never
// freed); their entries do not depend on the generation.
class TextureBindingMemo {
public:
	struct Key {
		std::array<uint32_t, 8>                   words {};
		ShaderRecompiler::IR::ImageResourceClass  resource_class {};
		Prospero::TextureNumericClass             numeric_class {};
		ShaderRecompiler::Decoder::ImageDimension dimension {};
		ShaderRecompiler::IR::ImageMipMode        mip_mode {};
		uint32_t                                  mip_count         = 0;
		Prospero::BufferFormat                    conversion_format {};
		uint32_t                                  shader_swizzle    = 0;
		bool                                      read              = false;
		bool                                      written           = false;
		bool                                      atomic            = false;
		bool                                      depth_compare     = false;
		bool                                      cube              = false;
		bool                                      r128              = false;

		bool operator==(const Key&) const = default;
	};

	TextureBindingMemo();
	~TextureBindingMemo();
	TextureBindingMemo(const TextureBindingMemo&)            = delete;
	TextureBindingMemo& operator=(const TextureBindingMemo&) = delete;

	[[nodiscard]] static bool Enabled();

	[[nodiscard]] static Key MakeKey(const ShaderRecompiler::IR::ImageResource& resource,
	                                 const uint32_t (&words)[8]);
	// The same hash the texture description cache uses for these words and fields.
	[[nodiscard]] static uint64_t Hash(const Key& key);

	// ResolveTexture fast path. On success `binding` holds exactly what the full resolution
	// would produce (image id and final description; view/layout/mip views are reset by the
	// caller) and the FindImage touch was performed.
	[[nodiscard]] bool TryResolve(TextureCache& cache, const Key& key, uint64_t hash,
	                              TextureBinding& binding);
	// After a full resolution: records `binding` when the answer is memoizable. `found` is what
	// FindImage returned, `view_rebased` whether FindImage changed the view's base level/layer.
	void Record(TextureCache& cache, const Key& key, uint64_t hash, TextureBinding& binding,
	            ImageId found, bool exact_format, bool view_rebased);
	// Marks `binding` as not described by any entry (its description was built elsewhere).
	static void Forget(TextureBinding& binding);

	// RebindImages fast path for a sampled binding: the FindTexture touch and view, when
	// FindTexture would do nothing else.
	[[nodiscard]] bool TryAcquireView(TextureCache& cache, TextureBinding& binding);
	// Remembers the view FindTexture returned for a binding described by an entry.
	void RecordView(const TextureBinding& binding, vk::ImageView view);

private:
	struct Entry;
	static constexpr uint32_t Slots = 4096;

	[[nodiscard]] static bool RefreshIsNoOp(const Image& image);

	std::unique_ptr<Entry[]> m_entries;
	uint64_t                 m_next_tag = 1;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_TEXTUREBINDINGMEMO_H_
