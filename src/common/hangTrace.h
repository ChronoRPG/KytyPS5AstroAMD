#ifndef EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_
#define EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_

// Loading/hang flight recorder.
//
// Always-cheap, opt-in (KYTY_HANG_TRACE=1) recorder that runs from process start without a
// profiler connection. A background thread writes plain CSV files once per second, so an
// intermittent failure is captured whether or not anyone armed a trace beforehand. Two runs
// (one that loads, one that does not) can then be diffed directly.
//
// Output directory: KYTY_HANG_TRACE_DIR, or ./_HangTrace/<timestamp>-pid<pid>.
//   summary.csv        per-second totals (flips, APR reads/repeats, LOD packets, textures with
//                      mip-stat counters, per-queue GPU wait/busy, AgcSuspendPoint/Done waits)
//   apr.csv            every APR file read: path, range, destination, repeat count, guest callers
//   imports.csv        per-second call counts of every guest->HLE import (KYTY_HANG_TRACE_IMPORTS)
//   imports-index.csv  import id -> NID / host function / importing module
//   lodstats.csv       sampled GET_LOD_STATS packets with the buffer contents *before* the
//                      emulator overwrites them (shows what the guest left there)
//   texstats.csv       per-second use of T# mip-statistics counters (id, base, min LOD)
//   modules.csv        guest module load addresses, to map caller addresses to module offsets
//   queues.csv         per-second, per active guest GPU queue: submission wait and busy time
//   readbacks.csv      every GPU->CPU readback: cause, range, duration, guest thread and pc
//   images.csv         texture-cache image deletions with reason, and per-second native image
//                      create/destroy totals grouped by format, extent and usage

#include <cstdint>
#include <string_view>

namespace HangTrace {

[[nodiscard]] bool Enabled();
[[nodiscard]] bool ImportsEnabled();

void Initialize();
void Shutdown();

[[nodiscard]] uint64_t NowNs();

// Guest code map, used to validate stack-scanned return addresses.
void RegisterGuestCode(uint64_t base, uint64_t size, std::string_view name);
[[nodiscard]] bool IsGuestAddress(uint64_t address);

// Import call counters. Returns the address of a 64-bit counter the import thunk increments.
// One counter per distinct target, shared by all call sites.
[[nodiscard]] uint64_t* AllocateImportCounter(uint64_t target, std::string_view nid,
                                              std::string_view dbg_name, std::string_view program);
[[nodiscard]] uint64_t FindImportThunk(uint64_t target);
void                   SetImportThunk(uint64_t target, uint64_t thunk);

// APR: the guest return address of the current APR submit call on this thread.
void SetGuestCaller(uint64_t return_address);
void RecordAprRead(uint32_t file_id, std::string_view host_path, uint64_t file_offset,
                   uint64_t size, uint64_t destination, uint64_t bytes_read, int result,
                   std::string_view thread_name);

// GET_LOD_STATS: call before the emulator writes the destination.
void RecordLodStats(const void* destination, uint32_t size, uint32_t control);

// LOD report consumer discovery (KYTY_HANG_TRACE_LOD_WATCH, default on with the trace): after
// the emulator writes a report, its plain read/write pages are briefly made inaccessible; the
// first guest access is logged (pc, registers, surrounding code bytes) and the original
// protection restored. Disarm before the emulator itself reads or writes the report.
void DisarmLodReportWatch();
void ArmLodReportWatch(const void* destination, uint32_t size);
// Called from the host access-violation handler. Returns true when the fault was a watch hit.
[[nodiscard]] bool HandleLodWatchFault(uint64_t fault_vaddr, bool write, uint64_t pc,
                                       const uint64_t* gpr16, std::string_view thread_name);

// Texture descriptor (8 dwords) resolved for a draw/dispatch.
void RecordTexture(const uint32_t* fields);

// GPU->CPU readbacks (BufferCache::ReadMemory), attributed to the path that requested them.
enum class ReadbackKind : uint8_t { Invalidate, FaultRead, FaultWrite, GpuSync };
// Guest access fault context for readbacks on this thread (instruction address, guest thread).
void SetFaultContext(uint64_t pc, std::string_view thread_name);
void ClearFaultContext();
void SetReadbackKind(ReadbackKind kind);
void RecordReadback(uint64_t vaddr, uint64_t size, uint64_t window_begin, uint64_t window_size,
                    bool downloaded, uint64_t duration_ns);

// Which kind of recorded GPU write last marked a guest page GPU-owned (reported per readback).
enum class GpuWriteKind : uint8_t { ShaderStorage, OcclusionDump, Fill, Copy };
class ScopedGpuWriteKind {
public:
	explicit ScopedGpuWriteKind(GpuWriteKind kind);
	~ScopedGpuWriteKind();
	ScopedGpuWriteKind(const ScopedGpuWriteKind&)            = delete;
	ScopedGpuWriteKind& operator=(const ScopedGpuWriteKind&) = delete;

private:
	GpuWriteKind m_previous;
};
void NoteGpuWrite(uint64_t vaddr, uint64_t size);

// Texture-cache image deletion reasons (set around FreeImage calls) and native image churn.
enum class ImageFreeReason : uint8_t {
	Other,
	DepthAssociation,
	DepthRecreate,
	OverlapLayout,
	OverlapMipMerge,
	OverlapStale,
	Expand,
	SmallerResources,
	Unmap,
	GarbageCollect,
	PressureCollect,
	Count
};
void SetImageFreeReason(ImageFreeReason reason);
void RecordImageFree(uint64_t address, uint32_t width, uint32_t height, uint32_t levels,
                     uint32_t layers, uint32_t format, uint64_t bytes, uint64_t age_ticks);
void RecordNativeImage(bool create, bool pool_hit, uint32_t format, uint32_t width, uint32_t height,
                       uint32_t levels, uint32_t usage, uint64_t bytes);

// Guest GPU scheduler.
void RecordQueueWait(uint32_t queue, uint64_t wait_ns);
void RecordQueueBusy(uint32_t queue, uint64_t busy_ns, bool complete);
void RecordDoneWait(uint64_t wait_ns);
void RecordFlip();

} // namespace HangTrace

#endif /* EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_ */
