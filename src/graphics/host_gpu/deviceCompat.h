#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_DEVICECOMPAT_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_DEVICECOMPAT_H_

#include <cstdint>

// Choices that let the emulator run on devices without every optional Vulkan feature it uses (AMD,
// Intel, older NVIDIA): pure functions of what the device reports, tested without a device
// (tests/DeviceCompatTests.cpp). A device that has everything (NVIDIA RTX) gets what it got before.
namespace Libs::Graphics::DeviceCompat {

// What the device reports about subgroup sizes (VK_EXT_subgroup_size_control, core in Vulkan 1.3).
struct SubgroupSizeControl {
	uint32_t min_size       = 0;     // minSubgroupSize
	uint32_t max_size       = 0;     // maxSubgroupSize
	bool     enabled        = false; // the subgroupSizeControl feature is enabled
	bool     compute        = false; // requiredSubgroupSizeStages includes the compute stage
	bool     compute_wave64 = false; // GraphicContext::compute_subgroup_size_control_enabled
};

// The subgroup size a compute pipeline requires so that one host subgroup holds one guest wave, or
// 0 to require nothing. A program runs a wave on min(wave_size, host_subgroup_size) invocations (a
// wave64 program on a 32-wide subgroup runs two lanes per invocation). A device that can run wave64
// (AMD) requires the wave size, as before. One with several sizes but no 64 (Intel: 8 to 32)
// requires that width, so the driver cannot pick a narrower one (SIMD8 or SIMD16) that splits each
// wave over several subgroups. A device with one size (NVIDIA: 32) requires nothing.
[[nodiscard]] constexpr uint32_t ComputeSubgroupSize(const SubgroupSizeControl& device,
                                                     uint32_t                   wave_size,
                                                     uint32_t host_subgroup_size) noexcept {
	if (device.compute_wave64) {
		return wave_size >= device.min_size && wave_size <= device.max_size ? wave_size : 0u;
	}
	const uint32_t size = wave_size < host_subgroup_size ? wave_size : host_subgroup_size;
	if (!device.enabled || !device.compute || device.min_size >= device.max_size ||
	    size < device.min_size || size > device.max_size) {
		return 0u;
	}
	return size;
}

} // namespace Libs::Graphics::DeviceCompat

#endif /* EMULATOR_SRC_GRAPHICS_HOST_GPU_DEVICECOMPAT_H_ */
