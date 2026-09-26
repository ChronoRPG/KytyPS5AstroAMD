#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"

#include "common/assert.h"
#include "common/emulatorConfig.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "common/rendererBatch.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/image/imageView.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/shader/recompiler/ShaderRecompiler.h"
#include "graphics/shader/shaderCompiler.h"
#include "kernel/memory.h"
#include "kytyGitVersion.h"
#include "loader/systemContent.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <limits>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <span>
#include <spirv-tools/libspirv.hpp>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <xxhash.h>

namespace Libs::Graphics {

namespace {

vk::PolygonMode ResolvePolygonMode(const HW::ModeControl& mode, bool cull_front, bool cull_back) {
	// CxPrimitiveSetup::PolygonMode disables both per-face modes when it is zero.
	if (mode.poly_mode == 0) {
		return vk::PolygonMode::eFill;
	}
	EXIT_NOT_IMPLEMENTED(mode.poly_mode != 1);
	if (cull_front && cull_back) {
		return vk::PolygonMode::eFill;
	}
	if (!cull_front && !cull_back && mode.polymode_front_ptype != mode.polymode_back_ptype) {
		EXIT("Pipeline: different polygon modes for two visible faces are unsupported\n");
	}
	// Vulkan has one polygon mode. A culled face does not constrain that mode.
	const auto polygon_mode = cull_front ? mode.polymode_back_ptype : mode.polymode_front_ptype;
	switch (polygon_mode) {
		case 0: return vk::PolygonMode::ePoint;
		case 1: return vk::PolygonMode::eLine;
		case 2: return vk::PolygonMode::eFill;
		default: EXIT("Pipeline: invalid polygon mode %u\n", polygon_mode);
	}
}

// Entries from older revisions accumulate in the driver cache; start over beyond this size.
constexpr uint64_t MaxDriverCacheFileSize = 512ull * 1024 * 1024;

std::string DriverCacheSignature(const vk::PhysicalDeviceProperties& properties) {
	constexpr char hex[] = "0123456789abcdef";
	std::string    uuid(VK_UUID_SIZE * 2, '0');
	for (size_t i = 0; i < VK_UUID_SIZE; i++) {
		uuid[i * 2]     = hex[properties.pipelineCacheUUID[i] >> 4u];
		uuid[i * 2 + 1] = hex[properties.pipelineCacheUUID[i] & 0xfu];
	}
	// The driver validates its own cache header and keys every entry by the exact shader code and
	// pipeline state, so data written by another emulator revision can only miss, never match a
	// different pipeline. Tying the file to the git revision (and disabling it for uncommitted
	// builds) made every experimental build compile all pipelines from scratch at each boot.
	return fmt::format("KytyPC2:{:08x}:{:08x}:{:08x}:{}\n", properties.vendorID,
	                   properties.deviceID, properties.driverVersion, uuid);
}

std::string PipelineCacheTitleId() {
	std::string title_id;
	if ((!Loader::SystemContentParamSfoGetString("TITLE_ID", &title_id) || title_id.empty()) &&
	    (!Loader::SystemContentParamSfoGetString("CONTENT_ID", &title_id) || title_id.empty())) {
		return {};
	}
	if (!std::ranges::all_of(title_id, [](unsigned char c) {
		    return std::isalnum(c) != 0 || c == '-' || c == '_';
	    })) {
		return {};
	}
	return title_id;
}

template <typename... Args>
void PipelineCacheLog(fmt::format_string<Args...> format, Args&&... args) {
	auto message = fmt::format(format, std::forward<Args>(args)...);
	message += '\n';
	Log::WriteToConsoleAndLog(message);
}

struct ShaderReadAttempt {
	std::array<GuestRange, 64> missing {};
	size_t count = 0;
	bool materialization_failed = false;
	bool overflow = false;

	void Missing(uint64_t address, uint64_t size) {
		const GuestRange range {address, size};
		if (!range.Valid()) return;
		for (size_t i = 0; i < count; ++i) {
			if (missing[i] == range) return;
		}
		if (count < missing.size()) missing[count++] = range;
		else overflow = true;
	}

	bool Synchronize() const {
		KYTY_PROFILER_DETAIL_BLOCK("SRT::ReadinessWait");
		Profiler::ScopedFrameWait frame_wait(Profiler::FrameWait::ShaderReadiness);
		if (overflow) EXIT("resource readiness exceeded 64 missing ranges\n");
		bool ready = false;
		for (size_t i = 0; i < count; ++i) {
			ready |= LibKernel::Memory::SynchronizeGpuBackingForRead(missing[i].address,
			                                                          missing[i].size);
		}
		return ready;
	}
};

bool NativeDccEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_DCC_GPU");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

bool ReadShaderGuestMemory(void* userdata, uint64_t address, std::span<uint32_t> values) {
	const bool read = !values.empty() &&
	    LibKernel::Memory::TryReadGpuCleanBacking(address, values.data(), values.size_bytes());
	if (!read && userdata != nullptr) {
		static_cast<ShaderReadAttempt*>(userdata)->Missing(address, values.size_bytes());
	}
	return read;
}

bool SrtReadRunsEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_SRT_READ_RUNS");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

bool TryReadShaderCleanBacking(void*, uint64_t address, std::span<uint32_t> values) {
	// A failed probe must not request synchronization or alter the shader retry list.
	const bool read = !values.empty() &&
	    LibKernel::Memory::TryReadGpuCleanBacking(address, values.data(), values.size_bytes());
	Profiler::CountFrameEvent(read ? Profiler::FrameEvent::SrtProbeHits
	                              : Profiler::FrameEvent::SrtProbeMisses);
	if (read) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::SrtProbeBytes, values.size_bytes());
		if (values.size() > 1) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::SrtProbeBatchHits);
		}
	}
	return read;
}

bool ResourceReuseEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_RESOURCE_REUSE");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

bool ResourceDependencyCacheEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_RESOURCE_DEPENDENCY_CACHE");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

bool SharedResourceEvaluationEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_SRT_SHARED_CLEAN_VALUES");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

// A successful materialization certificate. Every hit rereads all exact observed
// bytes through the current clean-backing predicate. Its output bundle travels
// with the certificate and is invalidated before a refresh can modify it.
class PreparedResourceReads {
public:
	static constexpr size_t MaxBytes = 32u * 1024u;
	static constexpr size_t MaxRanges = 256u;

	void Begin(std::span<const uint32_t> user_data, uint64_t shader_base,
	           std::span<uint8_t, MaxBytes> scratch) {
		m_valid = false;
		m_recordable = true;
		m_ranges.clear();
		m_storage.clear();
		m_capture_scratch = scratch;
		m_user_data.assign(user_data.begin(), user_data.end());
		m_shader_base = shader_base;
	}

	void Invalidate() { m_valid = false; }
	bool Valid() const { return m_valid; }
	size_t CapacityBytes() const {
		return m_ranges.capacity() * sizeof(Range) + m_storage.capacity() +
		       m_user_data.capacity() * sizeof(uint32_t);
	}

	void Finish(bool success) {
		m_valid = success && m_recordable;
		m_capture_scratch = {};
		if (!m_valid) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceReuseRejectedCaptures);
		}
	}

	static void Observe(void* userdata, uint64_t address, std::span<const uint32_t> values,
	                    bool success) {
		static_cast<PreparedResourceReads*>(userdata)->Record(address, values, success);
	}

	bool Matches(std::span<const uint32_t> user_data, uint64_t shader_base,
	             std::span<const uint32_t> dependencies, bool projected) const {
		if (!m_valid || shader_base != m_shader_base || user_data.size() != m_user_data.size()) return false;
		if (!projected) return std::ranges::equal(user_data, m_user_data);
		for (const auto index: dependencies) {
			if (index >= user_data.size() || user_data[index] != m_user_data[index]) return false;
		}
		return true;
	}

	bool Validate(std::span<uint8_t, MaxBytes> scratch) const {
		if (!m_valid) return false;
		Profiler::ScopedFrameWait wait(Profiler::FrameWait::ResourceReuseValidation);
		uint64_t validation_bytes = 0;
		const auto finish = [&](bool valid) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceReuseValidationBytes,
			                          validation_bytes);
			return valid;
		};
		for (const auto& range: m_ranges) {
			validation_bytes += range.size;
			// This is a silent validation. Failure must not synchronize, alter protection,
			// or append a missing range; the normal refresh below owns those operations.
			if (!LibKernel::Memory::TryReadGpuCleanBacking(range.address, scratch.data(),
			                                               range.size) ||
			    std::memcmp(scratch.data(), m_storage.data() + range.offset, range.size) != 0) {
				return finish(false);
			}
		}
		return finish(true);
	}

