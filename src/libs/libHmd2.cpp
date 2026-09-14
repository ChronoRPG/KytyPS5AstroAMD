#include "common/emulatorConfig.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "loader/symbolDatabase.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Libs {

LIB_VERSION("Hmd2", 1, "Hmd2", 1, 1);

namespace Hmd2 {

constexpr int32_t ERROR_ALREADY_INITIALIZED = -1972240383; // 0x8a720001
constexpr int32_t ERROR_NOT_INITIALIZED     = -1972240382; // 0x8a720002
constexpr int32_t ERROR_PARAMETER_NULL      = -1972240376; // 0x8a720008

struct SceHmd2InitializeParam {
	void*   reserved0;
	uint8_t reserved[8];
};

struct SceHmd2DeviceInformation {
	uint32_t status;
	uint32_t reserve0;
	struct {
		struct {
			uint32_t width;
			uint32_t height;
		} panelResolution;
		struct {
			uint16_t refreshRate90Hz;
			uint16_t refreshRate120Hz;
		} flipToDisplayLatency;
	} deviceInfo;
	uint8_t hmuMount;
	uint8_t lensSeparationDistance;
	uint8_t reserve1[2];
	float   virtualImageDistance;
};

static_assert(sizeof(SceHmd2InitializeParam) == 16);
static_assert(offsetof(SceHmd2InitializeParam, reserved) == 8);
static_assert(sizeof(SceHmd2DeviceInformation) == 28);
static_assert(offsetof(SceHmd2DeviceInformation, deviceInfo) == 8);
static_assert(offsetof(SceHmd2DeviceInformation, hmuMount) == 20);
static_assert(offsetof(SceHmd2DeviceInformation, virtualImageDistance) == 24);

static std::atomic<bool> g_initialized = false;

static int32_t KYTY_SYSV_ABI Hmd2Initialize(const SceHmd2InitializeParam* param) {
	PRINT_NAME();
	if (param == nullptr) {
		return ERROR_PARAMETER_NULL;
	}
	if (g_initialized.exchange(true)) {
		return ERROR_ALREADY_INITIALIZED;
	}
	LOGF("Hmd2: initialized, virtual headset=%s\n", Config::VrEnabled() ? "enabled" : "disabled");
	return OK;
}

static int32_t KYTY_SYSV_ABI Hmd2GetDeviceInformation(SceHmd2DeviceInformation* info) {
	PRINT_NAME();
	if (!g_initialized.load()) {
		return ERROR_NOT_INITIALIZED;
	}
	if (info == nullptr) {
		return ERROR_PARAMETER_NULL;
	}
	*info = {};
	if (Config::VrEnabled()) {
		// Virtual headset defaults: READY, MOUNT, 63 mm lens separation.
		info->hmuMount               = 1;
		info->lensSeparationDistance = 63;
		// Headset characteristics: 4000x2040 panel and 2 m virtual image distance.
		info->deviceInfo.panelResolution = {4000, 2040};
		info->virtualImageDistance       = 2.0f;
		// Host presentation has no emulated headset scan-out delay; latency remains zero.
	} else {
		info->status = 2; // SCE_HMD2_DEVICE_STATUS_NOT_DETECTED
	}
	LOGF("Hmd2: device status=%u panel=%ux%u mounted=%u\n", info->status,
	     info->deviceInfo.panelResolution.width, info->deviceInfo.panelResolution.height,
	     info->hmuMount);
	return OK;
}

} // namespace Hmd2

LIB_DEFINE(InitHmd2_1) {
	LIB_FUNC("c812oYs7Vsc", Hmd2::Hmd2Initialize);
	LIB_FUNC("bIi4YUfSRys", Hmd2::Hmd2GetDeviceInformation);
}

} // namespace Libs
