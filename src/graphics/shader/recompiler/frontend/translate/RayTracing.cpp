#include "graphics/shader/recompiler/frontend/translate/Translator.h"

#include <algorithm>
#include <array>

// IMAGE_BVH_INTERSECT_RAY / IMAGE_BVH64_INTERSECT_RAY (MIMG 0xe6/0xe7, RDNA2 ISA 8.2.10).

namespace Libs::Graphics::ShaderRecompiler::Frontend {

IR::U32 Translator::ReadBvhAddress(const Decoder::Instruction& inst, uint32_t index) {
	// NSA supplies every address VGPR after the first one individually (VADDR is address 0).
	const auto nsa_components =
	    std::min(inst.image_nsa_dwords * 4u, Decoder::MaxImageNsaAddressComponents);
	if (index != 0u && index - 1u < nsa_components) {
		return ir.GetVectorReg(static_cast<IR::VectorReg>(inst.image_nsa_addr[index - 1u]));
	}
	return ReadRawU32(OffsetOperand(PlainOperand(inst.src0), index));
}

void Translator::IMAGE_BVH_INTERSECT_RAY(const Decoder::Instruction& inst) {
	// KYTY_RT_STUB: the decoder only lets these instructions through when a BVH mode is on, and
	// the stub is the only one so far. Every lane gets the result of a node the ray misses. The
	// node type is the low three bits of node_pointer: 0-3 triangle, 4 box16, 5 box32, 6-7 user
	// (instance/procedural) nodes, which traversal handles in the shader.
	program.info.uses_bvh = true;
	const auto u        = [](uint32_t value) { return IR::U32(IR::Value(value)); };
	const auto type     = ir.BitwiseAnd(ReadBvhAddress(inst, 0), u(7u));
	const auto triangle = ir.ULessThan(type, u(4u));
	// Triangle miss: t_num=+inf over t_denom=1.0 gives t=+inf, beyond any ray extent; dword 3 is
	// a cleared hit_status in return mode 0 (dword 2 is triangle_id there, I_num/J_num in mode 1).
	constexpr std::array<uint32_t, 4> TriangleMiss {0x7f800000u, 0x3f800000u, 0u, 0u};
	// Box miss: every child slot holds the invalid node pointer.
	constexpr uint32_t InvalidNode = 0xffffffffu;
	for (uint32_t dword = 0; dword < 4u; dword++) {
		WriteOperand(OffsetOperand(inst.dst, dword),
		             ir.Select(triangle, u(TriangleMiss[dword]), u(InvalidNode)));
	}
	return;
}

} // namespace Libs::Graphics::ShaderRecompiler::Frontend
