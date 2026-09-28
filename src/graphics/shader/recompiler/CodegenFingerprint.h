#ifndef EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENFINGERPRINT_H_
#define EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENFINGERPRINT_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler {

// The persistent program cache (KYTY_PROGRAM_CACHE, pipeline/programDiskCache.h) keeps what
// translating a program produces across runs. Each program is keyed by its own inputs (guest
// code, stage key, compile options); everything else that can change a translation is here:
//
// - CodegenVersion(): a hash, computed at build time (src/codegen_version.cmake), of every source
//   file of the recompiler and of the code it calls (the translator, IR passes, SPIR-V emitter,
//   resource planning, the stage static key, guest format tables and the cache's own encoding),
//   plus the compiler and its flags. A build that changes none of them keeps its cache; any
//   change to one of them starts a new cache file. The build fails when one of these files
//   includes a project header outside that set without an entry in the script's exemption list.
// - CodegenFingerprint(): the runtime state translation reads besides a program's inputs: every
//   CodegenOptions field, the host device state the emitter reads (float controls, buffer
//   robustness, image features), KYTY_RENDERER_BATCH (resource plan flow aliases), the buffer
//   cache page constants the emitter bakes in, and the raw value of every KYTY_* environment
//   variable named in a string literal of the build-time source set (CodegenSwitches(): collected
//   by the same script, so a switch added to the recompiler joins the fingerprint on its own).
[[nodiscard]] std::string_view CodegenVersion();
[[nodiscard]] std::span<const char* const> CodegenSwitches();
[[nodiscard]] std::vector<uint8_t> CodegenFingerprint();

} // namespace Libs::Graphics::ShaderRecompiler

#endif // EMULATOR_SRC_GRAPHICS_SHADER_RECOMPILER_CODEGENFINGERPRINT_H_
