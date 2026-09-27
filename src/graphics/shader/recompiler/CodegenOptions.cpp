#include "graphics/shader/recompiler/CodegenOptions.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace Libs::Graphics::ShaderRecompiler {
namespace {

// Unset or empty keeps the default; "0" disables; anything else enables.
bool EnvFlag(const char* name, bool default_value) {
	const auto* value = std::getenv(name);
	if (value == nullptr || value[0] == '\0') {
		return default_value;
	}
	return std::strcmp(value, "0") != 0;
}

CodegenOptions FromEnvironment() {
	CodegenOptions options;
	options.movrel_range = EnvFlag("KYTY_MOVREL_RANGE", options.movrel_range);
	options.fast_float_min_max = EnvFlag("KYTY_FAST_FMINMAX", options.fast_float_min_max);
	options.fast_pkrtz         = EnvFlag("KYTY_FAST_PKRTZ", options.fast_pkrtz);
	options.single_f2i_saturation =
	    EnvFlag("KYTY_SINGLE_F2I_SATURATION", options.single_f2i_saturation);
	options.lod_stats_gate = EnvFlag("KYTY_LOD_STATS_GATE", options.lod_stats_gate);
	options.robust_buffer_loads = EnvFlag("KYTY_ROBUST_BUFFER_LOADS", options.robust_buffer_loads);
	options.interp_modes = EnvFlag("KYTY_INTERP_MODES", options.interp_modes);
	options.sample_offsets = EnvFlag("KYTY_SAMPLE_OFFSETS", options.sample_offsets);
	options.sample_lod_clamp = EnvFlag("KYTY_SAMPLE_LOD_CLAMP", options.sample_lod_clamp);
	options.host_ftz_inputs  = EnvFlag("KYTY_HOST_FTZ_INPUTS", options.host_ftz_inputs);
	options.exec_selects     = EnvFlag("KYTY_EXEC_SELECTS", options.exec_selects);
	options.ps_append_live_election =
	    EnvFlag("KYTY_PS_APPEND_LIVE_ELECTION", options.ps_append_live_election);
	if (const auto* mode = std::getenv("KYTY_PS_LIVE_EXEC"); mode != nullptr && mode[0] != '\0') {
		options.ps_live_exec = std::strcmp(mode, "all") == 0 ? PsLiveExec::All
		                       : std::strcmp(mode, "0") == 0 ? PsLiveExec::Off
		                                                     : PsLiveExec::AppendConsume;
	}
	if (const auto* budget = std::getenv("KYTY_LOOP_GUARD"); budget != nullptr) {
		options.loop_guard_budget = static_cast<uint32_t>(std::strtoul(budget, nullptr, 0));
	}
	if (const auto* list = std::getenv("KYTY_LOOP_GUARD_SHADERS"); list != nullptr) {
		std::string_view text(list);
		while (!text.empty()) {
			const auto comma = text.find(',');
			const auto token = std::string(text.substr(0, comma));
			if (!token.empty()) {
				options.loop_guard_shaders.push_back(std::strtoull(token.c_str(), nullptr, 16));
			}
			if (comma == std::string_view::npos) {
				break;
			}
			text.remove_prefix(comma + 1);
		}
	}
	if (const auto* mode = std::getenv("KYTY_MAD_MODE"); mode != nullptr) {
		if (std::strcmp(mode, "exact") == 0) {
			options.mad_mode = MadMode::Exact;
		} else if (std::strcmp(mode, "fused") == 0) {
			options.mad_mode = MadMode::Fused;
		} else if (std::strcmp(mode, "position") == 0) {
			options.mad_mode = MadMode::Position;
		}
	}
	return options;
}

CodegenOptions& Storage() {
	static CodegenOptions options = FromEnvironment();
	return options;
}

} // namespace

const CodegenOptions& GetCodegenOptions() {
	return Storage();
}

bool LoopGuardApplies(uint64_t shader_hash) {
	const auto& options = Storage();
	return options.loop_guard_budget != 0 &&
	       std::ranges::find(options.loop_guard_shaders, shader_hash) !=
	           options.loop_guard_shaders.end();
}

void SetCodegenOptions(const CodegenOptions& options) {
	Storage() = options;
}

} // namespace Libs::Graphics::ShaderRecompiler
