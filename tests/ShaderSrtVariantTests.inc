// KYTY_SRT_VARIANT_READS GPU tests (included by ShaderRecompilerComputeTests.cpp after
// ShaderCodegenTests.inc).
//
// Astro Bot's tiled deferred-lighting kernels walk a BVH: they load an instance record through a
// loop-carried pointer (S_LOAD) and read the record's V# with S_BUFFER_LOAD. Without the switch
// such a record read is a flat SRT slot that cannot be evaluated before the dispatch (the dispatch
// is dropped), and an S_BUFFER_LOAD through a runtime V# fails resource tracking. With it, both
// are read in the shader through BDA, with the RDNA2 bounds of a bound scalar buffer.

namespace SrtVariantTests {

using CodegenTests::ScopedCodegenOptions;
using ShaderRecompiler::GetCodegenOptions;

// S_BUFFER_LOAD_DWORDX4 through V#s the shader selects at runtime. RDNA2 ISA 7.2.1: only base,
// stride and num_records are used, addr = (base + OFFSET + SOFFSET) & ~3. The bound matches a bound
// scalar buffer (stride 0: num_records bytes, else stride * num_records), and a dword reads 0
// unless all four of its bytes are below it.
TestCase ScalarBufferLoadsGpuSelectedDescriptors() {
  using O = ShaderOpcode;
  constexpr uint64_t GuestBase = 0x0000000118000000ull;
  struct DescriptorCase {
    u32 stride, records, offset, soffset;
  };
  const DescriptorCase cases[] = {
      {0, 16, 8, 0},  // dwords 2-3 in, 4-5 past the 16-byte bound
      {0, 24, 8, 0},  // all four in
      {12, 2, 12, 0}, // stride * records = 24 bytes: dwords 3-5 in, 6 out
      {0, 0, 0, 0},   // empty
      {0, 20, 4, 8},  // SOFFSET is added: dwords 3-4 in, 5-6 out
      {0, 32, 10, 0}, // the two LSBs of the offset are ignored: reads from byte 8
      {4, 5, 8, 0},   // 20 bytes: dwords 2-4 in, 5 out
      {0, 15, 8, 0},  // 15 bytes: dword 2 in, dword 3 (bytes 12-15) out
  };
  constexpr u32 Count = static_cast<u32>(std::size(cases));
  TestCase test;
  test.name = "ScalarBufferLoadsGpuSelectedDescriptors";
  test.initial.resize(2048);
  for (u32 i = 0; i < Count; ++i) {
    const u32 data_offset = 4096 + i * 128;
    const uint64_t base = GuestBase + data_offset;
    const std::array<u32, 4> descriptor{static_cast<u32>(base),
                                        (cases[i].stride << 16u) | static_cast<u32>(base >> 32u),
                                        cases[i].records, 0x5204u};
    std::copy(descriptor.begin(), descriptor.end(), test.initial.begin() + 130 + i * 30);
    for (u32 word = 0; word < 32; ++word) {
      test.initial[data_offset / 4 + word] = i * 100 + word + 1;
    }
  }
  for (u32 i = 0; i < Count; ++i) {
    // Select the table entry through a GPU load and readfirstlane, not host constants.
    const u32 selected = Count - 1 - i;
    const auto &input = cases[selected];
    test.initial[64 + i] = selected;
    AppendVMovU32(&test.code, 30, (64 + i) * 4);
    AppendBufferLoadDword(&test.code, 0, 30);
    test.code.push_back(EncodeVop1(0x02, 20, Vgpr(0))); // v_readfirstlane_b32 s20, v0
    test.code.push_back(EncodeSop2(0x26, 20, 20, 255)); // s_mul_i32 s20, s20, 120
    test.code.push_back(120);
    test.code.push_back(EncodeSmem0(0x0a, 8, 0)); // s_buffer_load_dwordx4 s[8:11], s[0:3]
    test.code.push_back(EncodeSmem1(520, 20));
    AppendSMovLiteral(&test.code, 22, input.soffset);
    test.code.push_back(EncodeSmem0(0x0a, 12, 4)); // s_buffer_load_dwordx4 s[12:15], s[8:11]
    test.code.push_back(EncodeSmem1(input.offset, 22));
    const u32 size = input.stride == 0 ? input.records : input.stride * input.records;
    const u32 first = (input.offset + input.soffset) & ~3u;
    for (u32 component = 0; component < 4; ++component) {
      AppendStoreSgpr(&test.code, 12 + component, i * 4 + component);
      const u32 byte = first + component * 4;
      test.expected.push_back(byte + 4 <= size ? selected * 100 + byte / 4 + 1 : 0);
    }
  }
  AppendEnd(&test.code);
  test.bda_mappings = {{GuestBase, 0}};
  test.opcodes = {O::V_MOV_B32, O::S_MOV_B32, O::BUFFER_LOAD_DWORD, O::V_READFIRSTLANE_B32,
                  O::S_MUL_I32, O::S_BUFFER_LOAD_DWORDX4, O::BUFFER_STORE_DWORD, O::S_ENDPGM};
  test.required_spirv = {"OpConvertUToPtr", "PhysicalStorageBuffer"};
  return test;
}

// The BVH traversal shape: a loop follows `next` pointers through records (s_load_dwordx2 through
// the loop-carried pointer) and reads each record's V# (s_load_dwordx4 at +16) to
// s_buffer_load_dwordx4 its data. Record 0: 16-byte buffer, record 1: stride 4 x 6 records,
// record 2: empty; the walk ends at a null `next`.
TestCase ScalarLoadsThroughLoopCarriedRecords() {
  using O = ShaderOpcode;
  constexpr uint64_t GuestBase = 0x0000000120000000ull;
  constexpr u32 Records = 3;
  constexpr u32 RecordBytes = 4096;
  constexpr u32 DataBytes = 6144;
  struct Buffer {
    u32 stride, records;
  };
  const Buffer buffers[Records] = {{0, 16}, {4, 6}, {0, 0}};
  TestCase test;
  test.name = "ScalarLoadsThroughLoopCarriedRecords";
  test.initial.resize(2048);
  for (u32 n = 0; n < Records; ++n) {
    const u32 record = (RecordBytes + n * 64) / 4;
    const uint64_t next = n + 1 < Records ? GuestBase + RecordBytes + (n + 1) * 64 : 0;
    const uint64_t data = GuestBase + DataBytes + n * 256;
    test.initial[record + 0] = static_cast<u32>(next);
    test.initial[record + 1] = static_cast<u32>(next >> 32u);
    test.initial[record + 4] = static_cast<u32>(data);
    test.initial[record + 5] = (buffers[n].stride << 16u) | static_cast<u32>(data >> 32u);
    test.initial[record + 6] = buffers[n].records;
    test.initial[record + 7] = 0x5204u;
    for (u32 word = 0; word < 64; ++word) {
      test.initial[(DataBytes + n * 256) / 4 + word] = 1000 + n * 100 + word;
    }
  }
  auto &code = test.code;
  const uint64_t root = GuestBase + RecordBytes;
  AppendSMovLiteral(&code, 20, static_cast<u32>(root));
  AppendSMovLiteral(&code, 21, static_cast<u32>(root >> 32u));
  AppendSMovLiteral(&code, 30, 0); // output byte offset
  const auto loop = code.size();
  code.push_back(EncodeSopc(0x13, 20, InlineU32(0))); // s_cmp_lg_u64 s[20:21], 0
  const auto exit_branch = code.size();
  code.push_back(EncodeSopp(0x04, 0)); // s_cbranch_scc0 done (patched below)
  code.push_back(EncodeSmem0(0x02, 24, 10)); // s_load_dwordx4 s[24:27], s[20:21], 16
  code.push_back(EncodeSmem1(16, 125));
  code.push_back(EncodeSmem0(0x01, 22, 10)); // s_load_dwordx2 s[22:23], s[20:21], 0
  code.push_back(EncodeSmem1(0, 125));
  code.push_back(EncodeSopp(0x0c, 0xc07f)); // s_waitcnt lgkmcnt(0)
  code.push_back(EncodeSmem0(0x0a, 36, 12)); // s_buffer_load_dwordx4 s[36:39], s[24:27], 8
  code.push_back(EncodeSmem1(8, 125));
  code.push_back(EncodeSopp(0x0c, 0xc07f));
  code.push_back(EncodeVop1(0x01, 31, 30)); // v_mov_b32 v31, s30
  for (u32 component = 0; component < 4; ++component) {
    code.push_back(EncodeVop1(0x01, 1, 36 + component)); // v_mov_b32 v1, s[36 + component]
    code.push_back(EncodeVop2(0x25, 32, InlineU32(component * 4), 31)); // v32 = v31 + 4k
    AppendBufferStoreDword(&code, 1, 32);
  }
  code.push_back(EncodeSop2(0x00, 30, 30, InlineU32(16))); // s_add_u32 s30, s30, 16
  code.push_back(EncodeSop1(0x04, 20, 22)); // s_mov_b64 s[20:21], s[22:23]
  const auto back_branch = code.size();
  code.push_back(EncodeSopp(0x02, static_cast<u32>(static_cast<int32_t>(loop) -
                                                   static_cast<int32_t>(back_branch + 1)) &
                                      0xffffu)); // s_branch loop
  code[exit_branch] = EncodeSopp(0x04, static_cast<u32>(code.size() - (exit_branch + 1)));
  AppendEnd(&code);
  for (u32 n = 0; n < Records; ++n) {
    const u32 size = buffers[n].stride == 0 ? buffers[n].records : buffers[n].stride * buffers[n].records;
    for (u32 component = 0; component < 4; ++component) {
      const u32 byte = 8 + component * 4;
      test.expected.push_back(byte + 4 <= size ? 1000 + n * 100 + byte / 4 : 0);
    }
  }
  test.bda_mappings = {{GuestBase, 0}};
  test.opcodes = {O::S_MOV_B32, O::S_CMP_LG_U64, O::S_CBRANCH_SCC0, O::S_LOAD_DWORDX4,
                  O::S_LOAD_DWORDX2, O::S_WAITCNT, O::S_BUFFER_LOAD_DWORDX4, O::V_MOV_B32,
                  O::V_ADD_NC_U32, O::BUFFER_STORE_DWORD, O::S_ADD_U32, O::S_MOV_B64,
                  O::S_BRANCH, O::S_ENDPGM};
  test.required_spirv = {"OpConvertUToPtr", "PhysicalStorageBuffer"};
  return test;
}

void RunAll(VulkanHarness *vulkan) {
  auto options = GetCodegenOptions();
  options.srt_variant_reads = true;
  ScopedCodegenOptions scope(options);
  RunCase(vulkan, ScalarBufferLoadsGpuSelectedDescriptors());
  RunCase(vulkan, ScalarLoadsThroughLoopCarriedRecords());
}

} // namespace SrtVariantTests
