#include "common/hangTrace.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <process.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace HangTrace {
namespace {

constexpr uint32_t kMaxQueues   = 64;
constexpr uint32_t kMaxModules  = 256;
constexpr uint32_t kMaxCallers  = 10;
constexpr uint64_t kScanBytes   = 48 * 1024;
constexpr uint64_t kAprRowLimit = 4'000'000;

bool EnvFlag(const char* name, bool default_value) {
	const auto* value = std::getenv(name);
	if (value == nullptr || value[0] == '\0') {
		return default_value;
	}
	return !(value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' ||
	         value[0] == 'F');
}

const auto g_start = std::chrono::steady_clock::now();

struct GuestModule {
	uint64_t    base = 0;
	uint64_t    size = 0;
	std::string name;
};
std::array<GuestModule, kMaxModules> g_modules {};
std::atomic<uint32_t>                g_module_count {0};
std::mutex                           g_module_mutex;
std::vector<std::string>             g_pending_module_rows;

struct ImportEntry {
	alignas(64) uint64_t count = 0;
	uint64_t    published      = 0;
	uint64_t    target         = 0;
	uint64_t    thunk          = 0;
	uint32_t    id             = 0;
	std::string nid;
	std::string dbg_name;
	std::string program;
};
std::mutex                                     g_import_mutex;
std::deque<std::unique_ptr<ImportEntry>>       g_imports;
std::unordered_map<uint64_t, ImportEntry*>     g_imports_by_target;
std::atomic<uint32_t>                          g_import_count {0};
std::vector<std::string>                       g_pending_import_index_rows;

thread_local uint64_t g_guest_caller = 0;

struct AprKey {
	uint32_t file_id = 0;
	uint64_t offset  = 0;
	uint64_t size    = 0;
	bool     operator==(const AprKey&) const = default;
};
struct AprKeyHash {
	size_t operator()(const AprKey& k) const {
		uint64_t h = k.file_id * 0x9E3779B97F4A7C15ull;
		h ^= k.offset + 0x632BE59BD9B4E019ull + (h << 6u) + (h >> 2u);
		h ^= k.size + 0x94D049BB133111EBull + (h << 6u) + (h >> 2u);
		return static_cast<size_t>(h);
	}
};
struct AprInfo {
	uint64_t count     = 0;
	uint64_t first_ms  = 0;
	uint64_t last_ms   = 0;
	uint64_t last_dest = 0;
};
std::mutex                                        g_apr_mutex;
std::unordered_map<AprKey, AprInfo, AprKeyHash>   g_apr_keys;
std::vector<std::string>                          g_pending_apr_rows;
uint64_t                                          g_apr_rows_total = 0;

std::mutex               g_lod_mutex;
std::vector<std::string> g_pending_lod_rows;
std::atomic<uint64_t>    g_lod_sequence {0};

struct QueueStats {
	std::atomic<uint64_t> starts {0};
	std::atomic<uint64_t> wait_ns {0};
	std::atomic<uint64_t> wait_max_ns {0};
	std::atomic<uint64_t> busy_ns {0};
	std::atomic<uint64_t> slices {0};
	std::atomic<uint64_t> incomplete {0};
};
std::array<QueueStats, kMaxQueues> g_queues {};

std::array<std::atomic<uint32_t>, 256> g_tex_count {};
std::array<std::atomic<uint64_t>, 256> g_tex_base {};
std::array<std::atomic<uint32_t>, 256> g_tex_info {};

struct Totals {
	std::atomic<uint64_t> flips {0};
	std::atomic<uint64_t> apr_reads {0};
	std::atomic<uint64_t> apr_bytes {0};
	std::atomic<uint64_t> apr_repeats {0};
	std::atomic<uint64_t> apr_errors {0};
	std::atomic<uint64_t> lod_packets {0};
	std::atomic<uint64_t> lod_prior_nonzero {0};
	std::atomic<uint64_t> tex_total {0};
	std::atomic<uint64_t> tex_mipstats {0};
	std::atomic<uint64_t> done_waits {0};
	std::atomic<uint64_t> done_ns {0};
	std::atomic<uint64_t> done_max_ns {0};
};
Totals g_totals;

std::mutex                  g_publish_mutex;
std::condition_variable_any g_publish_condition;
std::jthread                g_publisher;
std::filesystem::path       g_dir;

struct Files {
	std::FILE* summary       = nullptr;
	std::FILE* apr           = nullptr;
	std::FILE* imports       = nullptr;
	std::FILE* imports_index = nullptr;
	std::FILE* lod           = nullptr;
	std::FILE* tex           = nullptr;
	std::FILE* modules       = nullptr;
	std::FILE* queues        = nullptr;
};
Files g_files;

void UpdateMax(std::atomic<uint64_t>& target, uint64_t value) {
	auto current = target.load(std::memory_order_relaxed);
	while (value > current &&
	       !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
	}
}

uint64_t NowMs() {
	return NowNs() / 1'000'000u;
}

uint64_t OsThreadId() {
#ifdef _WIN32
	return GetCurrentThreadId();
#else
	return static_cast<uint64_t>(gettid());
#endif
}

int ProcessId() {
#ifdef _WIN32
	return _getpid();
#else
	return static_cast<int>(getpid());
#endif
}

const GuestModule* FindModule(uint64_t address) {
	const auto count = g_module_count.load(std::memory_order_acquire);
	for (uint32_t i = 0; i < count; i++) {
		const auto& m = g_modules[i];
		if (address >= m.base && address < m.base + m.size) {
			return &m;
		}
	}
	return nullptr;
}

#ifdef _WIN32
bool IsReadablePage(uint64_t address) {
	MEMORY_BASIC_INFORMATION info {};
	if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == 0) {
		return false;
	}
	if (info.State != MEM_COMMIT) {
		return false;
	}
	const auto protect = info.Protect & 0xffu;
	if ((info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
		return false;
	}
	return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
	       protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE ||
	       protect == PAGE_EXECUTE_WRITECOPY;
}

bool IsExecutablePage(uint64_t address) {
	MEMORY_BASIC_INFORMATION info {};
	if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == 0) {
		return false;
	}
	if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
		return false;
	}
	const auto protect = info.Protect & 0xffu;
	return protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE ||
	       protect == PAGE_EXECUTE_WRITECOPY;
}
#endif

