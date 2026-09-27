#include "loader/gamePatchFilter.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using namespace Loader::GamePatch;

void Check(bool value, const char* message) {
	if (!value) {
		std::fprintf(stderr, "GamePatchFilterTests: failed: %s\n", message);
		std::abort();
	}
}

// The two mods of the Astro Bot "non RT patch" (PPSA21567.json), in file order.
std::vector<ModEntry> AstroMods() {
	return {{"Select the existing non-tiled deferred-lighting renderer", true},
	        {"Disable GI probes and lighting shaders", true}};
}

void TestParse() {
	Check(ParseDisableList("").empty(), "an empty variable disables nothing");
	Check(ParseDisableList(" ; |;\t").empty(), "blank patterns are ignored");

	const auto one = ParseDisableList("non-tiled");
	Check(one.size() == 1 && one[0] == "non-tiled", "a single pattern");

	const auto two = ParseDisableList(" renderer ;GI probes\r\n");
	Check(two.size() == 2 && two[0] == "renderer" && two[1] == "GI probes",
	      "';' separates patterns and surrounding blanks are trimmed");

	const auto piped = ParseDisableList("renderer|GI probes");
	Check(piped.size() == 2 && piped[0] == "renderer" && piped[1] == "GI probes",
	      "'|' separates patterns too (Start-Comparison -ExtraEnv cannot pass ';')");

	const auto inner = ParseDisableList("lighting shaders");
	Check(inner.size() == 1 && inner[0] == "lighting shaders", "inner spaces are kept");
}

void TestMatch() {
	Check(ContainsNoCase("Disable GI probes and lighting shaders", "gi PROBES"),
	      "matching ignores case");
	Check(!ContainsNoCase("Disable GI probes", "GI probes and more"),
	      "a pattern longer than the name never matches");
	Check(!ContainsNoCase("anything", ""), "an empty pattern never matches");
	Check(FindDisablingPattern("Select the existing non-tiled deferred-lighting renderer",
	                           {"probe", "NON-TILED"}) == 1,
	      "the first matching pattern is reported");
	Check(FindDisablingPattern("Disable GI probes and lighting shaders", {"renderer"}) == -1,
	      "a non-matching pattern leaves the mod alone");
}

void TestSelectAstro() {
	const auto mods = AstroMods();

	auto none = SelectMods(mods, {});
	Check(none.decisions[0] == ModDecision::Apply && none.decisions[1] == ModDecision::Apply,
	      "without patterns every enabled mod applies");

	auto mod1 = SelectMods(mods, ParseDisableList("non-tiled"));
	Check(mod1.decisions[0] == ModDecision::SkipDisabledByEnv &&
	          mod1.decisions[1] == ModDecision::Apply && mod1.pattern_of_mod[0] == 0 &&
	          mod1.pattern_of_mod[1] == -1 && mod1.pattern_matched[0],
	      "'non-tiled' removes only the renderer-selection mod");

	auto mod2 = SelectMods(mods, ParseDisableList("GI probes"));
	Check(mod2.decisions[0] == ModDecision::Apply &&
	          mod2.decisions[1] == ModDecision::SkipDisabledByEnv,
	      "'GI probes' removes only the GI-probe mod");

	auto both = SelectMods(mods, ParseDisableList("renderer|gi probes"));
	Check(both.decisions[0] == ModDecision::SkipDisabledByEnv &&
	          both.decisions[1] == ModDecision::SkipDisabledByEnv && both.pattern_of_mod[0] == 0 &&
	          both.pattern_of_mod[1] == 1,
	      "two patterns remove both mods");

	auto shared = SelectMods(mods, ParseDisableList("lighting"));
	Check(shared.decisions[0] == ModDecision::SkipDisabledByEnv &&
	          shared.decisions[1] == ModDecision::SkipDisabledByEnv,
	      "a substring common to both names removes both");

	auto typo = SelectMods(mods, ParseDisableList("non tiled;renderer"));
	Check(!typo.pattern_matched[0] && typo.pattern_matched[1] &&
	          typo.decisions[0] == ModDecision::SkipDisabledByEnv &&
	          typo.decisions[1] == ModDecision::Apply,
	      "a pattern that matches no mod is reported as unmatched");
}

void TestFileDisabled() {
	std::vector<ModEntry> mods = {{"First mod", false}, {"Second mod", true}};

	auto plain = SelectMods(mods, {});
	Check(plain.decisions[0] == ModDecision::SkipDisabledInFile &&
	          plain.decisions[1] == ModDecision::Apply,
	      "a mod disabled in the file stays skipped");

	auto matched = SelectMods(mods, ParseDisableList("first"));
	Check(matched.decisions[0] == ModDecision::SkipDisabledInFile &&
	          matched.pattern_of_mod[0] == -1 && matched.pattern_matched[0] &&
	          matched.decisions[1] == ModDecision::Apply,
	      "a pattern never re-enables a mod and still counts as matched");

	auto all = SelectMods(mods, ParseDisableList("mod"));
	Check(all.decisions[0] == ModDecision::SkipDisabledInFile &&
	          all.decisions[1] == ModDecision::SkipDisabledByEnv,
	      "patterns can leave no mod to apply");

	auto empty = SelectMods({}, ParseDisableList("x"));
	Check(empty.decisions.empty() && empty.pattern_matched.size() == 1 &&
	          !empty.pattern_matched[0],
	      "a file without mods reports every pattern as unmatched");
}

} // namespace

int main() {
	TestParse();
	TestMatch();
	TestSelectAstro();
	TestFileDisabled();
	std::printf("GamePatchFilterTests: all passed\n");
	return 0;
}
