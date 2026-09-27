#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_PACKETCLASS_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_PACKETCLASS_H_

#include "graphics/guest_gpu/pm4.h"

#include <cstdint>

namespace Libs::Graphics::DrawPrep {

// PM4 packet classes for the draw-preparation window (S0 histogram, S6 fences).
enum class PacketClass : uint8_t {
	WindowSafe, // only CP register/state writes, markers, control flow: no drain needed
	Draw,       // direct draw
	Fence,      // anything else: commit the window first
};

// `header` without the predication bit; `body` points after the header; `remaining_dw` counts
// the dwords from the header to the end of the command buffer. Conservative: every packet that
// is not known to only touch command-processor state is a fence.
//
// Window-safe packets and why (pm4Handlers.cpp):
// - SET_CONTEXT_REG / SET_SH_REG / SET_UCONFIG_REG(_INDEX): register parsers only decode into
//   the register files (and the index type / user-data marker CP state). The *_REG_INDIRECT
//   forms read guest memory and are fences.
// - INDEX_TYPE, INDEX_BASE, INDEX_BUFFER_SIZE, NUM_INSTANCES, SET_BASE: CP draw state, captured
//   into a draw's arguments when its packet is parsed.
// - CLEAR_STATE and NOP/R_CONTEXT_STATE: register-file copies (the latter also counts a DE
//   event, which the constant engine only observes after this command stream yields, and the
//   window is committed whenever a stream yields).
// - PFP_SYNC_ME: no-op. Plain NOPs, push/pop markers, user-data markers (0x4, 0xd).
// - INDIRECT_BUFFER call/chain (4 dwords): moves the fetcher. The 14-dword form is a
//   conditional branch on guest memory: a fence.
// Draws: DRAW_INDEX_2, DRAW_INDEX_OFFSET_2, DRAW_INDEX_AUTO, DISPATCH_DRAW_PREAMBLE (handled as
// DRAW_INDEX_2). Indirect draws read GPU-written arguments and are fences.
[[nodiscard]] inline PacketClass ClassifyPacket(uint32_t header, const uint32_t* body,
                                                uint32_t remaining_dw) {
	const auto opcode = (header >> 8u) & 0xffu;
	switch (opcode) {
		case Pm4::IT_SET_CONTEXT_REG:
		case Pm4::IT_SET_SH_REG:
		case Pm4::IT_SET_UCONFIG_REG:
		case Pm4::IT_SET_UCONFIG_REG_INDEX:
		case Pm4::IT_INDEX_TYPE:
		case Pm4::IT_INDEX_BASE:
		case Pm4::IT_INDEX_BUFFER_SIZE:
		case Pm4::IT_NUM_INSTANCES:
		case Pm4::IT_SET_BASE:
		case Pm4::IT_CLEAR_STATE:
		case Pm4::IT_PFP_SYNC_ME: return PacketClass::WindowSafe;
		case Pm4::IT_INDIRECT_BUFFER:
			return KYTY_PM4_LEN(header) == 4u ? PacketClass::WindowSafe : PacketClass::Fence;
		case Pm4::IT_DRAW_INDEX_2:
		case Pm4::IT_DRAW_INDEX_OFFSET_2:
		case Pm4::IT_DRAW_INDEX_AUTO:
		case Pm4::IT_DISPATCH_DRAW_PREAMBLE: return PacketClass::Draw;
		case Pm4::IT_NOP: {
			const auto r = KYTY_PM4_R(header);
			if (r == Pm4::R_ZERO) {
				if (remaining_dw >= 2 && (body[0] & 0xffff0000u) == 0x68750000u) {
					// Markers: 0 (none), 0x4 and 0xd (user-data markers) only set CP state;
					// the others are flips.
					const auto id = body[0] & 0xfffu;
					return id == 0x0u || id == 0x4u || id == 0xdu ? PacketClass::WindowSafe
					                                               : PacketClass::Fence;
				}
				return PacketClass::WindowSafe;
			}
			return r == Pm4::R_CONTEXT_STATE || r == Pm4::R_PUSH_MARKER || r == Pm4::R_POP_MARKER
			           ? PacketClass::WindowSafe
			           : PacketClass::Fence;
		}
		default: break;
	}
	return PacketClass::Fence;
}

} // namespace Libs::Graphics::DrawPrep

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_PACKETCLASS_H_
