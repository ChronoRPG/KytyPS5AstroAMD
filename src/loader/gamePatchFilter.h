#ifndef KYTY_LOADER_GAME_PATCH_FILTER_H_
#define KYTY_LOADER_GAME_PATCH_FILTER_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Loader::GamePatch {

// KYTY_GAME_PATCH_DISABLE=<name substring>[;<name substring>...] skips the matching mods of the
// --game-patch file when it is loaded, without editing the file. A mod matches when a pattern is
// a case-insensitive substring of its "name". ';' and '|' both separate patterns ('|' because
// Start-Comparison -ExtraEnv values cannot contain ';'). Blank patterns are ignored, so an unset
// or empty variable changes nothing.
inline constexpr const char* kDisableEnv = "KYTY_GAME_PATCH_DISABLE";

inline char LowerAscii(char c) {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::vector<std::string> ParseDisableList(std::string_view text) {
	std::vector<std::string> patterns;
	size_t                   begin = 0;
	while (begin <= text.size()) {
		size_t end = text.find_first_of(";|", begin);
		if (end == std::string_view::npos) {
			end = text.size();
		}
		auto piece = text.substr(begin, end - begin);
		while (!piece.empty() && (piece.front() == ' ' || piece.front() == '\t')) {
			piece.remove_prefix(1);
		}
		while (!piece.empty() && (piece.back() == ' ' || piece.back() == '\t' ||
		                          piece.back() == '\r' || piece.back() == '\n')) {
			piece.remove_suffix(1);
		}
		if (!piece.empty()) {
			patterns.emplace_back(piece);
		}
		begin = end + 1;
	}
	return patterns;
}

inline bool ContainsNoCase(std::string_view text, std::string_view pattern) {
	if (pattern.empty() || pattern.size() > text.size()) {
		return false;
	}
	for (size_t start = 0; start + pattern.size() <= text.size(); start++) {
		size_t index = 0;
		while (index < pattern.size() &&
		       LowerAscii(text[start + index]) == LowerAscii(pattern[index])) {
			index++;
		}
		if (index == pattern.size()) {
			return true;
		}
	}
	return false;
}

// Index of the first pattern that disables the mod, or -1.
inline int FindDisablingPattern(std::string_view                mod_name,
                                const std::vector<std::string>& patterns) {
	for (size_t index = 0; index < patterns.size(); index++) {
		if (ContainsNoCase(mod_name, patterns[index])) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

enum class ModDecision { Apply, SkipDisabledInFile, SkipDisabledByEnv };

struct ModEntry {
	std::string name;
	bool        enabled = true;
};

struct ModSelection {
	std::vector<ModDecision> decisions;       // one per mod, in file order
	std::vector<int>         pattern_of_mod;  // disabling pattern of SkipDisabledByEnv mods, else -1
	std::vector<bool>        pattern_matched; // one per pattern: it matched at least one mod name
};

// A mod the file itself disables stays skipped whatever the patterns say; patterns only remove
// mods. A pattern that matches no mod name at all is reported by pattern_matched, so a typo is
// visible in the log instead of silently applying the mod.
inline ModSelection SelectMods(const std::vector<ModEntry>&    mods,
                               const std::vector<std::string>& patterns) {
	ModSelection selection;
	selection.decisions.reserve(mods.size());
	selection.pattern_of_mod.reserve(mods.size());
	selection.pattern_matched.assign(patterns.size(), false);
	for (const auto& mod: mods) {
		for (size_t index = 0; index < patterns.size(); index++) {
			if (ContainsNoCase(mod.name, patterns[index])) {
				selection.pattern_matched[index] = true;
			}
		}
		const int pattern = FindDisablingPattern(mod.name, patterns);
		if (!mod.enabled) {
			selection.decisions.push_back(ModDecision::SkipDisabledInFile);
			selection.pattern_of_mod.push_back(-1);
		} else if (pattern >= 0) {
			selection.decisions.push_back(ModDecision::SkipDisabledByEnv);
			selection.pattern_of_mod.push_back(pattern);
		} else {
			selection.decisions.push_back(ModDecision::Apply);
			selection.pattern_of_mod.push_back(-1);
		}
	}
	return selection;
}

} // namespace Loader::GamePatch

#endif // KYTY_LOADER_GAME_PATCH_FILTER_H_