private:
	struct Range {
		uint64_t address;
		size_t offset;
		size_t size;
		uint64_t End() const { return address + size; }
	};

	void Record(uint64_t address, std::span<const uint32_t> values, bool success) {
		if (!m_recordable) return;
		constexpr uint64_t gpu_limit = uint64_t {1} << 40u;
		const auto bytes = values.size_bytes();
		if (!success || bytes == 0 || bytes > MaxBytes || address == 0 ||
		    address >= gpu_limit || bytes >= gpu_limit - address) {
			m_recordable = false;
			return;
		}
		const auto end = address + bytes;
		const auto* observed = reinterpret_cast<const uint8_t*>(values.data());
		auto first = std::lower_bound(m_ranges.begin(), m_ranges.end(), address,
		                             [](const Range& range, uint64_t value) {
			                             return range.address < value;
		                             });
		if (first != m_ranges.begin() && std::prev(first)->End() >= address) --first;
		auto last = first;
		uint64_t begin_union = address;
		uint64_t end_union = end;
		size_t replaced_bytes = 0;
		while (last != m_ranges.end() && last->address <= end_union) {
			// Reject differing snapshots of any overlapping bytes. No final reread is
			// substituted for what a branch, pointer or descriptor actually observed.
			const auto begin_overlap = std::max(address, last->address);
			const auto end_overlap = std::min(end, last->End());
			if (begin_overlap < end_overlap &&
			    std::memcmp(observed + (begin_overlap - address),
			                m_storage.data() + last->offset + (begin_overlap - last->address),
			                static_cast<size_t>(end_overlap - begin_overlap)) != 0) {
				m_recordable = false;
				return;
			}
			begin_union = std::min(begin_union, last->address);
			end_union = std::max(end_union, last->End());
			replaced_bytes += last->size;
			++last;
		}
		const auto merged_bytes = static_cast<size_t>(end_union - begin_union);
		const auto replaced_ranges = static_cast<size_t>(last - first);
		const auto stored_bytes = m_storage.size();
		const auto new_bytes = stored_bytes - replaced_bytes + merged_bytes;
		if (new_bytes > MaxBytes ||
		    m_ranges.size() - replaced_ranges + 1u > MaxRanges) {
			m_recordable = false;
			return;
		}
		if (replaced_ranges == 1u && begin_union == first->address &&
		    end_union == first->End()) return;
		// Allocate at most one bounded byte arena per source, then retain it across
		// misses. Range metadata likewise keeps its capacity when Begin clears it.
		if (m_storage.capacity() < MaxBytes) m_storage.reserve(MaxBytes);
		if (replaced_ranges == 1u && begin_union == first->address && last == m_ranges.end()) {
			// The usual ascending SRT sequence extends the final range in place.
			m_storage.resize(new_bytes);
			std::memcpy(m_storage.data() + first->offset + (address - begin_union), observed, bytes);
			first->size = merged_bytes;
			return;
		}
		// Existing ranges are packed in address order in the arena. Reconstruct the
		// changed union in shared scratch, shift its packed suffix, and keep no holes.
		const auto offset = first != m_ranges.end() ? first->offset : stored_bytes;
		const auto old_suffix = offset + replaced_bytes;
		for (auto it = first; it != last; ++it) {
			std::memcpy(m_capture_scratch.data() + (it->address - begin_union),
			            m_storage.data() + it->offset, it->size);
		}
		std::memcpy(m_capture_scratch.data() + (address - begin_union), observed, bytes);
		m_storage.resize(new_bytes);
		std::memmove(m_storage.data() + offset + merged_bytes,
		             m_storage.data() + old_suffix, stored_bytes - old_suffix);
		std::memcpy(m_storage.data() + offset, m_capture_scratch.data(), merged_bytes);
		const auto position = m_ranges.erase(first, last);
		for (auto it = position; it != m_ranges.end(); ++it) {
			it->offset += merged_bytes - replaced_bytes;
		}
		m_ranges.insert(position, Range {begin_union, offset, merged_bytes});
	}

	std::vector<Range> m_ranges;
	std::vector<uint8_t> m_storage;
	std::vector<uint32_t> m_user_data;
	std::span<uint8_t> m_capture_scratch;
	uint64_t m_shader_base = 0;
	bool m_valid = false;
	bool m_recordable = false;
};

void DumpShaderSpirv(const char* stage_name, uint64_t shader_hash,
                     const std::vector<uint32_t>& spirv) {
	if (!Config::GraphicsDebugDumpEnabled()) {
		return;
	}
	static std::atomic_int id = 0;
	const auto path = Config::GetShaderLogFolder() / fmt::format("{:04d}_new_shader_{}_{:016x}.spv",
	                                                             id++, stage_name, shader_hash);
	Common::File::CreateDirectories(path.parent_path());
	Common::File file(path);
	if (file.IsInvalid()) {
		const auto path_text = Common::PathToString(path);
		LOGF_COLOR(Log::Color::BrightRed, "Can't create file: %s\n", path_text.c_str());
		return;
	}
	file.Write(spirv.data(), spirv.size() * sizeof(uint32_t));
}

void DumpShaderOriginal(const char* stage_name, uint64_t shader_hash,
                        std::span<const uint32_t> code, const std::string& decoded_dump) {
	if (!Config::GraphicsDebugDumpEnabled()) {
		return;
	}
	EXIT_IF(code.empty());
	static std::atomic_int id = 0;
	const auto base = Config::GetShaderLogFolder() / "original" /
	                  fmt::format("{:04d}_new_shader_{}_{:016x}", id++, stage_name, shader_hash);
	Common::File::CreateDirectories(base.parent_path());
	for (const auto& [suffix, data, size]: {
	         std::tuple {".bin", static_cast<const void*>(code.data()), code.size_bytes()},
	         std::tuple {".rdna2", static_cast<const void*>(decoded_dump.data()),
	                     decoded_dump.size()},
	     }) {
		if (size == 0) {
			continue;
		}
		auto path = base;
		path += suffix;
		Common::File file(path);
		if (file.IsInvalid()) {
			const auto path_text = Common::PathToString(path);
			LOGF_COLOR(Log::Color::BrightRed, "Can't create file: %s\n", path_text.c_str());
		} else {
			file.Write(data, size);
		}
	}
}

