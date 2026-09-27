#include "graphics/host_gpu/renderer/pipeline/textureBindingMemo.h"

#include "common/assert.h"
#include "common/profiler.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/renderer/pipeline/descriptors.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <xxhash.h>

namespace Libs::Graphics {

struct TextureBindingMemo::Entry {
	Key key;
	// 0: empty. Unique per recording, so a binding that holds this tag holds this description.
	uint64_t tag = 0;
	// The ImagePageTable page holding the description's first byte, and its structure version
	// (TextureCache::PageVersion) when recorded (both unused for null images).
	uint64_t page         = 0;
	uint64_t page_version = 0;
	ImageId  image;
	// TextureCache::RequestedFirstLevel of the description for this image: FindImage and
	// FindTexture extend the image's resident levels (EnsureResidency) unless its resident_first
	// is at most this.
	uint32_t requested_first = 0;
	// Another registered image has the same backing range, extent and sample count, so
	// SyncAliasFromOwner may copy into this one unless it owns the bytes.
	bool                    has_partner = false;
	bool                    null_image  = false;
	// FindImage's exact_format for the description (KYTY_TEXTURE_MEMO_REVALIDATE).
	bool                    exact_format = false;
	vk::ImageView           view        = nullptr;
	TextureCache::ImageDesc desc;
};

namespace {

// KYTY_TEXTURE_MEMO_REVALIDATE (default on; =0 off): an entry whose first page changed its owner
// list (the page version moved: an image registered or unregistered there, often a transient
// render target sharing the 1 MiB page) is checked again instead of being dropped: the lookup
// FindImage makes (FindImageWithSameBacking on the recorded description), the alias-partner scan
// and the version are redone exactly as Record() does them, under the same lock. If the lookup
// still returns the recorded image, the entry (and its view) stay valid for the new version.
bool RevalidateEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_TEXTURE_MEMO_REVALIDATE");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

// KYTY_TEXTURE_MEMO_REVALIDATE_VERIFY=1|exit: every revalidated hit is followed by the full
// resolution (RenderExecutor::ResolveTexture), and a different image or description is counted
// (TextureBindingMemoRevalidateMismatches; exit stops on the first).
int RevalidateVerifyMode() {
	static const int mode = [] {
		const auto* value = std::getenv("KYTY_TEXTURE_MEMO_REVALIDATE_VERIFY");
		if (value == nullptr || *value == '\0' || std::strcmp(value, "0") == 0) {
			return 0;
		}
		return std::strcmp(value, "exit") == 0 ? 2 : 1;
	}();
	return mode;
}

} // namespace

bool TextureBindingMemo::HasPartner(const TextureCache& cache, uint64_t page, ImageId found,
                                    const Image& image) {
	bool partner = false;
	if (const auto* owners = cache.m_image_page_table.Find(page); owners != nullptr) {
		owners->ForEach([&](ImageId id) {
			const auto* other = cache.m_slot_images.try_get(id);
			if (id != found && other != nullptr && other->registered &&
			    other->info.data == image.info.data && other->info.extent == image.info.extent &&
			    other->backing.samples == image.backing.samples) {
				partner = true;
			}
		});
	}
	return partner;
}

bool TextureBindingMemo::RevalidateVerify() {
	return RevalidateVerifyMode() != 0;
}

void TextureBindingMemo::ReportRevalidateMismatch() {
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoRevalidateMismatches);
	static std::atomic<uint32_t> logged {0};
	if (logged.fetch_add(1, std::memory_order_relaxed) < 16) {
		std::fprintf(stderr, "TextureMemoRevalidateVerify: a revalidated entry resolved differently\n");
	}
	if (RevalidateVerifyMode() == 2) {
		EXIT("TextureMemoRevalidateVerify: a revalidated entry resolved differently\n");
	}
}

TextureBindingMemo::TextureBindingMemo() = default;
TextureBindingMemo::~TextureBindingMemo() = default;

bool TextureBindingMemo::Enabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_TEXTURE_BINDING_MEMO");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