bool LooksLikeCallSite(uint64_t ra, const GuestModule& m) {
#ifndef _WIN32
	if (ra != 0) {
		return false; // stack scanning is implemented for Windows only
	}
#endif
	if (ra < m.base + 7) {
		return false;
	}
#ifdef _WIN32
	// Only inspect committed executable pages: a stale stack value may point into data that is
	// unmapped or GPU-protected, and touching it must never fault.
	if (!IsExecutablePage(ra - 7) || !IsExecutablePage(ra)) {
		return false;
	}
#endif
	const auto* p = reinterpret_cast<const uint8_t*>(ra);
	if (p[-5] == 0xE8) return true;                                       // call rel32
	if (p[-6] == 0xFF && p[-5] == 0x15) return true;                      // call [rip+disp32]
	if (p[-2] == 0xFF && (p[-1] & 0xF8u) == 0xD0) return true;            // call reg
	if (p[-2] == 0xFF && (p[-1] & 0xF8u) == 0x10 && (p[-1] & 7u) != 4u &&
	    (p[-1] & 7u) != 5u)
		return true;                                                      // call [reg]
	if (p[-3] == 0xFF && (p[-2] & 0xF8u) == 0x50 && (p[-2] & 7u) != 4u) return true; // [reg+d8]
	if (p[-4] == 0xFF && p[-3] == 0x54) return true;                      // [sib+d8]
	if (p[-6] == 0xFF && (p[-5] & 0xF8u) == 0x90 && (p[-5] & 7u) != 4u) return true; // [reg+d32]
	if (p[-7] == 0xFF && p[-6] == 0x94) return true;                      // [sib+d32]
	if (p[-3] == 0xFF && p[-2] == 0x14) return true;                      // [sib]
	return false;
}

