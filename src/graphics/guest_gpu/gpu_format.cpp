#include "graphics/guest_gpu/gpu_format.h"

#include "graphics/guest_gpu/gpu_defs.h"

#include <array>

namespace Libs::Graphics::Prospero {
namespace {

constexpr FormatInfo kFormatInfo[] = {
    {GpuEnumValue(BufferFormat::k8UNorm), vk::Format::eR8Unorm, 1, 0, 1, true, false},
    {GpuEnumValue(BufferFormat::k8SNorm), vk::Format::eUndefined, 0, 0, 1, false, false},
    {GpuEnumValue(BufferFormat::k8UInt), vk::Format::eR8Uint, 1, 0, 1, true, true},
    {GpuEnumValue(BufferFormat::k16UNorm), vk::Format::eR16Unorm, 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k16SNorm), vk::Format::eR16Snorm, 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k16UInt), vk::Format::eR16Uint, 2, 0, 2, true, true},
    {GpuEnumValue(BufferFormat::k16SInt), vk::Format::eR16Sint, 2, 0, 2, false, false},
    {GpuEnumValue(BufferFormat::k16Float), vk::Format::eR16Sfloat, 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8UNorm), vk::Format::eR8G8Unorm, 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8SNorm), vk::Format::eR8G8Snorm, 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8UInt), vk::Format::eR8G8Uint, 2, 0, 2, true, true},
    {GpuEnumValue(BufferFormat::k8_8SInt), vk::Format::eR8G8Sint, 2, 0, 2, false, false},
    {GpuEnumValue(BufferFormat::k32UInt), vk::Format::eR32Uint, 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k32SInt), vk::Format::eR32Sint, 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k32Float), vk::Format::eR32Sfloat, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16UNorm), vk::Format::eR16G16Unorm, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16SNorm), vk::Format::eR16G16Snorm, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16UInt), vk::Format::eR16G16Uint, 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k16_16SInt), vk::Format::eR16G16Sint, 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k16_16Float), vk::Format::eR16G16Sfloat, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k11_11_10Float), vk::Format::eB10G11R11UfloatPack32, 4, 0, 4, true,
     false},
    {GpuEnumValue(BufferFormat::k10_10_10_2UNorm), vk::Format::eA2B10G10R10UnormPack32, 4, 0, 4,
     true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8UNorm), vk::Format::eR8G8B8A8Unorm, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8SNorm), vk::Format::eR8G8B8A8Snorm, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8UInt), vk::Format::eR8G8B8A8Uint, 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k8_8_8_8SInt), vk::Format::eR8G8B8A8Sint, 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k32_32UInt), vk::Format::eR32G32Uint, 8, 0, 8, true, true},
    {GpuEnumValue(BufferFormat::k32_32SInt), vk::Format::eR32G32Sint, 8, 0, 8, false, false},
    {GpuEnumValue(BufferFormat::k32_32Float), vk::Format::eR32G32Sfloat, 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16UNorm), vk::Format::eR16G16B16A16Unorm, 8, 0, 8, true,
     false},
    {GpuEnumValue(BufferFormat::k16_16_16_16SNorm), vk::Format::eR16G16B16A16Snorm, 8, 0, 8, true,
     false},
    {GpuEnumValue(BufferFormat::k16_16_16_16UInt), vk::Format::eR16G16B16A16Uint, 8, 0, 8, true,
     true},
    {GpuEnumValue(BufferFormat::k16_16_16_16SInt), vk::Format::eR16G16B16A16Sint, 8, 0, 8, false,
     false},
    {GpuEnumValue(BufferFormat::k16_16_16_16Float), vk::Format::eR16G16B16A16Sfloat, 8, 0, 8, true,
     false},
    {GpuEnumValue(BufferFormat::k32_32_32UInt), vk::Format::eR32G32B32Uint, 12, 0, 12, true, true},
    {GpuEnumValue(BufferFormat::k32_32_32SInt), vk::Format::eR32G32B32Sint, 12, 0, 12, false,
     false},
    {GpuEnumValue(BufferFormat::k32_32_32Float), vk::Format::eR32G32B32Sfloat, 12, 0, 12, true,
     false},
    {GpuEnumValue(BufferFormat::k32_32_32_32UInt), vk::Format::eR32G32B32A32Uint, 16, 0, 16, true,
     true},
    {GpuEnumValue(BufferFormat::k32_32_32_32SInt), vk::Format::eR32G32B32A32Sint, 16, 0, 16, false,
     false},
    {GpuEnumValue(BufferFormat::k32_32_32_32Float), vk::Format::eR32G32B32A32Sfloat, 16, 0, 16,
     true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8Srgb), vk::Format::eR8G8B8A8Srgb, 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k9_9_9_5Float), vk::Format::eE5B9G9R9UfloatPack32, 4, 0, 0, true,
     false},
    {GpuEnumValue(BufferFormat::k5_6_5UNorm), vk::Format::eB5G6R5UnormPack16, 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k5_5_5_1UNorm), vk::Format::eR5G5B5A1UnormPack16, 2, 0, 2, true,
     false},
    {GpuEnumValue(BufferFormat::k4_4_4_4UNorm), vk::Format::eR4G4B4A4UnormPack16, 2, 0, 2, true,
     false},
    {GpuEnumValue(BufferFormat::kFmask8_S4_F4), vk::Format::eR32Sfloat, 1, 0, 1, true, false},
    {GpuEnumValue(BufferFormat::kBc1UNorm), vk::Format::eBc1RgbaUnormBlock, 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc1Srgb), vk::Format::eBc1RgbaSrgbBlock, 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc2UNorm), vk::Format::eBc2UnormBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc2Srgb), vk::Format::eBc2SrgbBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc3UNorm), vk::Format::eBc3UnormBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc3Srgb), vk::Format::eBc3SrgbBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc4UNorm), vk::Format::eBc4UnormBlock, 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc4SNorm), vk::Format::eBc4SnormBlock, 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc5UNorm), vk::Format::eBc5UnormBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc5SNorm), vk::Format::eBc5SnormBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc6UFloat), vk::Format::eBc6HUfloatBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc6SFloat), vk::Format::eBc6HSfloatBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc7UNorm), vk::Format::eBc7UnormBlock, 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc7Srgb), vk::Format::eBc7SrgbBlock, 0, 16, 0, true, false},
};

