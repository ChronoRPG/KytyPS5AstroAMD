#include "graphics/host_gpu/renderer/lodStats.h"

#include "common/assert.h"
#include "common/hangTrace.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/cache/streamBuffer.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "kernel/memory.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace Libs::Graphics {

namespace {

constexpr uint64_t CounterBytes = uint64_t {LodStatsCounter::Entries} * 2u * sizeof(uint32_t);
constexpr uint64_t NoData       = 0x0F00000000000000ull;

} // namespace

bool LodStatsCounter::Enabled() {
	static const bool enabled = [] {
		const auto* mode = std::getenv("KYTY_LOD_STATS_MODE");
		return mode == nullptr || mode[0] == 0 || std::strcmp(mode, "gpu") == 0;
	}();
	return enabled;
}

// After a report's GPU copy completes, rewrite its guest slot with that packet's own interval
// (hardware semantics) when the guest has not touched it yet. KYTY_LOD_REPORT_COMPLETION_WRITE=0
// keeps only the record-time write of older statistics (U26-U32 behaviour).
bool LodStatsCounter::CompletionWriteEnabled() {
	static const bool enabled = [] {
		const char* value = std::getenv("KYTY_LOD_REPORT_COMPLETION_WRITE");
		return value == nullptr || value[0] != '0';
	}();
	return enabled;
}

LodStatsCounter::LodStatsCounter(RenderContext& context): m_context(context) {}

LodStatsCounter::~LodStatsCounter() = default;

Buffer& LodStatsCounter::CounterBuffer() {
	if (m_counters == nullptr) {
		// Contents stay undefined until the first report resets them; that report is not
		// published.
		const auto usage = vk::BufferUsageFlagBits::eStorageBuffer |
		                   vk::BufferUsageFlagBits::eTransferSrc |
		                   vk::BufferUsageFlagBits::eTransferDst;
		m_counters = std::make_unique<Buffer>(m_context.GetGraphics(),
		                                      m_context.GetCommandScheduler(),
		                                      MemoryUsage::DeviceLocal, 0, usage, CounterBytes);
	}
	return *m_counters;
}