std::string FormatAddress(uint64_t address) {
	if (const auto* m = FindModule(address); m != nullptr) {
		return fmt::format("{}+0x{:x}", m->name, address - m->base);
	}
	return fmt::format("0x{:x}", address);
}

// Heuristic guest call chain: scan the current stack for values that point just after a call
// instruction inside a registered guest module. Host frames are skipped automatically.
std::string CaptureGuestCallers() {
	std::string out;
#ifdef _WIN32
	uint8_t    marker = 0;
	const auto sp     = reinterpret_cast<uint64_t>(&marker) & ~uint64_t {7};
	uint64_t   checked_page = 0;
	uint32_t   found        = 0;
	for (uint64_t address = sp; address < sp + kScanBytes && found < kMaxCallers;
	     address += 8) {
		const auto page = address & ~uint64_t {0xfff};
		if (page != checked_page) {
			if (!IsReadablePage(page)) {
				break;
			}
			checked_page = page;
		}
		const auto value = *reinterpret_cast<const uint64_t*>(address);
		const auto* m    = FindModule(value);
		if (m == nullptr || !LooksLikeCallSite(value, *m)) {
			continue;
		}
		if (!out.empty()) {
			out += ';';
		}
		out += fmt::format("{}+0x{:x}", m->name, value - m->base);
		found++;
	}
#endif
	return out;
}

std::string CsvEscape(std::string_view text) {
	std::string out;
	out.reserve(text.size() + 2);
	out += '"';
	for (char c: text) {
		if (c == '"') {
			out += '"';
		}
		out += c;
	}
	out += '"';
	return out;
}

std::FILE* OpenFile(const char* name, const char* header) {
	const auto path = g_dir / name;
	auto*      file = std::fopen(path.string().c_str(), "wb");
	if (file != nullptr && header != nullptr) {
		std::fputs(header, file);
		std::fputc('\n', file);
	}
	return file;
}

void WriteRows(std::FILE* file, std::vector<std::string>& rows) {
	if (file == nullptr) {
		rows.clear();
		return;
	}
	for (const auto& row: rows) {
		std::fputs(row.c_str(), file);
		std::fputc('\n', file);
	}
	rows.clear();
}