TextureBindingMemo::Key TextureBindingMemo::MakeKey(const ShaderRecompiler::IR::ImageResource& resource,
                                                    const uint32_t (&words)[8]) {
	Key key;
	std::memcpy(key.words.data(), words, sizeof(words));
	key.resource_class    = resource.resource_class;
	key.numeric_class     = resource.numeric_class;
	key.dimension         = resource.dimension;
	key.mip_mode          = resource.mip_mode;
	key.mip_count         = resource.mip_count;
	key.conversion_format = resource.conversion_format;
	key.shader_swizzle    = resource.shader_swizzle;
	key.read              = resource.read;
	key.written           = resource.written;
	key.atomic            = resource.atomic;
	key.depth_compare     = resource.depth_compare;
	key.cube              = resource.cube;
	key.r128              = resource.r128;
	return key;
}

uint64_t TextureBindingMemo::Hash(const Key& key) {
	return XXH3_64bits_withSeed(key.words.data(), sizeof(key.words),
	                            (static_cast<uint64_t>(key.dimension) << 32u) ^
	                                (static_cast<uint64_t>(key.numeric_class) << 16u) ^
	                                (key.written ? 1u : 0u) ^ (key.depth_compare ? 2u : 0u) ^
	                                (static_cast<uint64_t>(key.mip_mode) << 8u));
}

bool TextureBindingMemo::RefreshIsNoOp(const Image& image) {
	// Mirrors TextureCache::RefreshImage for a registered image without a stencil association:
	// TrackImage returns at once when the resident range (`live`) is watched as a whole, or, for
	// a chunk-tracked image, when it is watched and no chunk was released; the maybe-dirty edge
	// hash runs only for a maybe-dirty image; InitializeImage (upload, partial or whole) runs
	// only for a buffer-modified or CPU-dirty image (chunk writes and residency extensions mark
	// the image CPU-dirty). Any new refresh trigger added there must be added here.
	if (image.IsCpuDirty() || image.IsBufferModified() || !image.IsTracked()) {
		return false;
	}
	if (image.ChunkTracked()) {
		return image.chunks.untracked_count == 0;
	}
	return image.track_addr == image.live.address && image.track_addr_end == image.live.End();
}

void TextureBindingMemo::Forget(TextureBinding& binding) {
	binding.memo_tag  = 0;
	binding.memo_slot = 0;
}

bool TextureBindingMemo::TryResolve(TextureCache& cache, const Key& key, uint64_t hash,
                                    TextureBinding& binding) {
	m_last_revalidated = false;
	if (!m_entries) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoMisses);
		return false;
	}
	const auto slot  = static_cast<uint32_t>(hash % Slots);
	auto&      entry = m_entries[slot];
	if (entry.tag == 0 || !(entry.key == key)) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoMisses);
		return false;
	}
	bool revalidated = false;
	if (!entry.null_image) {
		std::scoped_lock lock {cache.m_lock};
		auto*            image = cache.m_slot_images.try_get(entry.image);
		const auto       stale = [&] {
            Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoStale);
            m_totals.stale++;
            return false;
		};
		// A stencil association redirects the binding; a pending rebind, a copy from the alias
		// owner and a residency extension (resident levels changed, or fewer than the view
		// samples) are FindImage/RebindImages work: all take the full resolution.
		if (image == nullptr || !image->registered || image->depth_id ||
		    image->binding.needs_rebind || entry.requested_first < image->resident_first) {
			return stale();
		}
		// A structural change on the description's first page may alter what FindImage returns:
		// redo its lookup and the partner scan for the new owner list (RevalidateEnabled).
		if (const auto version = cache.PageVersion(entry.page); entry.page_version != version) {
			if (!RevalidateEnabled() ||
			    cache.FindImageWithSameBacking(entry.desc.info, entry.exact_format) !=
			        entry.image) {
				return stale();
			}
			entry.has_partner  = HasPartner(cache, entry.page, entry.image, *image);
			entry.page_version = version;
			revalidated        = true;
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoRevalidated);
			m_totals.revalidated++;
		}
		if (entry.has_partner && !image->alias_owner && !image->info.HasStencil()) {
			return stale();
		}
		// FindImage's access bookkeeping for the returned image.
		image->tick_accessed_last = cache.m_scheduler.CurrentTick();
		cache.TouchImage(*image);
	}
	binding.image_id = entry.image;
	if (binding.memo_tag == entry.tag && binding.memo_slot == slot) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingDescCopiesAvoided);
	} else {
		binding.desc      = entry.desc;
		binding.memo_tag  = entry.tag;
		binding.memo_slot = slot;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoHits);
	m_totals.hits++;
	m_last_revalidated = revalidated;
	return true;
}