constexpr auto MakeFormatInfoLookup() {
	constexpr uint32_t                            kMaxFormat = GpuEnumValue(BufferFormat::kBc7Srgb);
	std::array<const FormatInfo*, kMaxFormat + 1> lookup {};
	for (const auto& info: kFormatInfo) {
		lookup[info.format] = &info;
	}
	return lookup;
}

constexpr auto kFormatInfoLookup = MakeFormatInfoLookup();

} // namespace

std::span<const FormatInfo> Formats() {
	return kFormatInfo;
}

const FormatInfo* FindFormatInfo(uint32_t format) {
	return format < kFormatInfoLookup.size() ? kFormatInfoLookup[format] : nullptr;
}

vk::Format SurfaceFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->vk_format : vk::Format::eUndefined;
}

uint32_t NumBytesPerElement(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->bytes_per_element : 0;
}

uint32_t BlockCompressedBytesPerBlock(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->block_compressed_bytes_per_block : 0;
}

uint32_t RenderTargetBytesPerElement(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->render_target_bytes_per_element : 0;
}

bool IsSupportedTextureFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr && info->supported_sampled_texture_format;
}

bool IsUintTextureFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr && info->unsigned_integer_texture_format;
}

bool IsFmaskTextureFormat(uint32_t format) {
	return format == GpuEnumValue(BufferFormat::kFmask8_S4_F4);
}

} // namespace Libs::Graphics::Prospero