void Publish() {
	const auto t_ms = NowMs();

	std::vector<std::string> rows;
	{
		std::scoped_lock lock(g_module_mutex);
		rows.swap(g_pending_module_rows);
	}
	WriteRows(g_files.modules, rows);

	{
		std::scoped_lock lock(g_import_mutex);
		rows.swap(g_pending_import_index_rows);
	}
	WriteRows(g_files.imports_index, rows);

	if (g_files.imports != nullptr) {
		std::scoped_lock lock(g_import_mutex);
		for (auto& entry: g_imports) {
			const auto value =
			    std::atomic_ref<uint64_t>(entry->count).load(std::memory_order_relaxed);
			if (value != entry->published) {
				std::fprintf(g_files.imports, "%" PRIu64 ",%u,%" PRIu64 ",%" PRIu64 "\n", t_ms,
				             entry->id, value - entry->published, value);
				entry->published = value;
			}
		}
	}

	{
		std::scoped_lock lock(g_apr_mutex);
		rows.swap(g_pending_apr_rows);
	}
	WriteRows(g_files.apr, rows);

	{
		std::scoped_lock lock(g_lod_mutex);
		rows.swap(g_pending_lod_rows);
	}
	WriteRows(g_files.lod, rows);

	if (g_files.tex != nullptr) {
		for (uint32_t id = 0; id < 256; id++) {
			const auto count = g_tex_count[id].exchange(0, std::memory_order_relaxed);
			if (count == 0) {
				continue;
			}
			const auto base = g_tex_base[id].load(std::memory_order_relaxed);
			const auto info = g_tex_info[id].load(std::memory_order_relaxed);
			std::fprintf(g_files.tex,
			             "%" PRIu64 ",%u,%u,0x%" PRIx64 ",%u,%u,%u,%u\n", t_ms, id, count, base,
			             info & 0xfffu, (info >> 12u) & 0xfffu, (info >> 24u) & 0xfu,
			             (info >> 28u) & 0xfu);
		}
	}

	if (g_files.summary != nullptr) {
		auto take = [](std::atomic<uint64_t>& v) { return v.exchange(0, std::memory_order_relaxed); };
		const auto flips       = take(g_totals.flips);
		const auto apr_reads   = take(g_totals.apr_reads);
		const auto apr_bytes   = take(g_totals.apr_bytes);
		const auto apr_repeats = take(g_totals.apr_repeats);
		const auto apr_errors  = take(g_totals.apr_errors);
		const auto lod         = take(g_totals.lod_packets);
		const auto lod_prior   = take(g_totals.lod_prior_nonzero);
		const auto tex_total   = take(g_totals.tex_total);
		const auto tex_mip     = take(g_totals.tex_mipstats);
		const auto done_n      = take(g_totals.done_waits);
		const auto done_ns     = take(g_totals.done_ns);
		const auto done_max    = take(g_totals.done_max_ns);
		std::string line = fmt::format(
		    "{},{},{},{},{},{},{},{},{},{},{},{},{}", t_ms, flips, apr_reads, apr_bytes,
		    apr_repeats, apr_errors, lod, lod_prior, tex_total, tex_mip, done_n,
		    done_n != 0 ? done_ns / done_n / 1000u : 0, done_max / 1000u);
		uint64_t c_starts = 0, c_wait = 0, c_wmax = 0, c_busy = 0, c_slices = 0, c_inc = 0;
		for (uint32_t index = 0; index < kMaxQueues; index++) {
			auto&      q      = g_queues[index];
			const auto starts = take(q.starts);
			const auto wait   = take(q.wait_ns);
			const auto wmax   = take(q.wait_max_ns);
			const auto busy   = take(q.busy_ns);
			const auto slices = take(q.slices);
			const auto inc    = take(q.incomplete);
			if (starts == 0 && slices == 0) {
				if (index == 0) {
					line += ",0,0,0,0,0,0";
				}
				continue;
			}
			if (g_files.queues != nullptr) {
				std::fprintf(g_files.queues,
				             "%" PRIu64 ",%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
				             ",%" PRIu64 ",%" PRIu64 "\n",
				             t_ms, index, starts, starts != 0 ? wait / starts / 1000u : 0,
				             wmax / 1000u, busy / 1000u, slices, inc);
			}
			if (index == 0) {
				line += fmt::format(",{},{},{},{},{},{}", starts,
				                    starts != 0 ? wait / starts / 1000u : 0, wmax / 1000u,
				                    busy / 1000u, slices, inc);
			} else {
				c_starts += starts;
				c_wait += wait;
				c_wmax = std::max(c_wmax, wmax);
				c_busy += busy;
				c_slices += slices;
				c_inc += inc;
			}
		}
		line += fmt::format(",{},{},{},{},{},{}", c_starts,
		                    c_starts != 0 ? c_wait / c_starts / 1000u : 0, c_wmax / 1000u,
		                    c_busy / 1000u, c_slices, c_inc);
		std::fputs(line.c_str(), g_files.summary);
		std::fputc('\n', g_files.summary);
	}

	for (auto* file: {g_files.summary, g_files.apr, g_files.imports, g_files.imports_index,
	                  g_files.lod, g_files.tex, g_files.modules, g_files.queues}) {
		if (file != nullptr) {
			std::fflush(file);
		}
	}
}

} // namespace

bool Enabled() {
	static const bool enabled = EnvFlag("KYTY_HANG_TRACE", false);
	return enabled;
}

bool ImportsEnabled() {
	static const bool enabled = Enabled() && EnvFlag("KYTY_HANG_TRACE_IMPORTS", true);
	return enabled;
}

uint64_t NowNs() {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                 std::chrono::steady_clock::now() - g_start)
	                                 .count());
}

