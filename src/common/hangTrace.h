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

// Texture descriptor (8 dwords) resolved for a draw/dispatch.
void RecordTexture(const uint32_t* fields);

// Guest GPU scheduler.
void RecordQueueWait(uint32_t queue, uint64_t wait_ns);
void RecordQueueBusy(uint32_t queue, uint64_t busy_ns, bool complete);
void RecordDoneWait(uint64_t wait_ns);
void RecordFlip();

} // namespace HangTrace

#endif /* EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_ */
