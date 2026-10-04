// Device-capability choices (graphics/host_gpu/deviceCompat.h) for the subgroup sizes, features and
// formats that NVIDIA, AMD and Intel GPUs report.
#include "graphics/host_gpu/deviceCompat.h"

#include <cstdint>
#include <cstdio>

namespace {

using namespace Libs::Graphics::DeviceCompat;

int g_failures = 0;

void Expect(bool condition, const char* what) {
	if (!condition) {
		std::printf("DeviceCompatTests: failed: %s\n", what);
		g_failures++;
	}
}

void TestComputeSubgroupSize() {
	// NVIDIA (RTX 3090): one size, so nothing is required, as before.
	const SubgroupSizeControl nvidia {.min_size = 32, .max_size = 32, .enabled = false,
	                                  .compute = true, .compute_wave64 = false};
	Expect(ComputeSubgroupSize(nvidia, 32, 32) == 0, "NVIDIA: wave32 requires nothing");
	Expect(ComputeSubgroupSize(nvidia, 64, 32) == 0, "NVIDIA: wave64 (two lanes) requires nothing");
	// AMD (RDNA, Windows or RADV): the wave size, as before.
	const SubgroupSizeControl amd {.min_size = 32, .max_size = 64, .enabled = true, .compute = true,
	                               .compute_wave64 = true};
	Expect(ComputeSubgroupSize(amd, 32, 64) == 32, "AMD: wave32 requires 32");
	Expect(ComputeSubgroupSize(amd, 64, 64) == 64, "AMD: wave64 requires 64");
	// Intel (Arc, Iris Xe, UHD: 8 to 32): 32 for both wave sizes (wave64 runs two lanes each).
	const SubgroupSizeControl intel {.min_size = 8, .max_size = 32, .enabled = true, .compute = true,
	                                 .compute_wave64 = false};
	Expect(ComputeSubgroupSize(intel, 32, 32) == 32, "Intel: wave32 requires 32");
	Expect(ComputeSubgroupSize(intel, 64, 32) == 32, "Intel: wave64 on 32 lanes requires 32");
	// Intel Xe2 (16 to 32).
	const SubgroupSizeControl xe2 {.min_size = 16, .max_size = 32, .enabled = true, .compute = true,
	                               .compute_wave64 = false};
	Expect(ComputeSubgroupSize(xe2, 32, 32) == 32, "Xe2: wave32 requires 32");
	// Without the feature, or without the compute stage, nothing can be required.
	auto no_feature    = intel;
	no_feature.enabled = false;
	Expect(ComputeSubgroupSize(no_feature, 32, 32) == 0, "no subgroupSizeControl: nothing");
	auto no_stage    = intel;
	no_stage.compute = false;
	Expect(ComputeSubgroupSize(no_stage, 32, 32) == 0, "compute not in the required stages: nothing");
	// A size outside the device's range is never required.
	const SubgroupSizeControl narrow {.min_size = 4, .max_size = 16, .enabled = true, .compute = true,
	                                  .compute_wave64 = false};
	Expect(ComputeSubgroupSize(narrow, 32, 32) == 0, "32 above the maximum: nothing");
	// GCN on AMD's Windows driver (64 only) keeps its wave64 requirement and cannot narrow wave32.
	const SubgroupSizeControl gcn {.min_size = 64, .max_size = 64, .enabled = true, .compute = true,
	                               .compute_wave64 = true};
	Expect(ComputeSubgroupSize(gcn, 64, 64) == 64, "GCN: wave64 requires 64");
	Expect(ComputeSubgroupSize(gcn, 32, 64) == 0, "GCN: wave32 cannot be required");
}

} // namespace

int main() {
	TestComputeSubgroupSize();
	if (g_failures != 0) {
		std::printf("DeviceCompatTests: failed: %d check(s)\n", g_failures);
		return 1;
	}
	std::printf("DeviceCompatTests: ok\n");
	return 0;
}
