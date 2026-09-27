#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_REPEATTRACE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_REPEATTRACE_H_

#include <array>
#include <cstdint>
#include <span>

// Command-stream repetition trace (diagnostic only; KYTY_CP_REPEAT_TRACE=1, default off).
//
// Measures how much of the guest command stream repeats from one guest frame to the next, at
// several levels, to size cross-frame reuse of prepared draws, bindings and recorded command
// buffers (Profiling/analysis/CP-ARCHITECTURE-20260927.md). Nothing it computes feeds back into
// emulation: with the switch unset every hook is one predictable branch.
//
// Levels, per guest frame (a frame starts at the first graphics submission after
// sceAgcSuspendPoint, i.e. the processor reset):
//   - guest command buffers: content hash of every graphics/compute submission and of every
//     indirect buffer it calls or chains to;
//   - per draw (draw-prep path, KYTY_DRAW_PREP=inline|parallel): hashes of the draw arguments,
//     render targets, viewports, remaining fixed-function state, shader registers, user SGPRs,
//     prepared program ids, V#/T#/S# words, flattened SRT and user data of the prepared resource
//     snapshot, the preparation's complete read set, and the bytes of the constant/vertex data
//     its V#s point to (payload);
//   - per dispatch: compute shader registers, user SGPRs and group counts.
// The hashes of a draw are computed by the thread that prepares it (a DrawPrep worker in
// parallel mode); the GPU thread only appends a record per draw in commit (guest) order.
//
// Output (directory KYTY_CP_REPEAT_DIR, else the hang-trace directory, else
// ./_HangTrace/cprepeat-<pid>), written by a background thread:
//   repeat-frames.csv     one row per frame: counts, and for every component and composite key the
//                         fraction of the frame's draws whose key also occurs in frame-1 (m1),
//                         frame-2 (m2), frame-3 (m3) (multiset matching), in any of the three
//                         (many), and at the same draw index in frame-1 (p1)
//   repeat-draws.csv      per-draw rows (all hashes, 48-bit hex) for burst frames
//   repeat-dispatches.csv per-dispatch rows for burst frames
//   repeat-submits.csv    one row per guest submission and indirect buffer, every frame
//   repeat-dcb-<frame>.bin raw submission dwords of burst frames (KYTY_CP_REPEAT_DCB_DUMP, default
//                         on for the first 12 bursts): records of {u32 magic 'KDCB', u32 queue,
//                         u64 seq, u64 guest address, u32 dwords, u32 kind (0 dcb, 1 ce, 2 ib,
//                         3 ib-chain)} followed by the dwords
// Bursts: KYTY_CP_REPEAT_BURST_FRAMES consecutive frames (default 4) every
// KYTY_CP_REPEAT_BURST_PERIOD_MS (default 20000), at most KYTY_CP_REPEAT_BURSTS (default 30).
// Payload: the first KYTY_CP_REPEAT_PAYLOAD_PER_VSHARP bytes (default 4096) of every V# range, at
// most KYTY_CP_REPEAT_PAYLOAD_LIMIT (default 16384) per draw; KYTY_CP_REPEAT_PAYLOAD=0 skips it.
namespace Libs::Graphics {

namespace HW {
class Context;
class UserConfig;
class Shader;
struct ComputeShaderInfo;
} // namespace HW

class RenderContext;
struct DrawIndexArgs;
struct DrawAutoArgs;

namespace DrawPrep {
struct RegisterSnapshot;
struct PreparedDraw;
} // namespace DrawPrep

namespace RepeatTrace {

enum Component : uint32_t {
	Args,      // draw packet arguments, index address, index offset/object id registers
	ArgShape,  // the arguments without addresses and base vertex / first instance / offsets
	Targets,   // color and depth render targets (addresses, formats, views) and the target mask
	Viewport,  // viewports, scissors, guard bands, clip rects
	State,     // blend, depth/stencil, raster, AA, shader-stage and ShaderRegisters state
	Shader,    // shader addresses and resource registers (no user data)
	UserSgpr,  // user SGPRs of every stage and the user-data address registers
	Programs,  // prepared program permutation ids (0 when the preparation failed)
	VSharp,    // V# words of both stages (and fetch-shader vertex buffers)
	VShape,    // the same V# words without their base addresses
	TSharp,    // T# words
	SSharp,    // S# words
	Srt,       // flattened SRT words
	UserData,  // user data of the resource snapshots
	Reads,     // the preparation's read set: ranges and bytes
	ReadBytes, // the read set's bytes only (content repetition at other addresses)
	Payload,   // leading bytes behind every V# (constant and vertex data), content only
	Count
};

struct DrawRecord {
	std::array<uint64_t, Component::Count> h {};
	uint64_t packet        = 0; // guest address of the draw packet
	uint64_t vs_addr       = 0;
	uint64_t ps_addr       = 0;
	uint64_t index_addr    = 0;
	uint64_t rt0_addr      = 0;
	uint64_t depth_addr    = 0;
	uint32_t count         = 0;
	uint32_t instances     = 0;
	uint32_t n_vsharp      = 0;
	uint32_t n_tsharp      = 0;
	uint32_t n_ssharp      = 0;
	uint32_t srt_words     = 0;
	uint32_t read_ranges   = 0;
	uint32_t read_bytes    = 0;
	uint32_t payload_bytes = 0;
	uint8_t  kind          = 0; // 0 indexed, 1 auto
	uint8_t  eligible      = 0;
	uint8_t  prep_ok       = 0;
	uint8_t  committed     = 0; // the commit used the preparation
	uint8_t  failure       = 0; // DrawPrep::Failure of the preparation or its validation
	uint8_t  payload_gpu   = 0; // V# ranges skipped because the GPU owns them
	uint8_t  hashed        = 0; // the preparing thread filled the hashes
};

[[nodiscard]] bool Enabled();

// Registered once (GPU thread) so the preparing threads can consult the buffer cache's
// thread-safe GPU-ownership hint before hashing payload bytes.
void SetRenderContext(RenderContext* renderer);

// Preparing thread (any thread): fill the register, argument, preparation and payload hashes.
void HashDraw(const DrawPrep::RegisterSnapshot& registers, const DrawIndexArgs* index_args,
              const DrawAutoArgs* auto_args, bool eligible, const DrawPrep::PreparedDraw& prepared,
              DrawRecord& record);

// GPU thread.
void NoteDrawPacket(const void* packet);
[[nodiscard]] uint64_t TakeDrawPacket();
void OnDraw(const DrawRecord& record);
void OnDispatch(uint32_t queue, const HW::ComputeShaderInfo& cs, uint32_t groups_x,
                uint32_t groups_y, uint32_t groups_z, uint32_t mode);
void OnSubmission(uint32_t queue, uint64_t sequence, std::span<const uint32_t> commands,
                  std::span<const uint32_t> constant_commands);
void OnIndirectBuffer(std::span<const uint32_t> commands, bool chain);
void OnFrameBoundary();
// Direct draws that bypassed the draw-prep engine (indirect draws, off mode): counted only.
void OnUnpreparedDraw();

} // namespace RepeatTrace
} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_REPEATTRACE_H_