void Initialize() {
	if (!Enabled() || g_publisher.joinable()) {
		return;
	}
	if (const auto* dir = std::getenv("KYTY_HANG_TRACE_DIR"); dir != nullptr && dir[0] != 0) {
		g_dir = std::filesystem::path(dir);
	} else {
		const auto now  = std::chrono::system_clock::now();
		const auto secs = std::chrono::system_clock::to_time_t(now);
		std::tm    tm_v {};
#ifdef _WIN32
		localtime_s(&tm_v, &secs);
#else
		localtime_r(&secs, &tm_v);
#endif
		char stamp[32] {};
		std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm_v);
		g_dir = std::filesystem::path("_HangTrace") / fmt::format("{}-pid{}", stamp, ProcessId());
	}
	std::error_code error;
	std::filesystem::create_directories(g_dir, error);
	if (error) {
		std::printf("Hang trace: cannot create output directory %s: %s\n", g_dir.string().c_str(),
		            error.message().c_str());
		return;
	}

	std::string summary_header =
	    "t_ms,flips,apr_reads,apr_bytes,apr_repeat_reads,apr_errors,lod_packets,"
	    "lod_prior_nonzero,tex_resolved,tex_mipstats,done_waits,done_avg_us,done_max_us";
	for (const char* q: {"gfx", "compute"}) {
		summary_header += fmt::format(",{0}_starts,{0}_wait_avg_us,{0}_wait_max_us,{0}_busy_us,"
		                              "{0}_slices,{0}_incomplete",
		                              q);
	}
	g_files.summary = OpenFile("summary.csv", summary_header.c_str());
	g_files.apr     = OpenFile("apr.csv",
	                           "t_ms,host_tid,thread,file_id,offset,size,destination,bytes_read,"
	                               "result,repeat_count,first_seen_ms,previous_destination,path,"
	                               "caller,stack_callers");
	if (ImportsEnabled()) {
		g_files.imports       = OpenFile("imports.csv", "t_ms,import_id,delta,total");
		g_files.imports_index = OpenFile("imports-index.csv", "import_id,target,nid,host_function,program");
	}
	g_files.lod     = OpenFile("lodstats.csv",
	                           "t_ms,sequence,destination,size,control,report_reset,force_reset,"
	                               "prior_nonzero_dwords,prior_first_nonzero,prior_hash,prior_head8,"
	                               "prior_tail8");
	g_files.tex     = OpenFile("texstats.csv",
	                           "t_ms,counter_id,uses,last_base,min_lod,min_lod_warn,base_level,"
	                               "last_level");
	g_files.modules = OpenFile("modules.csv", "name,base,size");
	g_files.queues  = OpenFile("queues.csv",
	                           "t_ms,queue,starts,wait_avg_us,wait_max_us,busy_us,slices,incomplete");

	g_publisher = std::jthread([](std::stop_token stop) {
		std::unique_lock lock(g_publish_mutex);
		while (!stop.stop_requested()) {
			g_publish_condition.wait_for(lock, stop, std::chrono::seconds(1),
			                             [] { return false; });
			Publish();
		}
	});
	std::printf("Hang trace enabled (KYTY_HANG_TRACE=1): writing %s\n", g_dir.string().c_str());
}

void Shutdown() {
	if (g_publisher.joinable()) {
		g_publisher.request_stop();
		g_publisher.join();
		Publish();
	}
	for (auto** file: {&g_files.summary, &g_files.apr, &g_files.imports, &g_files.imports_index,
	                   &g_files.lod, &g_files.tex, &g_files.modules, &g_files.queues}) {
		if (*file != nullptr) {
			std::fclose(*file);
			*file = nullptr;
		}
	}
}

void RegisterGuestCode(uint64_t base, uint64_t size, std::string_view name) {
	if (!Enabled() || size == 0) {
		return;
	}
	std::scoped_lock lock(g_module_mutex);
	const auto count = g_module_count.load(std::memory_order_relaxed);
	for (uint32_t i = 0; i < count; i++) {
		if (g_modules[i].base == base) {
			return;
		}
	}
	if (count >= kMaxModules) {
		return;
	}
	g_modules[count].base = base;
	g_modules[count].size = size;
	g_modules[count].name = std::string(name);
	g_module_count.store(count + 1, std::memory_order_release);
	g_pending_module_rows.push_back(fmt::format("{},0x{:x},0x{:x}", CsvEscape(name), base, size));
}