void TextureBindingMemo::Record(TextureCache& cache, const Key& key, uint64_t hash,
                                TextureBinding& binding, ImageId found, bool exact_format,
                                bool view_rebased) {
	Forget(binding);
	const auto& desc = binding.desc;
	if (view_rebased || binding.image_id != found ||
	    desc.info.metadata.kind == ImageMetadataKind::Dcc) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoRejects);
		return;
	}
	const bool null_image      = desc.info.data.Empty();
	bool       has_partner     = false;
	uint64_t   page            = 0;
	uint64_t   page_version    = 0;
	uint32_t   requested_first = 0;
	{
		// One critical section: the checks below and the page version describe the same state.
		std::scoped_lock lock {cache.m_lock};
		const auto*      image = cache.m_slot_images.try_get(found);
		if (null_image) {
			// FindImage returned the null image of this format; it is never registered or freed.
			if (image == nullptr || image->registered || !image->info.data.Empty()) {
				Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoRejects);
				return;
			}
		} else {
			// Only first-page answers: FindImage would return `found` again, and nothing else,
			// while the owner list of the description's first page is unchanged (SameBacking
			// requires the same start address, so `found` starts on that page too).
			TextureCache::ImagePageTable::PageRange pages {};
			if (cache.m_image_lookup_mode != TextureCache::ImageLookupMode::FirstPage ||
			    image == nullptr || !image->registered || image->depth_id ||
			    !TextureCache::ImagePageTable::TryGetPageRange(image->info.data.address,
			                                                   image->info.data.size, pages) ||
			    cache.FindImageWithSameBacking(desc.info, exact_format) != found) {
				Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoRejects);
				return;
			}
			// What FindImage/FindTexture pass to EnsureResidency for this description; they
			// already made these levels resident, and only EnsureResidency (re-registration)
			// changes resident_first.
			requested_first = cache.RequestedFirstLevel(desc, image->info.resources.levels);
			// SyncAliasFromOwner copies only from images with this exact backing range, extent
			// and sample count; they start on the same indexed page as this one. Their flags
			// (alias owner, GPU-modified, stencil) are not considered: any such image makes the
			// entry usable only while this image owns the bytes.
			has_partner  = HasPartner(cache, pages.first, found, *image);
			page         = pages.first;
			page_version = cache.PageVersion(page);
		}
	}
	if (!m_entries) {
		m_entries = std::make_unique<Entry[]>(Slots);
	}
	const auto slot  = static_cast<uint32_t>(hash % Slots);
	auto&      entry = m_entries[slot];
	entry.key             = key;
	entry.tag             = m_next_tag++;
	entry.page            = page;
	entry.page_version    = page_version;
	entry.image           = found;
	entry.requested_first = requested_first;
	entry.has_partner     = has_partner;
	entry.null_image      = null_image;
	entry.exact_format    = exact_format;
	entry.view            = nullptr;
	entry.desc            = desc;
	binding.memo_tag      = entry.tag;
	binding.memo_slot     = slot;
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoFills);
}

bool TextureBindingMemo::TryAcquireView(TextureCache& cache, TextureBinding& binding) {
	if (!m_entries || binding.memo_tag == 0 ||
	    binding.desc.type != TextureCache::BindingType::Texture) {
		return false;
	}
	const auto& entry = m_entries[binding.memo_slot % Slots];
	if (entry.tag != binding.memo_tag || entry.image != binding.image_id || entry.view == nullptr) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureViewMemoMisses);
		return false;
	}
	std::scoped_lock lock {cache.m_lock};
	auto*            image = cache.m_slot_images.try_get(binding.image_id);
	if (image == nullptr) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureViewMemoMisses);
		return false;
	}
	if (!image->info.data.Empty()) {
		// FindTexture's rediscovery checks, a no-op EnsureResidency (the view's levels are
		// resident), a no-op RefreshImage, and no stencil plane refresh.
		if (!image->registered || image->depth_id || image->binding.needs_rebind ||
		    entry.requested_first < image->resident_first || image->info.HasStencil() ||
		    !RefreshIsNoOp(*image)) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureViewMemoMisses);
			return false;
		}
	}
	cache.TouchImage(*image);
	binding.image_view = entry.view;
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureViewMemoHits);
	return true;
}

