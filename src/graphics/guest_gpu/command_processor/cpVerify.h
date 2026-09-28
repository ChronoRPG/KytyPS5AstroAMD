#ifndef EMULATOR_SRC_GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_CPVERIFY_H_
#define EMULATOR_SRC_GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_CPVERIFY_H_

// KYTY_CP_SEQ_VERIFY (cpOps.h): the reference front of one command processor.
//
// Protocol, per op k the resolver is about to execute (Before):
//   1. The reference front parses packets of its own copy of the stream (the same command
//      buffers, its own cursor and front state) until it captures an op. It reads guest memory
//      only where the serial command processor would (register pairs: ReadGuestForCp).
//   2. Its op must equal op k: kind, packet position, packets hash, payload and inline data.
//   3. A lockstep op suspends the reference's packet when captured (it has no result yet). After
//      the resolver executed op k (After), a result that completed the op becomes the answer the
//      reference's packet takes when it is parsed again; a suspended result leaves the reference
//      suspended at that packet, which it parses again when the resolver's next op arrives.
// A difference is counted, logged and ends the comparison of that stream (the reference is out
// of step); the next stream is followed again.

#include "graphics/guest_gpu/command_processor/commandProcessor.h"
#include "graphics/guest_gpu/command_processor/cpOps.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Libs::Graphics::CpSeq {

class Verifier {
public:
	Verifier(RenderContext& renderer, int interrupt_event_id);
	~Verifier();
	KYTY_CLASS_NO_COPY(Verifier);

	// ---- Primary side (the resolver of the followed processor) ----

	// A new stream of the primary: the reference takes the primary's front state and parses the
	// same commands from their start.
	void Attach(const CommandProcessor& primary, uint64_t stream_id,
	            std::span<const uint32_t> commands);
	void Detach() noexcept;
	[[nodiscard]] bool Follows(uint64_t stream_id) const noexcept {
		return m_following && stream_id != 0 && stream_id == m_stream_id;
	}
	// Before the resolver executes `op` of the followed stream.
	void Before(const OpView& op);
	// After it: the result of a lockstep op becomes the reference's answer.
	void After(const OpView& op, const Result& result);
	// The followed stream completed on the primary: the reference must end there too.
	void Finish();
	// External front-state changes between slices (GuestGpu sets the CE completion per round).
	void MirrorCeComplete(bool complete);

	// ---- Reference side (the reference front's Submit) ----
	[[nodiscard]] Result Capture(OpKind kind, const void* payload, uint32_t payload_size,
	                             const void* data, uint32_t data_size, uint64_t packet,
	                             uint64_t packets_hash);

private:
	struct CapturedOp {
		OpKind               kind         = OpKind::Count;
		uint64_t             packet       = 0;
		uint64_t             packets_hash = 0;
		uint32_t             payload_size = 0;
		uint32_t             data_size    = 0;
		std::vector<uint8_t> bytes; // payload, then inline data
	};
	struct Answer {
		OpKind   kind  = OpKind::Count;
		uint64_t value = 0;
	};
	enum class Step { Op, End, Suspended };

	// Parses the reference until it captured an op, its stream ended, or it stopped suspended
	// without an op.
	[[nodiscard]] Step Advance();
	void Fail(bool read_divergence, const char* what, const OpView* op,
	          const CapturedOp* reference);

	std::unique_ptr<CommandProcessor> m_reference;
	std::unique_ptr<Pm4Execution>     m_execution;
	std::deque<CapturedOp>            m_captured;
	std::optional<Answer>             m_answer;
	uint64_t                          m_stream_id = 0;
	bool                              m_following = false;
};

} // namespace Libs::Graphics::CpSeq

#endif // EMULATOR_SRC_GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_CPVERIFY_H_
