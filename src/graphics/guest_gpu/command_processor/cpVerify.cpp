#include "graphics/guest_gpu/command_processor/cpVerify.h"

#include "common/logging/log.h"
#include "common/profiler.h"

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstring>

namespace Libs::Graphics::CpSeq {

Verifier::Verifier(RenderContext& renderer, int interrupt_event_id)
    : m_reference(std::make_unique<CommandProcessor>(renderer, interrupt_event_id)),
      m_execution(std::make_unique<Pm4Execution>()) {
	m_reference->m_front_mode = CommandProcessor::FrontMode::Reference;
	m_reference->m_capture    = this;
}

Verifier::~Verifier() = default;

void Verifier::Attach(const CommandProcessor& primary, uint64_t stream_id,
                      std::span<const uint32_t> commands) {
	m_reference->CopyFrontState(primary);
	*m_execution = Pm4Execution {};
	m_execution->m_buffer_stack.push_back({commands});
	m_captured.clear();
	m_answer.reset();
	m_stream_id = stream_id;
	m_following = true;
	GetVerifyTotals().streams.fetch_add(1, std::memory_order_relaxed);
}

void Verifier::Detach() noexcept {
	m_following = false;
	m_stream_id = 0;
	m_captured.clear();
	m_answer.reset();
}

void Verifier::MirrorCeComplete(bool complete) {
	m_reference->m_ce_complete = complete;
}

Verifier::Step Verifier::Advance() {
	while (m_captured.empty()) {
		if (m_execution->m_buffer_stack.empty()) {
			return Step::End;
		}
		if (m_reference->ReferenceStep(*m_execution) && m_captured.empty()) {
			// Suspended without an op: a front-only wait (REWIND, CE counters) the primary passed.
			return Step::Suspended;
		}
	}
	return Step::Op;
}

void Verifier::Fail(bool read_divergence, const char* what, const OpView* op,
                    const CapturedOp* reference) {
	auto& totals = GetVerifyTotals();
	if (read_divergence) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::CpSeqVerifyReadDivergences);
		totals.read_divergences.fetch_add(1, std::memory_order_relaxed);
	} else {
		Profiler::CountFrameEvent(Profiler::FrameEvent::CpSeqVerifyMismatches);
		totals.mismatches.fetch_add(1, std::memory_order_relaxed);
	}
	static std::atomic<uint32_t> logged {0};
	if (logged.fetch_add(1, std::memory_order_relaxed) < 64) {
		uint64_t address = 0;
		if (!m_execution->m_buffer_stack.empty()) {
			const auto& cursor = m_execution->m_buffer_stack.back();
			address = reinterpret_cast<uint64_t>(cursor.commands.data() + cursor.offset_dw);
		}
		LOGF("CpSeqVerify: %s: stream %" PRIu64 ", op %" PRIu64 " %s at packet %" PRIu64
		     ", reference %s at packet %" PRIu64 " (reference cursor 0x%016" PRIx64 ")\n",
		     what, m_stream_id, op != nullptr ? op->header->sequence : 0,
		     op != nullptr ? OpKindName(op->Kind()) : "-",
		     op != nullptr ? op->header->packet : 0,
		     reference != nullptr ? OpKindName(reference->kind) : "-",
		     reference != nullptr ? reference->packet : 0, address);
	}
	// "exit" stops at a mismatch. A read divergence is the race class E2 (the fronts read
	// different guest bytes at their different times): counted and logged only; tests without
	// races assert that there is none.
	if (VerifyMode() == 2 && !read_divergence) {
		EXIT("CpSeqVerify: %s (%s)\n", what,
		     op != nullptr ? OpKindName(op->Kind()) : "end of stream");
	}
	Detach();
}