bool IsGuestAddress(uint64_t address) {
	return FindModule(address) != nullptr;
}

uint64_t* AllocateImportCounter(uint64_t target, std::string_view nid, std::string_view dbg_name,
                                std::string_view program) {
	std::scoped_lock lock(g_import_mutex);
	if (auto it = g_imports_by_target.find(target); it != g_imports_by_target.end()) {
		return &it->second->count;
	}
	auto entry      = std::make_unique<ImportEntry>();
	entry->target   = target;
	entry->id       = g_import_count.fetch_add(1, std::memory_order_relaxed);
	entry->nid      = std::string(nid);
	entry->dbg_name = std::string(dbg_name);
	entry->program  = std::string(program);
	auto* raw       = entry.get();
	g_imports.push_back(std::move(entry));
	g_imports_by_target.emplace(target, raw);
	g_pending_import_index_rows.push_back(fmt::format("{},0x{:x},{},{},{}", raw->id, target,
	                                                  CsvEscape(raw->nid),
	                                                  CsvEscape(raw->dbg_name),
	                                                  CsvEscape(raw->program)));
	return &raw->count;
}

uint64_t FindImportThunk(uint64_t target) {
	std::scoped_lock lock(g_import_mutex);
	if (auto it = g_imports_by_target.find(target); it != g_imports_by_target.end()) {
		return it->second->thunk;
	}
	return 0;
}

void SetImportThunk(uint64_t target, uint64_t thunk) {
	std::scoped_lock lock(g_import_mutex);
	if (auto it = g_imports_by_target.find(target); it != g_imports_by_target.end()) {
		it->second->thunk = thunk;
	}
}

void SetGuestCaller(uint64_t return_address) {
	g_guest_caller = return_address;
}

void RecordAprRead(uint32_t file_id, std::string_view host_path, uint64_t file_offset,
                   uint64_t size, uint64_t destination, uint64_t bytes_read, int result,
                   std::string_view thread_name) {
	if (!Enabled()) {
		return;
	}
	const auto t_ms = NowMs();
	g_totals.apr_reads.fetch_add(1, std::memory_order_relaxed);
	g_totals.apr_bytes.fetch_add(bytes_read, std::memory_order_relaxed);
	if (result != 0) {
		g_totals.apr_errors.fetch_add(1, std::memory_order_relaxed);
	}
	const auto caller  = g_guest_caller != 0 ? FormatAddress(g_guest_caller) : std::string();
	const auto callers = CaptureGuestCallers();

	std::scoped_lock lock(g_apr_mutex);
	auto& info = g_apr_keys[AprKey {file_id, file_offset, size}];
	const auto previous_destination = info.last_dest;
	if (info.count == 0) {
		info.first_ms = t_ms;
	} else {
		g_totals.apr_repeats.fetch_add(1, std::memory_order_relaxed);
	}
	info.count++;
	info.last_ms   = t_ms;
	info.last_dest = destination;
	if (g_apr_rows_total >= kAprRowLimit) {
		return;
	}
	g_apr_rows_total++;
	g_pending_apr_rows.push_back(fmt::format(
	    "{},{},{},0x{:08x},{},{},0x{:x},{},{},{},{},0x{:x},{},{},{}", t_ms,
	    OsThreadId(), CsvEscape(thread_name), file_id, file_offset, size, destination, bytes_read, result,
	    info.count, info.first_ms, previous_destination, CsvEscape(host_path), caller,
	    CsvEscape(callers)));
}

