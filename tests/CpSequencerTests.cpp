// Unit tests for the P3 op stream (graphics/guest_gpu/command_processor/cpOps.h), without a
// Vulkan device:
//  - every op kind round-trips through OpStream: header (kind, sequence, packet position, packets
//    hash, inline data size, verify hash) and payload and inline data bytes;
//  - wrap-around with random record sizes up to the largest record, single-threaded;
//  - a producer and a consumer thread with ring back-pressure;
//  - the op table (payload sizes, lockstep kinds, names).

#include "graphics/guest_gpu/command_processor/cpOps.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

// commandStream.cpp includes vulkan.hpp's dispatcher declarations; the tests never call Vulkan.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {

using namespace Libs::Graphics;
using namespace Libs::Graphics::CpSeq;

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (!condition) {
		std::printf("FAILED: %s\n", what);
		++g_failures;
	}
}

constexpr uint32_t KindCount = static_cast<uint32_t>(OpKind::Count);

// Inline data is carried by these kinds only (the front's WRITE_DATA dwords and constant RAM).
bool CarriesData(OpKind kind) {
	return kind == OpKind::WriteData || kind == OpKind::DumpConstRam;
}

struct Expected {
	OpKind               kind         = OpKind::Count;
	uint64_t             sequence     = 0;
	uint64_t             packet       = 0;
	uint64_t             packets_hash = 0;
	bool                 verify       = false;
	std::vector<uint8_t> payload;
	std::vector<uint8_t> data;
};

Expected MakeOp(std::mt19937_64& random, uint64_t sequence, uint32_t max_data) {
	Expected op;
	op.kind         = static_cast<OpKind>(random() % KindCount);
	op.sequence     = sequence;
	op.packet       = random();
	op.packets_hash = random();
	op.verify       = (random() & 1u) != 0;
	op.payload.resize(PayloadSize(op.kind));
	for (auto& byte: op.payload) {
		byte = static_cast<uint8_t>(random());
	}
	if (CarriesData(op.kind) && max_data != 0) {
		op.data.resize(random() % (max_data + 1u));
		for (auto& byte: op.data) {
			byte = static_cast<uint8_t>(random());
		}
	}
	return op;
}

void EmitOp(OpStream& stream, const Expected& op) {
	const auto sequence =
	    stream.Emit(op.kind, op.payload.data(), static_cast<uint32_t>(op.payload.size()),
	                op.data.empty() ? nullptr : op.data.data(),
	                static_cast<uint32_t>(op.data.size()), op.packet, op.packets_hash, op.verify);
	Check(sequence == op.sequence, "Emit returns the op's sequence number");
}

bool Matches(const OpView& view, const Expected& op) {
	const auto* header = view.header;
	bool        ok     = header->kind == op.kind && header->sequence == op.sequence &&
	          header->packet == op.packet && header->packets_hash == op.packets_hash &&
	          header->data_size == op.data.size() &&
	          ((header->flags & FlagVerify) != 0) == op.verify &&
	          header->ring.op == CommandStream::Op::Count &&
	          header->ring.size == RecordSize(static_cast<uint32_t>(op.payload.size()),
	                                          static_cast<uint32_t>(op.data.size()));
	ok = ok && std::memcmp(view.payload, op.payload.data(), op.payload.size()) == 0;
	ok = ok && (op.data.empty() || std::memcmp(view.data, op.data.data(), op.data.size()) == 0);
	if (op.verify) {
		ok = ok && header->verify_hash == HashOp(op.kind, view.payload,
		                                         static_cast<uint32_t>(op.payload.size()),
		                                         view.data,
		                                         static_cast<uint32_t>(op.data.size()));
	}
	return ok;
}

void TestTable() {
	for (uint32_t index = 0; index < KindCount; index++) {
		const auto kind = static_cast<OpKind>(index);
		Check(PayloadSize(kind) != 0 && PayloadSize(kind) % 8 == 0, "payload sizes");
		Check(std::strcmp(OpKindName(kind), "?") != 0, "op kind names");
	}
	Check(PayloadSize(OpKind::Count) == 0, "no payload for Count");
	Check(PayloadSize(OpKind::DrawIndirect) == sizeof(DrawIndirectOp) &&
	          PayloadSize(OpKind::DrawIndirectMulti) == sizeof(DrawIndirectOp) &&
	          PayloadSize(OpKind::WriteData) == sizeof(WriteDataOp) &&
	          PayloadSize(OpKind::ReadCheck) == sizeof(ReadCheckOp),
	      "payload size table");
	Check(IsLockstep(OpKind::WaitRegMem) && IsLockstep(OpKind::WaitFlipDone) &&
	          IsLockstep(OpKind::Predication) && IsLockstep(OpKind::CondExec) &&
	          IsLockstep(OpKind::Branch) && !IsLockstep(OpKind::DrawIndex) &&
	          !IsLockstep(OpKind::EndOfPipe) && !IsLockstep(OpKind::ReadCheck),
	      "lockstep kinds");
	Check(RecordSize(sizeof(WriteDataOp), 5) == sizeof(OpHeader) + sizeof(WriteDataOp) + 8,
	      "record sizes pad inline data to 8 bytes");
	// The hash covers the kind, the payload and the data.
	const WriteDataOp write {0x1000, 2, 0};
	const std::array<uint32_t, 2> dwords {1, 2};
	const auto base = HashOp(OpKind::WriteData, &write, sizeof(write), dwords.data(), 8);
	auto       other = dwords;
	other[1]         = 3;
	Check(base != HashOp(OpKind::WriteData, &write, sizeof(write), other.data(), 8) &&
	          base != HashOp(OpKind::WriteData, &write, sizeof(write), dwords.data(), 4) &&
	          base != HashOp(OpKind::ReferenceClock, &write, sizeof(write), dwords.data(), 8),
	      "op hash covers kind, payload and data");
}