void LodStatsCounter::Report(uint64_t destination, uint32_t size, uint32_t control) {
	KYTY_GPU_OP_SITE("lodstats.report");
	auto& scheduler = m_context.GetCommandScheduler();
	scheduler.EndRendering();
	auto& counters = CounterBuffer();
	if (m_publish == nullptr) {
		const auto usage = vk::BufferUsageFlagBits::eTransferDst;
		m_publish = std::make_unique<Buffer>(m_context.GetGraphics(), scheduler,
		                                     MemoryUsage::Download, 0, usage,
		                                     PublishSlots * PublishSlotSize);
	}
	// The guest may read this report as soon as the packet is recorded (EOP labels are written at
	// record time). Write the newest completed statistics now; until the first GPU copy has
	// completed, write an unready report ("no data yet").
	const bool writes_report = destination != 0 && size >= ReportSize;
	auto       record_time_report = std::make_shared<std::array<uint8_t, ReportSize>>();
	if (writes_report) {
		auto&                     report = *record_time_report;
		HangTrace::LodReportEvent event;
		{
			std::scoped_lock lock(m_latest_mutex);
			if (m_has_latest) {
				report = m_latest;
			}
			event.has_latest       = m_has_latest;
			event.sampled_counters = m_latest_sampled;
			event.total_samples    = m_latest_samples;
			event.mean_finest_mip  = m_latest_mean_finest;
		}
		(void)LibKernel::Memory::TryWriteBacking(destination, report.data(), report.size());
		if (HangTrace::Enabled()) {
			event.destination    = destination;
			event.control        = control;
			event.pending_copies = m_issued - m_completed.load(std::memory_order_acquire);
			HangTrace::RecordLodReport(event);
		}
	}

	auto native = scheduler.Current().Handle();

	vk::MemoryBarrier barrier {};
	barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                       vk::PipelineStageFlagBits::eTransfer, {}, 1, &barrier, 0, nullptr, 0,
	                       nullptr);

	const bool publish = m_initialized;
	uint64_t   slot_offset = 0;
	if (publish) {
		const auto slot = static_cast<uint32_t>(m_issued % PublishSlots);
		if (m_issued >= PublishSlots && !scheduler.IsFree(m_slot_ticks[slot])) {
			scheduler.Wait(m_slot_ticks[slot]);
		}
		slot_offset = uint64_t {slot} * PublishSlotSize;
		// The wait above may have finished the recording; record into the current one.
		native = scheduler.Current().Handle();
		const vk::BufferCopy copy {0, slot_offset, CounterBytes};
		native.copyBuffer(counters.Handle(), m_publish->Handle(), 1, &copy);
	}

	// GET_LOD_STATS control bit 19 reports and resets, bit 18 forces a reset.
	const bool reset = !m_initialized || ((control >> 18u) & 3u) != 0u;
	if (reset && publish) {
		// The reset overwrites the counters the copy above reads (write-after-read hazard found
		// by synchronization validation): the fills must not start before the copy has read.
		vk::MemoryBarrier war {};
		war.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		war.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		native.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
		                       vk::PipelineStageFlagBits::eTransfer, {}, 1, &war, 0, nullptr, 0,
		                       nullptr);
	}
	if (reset) {
		constexpr uint64_t MinBytes = uint64_t {Entries} * sizeof(uint32_t);
		native.fillBuffer(counters.Handle(), 0, MinBytes, 0xffffffffu);
		native.fillBuffer(counters.Handle(), MinBytes, MinBytes, 0u);
	}
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite |
	                        vk::AccessFlagBits::eHostRead;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
	                       vk::PipelineStageFlagBits::eAllCommands | vk::PipelineStageFlagBits::eHost,
	                       {}, 1, &barrier, 0, nullptr, 0, nullptr);
	m_initialized = true;

	if (!publish) {
		return;
	}
	m_slot_ticks[m_issued % PublishSlots] = scheduler.CurrentTick();
	++m_issued;
	const bool completion_write = writes_report && CompletionWriteEnabled();
	scheduler.DeferOperation([this, slot_offset, destination, completion_write,
	                          record_time_report, control] {
		m_publish->Invalidate(slot_offset, CounterBytes);
		const auto* words = reinterpret_cast<const uint32_t*>(m_publish->Mapped().data() + slot_offset);
		std::array<uint8_t, ReportSize> report {};
		const uint32_t valid = 1;
		std::memcpy(report.data(), &valid, sizeof(valid));
		if (completion_write) {
			// This packet's own interval is now known. The record-time write had to use older
			// statistics; replace them with what hardware writes at this packet, unless the guest
			// has already consumed or rewritten the slot (then a late write would look new).
			std::array<uint8_t, ReportSize> exact = report;
			uint32_t                        exact_sampled = 0;
			for (uint32_t counter = 0; counter < Counters; counter++) {
				const auto count = words[Entries + counter];
				uint64_t   entry = NoData;
				if (count != 0) {
					entry = (uint64_t {std::min<uint32_t>(words[counter], 14u)} << 56u) |
					        std::min<uint32_t>(count, 0xffffffu);
					exact_sampled++;
				}
				std::memcpy(exact.data() + 64 + counter * sizeof(uint64_t), &entry, sizeof(entry));
			}
			std::array<uint8_t, ReportSize> current {};
			const bool unchanged =
			    LibKernel::Memory::TryReadBacking(destination, current.data(), current.size()) &&
			    current == *record_time_report;
			if (unchanged) {
				(void)LibKernel::Memory::TryWriteBacking(destination, exact.data(), exact.size());
			}
			if (HangTrace::Enabled()) {
				HangTrace::LodReportEvent event;
				event.destination      = destination;
				event.control          = control;
				event.has_latest       = unchanged; // completion rows: 1 = slot rewritten
				event.sampled_counters = exact_sampled;
				event.pending_copies   = ~0ull;     // marks a completion row
				HangTrace::RecordLodReport(event);
			}
		}
		std::scoped_lock lock(m_latest_mutex);
		uint32_t sampled      = 0;
		uint64_t samples      = 0;
		uint64_t finest_total = 0;
		for (uint32_t counter = 0; counter < Counters; counter++) {
			// Guests issue several reports per frame, each covering part of it; combine the two
			// newest intervals so a texture sampled in only one part is not reported unsampled.
			const auto finest = std::min(words[counter], m_previous_finest[counter]);
			const auto count  = words[Entries + counter] + m_previous_count[counter];
			m_previous_finest[counter] = words[counter];
			m_previous_count[counter]  = words[Entries + counter];
			uint64_t   entry  = NoData;
			if (count != 0) {
				entry = (uint64_t {std::min<uint32_t>(finest, 14u)} << 56u) |
				        std::min<uint32_t>(count, 0xffffffu);
				sampled++;
				samples += count;
				finest_total += std::min<uint32_t>(finest, 14u);
			}
			std::memcpy(report.data() + 64 + counter * sizeof(uint64_t), &entry, sizeof(entry));
		}
		m_latest             = report;
		m_has_latest         = true;
		m_latest_sampled     = sampled;
		m_latest_samples     = samples;
		m_latest_mean_finest = sampled != 0 ? static_cast<double>(finest_total) / sampled : 0.0;
		m_completed.fetch_add(1, std::memory_order_release);
	});
}

} // namespace Libs::Graphics
