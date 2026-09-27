// IMAGE_BVH_INTERSECT_RAY / IMAGE_BVH64_INTERSECT_RAY (MIMG 0xe6/0xe7) GPU tests (included by
// ShaderRecompilerComputeTests.cpp after ShaderCodegenTests.inc).

namespace BvhTests {

using CodegenTests::ScopedCodegenOptions;
using ShaderRecompiler::GetCodegenOptions;

// RDNA2 ISA 8.2.10: DMASK=0xf, UNRM=1, R128=1, DIM=0 (1D), no sampler.
constexpr u32 EncodeBvh0(bool bvh64, u32 nsa_dwords = 0) {
  const u32 opcode = bvh64 ? 0xe7u : 0xe6u;
  return (0x3cu << 26u) | ((opcode >> 7u) & 0x1u) | ((nsa_dwords & 0x3u) << 1u) |
         (0xfu << 8u) | (1u << 12u) | (1u << 15u) | ((opcode & 0x7fu) << 18u);
}

constexpr u32 EncodeBvh1(u32 vdata, u32 vaddr, u32 srsrc_sgpr, bool a16) {
  return (((srsrc_sgpr / 4u) & 0x1fu) << 16u) | ((vdata & 0xffu) << 8u) | (vaddr & 0xffu) |
         (a16 ? (1u << 30u) : 0u);
}

constexpr u32 BvhAddressDwords(bool bvh64, bool a16) {
  return (bvh64 ? 2u : 1u) + 4u + (a16 ? 3u : 6u);
}

// Appends the instruction with its result in v[vdata:vdata+3]. Address dword 0 is VADDR; with
// NSA, dword i >= 1 comes from nsa_regs[i - 1].
void AppendBvh(std::vector<u32> *code, bool bvh64, bool a16, u32 vdata, u32 vaddr,
               u32 srsrc_sgpr, const std::vector<u32> &nsa_regs = {}) {
  const u32 nsa_dwords = static_cast<u32>((nsa_regs.size() + 3u) / 4u);
  code->push_back(EncodeBvh0(bvh64, nsa_dwords));
  code->push_back(EncodeBvh1(vdata, vaddr, srsrc_sgpr, a16));
  for (u32 dword = 0; dword < nsa_dwords; ++dword) {
    u32 packed = 0;
    for (u32 byte = 0; byte < 4u; ++byte) {
      const u32 index = dword * 4u + byte;
      if (index < nsa_regs.size()) packed |= (nsa_regs[index] & 0xffu) << (byte * 8u);
    }
    code->push_back(packed);
  }
}

constexpr u32 InvalidNode = 0xffffffffu;
constexpr std::array<u32, 4> TriangleMiss{0x7f800000u, 0x3f800000u, 0u, 0u};

// KYTY_RT_STUB: every lane misses. Lane l tests node pointer l (type l & 7), so one wave covers
// triangle (0-3), box16 (4), box32 (5) and user (6-7) nodes; the stored dwords are the stub's
// "no intersection" results in RDNA2 return layout.
TestCase StubReturnsMiss(u32 wave_size, bool bvh64, bool a16, bool nsa) {
  using O = ShaderOpcode;
  TestCase test;
  test.name = "BvhStubReturnsMiss";
  std::vector<u32> &code = test.code;
  const u32 dwords = BvhAddressDwords(bvh64, a16);
  constexpr u32 AddrBase = 40;
  constexpr u32 NsaBase = 60;
  constexpr u32 Result = 16;
  // T# in s[32:35]: base 0x100 (x256), size 0xffff nodes, triangle return mode 1, type 8.
  AppendSMovLiteral(&code, 32, 0x100u);
  AppendSMovLiteral(&code, 33, 0x80000000u);
  AppendSMovLiteral(&code, 34, 0xffffu);
  AppendSMovLiteral(&code, 35, 0x81000000u);
  std::vector<u32> nsa_regs;
  for (u32 i = 0; i < dwords; ++i) {
    // Address dword 0 is the node pointer: the lane index. The rest carry distinct values.
    const u32 reg = i == 0 || !nsa ? AddrBase + i : NsaBase + (dwords - 1u - i);
    if (i == 0) {
      code.push_back(EncodeVop1(0x01, reg, Vgpr(0)));
    } else {
      AppendVMovLiteral(&code, reg, 0x3f800000u + i);
      if (nsa) nsa_regs.push_back(reg);
    }
  }
  AppendBvh(&code, bvh64, a16, Result, AddrBase, 32, nsa_regs);
  for (u32 dword = 0; dword < 4u; ++dword) {
    AppendStoreVgprAtLaneDwordOffset(&code, Result + dword, 0, dword * wave_size);
  }
  AppendEnd(&code);
  for (u32 dword = 0; dword < 4u; ++dword) {
    for (u32 lane = 0; lane < wave_size; ++lane) {
      test.expected.push_back((lane & 7u) < 4u ? TriangleMiss[dword] : InvalidNode);
    }
  }
  test.initial.resize(test.expected.size());
  test.opcodes = {O::S_MOV_B32, O::V_MOV_B32,
                  bvh64 ? O::IMAGE_BVH64_INTERSECT_RAY : O::IMAGE_BVH_INTERSECT_RAY,
                  O::V_LSHLREV_B32, O::V_ADD_NC_U32, O::BUFFER_STORE_DWORD, O::S_ENDPGM};
  test.compute_info.threads_num[0] = wave_size;
  test.compute_info.threads_num[1] = test.compute_info.threads_num[2] = 1;
  test.compute_info.thread_ids_num = 1;
  test.compute_info.wave_size = wave_size;
  test.has_compute_info = true;
  return test;
}

void RunStub(VulkanHarness *vulkan) {
  auto options = GetCodegenOptions();
  options.rt_stub = true;
  const ScopedCodegenOptions scoped(options);
  for (const u32 wave_size : {32u, 64u}) {
    for (const bool bvh64 : {false, true}) {
      for (const bool a16 : {false, true}) {
        for (const bool nsa : {false, true}) {
          const auto test = StubReturnsMiss(wave_size, bvh64, a16, nsa);
          const auto compiled = CompileCase(test, vulkan->SubgroupSize());
          Require(test.name, "program info", compiled.program.info.uses_bvh,
                  "stubbed BVH program was not marked as using BVH instructions");
          RunCase(vulkan, test);
        }
      }
    }
  }
}

void RunAll(VulkanHarness *vulkan) { RunStub(vulkan); }

} // namespace BvhTests
