#include "libs/amprCounterBank.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

namespace CB = Libs::LibAmpr::CounterBank;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "AmprCounterBankTests: failed: %s\n", text);
		std::abort();
	}
}

CB::Lane Lane(uint8_t index, uint8_t access) {
	CB::Lane lane {};
	Check(CB::DecodeLane(index, access, &lane), "lane decodes");
	return lane;
}

void TestDecodeLane() {
	CB::Lane lane {};
	Check(CB::DecodeLane(0, CB::ACCESS_32, &lane) && lane.byte_offset == 0 && lane.bytes == 4,
	      "32-bit lane of counter 0");
	Check(CB::DecodeLane(3, CB::ACCESS_16_OFFSET_2, &lane) && lane.byte_offset == 14 &&
	          lane.bytes == 2,
	      "upper 16-bit lane of counter 3");
	Check(CB::DecodeLane(2, CB::ACCESS_8_OFFSET_3, &lane) && lane.byte_offset == 11 &&
	          lane.bytes == 1,
	      "top byte lane of counter 2");
	Check(CB::DecodeLane(254, CB::ACCESS_PAIR_64, &lane) && lane.byte_offset == 1016 &&
	          lane.bytes == 8,
	      "pair of the last two counters");
	Check(!CB::DecodeLane(255, CB::ACCESS_PAIR_64, nullptr), "pair past the bank is rejected");
	Check(CB::DecodeLane(255, CB::ACCESS_32, nullptr), "last counter is addressable");
	Check(!CB::DecodeLane(0, CB::ACCESS_MAX + 1, nullptr), "unknown access code is rejected");
}

void TestLanesShareStorage() {
	CB::Bank bank;
	bank.Write(Lane(4, CB::ACCESS_PAIR_64), 0x1111222233334444ull);
	Check(bank.Read(Lane(4, CB::ACCESS_32)) == 0x33334444u, "pair low word is counter N");
	Check(bank.Read(Lane(5, CB::ACCESS_32)) == 0x11112222u, "pair high word is counter N+1");
	Check(bank.Read(Lane(4, CB::ACCESS_16_OFFSET_2)) == 0x3333u, "16-bit lane is little-endian");
	Check(bank.Read(Lane(5, CB::ACCESS_8_OFFSET_0 + 1)) == 0x22u, "byte lane is little-endian");

	bank.Write(Lane(4, CB::ACCESS_8_OFFSET_0), 0xabcdu);
	Check(bank.Read(Lane(4, CB::ACCESS_32)) == 0x333344cdu, "byte write touches one byte");
	Check(bank.Read(Lane(3, CB::ACCESS_32)) == 0 && bank.Read(Lane(6, CB::ACCESS_32)) == 0,
	      "neighbouring counters untouched");
}

void TestWriteOps() {
	CB::Bank bank;
	const auto c = Lane(10, CB::ACCESS_32);
	Check(bank.Apply(c, 0x1'0000'00f0ull, CB::WRITE_STORE) == 0xf0u, "store truncates to lane");
	Check(bank.Apply(c, 0x0f, CB::WRITE_OR) == 0xffu, "or");
	Check(bank.Apply(c, 0x3c, CB::WRITE_AND_COMPLEMENT) == 0xc3u, "and-complement clears bits");
	Check(bank.Apply(c, 0xff, CB::WRITE_XOR) == 0x3cu, "xor");
	Check(bank.Apply(c, 4, CB::WRITE_ADD) == 0x40u, "add");
	Check(bank.Apply(c, 0xffffffffu, CB::WRITE_ADD) == 0x3fu, "add wraps within 32 bits");
	Check(bank.Read(Lane(11, CB::ACCESS_32)) == 0, "add does not carry into next counter");

	const auto b = Lane(20, CB::ACCESS_8_OFFSET_0 + 2);
	bank.Write(Lane(20, CB::ACCESS_32), 0x11ff2233u);
	Check(bank.Apply(b, 1, CB::WRITE_ADD) == 0, "byte add wraps");
	Check(bank.Read(Lane(20, CB::ACCESS_32)) == 0x11002233u, "byte add leaves other bytes");
}