void DumpMatchedShaderInputs(const ShaderParams& params,
                             const ShaderRecompiler::CompileOptions& options,
                             const char* stage_name, std::span<const uint32_t> static_state,
                             uint32_t push_data_start_dword,
                             const std::vector<uint32_t>& spirv, std::string_view ir_dump) {
	if (!Config::GraphicsDebugDumpEnabled() ||
	    !((options.stage == ShaderType::Pixel && options.shader_hash == 0x3b809f9d156a95ddull) ||
	      (options.stage == ShaderType::Vertex && options.shader_hash == 0xe5398a1c6007f356ull))) {
		return;
	}

	// This is a compile-permutation snapshot. User data may change on later
	// draws that reuse the compiled module; no draw-time work is added here.
	using Json = nlohmann::ordered_json;
	const auto spirv_bytes = spirv.size() * sizeof(uint32_t);
	const auto spirv_hash  = XXH3_64bits(spirv.data(), spirv_bytes);
	const auto key_hash    = XXH3_64bits(static_state.data(), static_state.size_bytes());
	static std::atomic_uint32_t id = 0;
	const auto base = Config::GetShaderLogFolder() /
	                  fmt::format("{:04d}_matched_inputs_{}_{:016x}_{:016x}", id++, stage_name,
	                              options.shader_hash, spirv_hash);
	auto spirv_path = base;
	auto json_path  = base;
	auto ir_path    = base;
	spirv_path += ".spv";
	json_path += ".json";
	ir_path += ".ir.txt";
	constexpr size_t MaxIrDumpBytes = 16 * 1024 * 1024;
	const auto ir_bytes = std::min(ir_dump.size(), MaxIrDumpBytes);
	Json metadata = {
	    {"schema_version", 1},
	    {"snapshot_kind", "compile_permutation"},
	    {"stage", stage_name},
	    {"shader_hash", fmt::format("0x{:016x}", options.shader_hash)},
	    {"guest_code_base", fmt::format("0x{:016x}", params.Base())},
	    {"guest_code_words", params.code.size()},
	    {"guest_code_xxh3_64", fmt::format("0x{:016x}", XXH3_64bits(params.code.data(), params.code.size_bytes()))},
	    {"spirv_file", Common::PathToString(spirv_path.filename())},
	    {"spirv_bytes", spirv_bytes},
	    {"spirv_xxh3_64", fmt::format("0x{:016x}", spirv_hash)},
	    {"ir_file", Common::PathToString(ir_path.filename())},
	    {"ir_bytes", ir_bytes}, {"ir_original_bytes", ir_dump.size()},
	    {"ir_truncated", ir_bytes != ir_dump.size()},
	    {"static_state_xxh3_64", fmt::format("0x{:016x}", key_hash)},
	    {"static_state_words", std::vector<uint32_t>(static_state.begin(), static_state.end())},
	    {"user_data_count", params.user_data_count},
	    {"user_data_base", options.user_data_base},
	    {"user_data_words", std::vector<uint32_t>(options.user_data.begin(), options.user_data.end())},
	    {"captured_user_data_storage", params.user_data},
	    {"compile_wave_size", options.wave_size},
	    {"push_data_start_dword", push_data_start_dword},
	};
	metadata["program_key"] = {
	    {"stage", static_cast<uint32_t>(options.stage)},
	    {"hash", metadata["shader_hash"]},
	    {"user_data_count", params.user_data_count},
	    {"code_size_words", params.code.size()},
	    {"static_state_words", metadata["static_state_words"]},
	};
	if (options.stage == ShaderType::Pixel) {
		const auto& ps = *options.input_info.pixel;
		const auto input_count = std::min<uint32_t>(ps.input_num, std::size(ps.interpolator_settings));
		auto& pixel = metadata["pixel"];
		pixel = {
		    {"input_num", ps.input_num},
		    {"interpolator_settings", std::vector<uint32_t>(ps.interpolator_settings,
		                                                   ps.interpolator_settings + input_count)},
		    {"custom_interpolation_mask", ps.custom_interpolation_mask},
		    {"wave_size", ps.wave_size},
		    {"scratch_size_dwords", ps.scratch_size_dwords},
		    {"ps_system_input_base", ps.ps_system_input_base},
		    {"ps_perspective_center_vgpr", ps.ps_perspective_center_vgpr},
		    {"ps_perspective_centroid_vgpr", ps.ps_perspective_centroid_vgpr},
		    {"ps_pos_x", ps.ps_pos_x}, {"ps_pos_y", ps.ps_pos_y},
		    {"ps_pos_z", ps.ps_pos_z}, {"ps_pos_w", ps.ps_pos_w},
		    {"ps_front_face", ps.ps_front_face}, {"ps_ancillary", ps.ps_ancillary},
		    {"ps_no_perspective", ps.ps_no_perspective},
		    {"ps_sample_shading", ps.ps_sample_shading},
		    {"ps_pixel_kill_enable", ps.ps_pixel_kill_enable},
		    {"ps_depth_export_enable", ps.ps_depth_export_enable},
		    {"ps_sample_mask_export_enable", ps.ps_sample_mask_export_enable},
		    {"dual_source_blending", ps.dual_source_blending},
		    {"ps_early_z", ps.ps_early_z}, {"ps_execute_on_noop", ps.ps_execute_on_noop},
		    {"target_output_mode", std::vector<uint32_t>(std::begin(ps.target_output_mode),
		                                               std::end(ps.target_output_mode))},
		};
		pixel["interpolator_settings_hex"] = Json::array();
		for (uint32_t i = 0; i < input_count; i++) {
			pixel["interpolator_settings_hex"].push_back(fmt::format("0x{:08x}", ps.interpolator_settings[i]));
		}
		pixel["target_export_mapping_packed"] = Json::array();
		for (const auto& mapping: ps.target_export_mapping) {
			pixel["target_export_mapping_packed"].push_back(static_cast<uint32_t>(mapping.packed));
		}

		// This exact guest PS uses s28:s29 for its original SRT. Read only
		// clean backing: this diagnostic must not force GPU synchronization.
		constexpr uint32_t SrtRegister = 28;
		constexpr size_t SrtDwords = 384;
		constexpr size_t MaterialDescriptorByteOffset = 1328;
		const bool has_srt_registers = options.user_data_base <= SrtRegister &&
		                              options.user_data.size() >=
		                                  SrtRegister - options.user_data_base + 2;
		auto& srt = pixel["original_srt"];
		srt = {{"snapshot_kind", "compile_permutation"},
		       {"base_sgpr", SrtRegister}, {"registers_available", has_srt_registers},
		       {"requested_dwords", SrtDwords}, {"read_success", false},
		       {"memory_reader", "TryReadGpuCleanBacking"}, {"words", Json::array()}};
		if (has_srt_registers) {
			const auto register_index = SrtRegister - options.user_data_base;
			const uint64_t raw_address = options.user_data[register_index] |
			                             (static_cast<uint64_t>(options.user_data[register_index + 1]) << 32u);
			const uint64_t address = raw_address & 0x0000ffffffffffffull;
			srt["raw_register_address"] = fmt::format("0x{:016x}", raw_address);
			srt["address"] = fmt::format("0x{:016x}", address);
			std::array<uint32_t, SrtDwords> words {};
			const bool success = address != 0 && ReadShaderGuestMemory(nullptr, address, words);
			srt["read_success"] = success;
			if (success) {
				srt["words"] = words;
				ShaderBufferResource descriptor {};
				std::copy_n(words.data() + MaterialDescriptorByteOffset / sizeof(uint32_t),
				            std::size(descriptor.fields), descriptor.fields);
				std::array<uint32_t, 64> material_words {};
				const auto material_dwords = static_cast<size_t>(
				    std::min<uint64_t>(descriptor.GetSize(), sizeof(material_words)) / sizeof(uint32_t));
				const bool material_valid = descriptor.Type() == 0 && descriptor.Base48() != 0 &&
				                            material_dwords != 0;
				const bool material_success = material_valid && ReadShaderGuestMemory(
				    nullptr, descriptor.Base48(), std::span(material_words).first(material_dwords));
				srt["material"] = {
				    {"descriptor_table_byte_offset", MaterialDescriptorByteOffset},
				    {"descriptor_words", std::vector<uint32_t>(std::begin(descriptor.fields),
				                                                std::end(descriptor.fields))},
				    {"address", fmt::format("0x{:016x}", descriptor.Base48())},
				    {"descriptor_size_bytes", descriptor.GetSize()},
				    {"requested_bytes", material_dwords * sizeof(uint32_t)},
				    {"descriptor_valid", material_valid}, {"read_success", material_success},
				    {"words", material_success
				                  ? Json(std::vector<uint32_t>(material_words.begin(),
				                                               material_words.begin() + material_dwords))
				                  : Json::array()},
				};
			}
		}
	} else {
		const auto& vs = *options.input_info.vertex;
		auto& vertex = metadata["vertex"];
		vertex = {
		    {"logical_stage", static_cast<uint32_t>(vs.logical_stage)},
		    {"wave_size", vs.wave_size}, {"scratch_size_dwords", vs.scratch_size_dwords},
		    {"pa_cl_vs_out_cntl", vs.pa_cl_vs_out_cntl},
		    {"fetch_attrib_reg", vs.fetch_attrib_reg}, {"fetch_buffer_reg", vs.fetch_buffer_reg},
		    {"fetch_external", vs.fetch_external}, {"fetch_embedded", vs.fetch_embedded},
		    {"resources_num", vs.resources_num}, {"buffers_num", vs.buffers_num},
		    {"resources", Json::array()}, {"buffers", Json::array()},
		};
		for (int i = 0; i < std::clamp(vs.resources_num, 0, ShaderVertexInputInfo::RES_MAX); i++) {
			const auto& resource = vs.resources[i];
			const auto& destination = vs.resources_dst[i];
			vertex["resources"].push_back({
			    {"index", i}, {"fields", std::vector<uint32_t>(std::begin(resource.fields), std::end(resource.fields))},
			    {"base", fmt::format("0x{:016x}", resource.Base48())},
			    {"stride", resource.Stride()}, {"num_records", resource.NumRecords()},
			    {"format", resource.RawFormat()}, {"dst_sel_xyzw", resource.DstSelXYZW()},
			    {"swizzle_enabled", resource.SwizzleEnabled()},
			    {"out_of_bounds", resource.OutOfBounds()}, {"add_tid", resource.AddTid()},
			    {"destination", {{"register_start", destination.register_start},
			                     {"registers_num", destination.registers_num},
			                     {"attr_id", destination.attr_id}, {"fetch_index", destination.fetch_index}}},
			});
		}
		for (int i = 0; i < std::clamp(vs.buffers_num, 0, ShaderVertexInputInfo::RES_MAX); i++) {
			const auto& buffer = vs.buffers[i];
			const auto attr_count = std::clamp(buffer.attr_num, 0, ShaderVertexInputBuffer::ATTR_MAX);
			vertex["buffers"].push_back({
			    {"index", i}, {"address", fmt::format("0x{:016x}", buffer.addr)},
			    {"stride", buffer.stride}, {"num_records", buffer.num_records},
			    {"fetch_index", buffer.fetch_index}, {"attr_num", buffer.attr_num},
			    {"attr_indices", std::vector<int>(buffer.attr_indices, buffer.attr_indices + attr_count)},
			    {"attr_offsets", std::vector<uint32_t>(buffer.attr_offsets, buffer.attr_offsets + attr_count)},
			});
		}
	}
	Common::File::CreateDirectories(base.parent_path());
	const auto write_dump = [](const auto& path, const void* data, size_t size) {
		Common::File file(path);
		if (file.IsInvalid()) {
			const auto path_text = Common::PathToString(path);
			LOGF_COLOR(Log::Color::BrightRed, "Can't create file: %s\n", path_text.c_str());
			return;
		}
		file.Write(data, static_cast<uint32_t>(size));
	};
	write_dump(spirv_path, spirv.data(), spirv_bytes);
	if (ir_bytes != 0) {
		write_dump(ir_path, ir_dump.data(), ir_bytes);
	}
	const auto text = metadata.dump(2);
	write_dump(json_path, text.data(), text.size());
}

bool ValidateShaderSpirv(const char* label, uint64_t shader_hash,
                         const std::vector<uint32_t>& spirv) {
	if (!Config::ShaderValidationEnabled()) {
		return true;
	}
	spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_3);
	std::string          messages;
	tools.SetMessageConsumer([&messages](spv_message_level_t, const char*,
	                                     const spv_position_t& position, const char* message) {
		messages += fmt::format("{}: {} ({}) {}\n", static_cast<int>(position.line),
		                        static_cast<int>(position.column), static_cast<int>(position.index),
		                        message);
	});
	if (tools.Validate(spirv)) {
		return true;
	}
	spvtools::SpirvTools disassembler(SPV_ENV_VULKAN_1_2);
	std::string          text;
	disassembler.Disassemble(spirv, &text,
	                         static_cast<uint32_t>(SPV_BINARY_TO_TEXT_OPTION_NO_HEADER) |
	                             static_cast<uint32_t>(SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES) |
	                             static_cast<uint32_t>(SPV_BINARY_TO_TEXT_OPTION_COMMENT) |
	                             static_cast<uint32_t>(SPV_BINARY_TO_TEXT_OPTION_INDENT) |
	                             static_cast<uint32_t>(SPV_BINARY_TO_TEXT_OPTION_COLOR));
	LOGF_COLOR(Log::Color::BrightRed, "%s SPIR-V validation failed hash=0x%016" PRIx64 ":\n%s",
	           label, shader_hash, messages.c_str());
	LOGF("%s\n", text.c_str());
	return false;
}

} // namespace

struct PipelineCache::Permutation {
	ShaderRecompiler::IR::ResourceSpecialization specialization;
	ShaderRecompiler::IR::CompiledShaderInfo     program;
	ShaderProgram                                handle;
	// Position in the owning source's PermutationList, fixed when it is published.
	uint32_t                                     index = 0;
};

// Program preparation is split into a shared lookup (FindSource), a pure per-draw
// materialization into caller-owned StagePrep (MaterializeStage), a lock-free permutation
// lookup (FindPermutation) and an exclusive compile path (CompileAndPublish). Get composes them
// serially. Invariants:
// - `programs` nodes are never erased or moved, so a SourceEntry* stays valid for the cache
//   lifetime. Finds take m_programs_mutex shared; inserts take it exclusively.
// - A SourceEntry's plan, key and dependency list are immutable once inserted.
// - Permutations are append-only with stable addresses (PermutationList).
// - Only the O15 reuse state (KYTY_RESOURCE_REUSE) mutates an entry after insertion; it is
//   guarded by m_reuse_mutex, which is taken before m_programs_mutex when both are needed.
struct PipelineCache::ProgramCache {
	using Permutation = PipelineCache::Permutation;
	using StagePrep   = PipelineCache::StagePrep;

	struct ProgramKey {
		ShaderType            stage           = ShaderType::Unknown;
		uint64_t              hash            = 0;
		uint32_t              user_data_count = 0;
		uint32_t              code_size       = 0;
		std::vector<uint32_t> static_state;

		bool operator==(const ProgramKey&) const = default;
	};

	// Append-only permutation storage whose entries never move. The first InlineCapacity
	// entries are published without a lock: the writer (holding m_programs_mutex exclusively)
	// fills the next slot and then release-stores the count, so a reader that acquire-loads the
	// count may use every slot below it. Beyond that a source keeps growing in `overflow`,
	// which, like the previous vector, is only accessed under m_programs_mutex.
	class PermutationList {
	public:
		static constexpr uint32_t InlineCapacity = 16;

		[[nodiscard]] uint32_t PublishedInline() const {
			return m_published.load(std::memory_order_acquire);
		}
		[[nodiscard]] const Permutation& Inline(uint32_t index) const { return *m_inline[index]; }
		// Requires m_programs_mutex (shared or exclusive).
		[[nodiscard]] const std::vector<std::unique_ptr<Permutation>>& Overflow() const {
			return m_overflow;
		}
		// Requires m_programs_mutex (shared or exclusive).
		[[nodiscard]] size_t Size() const { return PublishedInline() + m_overflow.size(); }

