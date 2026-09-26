#ifndef EMULATOR_INCLUDE_EMULATOR_LIBS_AMPR_COUNTER_BANK_H_
#define EMULATOR_INCLUDE_EMULATOR_LIBS_AMPR_COUNTER_BANK_H_

// Pure (lock-free, dependency-free) model of the AMPR counter bank and of the
// operand encodings taken by sceAmprCommandBufferWriteCounter*,
// sceAmprCommandBufferWaitOnCounter, sceAmprCommandBufferWaitOnAddress and
// sceAmprCommandBufferWriteAddressFromCounter*. libAmpr.cpp owns the process-wide
// instance and its synchronization; this header only decodes and evaluates.
//
// Layout: 256 little-endian 32-bit counters. An access code selects a lane of a
// counter: the whole 32-bit counter, one of its two 16-bit halves, one of its four
// bytes, or the 64-bit pair formed by counter N (low word) and counter N+1 (high
// word). Encodings outside these tables are reported as unsupported by the caller.

#include <cstdint>
#include <cstring>

namespace Libs::LibAmpr::CounterBank {

constexpr uint32_t NUM_COUNTERS  = 256;
constexpr uint32_t COUNTER_BYTES = 4;

// Access codes.
constexpr uint8_t ACCESS_PAIR_64       = 0;
constexpr uint8_t ACCESS_32            = 1;
constexpr uint8_t ACCESS_16_OFFSET_0   = 2;
constexpr uint8_t ACCESS_16_OFFSET_2   = 3;
constexpr uint8_t ACCESS_8_OFFSET_0    = 4;
constexpr uint8_t ACCESS_8_OFFSET_3    = 7;
constexpr uint8_t ACCESS_MAX           = ACCESS_8_OFFSET_3;

// Write operations.
constexpr uint8_t WRITE_STORE           = 0;
constexpr uint8_t WRITE_OR              = 1;
constexpr uint8_t WRITE_AND_COMPLEMENT  = 2;
constexpr uint8_t WRITE_XOR             = 3;
constexpr uint8_t WRITE_ADD             = 4;
constexpr uint8_t WRITE_OP_MAX          = WRITE_ADD;

// Wait compare operations: "observed <op> reference".
constexpr uint8_t COMPARE_EQUAL                     = 0;
constexpr uint8_t COMPARE_GREATER_UNSIGNED          = 1;
constexpr uint8_t COMPARE_LESS_UNSIGNED             = 2;
constexpr uint8_t COMPARE_NOT_EQUAL                 = 3;
constexpr uint8_t COMPARE_GREATER_EQUAL_WRAPPED     = 4;
constexpr uint8_t COMPARE_GREATER_SIGNED            = 5;
constexpr uint8_t COMPARE_LESS_SIGNED               = 6;
constexpr uint8_t COMPARE_MAX                       = COMPARE_LESS_SIGNED;

// Wait-on-counter mask operations.
constexpr uint8_t MASK_DISABLED = 0;
constexpr uint8_t MASK_AND      = 1;
constexpr uint8_t MASK_OP_MAX   = MASK_AND;

struct Lane {
	uint32_t byte_offset = 0;
	uint32_t bytes       = 0;
};

constexpr bool DecodeLane(uint8_t index, uint8_t access, Lane* out) {
	const uint32_t base = static_cast<uint32_t>(index) * COUNTER_BYTES;
	Lane           lane {};
	if (access == ACCESS_PAIR_64) {
		if (static_cast<uint32_t>(index) + 1u >= NUM_COUNTERS) {
			return false;
		}
		lane = {base, 8};
	} else if (access == ACCESS_32) {
		lane = {base, 4};
	} else if (access >= ACCESS_16_OFFSET_0 && access <= ACCESS_16_OFFSET_2) {
		lane = {base + (access - ACCESS_16_OFFSET_0) * 2u, 2};
	} else if (access >= ACCESS_8_OFFSET_0 && access <= ACCESS_8_OFFSET_3) {
		lane = {base + (access - ACCESS_8_OFFSET_0), 1};
	} else {
		return false;
	}
	if (out != nullptr) {
		*out = lane;
	}
	return true;
}

constexpr uint64_t WidthMask(uint32_t bytes) {
	return bytes >= 8 ? ~uint64_t {0} : ((uint64_t {1} << (bytes * 8u)) - 1u);
}

constexpr int64_t SignExtend(uint64_t value, uint32_t bytes) {
	if (bytes >= 8) {
		return static_cast<int64_t>(value);
	}
	const uint32_t shift = 64u - bytes * 8u;
	return static_cast<int64_t>(value << shift) >> shift;
}

constexpr bool IsValidWriteOp(uint8_t op) {
	return op <= WRITE_OP_MAX;
}

constexpr bool IsValidCompare(uint8_t compare) {
	return compare <= COMPARE_MAX;
}

constexpr bool IsValidMaskOp(uint8_t mask_op) {
	return mask_op <= MASK_OP_MAX;
}

// Result of applying a write operation to a lane holding `current`; truncated to
// the lane width (ADD wraps within the lane, it does not carry into neighbours).
constexpr uint64_t ApplyWrite(uint64_t current, uint64_t operand, uint8_t op, uint32_t bytes) {
	uint64_t next = operand;
	switch (op) {
		case WRITE_STORE: next = operand; break;
		case WRITE_OR: next = current | operand; break;
		case WRITE_AND_COMPLEMENT: next = current & ~operand; break;
		case WRITE_XOR: next = current ^ operand; break;
		case WRITE_ADD: next = current + operand; break;
		default: next = current; break;
	}
	return next & WidthMask(bytes);
}

// Evaluates "observed <compare> reference" at the given lane width. Both operands
// are truncated to the width first; the signed forms sign-extend from it and the
// wrapped form treats the difference as a signed distance (sequence-number style,
// so a counter that wrapped past the reference still counts as reached).
constexpr bool CompareSatisfied(uint64_t observed, uint64_t reference, uint8_t compare,
                                uint32_t bytes) {
	const uint64_t mask = WidthMask(bytes);
	observed &= mask;
	reference &= mask;
	switch (compare) {
		case COMPARE_EQUAL: return observed == reference;
		case COMPARE_GREATER_UNSIGNED: return observed > reference;
		case COMPARE_LESS_UNSIGNED: return observed < reference;
		case COMPARE_NOT_EQUAL: return observed != reference;
		case COMPARE_GREATER_EQUAL_WRAPPED:
			return SignExtend((observed - reference) & mask, bytes) >= 0;
		case COMPARE_GREATER_SIGNED:
			return SignExtend(observed, bytes) > SignExtend(reference, bytes);
		case COMPARE_LESS_SIGNED: return SignExtend(observed, bytes) < SignExtend(reference, bytes);
		default: return false;
	}
}

// Raw storage. Not synchronized: the owner serializes access.
class Bank {
public:
	uint64_t Read(const Lane& lane) const {
		uint64_t value = 0;
		std::memcpy(&value, m_bytes + lane.byte_offset, lane.bytes);
		return value;
	}
	void Write(const Lane& lane, uint64_t value) {
		std::memcpy(m_bytes + lane.byte_offset, &value, lane.bytes);
	}
	// Applies `op` and returns the new lane value.
	uint64_t Apply(const Lane& lane, uint64_t operand, uint8_t op) {
		const uint64_t next = ApplyWrite(Read(lane), operand, op, lane.bytes);
		Write(lane, next);
		return next;
	}
	void Clear() { std::memset(m_bytes, 0, sizeof(m_bytes)); }

private:
	alignas(8) uint8_t m_bytes[NUM_COUNTERS * COUNTER_BYTES] = {};
};

} // namespace Libs::LibAmpr::CounterBank

#endif /* EMULATOR_INCLUDE_EMULATOR_LIBS_AMPR_COUNTER_BANK_H_ */