void TestCompare() {
	// Unsigned and equality, with operands truncated to the lane width.
	Check(CB::CompareSatisfied(5, 0x1'0005, CB::COMPARE_EQUAL, 2), "equal after truncation");
	Check(!CB::CompareSatisfied(5, 6, CB::COMPARE_EQUAL, 4), "not equal");
	Check(CB::CompareSatisfied(5, 6, CB::COMPARE_NOT_EQUAL, 4), "not-equal");
	Check(CB::CompareSatisfied(7, 6, CB::COMPARE_GREATER_UNSIGNED, 4), "unsigned greater");
	Check(!CB::CompareSatisfied(6, 6, CB::COMPARE_GREATER_UNSIGNED, 4), "greater is strict");
	Check(CB::CompareSatisfied(5, 6, CB::COMPARE_LESS_UNSIGNED, 4), "unsigned less");
	Check(CB::CompareSatisfied(0xffff, 1, CB::COMPARE_GREATER_UNSIGNED, 2),
	      "0xffff is large unsigned");

	// Signed forms sign-extend from the lane width.
	Check(CB::CompareSatisfied(0xffff, 1, CB::COMPARE_LESS_SIGNED, 2), "0xffff is -1 at 16 bits");
	Check(CB::CompareSatisfied(0xffff, 1, CB::COMPARE_GREATER_SIGNED, 4),
	      "0xffff is positive at 32 bits");
	Check(!CB::CompareSatisfied(0xffff, 1, CB::COMPARE_GREATER_SIGNED, 2),
	      "0xffff is negative at 16 bits");
	Check(CB::CompareSatisfied(0x80, 0x7f, CB::COMPARE_LESS_SIGNED, 1), "-128 < 127 at 8 bits");
	Check(CB::CompareSatisfied(~0ull, 0, CB::COMPARE_LESS_SIGNED, 8), "-1 < 0 at 64 bits");

	// Wrapped greater-or-equal: sequence-number distance.
	Check(CB::CompareSatisfied(10, 10, CB::COMPARE_GREATER_EQUAL_WRAPPED, 4), "wrapped: equal");
	Check(CB::CompareSatisfied(11, 10, CB::COMPARE_GREATER_EQUAL_WRAPPED, 4), "wrapped: ahead");
	Check(!CB::CompareSatisfied(9, 10, CB::COMPARE_GREATER_EQUAL_WRAPPED, 4), "wrapped: behind");
	Check(CB::CompareSatisfied(2, 0xfffffffeu, CB::COMPARE_GREATER_EQUAL_WRAPPED, 4),
	      "wrapped: counter wrapped past the reference");
	Check(!CB::CompareSatisfied(0xfffffffeu, 2, CB::COMPARE_GREATER_EQUAL_WRAPPED, 4),
	      "wrapped: reference wrapped, counter behind");
	Check(CB::CompareSatisfied(1, 0xff, CB::COMPARE_GREATER_EQUAL_WRAPPED, 1),
	      "wrapped at 8 bits");

	Check(!CB::CompareSatisfied(0, 0, CB::COMPARE_MAX + 1, 4), "unknown compare never passes");
	Check(CB::IsValidCompare(CB::COMPARE_MAX) && !CB::IsValidCompare(CB::COMPARE_MAX + 1),
	      "compare validation");
	Check(CB::IsValidWriteOp(CB::WRITE_OP_MAX) && !CB::IsValidWriteOp(CB::WRITE_OP_MAX + 1),
	      "write-op validation");
	Check(CB::IsValidMaskOp(CB::MASK_AND) && !CB::IsValidMaskOp(CB::MASK_OP_MAX + 1),
	      "mask-op validation");
}

static_assert(CB::WidthMask(1) == 0xffu && CB::WidthMask(8) == ~uint64_t {0});
static_assert(CB::SignExtend(0x8000, 2) == -32768 && CB::SignExtend(0x8000, 4) == 0x8000);
static_assert(CB::ApplyWrite(0xff, 1, CB::WRITE_ADD, 1) == 0);

} // namespace

int main() {
	TestDecodeLane();
	TestLanesShareStorage();
	TestWriteOps();
	TestCompare();
	std::printf("AmprCounterBankTests: passed\n");
	return 0;
}