		// Requires m_programs_mutex exclusively.
		const Permutation& Append(Permutation permutation) {
			const auto published = m_published.load(std::memory_order_relaxed);
			auto       entry     = std::make_unique<Permutation>(std::move(permutation));
			entry->index         = static_cast<uint32_t>(published + m_overflow.size());
			if (published < InlineCapacity) {
				m_inline[published] = std::move(entry);
				m_published.store(published + 1u, std::memory_order_release);
				return *m_inline[published];
			}
			m_overflow.push_back(std::move(entry));
			return *m_overflow.back();
		}

		// Requires m_programs_mutex (shared or exclusive).
		template <typename Visit>
		void ForEach(Visit&& visit) const {
			for (uint32_t i = 0; i < PublishedInline(); ++i) {
				visit(*m_inline[i]);
			}
			for (const auto& permutation: m_overflow) {
				visit(*permutation);
			}
		}

	private:
		std::array<std::unique_ptr<Permutation>, InlineCapacity> m_inline;
		std::atomic<uint32_t>                                    m_published {0};
		std::vector<std::unique_ptr<Permutation>>                m_overflow;
	};

	// O15 (KYTY_RESOURCE_REUSE): a certified output kept per source, plus a small history.
	struct PreparedState {
		ShaderRecompiler::IR::ResourceSnapshot       resources;
		ShaderRecompiler::IR::ResourceSpecialization specialization;
		PreparedResourceReads                       prepared_reads;
		size_t permutation_index = std::numeric_limits<size_t>::max();

		size_t CapacityBytes() const {
			const auto bytes = [](const auto& values) {
				return values.capacity() * sizeof(typename std::decay_t<decltype(values)>::value_type);
			};
			return prepared_reads.CapacityBytes() + bytes(resources.buffers) + bytes(resources.images) +
			       bytes(resources.samplers) + bytes(resources.flattened_srt) + bytes(resources.user_data) +
			       bytes(specialization.buffers) + bytes(specialization.images);
		}
	};

	struct ReuseState {
		PreparedState                current;
		std::array<PreparedState, 3> history;
		size_t                       next_victim = 0;
	};

	struct SourceEntry {
		explicit SourceEntry(ShaderRecompiler::IR::ResourcePlan plan)
		    : resource_plan(std::move(plan)) {
			if (!ResourceDependencyCacheEnabled()) return;
			using namespace ShaderRecompiler::IR;
			// ExtractResourcePlan clones every descriptor, flat read, branch condition,
			// indirect selector and uniform-fill root into this owned graph. Include all
			// retained register reads, even those on currently inactive paths.
			projected_key = true;
			for (const auto& inst: resource_plan.value_storage) {
				if (inst.GetOpcode() != ValueOpcode::GetUserData) continue;
				if (inst.NumArgs() != 1 || !inst.Arg(0).IsImmediate() ||
				    inst.Arg(0).GetType() != Type::ScalarReg) {
					projected_key = false;
					break;
				}
				const auto reg = RegIndex(inst.Arg(0).ScalarRegister());
				if (reg < resource_plan.user_data_base ||
				    reg - resource_plan.user_data_base >= resource_plan.user_data_count) {
					projected_key = false;
					break;
				}
				user_dependencies.push_back(reg - resource_plan.user_data_base);
			}
			std::ranges::sort(user_dependencies);
			user_dependencies.erase(std::unique(user_dependencies.begin(), user_dependencies.end()),
			                        user_dependencies.end());
		}

		// Sealed at extraction; evaluation never writes to it.
		ShaderRecompiler::IR::ResourcePlan resource_plan;
		std::vector<uint32_t>              user_dependencies;
		bool                               projected_key = false;
		PermutationList                    permutations;
		// Set under the exclusive programs lock; may be set on an already published entry.
		std::atomic<bool>                  skip_dispatch {false};
		// Mutated in place by reuse-mode refreshes; only touched under m_reuse_mutex.
		mutable ReuseState                 reuse;
	};

	// Per-caller lookup scratch (previously shared cache members). One instance must not be
	// used by two threads at once.
	struct ProgramScratch {
		ProgramScratch() { key.static_state.reserve(MaxStaticKeyWords); }

		std::span<uint8_t, PreparedResourceReads::MaxBytes> Validation() {
			// Only reuse mode reads certificates; allocate its bounded buffer on first use.
			if (validation.size() != PreparedResourceReads::MaxBytes) {
				validation.resize(PreparedResourceReads::MaxBytes);
			}
			return std::span<uint8_t, PreparedResourceReads::MaxBytes>(validation.data(),
			                                                           validation.size());
		}

		ProgramKey           key;
		std::vector<uint8_t> validation;
	};

	static ProgramScratch& ThreadScratch() {
		thread_local ProgramScratch scratch;
		return scratch;
	}

	static constexpr size_t MaxHistoryBytes = 64u * 1024u * 1024u;
	static constexpr size_t MaxHistoryEntryBytes = 256u * 1024u;

