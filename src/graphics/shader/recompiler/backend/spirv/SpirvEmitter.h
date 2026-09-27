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

// Storage-buffer robustness the device guarantees. With robustBufferAccess2 enabled and
// robustStorageBufferAccessSizeAlignment == 1, a 32-bit load from a storage-buffer descriptor
// returns 0 exactly when some byte of it lies outside the descriptor range, which is the
// shader's own "dword index < OpArrayLength" check. The device layer sets this once.
struct HostBufferRobustness {
	bool storage_dword_loads_return_zero = false;
};

void                 SetHostBufferRobustness(const HostBufferRobustness& robustness);
HostBufferRobustness GetHostBufferRobustness();

// Optional image features of the device (set once by the device layer).
struct HostImageFeatures {
	// shaderResourceMinLod: the MinLod image operand, used for IMAGE_SAMPLE*_CL.
	bool min_lod = false;
};

void              SetHostImageFeatures(const HostImageFeatures& features);
HostImageFeatures GetHostImageFeatures();

// mip_stats_records=false emits the plain variant of a GET_LOD_STATS-instrumented pixel shader:
// the same bindings and code, without the per-sample feedback (and so without its storage-buffer
// atomics, which force depth/stencil tests after the shader for a shader that can discard).
std::vector<uint32_t> EmitProgram(const IR::Program& program,
                                  ShaderStageInputInfo input_info, bool mip_stats_records = true);

} // namespace Libs::Graphics::ShaderRecompiler::Spirv

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_SPIRVEMITTER_H_ */
