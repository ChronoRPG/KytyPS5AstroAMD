#ifndef KYTY_COMMON_RENDERER_BATCH_H_
#define KYTY_COMMON_RENDERER_BATCH_H_

#include <cstdlib>
#include <cstring>

namespace Common {

// One opt-in switch for the combined renderer candidate. The default retains the
// established paths so the batch can be disabled without replacing the binary.
inline bool RendererBatchEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_RENDERER_BATCH");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

} // namespace Common

#endif