	// Requires m_reuse_mutex.
	void ExchangeHistory(ReuseState& state, size_t index) {
		auto& current = state.current;
		auto& previous = state.history[index];
		const auto old_bytes = previous.CapacityBytes();
		auto new_bytes = current.CapacityBytes();
		// The current working output is mandatory; only the three extra states count
		// against this cache budget. Release an oversized outgoing state before a hit.
		if (new_bytes > MaxHistoryEntryBytes || history_bytes - old_bytes + new_bytes > MaxHistoryBytes) {
			current = PreparedState {};
			new_bytes = 0;
			Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceCacheBudgetRejects);
		}
		std::swap(current, previous);
		history_bytes = history_bytes - old_bytes + new_bytes;
	}

	struct ProgramKeyHash {
		std::size_t operator()(const ProgramKey& key) const {
			std::size_t hash = static_cast<std::size_t>(key.stage);
			PipelineKeyHash::Mix(hash, static_cast<std::size_t>(key.hash));
			if constexpr (sizeof(std::size_t) < sizeof(uint64_t)) {
				PipelineKeyHash::Mix(hash, static_cast<std::size_t>(key.hash >> 32u));
			}
			PipelineKeyHash::Mix(hash, key.user_data_count);
			PipelineKeyHash::Mix(hash, key.code_size);
			PipelineKeyHash::Mix(hash, key.static_state.size());
			// Bucket same-shape static variants by source. ProgramKey equality performs the one
			// exact state comparison needed on a stable hit without hashing up to 429 words first.
			return hash;
		}
	};

	static constexpr std::size_t MaxStaticKeyWords = 13 + ShaderVertexInputInfo::RES_MAX * 13;

	Permutation CompilePermutation(const ShaderParams&                          params,
	                               const ShaderRecompiler::CompileOptions&      options,
	                               ShaderRecompiler::TranslateResult            translated,
	                               ShaderRecompiler::IR::ResourceSpecialization specialization,
	                               uint32_t push_data_start_dword,
	                               std::span<const uint32_t> static_state) {
		const char* stage_name = nullptr;
		switch (options.stage) {
			case ShaderType::Vertex: stage_name = "vs"; break;
			case ShaderType::Mesh: stage_name = "ms"; break;
			case ShaderType::Local: stage_name = "ls"; break;
			case ShaderType::TessellationControl: stage_name = "hs"; break;
			case ShaderType::TessellationEvaluation: stage_name = "ds"; break;
			case ShaderType::Pixel: stage_name = "ps"; break;
			case ShaderType::Compute: stage_name = "cs"; break;
			default: EXIT("invalid pipeline shader stage\n");
		}
		auto result = ShaderRecompiler::CompileProgram(std::move(translated), options,
		                                               specialization, push_data_start_dword);
		DumpMatchedShaderInputs(params, options, stage_name, static_state,
		                        push_data_start_dword, result.spirv, result.ir_dump);
		DumpShaderOriginal(stage_name, options.shader_hash, params.code, result.decoded_dump);
		if (!ValidateShaderSpirv(options.dump_label, options.shader_hash, result.spirv)) {
			DumpShaderSpirv(stage_name, options.shader_hash, result.spirv);
			EXIT("%s failed hash=0x%016" PRIx64 ": SPIR-V validation failed\n", options.dump_label,
			     options.shader_hash);
		}
		DumpShaderSpirv(stage_name, options.shader_hash, result.spirv);

		const auto module = CompileSPV(result.spirv, device);
		EXIT_IF(module == nullptr);
		if (options.dump_ir) {
			LOGF("%s SPIR-V words=%" PRIu64 " wave_size=%u\n", options.dump_label,
			     static_cast<uint64_t>(result.spirv.size()), options.wave_size);
		}
		const auto id = ++next_shader_id;
		GpuOpProfiler::RegisterShader(id, stage_name, options.shader_hash);
		return {
		    .specialization = std::move(specialization),
		    .program        = std::move(result.program).TakeCompiledInfo(),
		    .handle         = {.id = id, .module = module},
		};
	}

	template <typename InputInfo>
	static ShaderType StageOf(const InputInfo& input_info) {
		if constexpr (std::is_same_v<InputInfo, ShaderVertexInputInfo>) {
			return input_info.logical_stage;
		} else if constexpr (std::is_same_v<InputInfo, ShaderPixelInputInfo>) {
			return ShaderType::Pixel;
		} else {
			static_assert(std::is_same_v<InputInfo, ShaderComputeInputInfo>);
			return ShaderType::Compute;
		}
	}

	template <typename InputInfo>
	static void BuildKey(const ShaderParams& params, const InputInfo& input_info, ProgramKey& key) {
		key.stage           = StageOf(input_info);
		key.hash            = params.hash;
		key.user_data_count = params.user_data_count;
		key.code_size       = static_cast<uint32_t>(params.code.size());
		BuildStageStaticKey(input_info, key.static_state);
	}

	static ShaderRecompiler::IR::SrtRuntime MakeRuntime(const ShaderParams& params,
	                                                    ShaderReadAttempt&  read_attempt) {
		return {
		    .user_data                  = std::span(params.user_data).first(params.user_data_count),
		    .shader_base                = params.Base(),
		    .userdata                   = NativeDccEnabled() ? &read_attempt : nullptr,
		    .read_specialization_memory = ReadShaderGuestMemory,
		    .try_read_clean_backing = SrtReadRunsEnabled() ? TryReadShaderCleanBacking : nullptr,
		    .share_clean_values = SharedResourceEvaluationEnabled(),
		};
	}

	// Shared lookup. The returned entry stays valid for the cache lifetime.
	const SourceEntry* FindSource(const ProgramKey& key) const {
		std::shared_lock lock(m_programs_mutex);
		const auto entry = programs.find(key);
		return entry != programs.end() ? &entry->second : nullptr;
	}

	// Pure: reads the sealed plan and guest memory through `runtime` and writes only `scratch`
	// and `prep`, so any number of threads may prepare one source with their own scratch/prep.
	// A failed refresh leaves `prep` partially written; it must not be used.
	static bool MaterializeStage(const SourceEntry& source,
	                             const ShaderRecompiler::IR::SrtRuntime& runtime,
	                             ShaderRecompiler::IR::EvaluationScratch& scratch, StagePrep& prep) {
		prep.permutation = nullptr;
		return ShaderRecompiler::IR::MaterializeResources(source.resource_plan, runtime, scratch,
		                                                  prep.resources, prep.specialization);
	}

	// O15 reuse mode: refresh or reuse the certified output in the entry, then copy it into
	// `prep`. Requires m_reuse_mutex.
	bool MaterializeReusing(const SourceEntry& source, const ShaderRecompiler::IR::SrtRuntime& runtime,
	                        ShaderRecompiler::IR::EvaluationScratch& evaluation,
	                        ProgramScratch& scratch, StagePrep& prep) {
		auto&      state       = source.reuse;
		const auto user_data   = runtime.user_data;
		const bool multi_state = ResourceDependencyCacheEnabled();
		const auto validate = [&](PreparedResourceReads& reads) {
			if (!reads.Matches(user_data, runtime.shader_base, source.user_dependencies,
			                   multi_state && source.projected_key)) return false;
			Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceCacheKeyMatches);
			if (reads.Validate(scratch.Validation())) return true;
			// Matching registers with stale/dirty backing must never become a hit.
			reads.Invalidate();
			Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceCacheBackingRejects);
			return false;
		};
		const auto publish = [&] {
			prep.resources      = state.current.resources;
			prep.specialization = state.current.specialization;
			prep.permutation    = nullptr;
			return true;
		};
		const auto hit = [&] {
			// These registers also supply shader constants and vertex/instance offsets.
			// Only descriptor evaluation uses the projected key; passthrough stays live.
			state.current.resources.user_data.assign(user_data.begin(), user_data.end());
			Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceReuseHits);
			return publish();
		};
		if (validate(state.current.prepared_reads)) return hit();
		if (multi_state) {
			for (size_t index = 0; index < state.history.size(); ++index) {
				if (!validate(state.history[index].prepared_reads)) continue;
				ExchangeHistory(state, index);
				Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceCacheHistoryHits);
				return hit();
			}
			if (state.current.prepared_reads.Valid()) {
				auto victim = state.next_victim;
				for (size_t index = 0; index < state.history.size(); ++index) {
					if (!state.history[index].prepared_reads.Valid()) { victim = index; break; }
				}
				ExchangeHistory(state, victim);
				state.next_victim = (victim + 1u) % state.history.size();
			}
		}
		// The entry owns the final successful outputs. Clear their certificate
		// before any refresh can partially overwrite either output object.
		state.current.prepared_reads.Invalidate();
		state.current.permutation_index = std::numeric_limits<size_t>::max();
		Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceReuseMisses);
		state.current.prepared_reads.Begin(user_data, runtime.shader_base, scratch.Validation());
		auto observed_runtime              = runtime;
		observed_runtime.observe_read      = PreparedResourceReads::Observe;
		observed_runtime.observer_userdata = &state.current.prepared_reads;
		if (ShaderRecompiler::IR::MaterializeResources(source.resource_plan, observed_runtime,
		                                               evaluation, state.current.resources,
		                                               state.current.specialization)) {
			state.current.prepared_reads.Finish(true);
			return publish();
		}
		state.current.prepared_reads.Finish(false);
		return false;
	}

	// Materializes one stage and records a readiness failure for the retry loop.
	bool Materialize(const SourceEntry& source, const ShaderRecompiler::IR::SrtRuntime& runtime,
	                 ShaderRecompiler::IR::EvaluationScratch& evaluation, ProgramScratch& scratch,
	                 StagePrep& prep, ShaderReadAttempt& read_attempt) {
		read_attempt.count = 0;
		read_attempt.materialization_failed = false;
		read_attempt.overflow = false;
		if (ResourceReuseEnabled() ? MaterializeReusing(source, runtime, evaluation, scratch, prep)
		                           : MaterializeStage(source, runtime, evaluation, prep)) {
			return true;
		}
		// An unsuccessful optional uniform-fill/active-source probe is harmless if the
		// complete refresh succeeded. Only a failed refresh requests a retry.
		EXIT_IF(!NativeDccEnabled() || read_attempt.count == 0);
		read_attempt.materialization_failed = true;
		return false;
	}

	static bool PermutationMatches(const Permutation& candidate,
	                               const ShaderRecompiler::IR::ResourceSpecialization& specialization,
	                               uint32_t push_data_cursor) {
		const auto& layout = candidate.program.bindings;
		return layout.push_data_start_dword ==
		           ShaderRecompiler::IR::PushData::StartFor(push_data_cursor,
		                                                    layout.ShaderDataDwords()) &&
		       candidate.specialization == specialization;
	}

	// Searches in publication order. Inline entries need no lock; overflow entries need
	// m_programs_mutex, which is taken here unless the caller already holds it.
	const Permutation* FindPermutation(const SourceEntry& source,
	                                   const ShaderRecompiler::IR::ResourceSpecialization& specialization,
	                                   uint32_t push_data_cursor, bool programs_locked = false) const {
		const auto& list      = source.permutations;
		const auto  published = list.PublishedInline();
		for (uint32_t i = 0; i < published; ++i) {
			if (PermutationMatches(list.Inline(i), specialization, push_data_cursor)) {
				return &list.Inline(i);
			}
		}
		if (published < PermutationList::InlineCapacity) return nullptr;
		std::shared_lock lock(m_programs_mutex, std::defer_lock);
		if (!programs_locked) lock.lock();
		for (const auto& candidate: list.Overflow()) {
			if (PermutationMatches(*candidate, specialization, push_data_cursor)) {
				return candidate.get();
			}
		}
		return nullptr;
	}

	const Permutation* PermutationAt(const SourceEntry& source, size_t index) const {
		const auto& list      = source.permutations;
		const auto  published = list.PublishedInline();
		if (index < published) return &list.Inline(static_cast<uint32_t>(index));
		if (published < PermutationList::InlineCapacity) return nullptr;
		std::shared_lock lock(m_programs_mutex);
		const auto overflow = index - PermutationList::InlineCapacity;
		return overflow < list.Overflow().size() ? list.Overflow()[overflow].get() : nullptr;
	}

	template <typename InputInfo>
	static ShaderProgram Bind(const Permutation& permutation, InputInfo& input_info,
	                          StagePrep& prep, uint32_t& push_data_cursor) {
		prep.permutation = &permutation;
		input_info.stage = {.program = &permutation.program, .resources = &prep.resources};
		permutation.program.bindings.AdvancePushData(push_data_cursor);
		return permutation.handle;
	}

	// The compile path: translates, inserts a new source, materializes it into `prep` when
	// `prep` was not prepared from an existing entry, compiles and publishes the permutation.
	// Holds m_programs_mutex exclusively throughout, which serializes all compiles. Returns
	// null for skip-dispatch shaders and for a failed materialization.
	template <typename InputInfo>
	const Permutation* CompileAndPublish(const ShaderParams& params, InputInfo& input_info,
	                                     uint32_t push_data_cursor, const ProgramKey& key,
	                                     const ShaderRecompiler::IR::SrtRuntime& runtime,
	                                     ShaderRecompiler::IR::EvaluationScratch& evaluation,
	                                     ProgramScratch& scratch, StagePrep& prep,
	                                     ShaderReadAttempt& read_attempt, bool prep_materialized) {
		// Cache hits returned before this. This covers translation through native shader-module
		// creation; readiness failures can retry, so count successful creations separately.
		Profiler::ScopedFrameWait shader_miss(Profiler::FrameWait::ShaderProgramMiss);
		std::unique_lock lock(m_programs_mutex);
		const auto publish_index = [&](const SourceEntry& source, const Permutation& permutation) {
			if (ResourceReuseEnabled()) source.reuse.current.permutation_index = permutation.index;
			return &permutation;
		};
		// Another preparer may have inserted or compiled this source since the shared lookup.
		auto entry = programs.find(key);
		if (entry != programs.end()) {
			if (entry->second.skip_dispatch.load(std::memory_order_relaxed)) return nullptr;
			if (!prep_materialized &&
			    !Materialize(entry->second, runtime, evaluation, scratch, prep, read_attempt)) {
				return nullptr;
			}
			prep_materialized = true;
			if (const auto* published = FindPermutation(entry->second, prep.specialization,
			                                            push_data_cursor, true)) {
				return publish_index(entry->second, *published);
			}
		}

		const auto stage = key.stage;
		ShaderStageInputInfo stage_input {};
		if constexpr (std::is_same_v<InputInfo, ShaderVertexInputInfo>) {
			stage_input.vertex = &input_info;
		} else if constexpr (std::is_same_v<InputInfo, ShaderPixelInputInfo>) {
			stage_input.pixel = &input_info;
		} else {
			stage_input.compute = &input_info;
		}
		const char* label = nullptr;
		switch (stage) {
			case ShaderType::Vertex: label = "ShaderRecompiler VS"; break;
			case ShaderType::Mesh: label = "ShaderRecompiler MS"; break;
			case ShaderType::Local: label = "ShaderRecompiler LS"; break;
			case ShaderType::TessellationControl: label = "ShaderRecompiler HS"; break;
			case ShaderType::TessellationEvaluation: label = "ShaderRecompiler DS"; break;
			case ShaderType::Pixel: label = "ShaderRecompiler PS"; break;
			case ShaderType::Compute: label = "ShaderRecompiler CS"; break;
			default: EXIT("invalid pipeline shader stage\n");
		}
		ShaderRecompiler::CompileOptions options;
		options.stage       = stage;
		options.shader_hash = params.hash;
		options.user_data   = runtime.user_data;
		options.back_code      = params.back_code;
		options.dump_ir     = Config::GetShaderLogDirection() != Config::LogDirection::Silent;
		options.early_dump  = options.dump_ir;
		options.dump_label  = label;
		options.input_info  = stage_input;

		if constexpr (std::is_same_v<InputInfo, ShaderVertexInputInfo>) {
			options.user_data_base = 8;
			options.wave_size = input_info.wave_size;
			if (stage == ShaderType::Mesh || stage == ShaderType::TessellationControl) {
				options.user_data_base = 0;
				options.wave_size = stage == ShaderType::Mesh ? input_info.mesh.wave_size : 64u;
			}
		} else {
			options.wave_size = input_info.wave_size;
		}
		auto translated = ShaderRecompiler::TranslateProgram(params.code, options);
		if (translated.skip_dispatch) {
			entry = programs.try_emplace(key, ShaderRecompiler::IR::ResourcePlan {}).first;
			entry->second.skip_dispatch.store(true, std::memory_order_relaxed);
			return nullptr;
		}
		if (entry == programs.end()) {
			entry = programs.try_emplace(key,
			    ShaderRecompiler::IR::ExtractResourcePlan(translated.program)).first;
			if (!Materialize(entry->second, runtime, evaluation, scratch, prep, read_attempt)) {
				return nullptr;
			}
		}
		const auto& permutation = entry->second.permutations.Append(CompilePermutation(
		    params, options, std::move(translated), prep.specialization, push_data_cursor,
		    entry->first.static_state));
		Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderProgramsCreated);

		std::array<size_t, static_cast<size_t>(ShaderType::TessellationEvaluation) + 1> counts {};
		for (const auto& [program_key, source]: programs) {
			counts[static_cast<size_t>(program_key.stage)] += source.permutations.Size();
		}
		// Guest geometry shaders are compiled through the host mesh stage.
		std::printf("Shaders: VS %zu | PS %zu | CS %zu | GS %zu | LS %zu | HS %zu | TES %zu\n",
		            counts[static_cast<size_t>(ShaderType::Vertex)],
		            counts[static_cast<size_t>(ShaderType::Pixel)],
		            counts[static_cast<size_t>(ShaderType::Compute)],
		            counts[static_cast<size_t>(ShaderType::Mesh)],
		            counts[static_cast<size_t>(ShaderType::Local)],
		            counts[static_cast<size_t>(ShaderType::TessellationControl)],
		            counts[static_cast<size_t>(ShaderType::TessellationEvaluation)]);
		return publish_index(entry->second, permutation);
	}

	// Serial composition of the pieces above for one stage. On success `input_info.stage`
	// points at the permutation and into `prep`.
	template <typename InputInfo>
	ShaderProgram Get(const ShaderParams& params, InputInfo& input_info, StagePrep& prep,
	                  uint32_t& push_data_cursor, ShaderReadAttempt& read_attempt,
	                  ProgramScratch& scratch, ShaderRecompiler::IR::EvaluationScratch& evaluation) {
		KYTY_PROFILER_DETAIL_BLOCK("ProgramCache::Get");
		std::unique_lock<std::mutex> reuse_lock;
		if (ResourceReuseEnabled()) reuse_lock = std::unique_lock(m_reuse_mutex);

		auto& key = scratch.key;
		BuildKey(params, input_info, key);
		const auto* source = FindSource(key);
		if (source != nullptr && source->skip_dispatch.load(std::memory_order_relaxed)) {
			return {};
		}
		const auto runtime = MakeRuntime(params, read_attempt);
		if (source != nullptr) {
			if (!Materialize(*source, runtime, evaluation, scratch, prep, read_attempt)) return {};
			if (ResourceReuseEnabled() && ResourceDependencyCacheEnabled() &&
			    source->reuse.current.prepared_reads.Valid()) {
				const auto* cached = PermutationAt(*source, source->reuse.current.permutation_index);
				if (cached != nullptr &&
				    cached->program.bindings.push_data_start_dword ==
				        ShaderRecompiler::IR::PushData::StartFor(
				            push_data_cursor, cached->program.bindings.ShaderDataDwords())) {
					Profiler::CountFrameEvent(Profiler::FrameEvent::ResourceCachePermutationHits);
					return Bind(*cached, input_info, prep, push_data_cursor);
				}
			}
			if (const auto* permutation =
			        FindPermutation(*source, prep.specialization, push_data_cursor)) {
				if (ResourceReuseEnabled()) {
					source->reuse.current.permutation_index = permutation->index;
				}
				return Bind(*permutation, input_info, prep, push_data_cursor);
			}
		}
		const auto* permutation =
		    CompileAndPublish(params, input_info, push_data_cursor, key, runtime, evaluation,
		                      scratch, prep, read_attempt, source != nullptr);
		if (permutation == nullptr) return {};
		return Bind(*permutation, input_info, prep, push_data_cursor);
	}

	explicit ProgramCache(vk::Device device): device(device) {}
	~ProgramCache() {
		for (const auto& [key, entry]: programs) {
			(void)key;
			entry.permutations.ForEach([&](const Permutation& permutation) {
				device.destroyShaderModule(permutation.handle.module, nullptr);
			});
		}
	}

	std::unordered_map<ProgramKey, SourceEntry, ProgramKeyHash> programs;
	mutable std::shared_mutex                                   m_programs_mutex;
	// Serializes O15 reuse-mode preparation; ordered before m_programs_mutex.
	std::mutex                                                  m_reuse_mutex;
	size_t history_bytes = 0; // Guarded by m_reuse_mutex.
	vk::Device                                                  device;
	uint64_t next_shader_id = 0; // Guarded by the exclusive m_programs_mutex.
};