void RecordLodStats(const void* destination, uint32_t size, uint32_t control) {
	if (!Enabled()) {
		return;
	}
	g_totals.lod_packets.fetch_add(1, std::memory_order_relaxed);
	if (destination == nullptr || size < 4) {
		return;
	}
	const auto* words      = static_cast<const uint32_t*>(destination);
	const auto  word_count = size / 4u;
	uint32_t    nonzero    = 0;
	int64_t     first_nz   = -1;
	uint64_t    hash       = 1469598103934665603ull;
	for (uint32_t i = 0; i < word_count; i++) {
		const auto w = words[i];
		if (w != 0) {
			nonzero++;
			if (first_nz < 0) {
				first_nz = i;
			}
		}
		hash = (hash ^ w) * 1099511628211ull;
	}
	if (nonzero != 0) {
		g_totals.lod_prior_nonzero.fetch_add(1, std::memory_order_relaxed);
	}
	const auto sequence = g_lod_sequence.fetch_add(1, std::memory_order_relaxed);
	if (sequence >= 512 && (sequence % 64u) != 0) {
		return;
	}
	std::string head;
	std::string tail;
	for (uint32_t i = 0; i < 8 && i < word_count; i++) {
		head += fmt::format("{}{:08x}", i == 0 ? "" : " ", words[i]);
	}
	const auto tail_start = word_count > 8 ? word_count - 8 : 0;
	for (uint32_t i = tail_start; i < word_count; i++) {
		tail += fmt::format("{}{:08x}", i == tail_start ? "" : " ", words[i]);
	}
	auto row = fmt::format("{},{},0x{:x},{},0x{:08x},{},{},{},{},0x{:016x},{},{}", NowMs(), sequence,
	                       reinterpret_cast<uint64_t>(destination), size, control,
	                       (control >> 19u) & 1u, (control >> 18u) & 1u, nonzero, first_nz, hash,
	                       head, tail);
	std::scoped_lock lock(g_lod_mutex);
	g_pending_lod_rows.push_back(std::move(row));
}

void RecordTexture(const uint32_t* fields) {
	if (!Enabled()) {
		return;
	}
	g_totals.tex_total.fetch_add(1, std::memory_order_relaxed);
	if (((fields[5] >> 25u) & 1u) == 0) {
		return;
	}
	g_totals.tex_mipstats.fetch_add(1, std::memory_order_relaxed);
	const auto id   = fields[6] & 0xffu;
	const auto base = ((fields[0] | (static_cast<uint64_t>(fields[1]) << 32u)) & 0xFFFFFFFFFFull)
	                  << 8u;
	const auto min_lod      = (fields[1] >> 8u) & 0xfffu;
	const auto min_lod_warn = (fields[5] >> 8u) & 0xfffu;
	const auto base_level   = (fields[3] >> 12u) & 0xfu;
	const auto last_level   = (fields[3] >> 16u) & 0xfu;
	g_tex_count[id].fetch_add(1, std::memory_order_relaxed);
	g_tex_base[id].store(base, std::memory_order_relaxed);
	g_tex_info[id].store(min_lod | (min_lod_warn << 12u) | (base_level << 24u) | (last_level << 28u),
	                     std::memory_order_relaxed);
}

void RecordQueueWait(uint32_t queue, uint64_t wait_ns) {
	if (!Enabled() || queue >= kMaxQueues) {
		return;
	}
	auto& q = g_queues[queue];
	q.starts.fetch_add(1, std::memory_order_relaxed);
	q.wait_ns.fetch_add(wait_ns, std::memory_order_relaxed);
	UpdateMax(q.wait_max_ns, wait_ns);
}

void RecordQueueBusy(uint32_t queue, uint64_t busy_ns, bool complete) {
	if (!Enabled() || queue >= kMaxQueues) {
		return;
	}
	auto& q = g_queues[queue];
	q.busy_ns.fetch_add(busy_ns, std::memory_order_relaxed);
	q.slices.fetch_add(1, std::memory_order_relaxed);
	if (!complete) {
		q.incomplete.fetch_add(1, std::memory_order_relaxed);
	}
}

void RecordDoneWait(uint64_t wait_ns) {
	if (!Enabled()) {
		return;
	}
	g_totals.done_waits.fetch_add(1, std::memory_order_relaxed);
	g_totals.done_ns.fetch_add(wait_ns, std::memory_order_relaxed);
	UpdateMax(g_totals.done_max_ns, wait_ns);
}

void RecordFlip() {
	if (!Enabled()) {
		return;
	}
	g_totals.flips.fetch_add(1, std::memory_order_relaxed);
}

} // namespace HangTrace