bool TextureBindingMemo::TryRepeatResolve(TextureCache& cache, std::span<TextureBinding> bindings,
                                          bool apply) {
	m_last_revalidated = false;
	if (!m_entries) {
		return false;
	}
	std::scoped_lock lock {cache.m_lock};
	for (const auto& binding: bindings) {
		const auto& entry = m_entries[binding.memo_slot % Slots];
		if (binding.memo_tag == 0 || entry.tag != binding.memo_tag || entry.image != binding.image_id) {
			return false;
		}
		if (entry.null_image) {
			continue;
		}
		// TryResolve's conditions for a hit that needs no revalidation.
		const auto* image = cache.m_slot_images.try_get(entry.image);
		if (image == nullptr || !image->registered || image->depth_id ||
		    image->binding.needs_rebind || entry.requested_first < image->resident_first ||
		    cache.PageVersion(entry.page) != entry.page_version ||
		    (entry.has_partner && !image->alias_owner && !image->info.HasStencil())) {
			return false;
		}
	}
	if (!apply) {
		return true;
	}
	const auto tick = cache.m_scheduler.CurrentTick();
	for (const auto& binding: bindings) {
		const auto& entry = m_entries[binding.memo_slot % Slots];
		if (!entry.null_image) {
			auto& image              = cache.m_slot_images[entry.image];
			image.tick_accessed_last = tick;
			cache.TouchImage(image);
		}
	}
	// The hits TryResolve would count (each finds its binding's own entry: no description copy).
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingDescCopiesAvoided, bindings.size());
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureBindingMemoHits, bindings.size());
	m_totals.hits += bindings.size();
	return true;
}

bool TextureBindingMemo::TryRepeatViews(TextureCache& cache, std::span<TextureBinding> bindings,
                                        bool apply) {
	if (!m_entries) {
		return false;
	}
	std::scoped_lock lock {cache.m_lock};
	for (const auto& binding: bindings) {
		const auto& entry = m_entries[binding.memo_slot % Slots];
		if (binding.memo_tag == 0 || binding.desc.type != TextureCache::BindingType::Texture ||
		    entry.tag != binding.memo_tag || entry.image != binding.image_id ||
		    entry.view == nullptr) {
			return false;
		}
		const auto* image = cache.m_slot_images.try_get(binding.image_id);
		// RebindImages resolves a binding again when its image is gone, unregistered (null images
		// never are) or awaits a rebind; then TryAcquireView's conditions.
		if (image == nullptr || image->binding.needs_rebind) {
			return false;
		}
		if (!image->info.data.Empty() &&
		    (!image->registered || image->depth_id ||
		     entry.requested_first < image->resident_first || image->info.HasStencil() ||
		     !RefreshIsNoOp(*image))) {
			return false;
		}
	}
	if (!apply) {
		return true;
	}
	for (auto& binding: bindings) {
		const auto& entry = m_entries[binding.memo_slot % Slots];
		cache.TouchImage(cache.m_slot_images[binding.image_id]);
		binding.image_view = entry.view;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureViewMemoHits, bindings.size());
	return true;
}

vk::ImageView TextureBindingMemo::EntryView(const TextureBinding& binding) const {
	if (!m_entries || binding.memo_tag == 0) {
		return nullptr;
	}
	const auto& entry = m_entries[binding.memo_slot % Slots];
	return entry.tag == binding.memo_tag ? entry.view : nullptr;
}

void TextureBindingMemo::RecordView(const TextureBinding& binding, vk::ImageView view) {
	if (!m_entries || binding.memo_tag == 0 || view == nullptr ||
	    binding.desc.type != TextureCache::BindingType::Texture) {
		return;
	}
	auto& entry = m_entries[binding.memo_slot % Slots];
	if (entry.tag == binding.memo_tag && entry.image == binding.image_id) {
		entry.view = view;
	}
}

} // namespace Libs::Graphics