void Verifier::Before(const OpView& op) {
	if ((op.header->flags & FlagVerify) != 0 &&
	    HashOp(op.Kind(), op.payload, PayloadSize(op.Kind()), op.data, op.DataSize()) !=
	        op.header->verify_hash) {
		Fail(false, "the op changed in the op ring", &op, nullptr);
		return;
	}
	const auto step = Advance();
	GetVerifyTotals().checks.fetch_add(1, std::memory_order_relaxed);
	Profiler::CountFrameEvent(Profiler::FrameEvent::CpSeqVerifyChecks);
	if (step != Step::Op) {
		Fail(false,
		     step == Step::End ? "the reference front's stream ended before this op"
		                       : "the reference front stopped at a wait the front passed",
		     &op, nullptr);
		return;
	}
	const auto reference = std::move(m_captured.front());
	m_captured.pop_front();
	const auto payload_size = PayloadSize(op.Kind());
	// The fronts parsed different command bytes (another packet count or content before this op):
	// a command buffer changed between the front's read and the reference's, which only a guest
	// race can do (e.g. the CPU patching a buffer the CP loops over). Later ops are not
	// comparable.
	if (reference.packet != op.header->packet) {
		Fail(true, "the fronts parsed a different number of packets", &op, &reference);
		return;
	}
	if (reference.packets_hash != op.header->packets_hash) {
		Fail(true, "the fronts parsed different packet bytes before the op", &op, &reference);
		return;
	}
	if (reference.kind != op.Kind()) {
		Fail(false, "different op kind", &op, &reference);
		return;
	}
	if (reference.data_size != op.DataSize() || reference.payload_size != payload_size) {
		Fail(false, "different inline data size", &op, &reference);
		return;
	}
	if (op.Kind() == OpKind::ReadCheck) {
		const auto& front = op.As<ReadCheckOp>();
		ReadCheckOp serial {};
		std::memcpy(&serial, reference.bytes.data(), sizeof(serial));
		if (front.address != serial.address || front.size != serial.size) {
			Fail(false, "different guest read", &op, &reference);
		} else if (front.hash != serial.hash) {
			Fail(true, "the front's guest read differs from the serial read", &op, &reference);
		}
		return;
	}
	// Thread-mode transport fields (window slot, snapshot) exist on the front's side only.
	alignas(8) std::array<uint8_t, 128> front {};
	alignas(8) std::array<uint8_t, 128> serial {};
	EXIT_IF(payload_size > front.size());
	std::memcpy(front.data(), op.payload, payload_size);
	std::memcpy(serial.data(), reference.bytes.data(), payload_size);
	NormalizeForCompare(op.Kind(), front.data());
	NormalizeForCompare(op.Kind(), serial.data());
	if (std::memcmp(serial.data(), front.data(), payload_size) != 0 ||
	    (op.DataSize() != 0 &&
	     std::memcmp(reference.bytes.data() + payload_size, op.data, op.DataSize()) != 0)) {
		Fail(false, "different payload", &op, &reference);
	}
}

void Verifier::After(const OpView& op, const Result& result) {
	if (!m_following || !IsLockstep(op.Kind()) || result.suspended) {
		return;
	}
	m_answer = Answer {op.Kind(), result.value};
	GetVerifyTotals().lockstep_answers.fetch_add(1, std::memory_order_relaxed);
}

void Verifier::Finish() {
	if (!m_following) {
		return;
	}
	const auto step = Advance();
	if (step == Step::Op) {
		const auto reference = m_captured.front();
		Fail(false, "the reference front has an op after the front's stream ended", nullptr,
		     &reference);
		return;
	}
	if (step == Step::Suspended) {
		Fail(false, "the reference front stopped at a wait at the stream's end", nullptr,
		     nullptr);
		return;
	}
	Detach();
}

Result Verifier::Capture(OpKind kind, const void* payload, uint32_t payload_size,
                         const void* data, uint32_t data_size, uint64_t packet,
                         uint64_t packets_hash) {
	if (IsLockstep(kind) && m_answer.has_value()) {
		// The packet parsed again after the resolver executed its op.
		EXIT_IF(m_answer->kind != kind);
		const Result result {false, m_answer->value};
		m_answer.reset();
		return result;
	}
	CapturedOp captured;
	captured.kind         = kind;
	captured.packet       = packet;
	captured.packets_hash = packets_hash;
	captured.payload_size = payload_size;
	captured.data_size    = data_size;
	captured.bytes.resize(static_cast<size_t>(payload_size) + data_size);
	std::memcpy(captured.bytes.data(), payload, payload_size);
	if (data_size != 0) {
		std::memcpy(captured.bytes.data() + payload_size, data, data_size);
	}
	m_captured.push_back(std::move(captured));
	// A lockstep op has no result yet: its packet suspends and is parsed again once the resolver
	// has executed the front's op.
	return Result {IsLockstep(kind), 0};
}

} // namespace Libs::Graphics::CpSeq