PipelineCache::PipelineCache(GraphicContext& graphics)
    : m_graphics(graphics), m_program_cache(std::make_unique<ProgramCache>(graphics.device)) {
	EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
	InitializeDriverCache();
}

PipelineCache::~PipelineCache() {
	Save();
	auto destroy = [this](const auto& pipelines) {
		for (const auto& [key, pipeline]: pipelines) {
			(void)key;
			m_graphics.device.destroyPipeline(pipeline->pipeline, nullptr);
			m_graphics.device.destroyPipelineLayout(pipeline->pipeline_layout, nullptr);
			m_graphics.device.destroyDescriptorSetLayout(pipeline->descriptor_set_layout, nullptr);
		}
	};
	destroy(m_graphics_pipelines);
	destroy(m_compute_pipelines);
	if (m_driver_cache != nullptr) {
		m_graphics.device.destroyPipelineCache(m_driver_cache, nullptr);
	}
}

void PipelineCache::InitializeDriverCache() {
	const auto title_id = PipelineCacheTitleId();
	if (title_id.empty()) {
		return;
	}
	if (KYTY_BUILD != KYTY_BUILD_RELEASE) {
		PipelineCacheLog("Vulkan pipeline cache: disabled (non-Release build)");
		return;
	}
	m_driver_cache_path    = std::filesystem::path("_PipelineCache") / (title_id + ".bin");
	const auto path         = Common::PathToString(m_driver_cache_path);
	const bool cache_exists = Common::File::IsFileExisting(m_driver_cache_path);
	if (cache_exists) {
		PipelineCacheLog("Vulkan pipeline cache: loading {}", path);
	} else {
		PipelineCacheLog("Vulkan pipeline cache: initializing {}", path);
	}
	std::vector<uint8_t> initial_data;
	if (cache_exists) {
		Common::File file(m_driver_cache_path, Common::File::Mode::Read);
		const auto   file_size = file.IsInvalid() ? 0 : file.Size();
		const auto   signature = DriverCacheSignature(m_graphics.GetPhysicalDeviceProperties());
		if (file_size >= signature.size() + sizeof(uint64_t) &&
		    file_size <= MaxDriverCacheFileSize) {
			std::string cached_signature(signature.size(), '\0');
			uint64_t    payload_hash = 0;
			initial_data.resize(file_size - signature.size() - sizeof(payload_hash));
			uint32_t signature_read = 0;
			uint32_t hash_read      = 0;
			uint32_t payload_read   = 0;
			file.Read(cached_signature.data(), static_cast<uint32_t>(cached_signature.size()),
			          &signature_read);
			file.Read(&payload_hash, sizeof(payload_hash), &hash_read);
			file.Read(initial_data.data(), static_cast<uint32_t>(initial_data.size()),
			          &payload_read);
			file.Close();
			if (signature_read != cached_signature.size() || hash_read != sizeof(payload_hash) ||
			    payload_read != initial_data.size() || cached_signature != signature ||
			    XXH3_64bits(initial_data.data(), initial_data.size()) != payload_hash) {
				initial_data.clear();
				PipelineCacheLog(
				    "Vulkan pipeline cache: invalidating {} (driver, emulator, or data mismatch)",
				    path);
			}
		} else {
			file.Close();
			PipelineCacheLog("Vulkan pipeline cache: invalidating {} (invalid file size)", path);
		}
	}

	vk::PipelineCacheCreateInfo create {};
	create.initialDataSize = initial_data.size();
	create.pInitialData    = initial_data.empty() ? nullptr : initial_data.data();
	auto result = m_graphics.device.createPipelineCache(&create, nullptr, &m_driver_cache);
	if (result != vk::Result::eSuccess && !initial_data.empty()) {
		PipelineCacheLog("Vulkan pipeline cache: driver rejected {} ({}); starting empty", path,
		                 vk::to_string(result));
		initial_data.clear();
		create.initialDataSize = 0;
		create.pInitialData    = nullptr;
		result = m_graphics.device.createPipelineCache(&create, nullptr, &m_driver_cache);
	}
	if (result != vk::Result::eSuccess) {
		PipelineCacheLog("Vulkan pipeline cache: disabled ({})", vk::to_string(result));
		m_driver_cache = nullptr;
		return;
	}
	if (!initial_data.empty()) {
		PipelineCacheLog("Vulkan pipeline cache: loaded {} bytes from {}", initial_data.size(),
		                 path);
	} else {
		PipelineCacheLog("Vulkan pipeline cache: initialized empty");
	}
}

