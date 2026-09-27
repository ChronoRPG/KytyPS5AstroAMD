// Code-generation equivalence tests (included by ShaderRecompilerComputeTests.cpp).
//
// Each change to emitted SPIR-V that must keep results bit-exact is exercised here twice: once with
// the new code generation and once with its KYTY_* switch turned off, on the same inputs. The
// outputs must match each other (and, where one exists, an exact CPU reference).

namespace CodegenTests {

using ShaderRecompiler::CodegenOptions;
using ShaderRecompiler::GetCodegenOptions;
using ShaderRecompiler::SetCodegenOptions;

class ScopedCodegenOptions {
public:
  explicit ScopedCodegenOptions(const CodegenOptions &options)
      : m_saved(GetCodegenOptions()) {
    SetCodegenOptions(options);
  }
  ~ScopedCodegenOptions() { SetCodegenOptions(m_saved); }
  ScopedCodegenOptions(const ScopedCodegenOptions &) = delete;
  ScopedCodegenOptions &operator=(const ScopedCodegenOptions &) = delete;

private:
  CodegenOptions m_saved;
};

size_t CountSpirv(const std::vector<u32> &spirv, const std::string &needle) {
  spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_2);
  std::string text;
  Require("CodegenTests", "SPIR-V disassembly", tools.Disassemble(spirv, &text),
          "failed to disassemble emitted SPIR-V");
  return CountText(text, needle);
}

// Compiles and dispatches a case, returning the first `dwords` words of its buffer.
std::vector<u32> RunCollect(VulkanHarness *vulkan, const TestCase &test,
                            size_t dwords, std::vector<u32> *spirv = nullptr) {
  auto compiled = CompileCase(test, vulkan->SubgroupSize());
  if (spirv != nullptr) {
    *spirv = compiled.spirv;
  }
  const auto size = std::max<size_t>({test.initial.size(), dwords, 1u});
  auto buffer = vulkan->CreateStorageBuffer(test.name, test.initial, size);
  vulkan->Dispatch(test, compiled, buffer, nullptr, nullptr, nullptr, nullptr,
                   nullptr);
  auto actual = vulkan->ReadBuffer(test.name, buffer, dwords);
  vulkan->DestroyBuffer(&buffer);
  return actual;
}

void RequireEqualWords(const char *name, const char *stage,
                       const std::vector<u32> &expected,
                       const std::vector<u32> &actual, size_t first = 0) {
  Require(name, stage, expected.size() == actual.size(), "size mismatch");
  for (size_t i = first; i < expected.size(); i++) {
    if (expected[i] != actual[i]) {
      Fail(name, stage,
           "word " + std::to_string(i) + ": expected " + Hex(expected[i]) +
               ", got " + Hex(actual[i]));
    }
  }
}

// V_MOVRELS with M0 = BFE(s, 24, 3) * 5, loaded from memory so it is not a compile-time
// constant: the value set {0, 5, ..., 35} folds all other compares of the select chain.
void CheckMoveRelValueSet(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenMoveRelValueSet";
  using O = ShaderOpcode;
  constexpr u32 OutBase = 16;
  TestCase test;
  test.name = name;
  AppendVMovLiteral(&test.code, 0, 0xdeadbeefu);
  for (u32 reg = 10; reg <= 50; reg++) {
    AppendVMovLiteral(&test.code, reg, 0xa0000000u + reg);
  }
  for (u32 k = 0; k < 8; k++) {
    AppendSmemLoadOpcode(&test.code, 0x08, 33, k * 4u); // s_buffer_load_dword s33
    test.code.push_back(EncodeSopp(0x0c, 0));             // s_waitcnt 0
    test.code.push_back(EncodeSop2(0x27, 34, 33, 255));   // s_bfe_u32 s34, s33, lit
    test.code.push_back(0x00030018u);
    test.code.push_back(EncodeSop2(0x26, 34, 34, InlineU32(5))); // s_mul_i32 s34, 5
    test.code.push_back(EncodeSMovB32(124, 34));                 // s_mov_b32 m0, s34
    test.code.push_back(EncodeVop1(0x43, 60, Vgpr(12)));         // v_movrels v60, v12
    test.code.push_back(EncodeVop1(0x43, 61, Vgpr(45)));         // v_movrels v61, v45
    AppendStoreVgpr(&test.code, 60, OutBase + 2u * k);
    AppendStoreVgpr(&test.code, 61, OutBase + 2u * k + 1u);
  }
  AppendEnd(&test.code);
  test.initial.assign(OutBase + 16u, 0u);
  for (u32 k = 0; k < 8; k++) {
    test.initial[k] = (k << 24u) | (0x00f0f0f0u + k) | (k == 3u ? 0xf8000000u : 0u);
  }
  test.opcodes = {O::V_MOV_B32, O::S_BUFFER_LOAD_DWORD, O::S_WAITCNT,
                  O::S_BFE_U32, O::S_MUL_I32,           O::S_MOV_B32,
                  O::V_MOVRELS_B32, O::BUFFER_STORE_DWORD, O::S_ENDPGM};

  // The highest named VGPR is v61, so offsets from v45 at or above 17 are out of range.
  const auto expected = [&](bool isa_out_of_range) {
    std::vector<u32> words(OutBase + 16u, 0u);
    for (u32 k = 0; k < 8; k++) {
      words[k] = test.initial[k];
      const u32 index = 5u * k;
      words[OutBase + 2u * k] = 0xa0000000u + 12u + index;
      const u32 reg = 45u + index;
      u32 value = 0;
      if (reg <= 50u) {
        value = 0xa0000000u + reg;
      } else if (reg == 60u) {
        value = words[OutBase + 2u * k]; // written by the first V_MOVRELS
      } else if (reg < 62u) {
        value = 0u; // never written
      } else {
        value = isa_out_of_range ? 0xdeadbeefu : 0xa0000000u + 45u;
      }
      words[OutBase + 2u * k + 1u] = value;
    }
    return words;
  };

  CodegenOptions fast = GetCodegenOptions();
  fast.movrel_range = true;
  CodegenOptions slow = fast;
  slow.movrel_range = false;
  std::vector<u32> fast_spirv;
  std::vector<u32> slow_spirv;
  std::vector<u32> fast_words;
  std::vector<u32> slow_words;
  {
    ScopedCodegenOptions scope(fast);
    fast_words = RunCollect(vulkan, test, OutBase + 16u, &fast_spirv);
  }
  {
    ScopedCodegenOptions scope(slow);
    slow_words = RunCollect(vulkan, test, OutBase + 16u, &slow_spirv);
  }
  RequireEqualWords(name, "value-set readback", expected(true), fast_words);
  RequireEqualWords(name, "legacy readback", expected(false), slow_words);
  const auto fast_selects = CountSpirv(fast_spirv, "OpSelect");
  const auto slow_selects = CountSpirv(slow_spirv, "OpSelect");
  Require(name, "select count", fast_selects * 3u < slow_selects,
          "value-set folding did not shrink the select chains: " +
              std::to_string(fast_selects) + " vs " +
              std::to_string(slow_selects));
  std::printf("[codegen] %-32s ok (OpSelect %zu -> %zu, words %zu -> %zu)\n", name,
              slow_selects, fast_selects, slow_spirv.size(), fast_spirv.size());
}

void RunAll(VulkanHarness *vulkan) {
  CheckMoveRelValueSet(vulkan);
}

} // namespace CodegenTests