// Every kind, emitted and consumed one by one (the inline protocol).
void TestRoundTrip() {
	OpStream        stream(512u << 10u);
	std::mt19937_64 random(1234);
	uint64_t        sequence = 0;
	for (uint32_t index = 0; index < KindCount * 64u; index++) {
		auto op = MakeOp(random, sequence++, 256);
		op.kind = static_cast<OpKind>(index % KindCount);
		op.payload.resize(PayloadSize(op.kind));
		if (!CarriesData(op.kind)) {
			op.data.clear();
		}
		EmitOp(stream, op);
		OpView view;
		Check(stream.Peek(view), "an emitted op is published");
		Check(Matches(view, op), "op round trip");
		stream.Pop();
		Check(!stream.Peek(view), "nothing after the consumed op");
	}
	// The largest WRITE_DATA (a 14-bit dword count) fits the default ring.
	Expected large;
	large.kind     = OpKind::WriteData;
	large.sequence = sequence++;
	large.verify   = true;
	large.payload.resize(sizeof(WriteDataOp));
	large.data.resize(16381u * 4u, 0x5a);
	Check(RecordSize(sizeof(WriteDataOp), 16381u * 4u) <= stream.MaxRecord(),
	      "the largest WRITE_DATA fits a record");
	EmitOp(stream, large);
	OpView view;
	Check(stream.Peek(view) && Matches(view, large), "largest op round trip");
	stream.Pop();
	Check(stream.Emitted() == sequence, "sequence numbers are consecutive");
}

// Several ops in flight, records of random size, many wraps of a small ring.
void TestWrap() {
	OpStream        stream(64u << 10u);
	std::mt19937_64 random(99);
	std::vector<Expected> pending;
	uint64_t              sequence = 0;
	uint64_t              consumed = 0;
	const uint32_t        max_data = stream.MaxRecord() - static_cast<uint32_t>(sizeof(OpHeader)) -
	                          static_cast<uint32_t>(sizeof(WriteDataOp)) - 8u;
	for (uint32_t round = 0; round < 4000; round++) {
		// At most 2 records of up to MaxRecord (a quarter of the ring) plus a wrap's padding
		// fit, so the single thread never waits for itself.
		const auto burst = 1u + static_cast<uint32_t>(random() % 2u);
		for (uint32_t i = 0; i < burst; i++) {
			auto op = MakeOp(random, sequence++, (random() % 8u) == 0 ? max_data : 512u);
			EmitOp(stream, op);
			pending.push_back(std::move(op));
		}
		while (!pending.empty()) {
			OpView view;
			if (!stream.Peek(view)) {
				Check(false, "a pending op is published");
				break;
			}
			Check(Matches(view, pending.front()), "op round trip across wraps");
			stream.Pop();
			pending.erase(pending.begin());
			consumed++;
		}
	}
	Check(consumed == sequence && stream.Bytes() > (64u << 10u) * 8u, "the ring wrapped");
}

// A producer thread against a consumer thread: back-pressure and publication order.
void TestThreaded() {
	OpStream          stream(64u << 10u);
	constexpr uint64_t Count = 20000;
	std::thread       producer([&] {
        std::mt19937_64 random(7);
        for (uint64_t index = 0; index < Count; index++) {
            EmitOp(stream, MakeOp(random, index, 2048));
        }
    });
	std::mt19937_64 random(7);
	uint64_t        received = 0;
	bool            ok       = true;
	while (received < Count) {
		OpView view;
		if (!stream.Peek(view)) {
			std::this_thread::yield();
			continue;
		}
		ok = ok && Matches(view, MakeOp(random, received, 2048));
		stream.Pop();
		received++;
	}
	producer.join();
	Check(ok, "threaded ops arrive intact and in order");
}

} // namespace

int main() {
	TestTable();
	TestRoundTrip();
	TestWrap();
	TestThreaded();
	if (g_failures != 0) {
		std::printf("cp sequencer tests: %d failure(s)\n", g_failures);
		return EXIT_FAILURE;
	}
	std::printf("cp sequencer tests passed\n");
	return EXIT_SUCCESS;
}