void PipelineCache::Save() {
	Common::LockGuard lock(m_mutex);
	if (m_driver_cache == nullptr) {
		return;
	}

	size_t               size = 0;
	vk::Result           result;
	std::vector<uint8_t> payload;
	for (uint32_t attempt = 0; attempt < 3; attempt++) {
		size   = 0;
		result = m_graphics.device.getPipelineCacheData(m_driver_cache, &size, nullptr);
		if (result != vk::Result::eSuccess || size == 0 ||
		    size > std::numeric_limits<uint32_t>::max()) {
			break;
		}
		payload.resize(size);
		result = m_graphics.device.getPipelineCacheData(m_driver_cache, &size, payload.data());
		if (result != vk::Result::eIncomplete) {
			break;
		}
	}
	if (result != vk::Result::eSuccess || size == 0 ||
	    size > std::numeric_limits<uint32_t>::max()) {
		PipelineCacheLog("Vulkan pipeline cache: save failed ({}, {} bytes)",
		                 vk::to_string(result), size);
		return;
	}
	payload.resize(size);
	auto       prefix       = DriverCacheSignature(m_graphics.GetPhysicalDeviceProperties());
	const auto payload_hash = XXH3_64bits(payload.data(), payload.size());
	prefix.append(reinterpret_cast<const char*>(&payload_hash), sizeof(payload_hash));
	if (!Common::File::CreateDirectories(m_driver_cache_path.parent_path())) {
		PipelineCacheLog("Vulkan pipeline cache: failed to create cache directory");
		return;
	}
	auto temp_path = m_driver_cache_path;
	temp_path += ".tmp";
	Common::File file;
	uint32_t     prefix_written  = 0;
	uint32_t     payload_written = 0;
	if (file.Create(temp_path)) {
		file.Write(prefix.data(), static_cast<uint32_t>(prefix.size()), &prefix_written);
		file.Write(payload.data(), static_cast<uint32_t>(payload.size()), &payload_written);
	}
	const bool flushed = !file.IsInvalid() && file.Flush();
	file.Close();
	if (prefix_written != prefix.size() || payload_written != payload.size() || !flushed ||
	    !Common::File::RenameFile(temp_path, m_driver_cache_path)) {
		PipelineCacheLog("Vulkan pipeline cache: failed to write {}",
		                 Common::PathToString(m_driver_cache_path));
		return;
	}
	PipelineCacheLog("Vulkan pipeline cache: saved {} bytes to {}", payload.size(),
	                 Common::PathToString(m_driver_cache_path));
	m_graphics.device.destroyPipelineCache(m_driver_cache, nullptr);
	m_driver_cache = nullptr;
}

bool PipelineCache::TessellationActive(const HW::UserConfig& user_config) {
	return user_config.GetPrimType() == Prospero::PrimitiveType::kPatch;
}

PipelineCache::GraphicsPrograms PipelineCache::GetGraphicsPrograms(
    const HW::VertexShaderInfo& vertex_regs, const HW::PixelShaderInfo& pixel_regs,
    const HW::ShaderRegisters& sh, const HW::Context& context, const HW::UserConfig& user_config,
    std::span<const Prospero::ColorComponentMapping, 8> target_export_mapping, bool pixel_active,
    std::array<ShaderVertexInputInfo, 3>& vertex_info, ShaderPixelInputInfo& pixel_info,
    GraphicsStagePreps& stage_preps) {
	KYTY_PROFILER_DETAIL_FUNCTION();
	const bool tess_active = TessellationActive(user_config);
	std::array<ShaderParams, 3> vertex_params;
	if (tess_active) {
		vertex_params = PrepareTessellationPrograms(vertex_regs, context, vertex_info);
	} else {
		vertex_params[0] = PrepareProgram(vertex_regs, context, user_config, vertex_info[0]);
	}
	const bool mesh_active = vertex_info[0].logical_stage == ShaderType::Mesh;
	if (mesh_active) {
		EXIT_NOT_IMPLEMENTED(!m_graphics.mesh_shader_enabled);
		auto& mesh              = vertex_info[0].mesh;
		mesh.host_subgroup_size = m_graphics.subgroup_size;
		const auto& limits      = m_graphics.mesh_shader_properties;
		const auto  logical_threads =
		    mesh.threads_num[0] * mesh.threads_num[1] * mesh.threads_num[2];
		const auto host_threads = ((logical_threads + mesh.wave_size - 1u) / mesh.wave_size) *
		                          std::min(mesh.host_subgroup_size, mesh.wave_size);
		if (host_threads > limits.maxMeshWorkGroupInvocations ||
		    host_threads > limits.maxMeshWorkGroupSize[0] ||
		    mesh.max_vertices > limits.maxMeshOutputVertices ||
		    mesh.max_primitives > limits.maxMeshOutputPrimitives ||
		    mesh.lds_size_dwords * sizeof(uint32_t) > limits.maxMeshSharedMemorySize) {
			EXIT("mesh shader exceeds host limits: threads=%u vertices=%u primitives=%u LDS=%u\n",
			     host_threads, mesh.max_vertices, mesh.max_primitives, mesh.lds_size_dwords);
		}
	}
	ShaderParams pixel_params;
	if (pixel_active) {
		pixel_params = PrepareProgram(pixel_regs, sh, target_export_mapping, pixel_info);
		const auto& blend          = context.GetBlendControl(0);
		const auto  is_dual_source = [](uint8_t factor) {
			return factor >= static_cast<uint8_t>(Prospero::BlendFactor::kSrc1Color) &&
			       factor <= static_cast<uint8_t>(Prospero::BlendFactor::kOneMinusSrc1Alpha);
		};
		pixel_info.dual_source_blending =
		    blend.enable && !context.GetRenderTarget(0).info.blend_bypass &&
		    (is_dual_source(blend.color_srcblend) || is_dual_source(blend.color_destblend) ||
		     (blend.separate_alpha_blend &&
		      (is_dual_source(blend.alpha_srcblend) || is_dual_source(blend.alpha_destblend))));
		if (pixel_info.dual_source_blending) {
			// MRT1 supplies a second blend source for the same render target as MRT0.
			pixel_info.target_output_mode[1]    = pixel_info.target_output_mode[0];
			pixel_info.target_export_mapping[1] = pixel_info.target_export_mapping[0];
		}
	}
	if (context.GetClipControl().clip_disable) {
		const auto& viewport = context.GetScreenViewport().viewports[0];
		const auto& limits   = m_graphics.GetPhysicalDeviceProperties().limits;
		auto&       clip     = vertex_info[tess_active ? 2u : 0u].clip_space;
		clip.scale[0]        = viewport.xscale;
		clip.scale[1]        = viewport.yscale;
		clip.offset[0]       = viewport.xoffset;
		clip.offset[1]       = viewport.yoffset;
		clip.half_extent[0] =
		    static_cast<float>(std::min(limits.maxViewportDimensions[0], 16384u)) * 0.5f;
		clip.half_extent[1] =
		    static_cast<float>(std::min(limits.maxViewportDimensions[1], 16384u)) * 0.5f;
		clip.enabled = true;
	}
	// The program cache locks internally, so no pipeline lock is held across materialization.
	auto& scratch    = ProgramCache::ThreadScratch();
	auto& evaluation = ShaderRecompiler::IR::ThreadEvaluationScratch();
	for (uint32_t attempt = 0; attempt < 64; ++attempt) {
		ShaderReadAttempt read_attempt;
		uint32_t push_data_cursor =
		    mesh_active ? ShaderRecompiler::IR::PushData::MeshDrawDwordCount : 0;
		GraphicsPrograms result;
		if (pixel_active) {
			result.pixel = m_program_cache->Get(pixel_params, pixel_info, stage_preps.pixel,
			                                   push_data_cursor, read_attempt, scratch, evaluation);
		}
		for (uint32_t i = 0; i < (tess_active ? 3u : 1u) &&
		                     !read_attempt.materialization_failed; ++i) {
			result.vertex[i] =
			    m_program_cache->Get(vertex_params[i], vertex_info[i], stage_preps.vertex[i],
			                         push_data_cursor, read_attempt, scratch, evaluation);
		}
		if (!read_attempt.materialization_failed) return result;
		// No pipeline or texture-cache lock is held while the scheduler publishes bytes.
		// Restart all stages before final bindings/uploads, including their SRT refresh.
		EXIT_IF(!read_attempt.Synchronize());
	}
	EXIT("graphics resource readiness did not converge after 64 attempts\n");
}

ShaderProgram PipelineCache::GetComputeProgram(const HW::ComputeShaderInfo& regs,
                                               const HW::ShaderRegisters&   sh,
                                               ShaderComputeInputInfo&      input_info,
                                               StagePrep&                   stage_prep) {
	input_info.host_subgroup_size = m_graphics.SupportsComputeWave64() ? 64u : 32u;
	const auto        params      = PrepareProgram(regs, sh, input_info);
	auto& scratch    = ProgramCache::ThreadScratch();
	auto& evaluation = ShaderRecompiler::IR::ThreadEvaluationScratch();
	for (uint32_t attempt = 0; attempt < 64; ++attempt) {
		ShaderReadAttempt read_attempt;
		uint32_t push_data_cursor = 0;
		const auto result = m_program_cache->Get(params, input_info, stage_prep, push_data_cursor,
		                                         read_attempt, scratch, evaluation);
		if (!read_attempt.materialization_failed) return result;
		EXIT_IF(!read_attempt.Synchronize());
	}
	EXIT("compute resource readiness did not converge after 64 attempts\n");
}

bool PipelineStaticParameters::operator==(const PipelineStaticParameters& other) const noexcept {
	return std::memcmp(this, &other, sizeof(*this)) == 0;
}

void PipelineCache::PipelineKeyHash::MixStaticParams(std::size_t& hash,
                                                    const PipelineStaticParameters& params) {
	if (Common::RendererBatchEnabled()) {
		// Equality already compares this exact byte representation, including padding.
		Mix(hash, static_cast<std::size_t>(XXH3_64bits(&params, sizeof(params))));
		return;
	}
	const auto* bytes = reinterpret_cast<const uint8_t*>(&params);
	for (std::size_t i = 0; i < sizeof(params); ++i) Mix(hash, bytes[i]);
}

