#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SPIRVEMITTER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SPIRVEMITTER_H_

#include "common/common.h"
#include "common/stringUtils.h"
#include "graphics/shader/recompiler/ir/ShaderIR.h"

#include <vector>

namespace Libs::Graphics::ShaderRecompiler::Spirv {

// Denormal execution modes the host device can honor (VK_KHR_shader_float_controls, core in
// Vulkan 1.2). Guest shaders run with FLOAT_MODE 0xC0: f32 denormals flushed, f16/f64 kept.
// The device layer computes which modes are legal under its denormBehaviorIndependence.
struct HostFloatControls {
	bool denorm_flush_f32    = false;
	bool denorm_preserve_f16 = false;
	bool denorm_preserve_f64 = false;
};

void              SetHostFloatControls(const HostFloatControls& controls);
HostFloatControls GetHostFloatControls();

std::vector<uint32_t> EmitProgram(const IR::Program& program,
                                  ShaderStageInputInfo input_info);

} // namespace Libs::Graphics::ShaderRecompiler::Spirv

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SPIRVEMITTER_H_ */