PipelineCache::Pipeline& PipelineCache::GetGraphicsPipeline(
    std::span<const RenderColorInfo> colors, const RenderDepthInfo& depth,
    std::span<const ShaderVertexInputInfo> vertex_info, CommandBuffer& command,
    const ShaderPixelInputInfo* ps_input_info, vk::PrimitiveTopology topology,
    bool primitive_restart_enable, const GraphicsPrograms& programs) {
	const auto& vs_input_info  = vertex_info.front();
	const auto& vertex_program = programs.vertex[0];
	const auto& pixel_program  = programs.pixel;
	KYTY_PROFILER_BLOCK("PipelineCache::CreatePipeline(Gfx)", profiler::colors::DeepOrangeA200);

	EXIT_IF(colors.size() > RENDER_COLOR_ATTACHMENTS_MAX);
	EXIT_IF(!vertex_program);
	const bool ps_active = ps_input_info != nullptr;
	EXIT_IF(ps_active && !pixel_program);
	const auto color_count = static_cast<uint32_t>(colors.size());

	Common::LockGuard lock(m_mutex);
	auto&             ctx = command.GetRegisters();

	const HW::ModeControl& mc = ctx.GetModeControl();

	const auto vs_id = vertex_program.id;
	const auto ps_id = ps_active ? pixel_program.id : 0;

	GraphicsPipelineKey key {};
	for (uint32_t i = 0; i < programs.vertex.size(); i++) {
		key.vertex_shader_ids[i] = programs.vertex[i].id;
	}
	key.ps_shader_id            = ps_id;
	auto& static_params         = key.static_params;
	auto& rendering             = key.rendering;
	rendering.color_count       = 0;
	uint32_t attachment_samples = 0;
	for (uint32_t i = 0; i < color_count; i++) {
		const auto slot = colors[i].target_slot;
		EXIT_IF(slot >= RENDER_COLOR_ATTACHMENTS_MAX);
		rendering.color_count = std::max(rendering.color_count, slot + 1);
		EXIT_IF(!colors[i].image_id || colors[i].desc.view_info.format == vk::Format::eUndefined);
		static_params.color_mask[slot] = colors[i].export_mapping.ApplyMask(
		    render_target_mask_slot(ctx.GetRenderTargetMask(), colors[i].target_slot));
		rendering.color_formats[slot] = colors[i].desc.view_info.format;
		if (attachment_samples == 0) {
			attachment_samples = colors[i].desc.info.samples;
		} else if (attachment_samples != colors[i].desc.info.samples) {
			EXIT("mixed color attachment sample counts are unsupported: %u and %u\n",
			     attachment_samples, colors[i].desc.info.samples);
		}
		const auto& rt                        = ctx.GetRenderTarget(colors[i].target_slot);
		const auto& bc                        = ctx.GetBlendControl(colors[i].target_slot);
		static_params.color_srcblend[slot]       = bc.color_srcblend;
		static_params.color_comb_fcn[slot]       = bc.color_comb_fcn;
		static_params.color_destblend[slot]      = bc.color_destblend;
		static_params.alpha_srcblend[slot]       = bc.alpha_srcblend;
		static_params.alpha_comb_fcn[slot]       = bc.alpha_comb_fcn;
		static_params.alpha_destblend[slot]      = bc.alpha_destblend;
		static_params.separate_alpha_blend[slot] = bc.separate_alpha_blend;
		static_params.blend_enable[slot]         = bc.enable && !rt.info.blend_bypass;
	}
	const bool with_depth =
	    depth.desc.view_info.format != vk::Format::eUndefined && static_cast<bool>(depth.image_id);
	if (with_depth) {
		const auto aspects       = ImageViewOps::DepthAspectMask(depth.desc.view_info.format);
		rendering.depth_format   = aspects & vk::ImageAspectFlagBits::eDepth
		                               ? depth.desc.view_info.format
		                               : vk::Format::eUndefined;
		rendering.stencil_format = aspects & vk::ImageAspectFlagBits::eStencil
		                               ? depth.desc.view_info.format
		                               : vk::Format::eUndefined;
		if (attachment_samples == 0) {
			attachment_samples = depth.desc.info.samples;
		} else if (attachment_samples != depth.desc.info.samples) {
			EXIT("mixed color/depth sample counts are unsupported: %u and %u\n", attachment_samples,
			     depth.desc.info.samples);
		}
	}
	if (color_count == 0 && !with_depth) {
		attachment_samples = render_sample_count(ctx.GetAaConfig().msaa_num_samples);
		EXIT_IF(!static_cast<bool>(
		    m_graphics.GetPhysicalDeviceProperties().limits.framebufferNoAttachmentsSampleCounts &
		    vulkan_sample_count(attachment_samples)));
	}
	EXIT_IF(attachment_samples == 0 ||
	        vulkan_sample_count(attachment_samples) == vk::SampleCountFlagBits {});

	if (ps_active && depth.depth_test_enable && ps_input_info->ps_execute_on_noop) {
		static std::atomic<uint32_t> log_count {0};
		if (log_count.fetch_add(1, std::memory_order_relaxed) < 16) {
			LOGF("Pipeline: temporary: accepting EXEC_ON_NOOP with depth test enabled\n");
		}
	}

	const auto& clip_control               = ctx.GetClipControl();
	static_params.negative_one_to_one      = !clip_control.dx_clip_space;
	static_params.depth_clip_enable        = clip_control.IsZClipEnabled();
	static_params.topology                 = topology;
	static_params.primitive_restart_enable = primitive_restart_enable;
	static_params.samples                  = attachment_samples;
	static_params.sample_shading_enable =
	    ps_active && attachment_samples > 1 && ps_input_info->ps_sample_shading;
	if (static_params.sample_shading_enable && !m_graphics.sample_rate_shading_enabled) {
		EXIT("Pipeline: sample-rate shading is required but unsupported by the host\n");
	}
	static_params.depth_bounds_test_enable = depth.depth_bounds_test_enable;
	static_params.depth_min_bounds         = depth.depth_min_bounds;
	static_params.depth_max_bounds         = depth.depth_max_bounds;
	const bool rect_list = Prospero::IsRectList(command.GetUserConfig().GetPrimType());
	static_params.cull_back  = !rect_list && mc.cull_back;
	static_params.cull_front = !rect_list && mc.cull_front;
	static_params.face       = mc.face;
	static_params.provoking_vtx_last = mc.provoking_vtx_last;
	static_params.polygon_mode =
	    ResolvePolygonMode(mc, static_params.cull_front, static_params.cull_back);

	if (vs_input_info.stage.program->stage != ShaderType::Mesh) {
		EXIT_IF(vs_input_info.buffers_num < 0 ||
		        vs_input_info.buffers_num > ShaderVertexInputInfo::RES_MAX ||
		        vs_input_info.resources_num < 0 ||
		        vs_input_info.resources_num > ShaderVertexInputInfo::RES_MAX);
		key.vertex_input.binding_count   = static_cast<uint8_t>(vs_input_info.buffers_num);
		key.vertex_input.attribute_count = static_cast<uint8_t>(vs_input_info.resources_num);
		uint32_t attributes_num          = 0;
		for (int binding = 0; binding < vs_input_info.buffers_num; binding++) {
			const auto& buffer = vs_input_info.buffers[binding];
			EXIT_IF(buffer.attr_num < 0 || buffer.attr_num > ShaderVertexInputBuffer::ATTR_MAX);
			attributes_num += static_cast<uint32_t>(buffer.attr_num);
			EXIT_IF(attributes_num > static_cast<uint32_t>(vs_input_info.resources_num));
			key.vertex_input.bindings[binding] = {.stride   = buffer.stride,
			                                      .instance = buffer.fetch_index != 0};
			for (int attribute = 0; attribute < buffer.attr_num; attribute++) {
				const auto index = buffer.attr_indices[attribute];
				EXIT_IF(index < 0 || index >= vs_input_info.resources_num);
				key.vertex_input.attributes[index] = {
				    .offset  = buffer.attr_offsets[attribute],
				    .binding = static_cast<uint8_t>(binding),
				};
			}
		}
		EXIT_IF(attributes_num != static_cast<uint32_t>(vs_input_info.resources_num));
	}

	if (auto iter = m_graphics_pipelines.find(key); iter != m_graphics_pipelines.end()) {
		return *iter->second;
	}

	if (graphics_debug_dump_enabled()) {
		ShaderDbgDumpInputInfo(vs_input_info);
		if (ps_active) {
			ShaderDbgDumpInputInfo(*ps_input_info);
		}
		LOGF("PipelineTrace: shader modules VS=%" PRIu64 " module=%p PS=%" PRIu64 " module=%p\n",
		     vs_id, static_cast<void*>(vertex_program.module), ps_id,
		     static_cast<void*>(pixel_program.module));
	}

	auto cached = std::make_unique<Pipeline>();
	LogPipelineTrace("CreatePipelineInternal begin", vs_id, ps_id);
	{
		Profiler::ScopedFrameWait pipeline_create(Profiler::FrameWait::GraphicsPipelineCreate);
		CreatePipelineInternal(m_graphics, *cached, rendering, key.vertex_input, vertex_info,
		                       ps_input_info, programs, static_params, m_driver_cache);
	}
	LogPipelineTrace("CreatePipelineInternal done", vs_id, ps_id);

	EXIT_NOT_IMPLEMENTED(cached->pipeline == nullptr);
	EXIT_NOT_IMPLEMENTED(cached->pipeline_layout == nullptr);
	Profiler::CountFrameEvent(Profiler::FrameEvent::GraphicsPipelinesCreated);
	GpuOpProfiler::RegisterGraphicsPipeline(cached->pipeline, key.vertex_shader_ids.data(),
	                                        static_cast<uint32_t>(key.vertex_shader_ids.size()),
	                                        ps_id);

	auto [iter, inserted] = m_graphics_pipelines.emplace(std::move(key), std::move(cached));
	EXIT_IF(!inserted);

	return *iter->second;
}

PipelineCache::Pipeline&
PipelineCache::GetComputePipeline(const ShaderComputeInputInfo& input_info,
                                  const ShaderProgram&          compute_program) {
	KYTY_PROFILER_BLOCK("PipelineCache::CreatePipeline(Compute)", profiler::colors::RedA100);

	EXIT_IF(!compute_program);

	Common::LockGuard lock(m_mutex);

	if (auto iter = m_compute_pipelines.find(compute_program.id);
	    iter != m_compute_pipelines.end()) {
		return *iter->second;
	}

	if (graphics_debug_dump_enabled()) {
		ShaderDbgDumpInputInfo(input_info);
	}

	auto cached = std::make_unique<Pipeline>();
	CreatePipelineInternal(m_graphics, *cached, input_info, compute_program.module, m_driver_cache);
	GpuOpProfiler::RegisterComputePipeline(cached->pipeline, compute_program.id);

	EXIT_NOT_IMPLEMENTED(cached->pipeline == nullptr);
	EXIT_NOT_IMPLEMENTED(cached->pipeline_layout == nullptr);

	auto [iter, inserted] = m_compute_pipelines.emplace(compute_program.id, std::move(cached));
	EXIT_IF(!inserted);

	return *iter->second;
}
} // namespace Libs::Graphics
