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
  ValidateSpirv(test.name, compiled.spirv);
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

// A 64-thread workgroup that loops over `count` elements (a multiple of 64). Input i of element
// e is at dword i * count + e; output o at (inputs + o) * count + e. The body reads inputs from
// v2.. and writes outputs to v10.. (v1 holds the lane byte offset, s40 the element byte offset).
struct BulkCase {
  TestCase test;
  u32 inputs = 0;
  u32 outputs = 0;
  u32 count = 0;
};

BulkCase MakeBulkCase(const char *name, u32 inputs, u32 outputs,
                      const std::vector<std::vector<u32>> &input_values,
                      const std::function<void(std::vector<u32> *)> &body,
                      std::vector<ShaderOpcode> opcodes) {
  BulkCase bulk;
  bulk.inputs = inputs;
  bulk.outputs = outputs;
  bulk.count = static_cast<u32>(input_values.front().size());
  Require(name, "bulk layout", bulk.count % 64u == 0u && input_values.size() == inputs,
          "bulk inputs must be one vector per input, a multiple of 64 long");
  auto &test = bulk.test;
  auto &code = test.code;
  test.name = name;
  code.push_back(EncodeVop2(0x1a, 1, InlineU32(2), 0)); // v_lshlrev_b32 v1, 2, v0
  code.push_back(EncodeSMovB32(40, InlineU32(0)));      // s_mov_b32 s40, 0
  const auto loop = static_cast<u32>(code.size());
  for (u32 i = 0; i < inputs; i++) {
    code.push_back(EncodeSop2(0x00, 41 + i, 40, 255)); // s_add_u32 s(41+i), s40, lit
    code.push_back(i * bulk.count * 4u);
    code.push_back(EncodeMubuf0(0x0c)); // buffer_load_dword v(2+i), v1, s[0:3], s(41+i)
    code.push_back(EncodeMubuf1(2 + i, 0, 1, 41 + i));
  }
  code.push_back(EncodeSopp(0x0c, 0)); // s_waitcnt 0
  body(&code);
  for (u32 o = 0; o < outputs; o++) {
    code.push_back(EncodeSop2(0x00, 50 + o, 40, 255)); // s_add_u32 s(50+o), s40, lit
    code.push_back((inputs + o) * bulk.count * 4u);
    code.push_back(EncodeMubuf0(0x1c)); // buffer_store_dword v(10+o), v1, s[0:3], s(50+o)
    code.push_back(EncodeMubuf1(10 + o, 0, 1, 50 + o));
  }
  code.push_back(EncodeSop2(0x00, 40, 40, 255)); // s_add_u32 s40, s40, 256
  code.push_back(256u);
  code.push_back(EncodeSopc(0x0a, 40, 255)); // s_cmp_lt_u32 s40, count * 4
  code.push_back(bulk.count * 4u);
  const auto branch = static_cast<int32_t>(loop) - static_cast<int32_t>(code.size()) - 1;
  code.push_back(EncodeSopp(0x05, static_cast<u32>(branch) & 0xffffu)); // s_cbranch_scc1
  AppendEnd(&code);
  test.initial.assign(static_cast<size_t>(inputs + outputs) * bulk.count, 0u);
  for (u32 i = 0; i < inputs; i++) {
    std::copy(input_values[i].begin(), input_values[i].end(),
              test.initial.begin() + static_cast<size_t>(i) * bulk.count);
  }
  test.compute_info.threads_num[0] = 64;
  test.compute_info.threads_num[1] = 1;
  test.compute_info.threads_num[2] = 1;
  test.compute_info.thread_ids_num = 1;
  test.compute_info.wave_size = 64;
  test.has_compute_info = true;
  opcodes.insert(opcodes.end(),
                 {ShaderOpcode::V_LSHLREV_B32, ShaderOpcode::S_MOV_B32,
                  ShaderOpcode::S_ADD_U32, ShaderOpcode::BUFFER_LOAD_DWORD,
                  ShaderOpcode::S_WAITCNT, ShaderOpcode::BUFFER_STORE_DWORD,
                  ShaderOpcode::S_CMP_LT_U32, ShaderOpcode::S_CBRANCH_SCC1,
                  ShaderOpcode::S_ENDPGM});
  test.opcodes = std::move(opcodes);
  return bulk;
}

// Output o of every element.
std::vector<u32> BulkOutput(const BulkCase &bulk, const std::vector<u32> &words, u32 output) {
  const auto begin = words.begin() + static_cast<size_t>(bulk.inputs + output) * bulk.count;
  return {begin, begin + bulk.count};
}

struct BulkRun {
  std::vector<u32> words;
  std::vector<u32> spirv;
};

BulkRun RunBulk(VulkanHarness *vulkan, const BulkCase &bulk, const CodegenOptions &options) {
  ScopedCodegenOptions scope(options);
  BulkRun run;
  run.words = RunCollect(vulkan, bulk.test, bulk.test.initial.size(), &run.spirv);
  return run;
}

// Compares every output of the new and the legacy code generation with each other and with an
// optional CPU reference (reference(output, element) -> expected bits). Elements for which\n// skip returns true are not compared; the number skipped is returned.
size_t CompareBulk(const char *name, const BulkCase &bulk, const BulkRun &fast,
                   const BulkRun &slow, const std::function<u32(u32, u32)> &reference,
                   const char *const *output_names,
                   const std::function<bool(u32, u32)> &skip = {}) {
  size_t skipped = 0;
  for (u32 o = 0; o < bulk.outputs; o++) {
    const auto fast_out = BulkOutput(bulk, fast.words, o);
    const auto slow_out = BulkOutput(bulk, slow.words, o);
    for (u32 e = 0; e < bulk.count; e++) {
      const auto input = [&](u32 i) {
        return i < bulk.inputs ? bulk.test.initial[static_cast<size_t>(i) * bulk.count + e] : 0u;
      };
      if (skip && skip(o, e)) {
        skipped++;
        continue;
      }
      const auto describe = [&]() {
        return std::string(output_names[o]) + " element " + std::to_string(e) + " inputs " +
               Hex(input(0)) + " " + Hex(input(1)) + " " + Hex(input(2));
      };
      if (fast_out[e] != slow_out[e]) {
        Fail(name, "new vs legacy",
             describe() + ": new " + Hex(fast_out[e]) + ", legacy " + Hex(slow_out[e]));
      }
      if (reference) {
        const auto expected = reference(o, e);
        if (fast_out[e] != expected) {
          Fail(name, "CPU reference",
               describe() + ": got " + Hex(fast_out[e]) + ", expected " + Hex(expected));
        }
      }
    }
  }
  return skipped;
}

// f32 bit patterns covering zeros, denormals, normals, infinities and NaNs with payloads.
const std::vector<u32> &SpecialF32() {
  static const std::vector<u32> values = {
      0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007fffffu, 0x807fffffu,
      0x00400000u, 0x80400000u, 0x00800000u, 0x80800000u, 0x3f800000u, 0xbf800000u,
      0x3fc00000u, 0xbfc00000u, 0x3f000000u, 0xbf000000u, 0x40000000u, 0xc0000000u,
      0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u,
      0x7f800001u, 0xff800001u, 0x7fa00000u, 0x7fffffffu, 0xffffffffu, 0x7fc12345u,
      0x3eaaaaabu, 0x42c80000u,
  };
  return values;
}

uint32_t NextRandom(uint64_t &state) {
  state = state * 6364136223846793005ull + 1442695040888963407ull;
  return static_cast<uint32_t>(state >> 32u);
}

// Mostly random bit patterns, a quarter drawn from the special set.
u32 RandomF32(uint64_t &state) {
  const auto r = NextRandom(state);
  if ((r & 3u) == 0u) {
    return SpecialF32()[NextRandom(state) % SpecialF32().size()];
  }
  return NextRandom(state);
}

// CPU model of the legacy f32 min/max emulation (EmitMinMaxF32Value). The NVIDIA driver flushes
// f32 denormals in float compares by default (the module declares no f32 denorm mode there), so
// the ordered compare sees denormals as zeros while the selected bits stay raw.
float FlushedF32(u32 bits) {
  return (bits & 0x7f800000u) == 0u ? std::bit_cast<float>(bits & 0x80000000u)
                                    : std::bit_cast<float>(bits);
}

u32 RefMinMaxF32(u32 lhs, u32 rhs, bool max_value) {
  const auto nan = [](u32 bits) { return (bits & 0x7fffffffu) > 0x7f800000u; };
  const auto zero = [](u32 bits) { return (bits & 0x7fffffffu) == 0u; };
  if (nan(lhs)) {
    return rhs;
  }
  if (nan(rhs)) {
    return lhs;
  }
  if (zero(lhs) && zero(rhs)) {
    return max_value ? (lhs & rhs) : (lhs | rhs);
  }
  const auto a = FlushedF32(lhs);
  const auto b = FlushedF32(rhs);
  return max_value ? (a >= b ? lhs : rhs) : (a < b ? lhs : rhs);
}

u32 RefMedTriF32(u32 a, u32 b, u32 c) {
  const auto nan = [](u32 bits) { return (bits & 0x7fffffffu) > 0x7f800000u; };
  const auto min_ab = RefMinMaxF32(a, b, false);
  const auto min3 = RefMinMaxF32(min_ab, c, false);
  const auto max_ab = RefMinMaxF32(a, b, true);
  const auto high_min = RefMinMaxF32(max_ab, c, false);
  const auto median = RefMinMaxF32(min_ab, high_min, true);
  return nan(a) || nan(b) || nan(c) ? min3 : median;
}

void CheckFloatMinMax(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenFloatMinMaxMed3";
  using O = ShaderOpcode;
  // Every triple of special values, then random triples.
  const auto &special = SpecialF32();
  std::vector<std::vector<u32>> inputs(3);
  for (const auto a : special) {
    for (const auto b : special) {
      for (const auto c : special) {
        inputs[0].push_back(a);
        inputs[1].push_back(b);
        inputs[2].push_back(c);
      }
    }
  }
  uint64_t seed = 0x1234567890abcdefull;
  while (inputs[0].size() < 65536u) {
    for (auto &input : inputs) {
      input.push_back(RandomF32(seed));
    }
  }
  const auto bulk = MakeBulkCase(
      name, 3, 7, inputs,
      [](std::vector<u32> *code) {
        code->push_back(EncodeVop2(0x0f, 10, Vgpr(2), 3)); // v_min_f32 v10, v2, v3
        code->push_back(EncodeVop2(0x10, 11, Vgpr(2), 3)); // v_max_f32 v11, v2, v3
        AppendVop3(code, 0x151, 12, Vgpr(2), Vgpr(3), Vgpr(4)); // v_min3_f32
        AppendVop3(code, 0x154, 13, Vgpr(2), Vgpr(3), Vgpr(4)); // v_max3_f32
        AppendVop3(code, 0x157, 14, Vgpr(2), Vgpr(3), Vgpr(4)); // v_med3_f32
        AppendVop3(code, 0x157, 15, Vgpr(4), Vgpr(2), Vgpr(3)); // v_med3_f32 (rotated)
        code->push_back(EncodeVop2(0x0f, 16, Vgpr(3), 2)); // v_min_f32 v16, v3, v2
      },
      {O::V_MIN_F32, O::V_MAX_F32, O::V_MIN3_F32, O::V_MAX3_F32, O::V_MED3_F32});
  CodegenOptions fast = GetCodegenOptions();
  fast.fast_float_min_max = true;
  CodegenOptions slow = fast;
  slow.fast_float_min_max = false;
  const auto fast_run = RunBulk(vulkan, bulk, fast);
  const auto slow_run = RunBulk(vulkan, bulk, slow);
  const auto in = [&](u32 i, u32 e) { return inputs[i][e]; };
  static const char *const names[] = {"min", "max", "min3", "max3", "med3", "med3(c,a,b)",
                                      "min(b,a)"};
  CompareBulk(name, bulk, fast_run, slow_run,
              [&](u32 o, u32 e) -> u32 {
                const auto a = in(0, e), b = in(1, e), c = in(2, e);
                switch (o) {
                case 0: return RefMinMaxF32(a, b, false);
                case 1: return RefMinMaxF32(a, b, true);
                case 2: return RefMinMaxF32(RefMinMaxF32(a, b, false), c, false);
                case 3: return RefMinMaxF32(RefMinMaxF32(a, b, true), c, true);
                case 4: return RefMedTriF32(a, b, c);
                case 5: return RefMedTriF32(c, a, b);
                default: return RefMinMaxF32(b, a, false);
                }
              },
              names);
  std::printf("[codegen] %-32s ok (%u elements, SPIR-V %zu -> %zu words, OpSelect %zu -> %zu)\n",
              name, bulk.count, slow_run.spirv.size(), fast_run.spirv.size(),
              CountSpirv(slow_run.spirv, "OpSelect"), CountSpirv(fast_run.spirv, "OpSelect"));
}

// med3/min/max with constant operands (the clamp idiom): x from the special set and random bits.
void CheckFloatClampConstants(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenFloatClampConstants";
  using O = ShaderOpcode;
  constexpr u32 Zero = 128, One = 242, MinusOne = 243, Half = 240, MinusHalf = 241, Four = 246,
                Literal = 255;
  std::vector<std::vector<u32>> inputs(1);
  inputs[0] = SpecialF32();
  uint64_t seed = 0xfeedfacecafebeefull;
  while (inputs[0].size() < 4096u) {
    inputs[0].push_back(RandomF32(seed));
  }
  struct Op {
    u32 opcode;
    u32 src0, src1, src2;
    u32 literal;
  };
  const std::vector<Op> ops = {
      {0x157, Vgpr(2), Zero, One, 0},        {0x157, Zero, Vgpr(2), One, 0},
      {0x157, One, Zero, Vgpr(2), 0},        {0x157, Vgpr(2), MinusOne, Zero, 0},
      {0x157, MinusOne, Vgpr(2), Zero, 0},   {0x157, Zero, MinusOne, Vgpr(2), 0},
      {0x157, Vgpr(2), Literal, One, 0x80000000u},
      {0x157, Vgpr(2), MinusOne, Literal, 0x80000000u},
      {0x157, Vgpr(2), Half, MinusHalf, 0},  {0x157, Four, Vgpr(2), Four, 0},
      {0x157, Vgpr(2), Literal, One, 0x7fc00000u},
      {0x10f, Vgpr(2), Zero, 0, 0},          {0x110, Vgpr(2), Zero, 0, 0},
      {0x110, One, Vgpr(2), 0, 0},           {0x10f, Vgpr(2), Literal, 0, 0x7fd00001u},
      {0x151, Vgpr(2), Zero, One, 0},        {0x154, MinusOne, Vgpr(2), Literal, 0x80000000u},
  };
  const auto bulk = MakeBulkCase(
      name, 1, static_cast<u32>(ops.size()), inputs,
      [&](std::vector<u32> *code) {
        for (u32 i = 0; i < ops.size(); i++) {
          AppendVop3(code, ops[i].opcode, 10 + i, ops[i].src0, ops[i].src1, ops[i].src2);
          if (ops[i].src0 == Literal || ops[i].src1 == Literal || ops[i].src2 == Literal) {
            code->push_back(ops[i].literal);
          }
        }
      },
      {O::V_MIN_F32, O::V_MAX_F32, O::V_MIN3_F32, O::V_MAX3_F32, O::V_MED3_F32});
  const auto operand = [&](const Op &op, u32 src, u32 x) -> u32 {
    switch (src) {
    case Zero: return 0x00000000u;
    case One: return 0x3f800000u;
    case MinusOne: return 0xbf800000u;
    case Half: return 0x3f000000u;
    case MinusHalf: return 0xbf000000u;
    case Four: return 0x40800000u;
    case Literal: return op.literal;
    default: return x;
    }
  };
  CodegenOptions fast = GetCodegenOptions();
  fast.fast_float_min_max = true;
  CodegenOptions slow = fast;
  slow.fast_float_min_max = false;
  const auto fast_run = RunBulk(vulkan, bulk, fast);
  const auto slow_run = RunBulk(vulkan, bulk, slow);
  std::vector<std::string> labels;
  std::vector<const char *> names;
  for (u32 i = 0; i < ops.size(); i++) {
    labels.push_back("op" + std::to_string(i));
  }
  for (const auto &label : labels) {
    names.push_back(label.c_str());
  }
  // A denormal x compared with a constant is where the NVIDIA compiler is inconsistent: it
  // flushes denormals in register compares but may lower a compare against an immediate to an
  // integer test that does not flush, per instance. The legacy code itself then returns
  // different bits for the same values depending on whether an operand is a constant (for
  // example med3(0, 0x00000001, 1.0) gives 0x00000001 with registers and 0 with constants), so
  // those elements have no single "current result" to match and are not compared.
  const auto denormal_x = [&](u32, u32 e) {
    const auto x = inputs[0][e];
    return (x & 0x7f800000u) == 0u && (x & 0x007fffffu) != 0u;
  };
  const auto skipped = CompareBulk(
      name, bulk, fast_run, slow_run,
      [&](u32 o, u32 e) -> u32 {
        const auto &op = ops[o];
        const auto x = inputs[0][e];
        const auto a = operand(op, op.src0, x), b = operand(op, op.src1, x),
                   c = operand(op, op.src2, x);
        switch (op.opcode) {
        case 0x10f: return RefMinMaxF32(a, b, false);
        case 0x110: return RefMinMaxF32(a, b, true);
        case 0x151: return RefMinMaxF32(RefMinMaxF32(a, b, false), c, false);
        case 0x154: return RefMinMaxF32(RefMinMaxF32(a, b, true), c, true);
        default: return RefMedTriF32(a, b, c);
        }
      },
      names.data(), denormal_x);
  std::printf("[codegen] %-32s ok (%u elements x %zu ops, %zu denormal skipped, SPIR-V %zu -> "
              "%zu words)\n",
              name, bulk.count, ops.size(), skipped, slow_run.spirv.size(),
              fast_run.spirv.size());
}

// CPU port of EmitF32ToF16RtzBits (the legacy V_CVT_PKRTZ_F16_F32 half conversion).
u32 RefF32ToF16Rtz(u32 bits) {
  const u32 sign = (bits >> 16u) & 0x8000u;
  const u32 exp = (bits >> 23u) & 0xffu;
  const u32 mant = bits & 0x007fffffu;
  if (exp == 255u) {
    return mant == 0u ? (sign | 0x7c00u) : (sign | 0x7c00u | (mant >> 13u) | 0x0200u);
  }
  if (exp >= 143u) {
    return sign | 0x7bffu;
  }
  if (exp < 103u) {
    return sign;
  }
  if (exp <= 112u) {
    const u32 shift = exp > 112u ? 14u : 126u - exp;
    return sign | ((mant | 0x00800000u) >> shift);
  }
  return (sign | ((exp - 112u) << 10u) | (mant >> 13u)) & 0xffffu;
}

// CPU model of EmitFastF32ToF16RtzBits, op for op.
u32 FastF32ToF16Rtz(u32 bits) {
  const u32 abs = bits & 0x7fffffffu;
  const u32 sign = (bits >> 16u) & 0x8000u;
  const u32 high13 = abs >> 13u;
  const u32 normal = std::min(high13 - (112u << 10u), 0x7bffu);
  const u32 exponent = abs >> 23u;
  const u32 shift = std::min(126u - exponent, 31u);
  const u32 subnormal = ((abs & 0x007fffffu) | 0x00800000u) >> shift;
  u32 result = abs < 0x38800000u ? subnormal : normal;
  const u32 special = (high13 & 0x7fffu) | (abs > 0x7f800000u ? 0x0200u : 0u);
  result = abs >= 0x7f800000u ? special : result;
  return result | sign;
}

// The integer formulation against the legacy one for all 2^32 inputs (CPU, a few seconds).
void CheckPackRtzExhaustive() {
  constexpr const char *name = "CodegenCvtPkrtzExhaustive";
  constexpr u32 Threads = 8;
  std::array<std::atomic<uint64_t>, Threads> mismatch{};
  std::vector<std::thread> workers;
  for (u32 t = 0; t < Threads; t++) {
    workers.emplace_back([t, &mismatch] {
      const uint64_t begin = (uint64_t{1} << 32u) / Threads * t;
      const uint64_t end = (uint64_t{1} << 32u) / Threads * (t + 1u);
      uint64_t first = UINT64_MAX;
      for (uint64_t value = begin; value < end; value++) {
        const auto bits = static_cast<u32>(value);
        if (FastF32ToF16Rtz(bits) != RefF32ToF16Rtz(bits)) {
          first = value;
          break;
        }
      }
      mismatch[t] = first;
    });
  }
  for (auto &worker : workers) {
    worker.join();
  }
  for (const auto &value : mismatch) {
    if (value != UINT64_MAX) {
      const auto bits = static_cast<u32>(value.load());
      Fail(name, "exhaustive", "input " + Hex(bits) + ": fast " + Hex(FastF32ToF16Rtz(bits)) +
                                   ", legacy " + Hex(RefF32ToF16Rtz(bits)));
    }
  }
  std::printf("[codegen] %-32s ok (2^32 inputs)\n", name);
}

void CheckPackRtz(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenCvtPkrtzF16F32";
  using O = ShaderOpcode;
  std::vector<std::vector<u32>> inputs(2);
  // Every exponent and sign with mantissas around the f16 rounding points, then random bits.
  const u32 mantissas[] = {0x000000u, 0x000001u, 0x000fffu, 0x001000u, 0x001001u, 0x001fffu,
                           0x002000u, 0x002001u, 0x3fe000u, 0x3fffffu, 0x400000u, 0x7fe000u,
                           0x7fefffu, 0x7ff000u, 0x7fffffu, 0x555555u};
  uint64_t seed = 0x0badc0ffee123457ull;
  for (u32 sign = 0; sign < 2u; sign++) {
    for (u32 exp = 0; exp < 256u; exp++) {
      for (const auto mant : mantissas) {
        inputs[0].push_back((sign << 31u) | (exp << 23u) | mant);
        inputs[1].push_back(RandomF32(seed));
      }
    }
  }
  for (const auto value : SpecialF32()) {
    inputs[0].push_back(value);
    inputs[1].push_back(value ^ 0x80000000u);
  }
  while (inputs[0].size() < (1u << 19u)) {
    inputs[0].push_back(RandomF32(seed));
    inputs[1].push_back(NextRandom(seed));
  }
  inputs[0].resize(inputs[0].size() / 64u * 64u);
  inputs[1].resize(inputs[0].size());
  const auto bulk = MakeBulkCase(
      name, 2, 2, inputs,
      [](std::vector<u32> *code) {
        code->push_back(EncodeVop2(0x2f, 10, Vgpr(2), 3)); // v_cvt_pkrtz_f16_f32 v10, v2, v3
        code->push_back(EncodeVop2(0x2f, 11, Vgpr(3), 2)); // v_cvt_pkrtz_f16_f32 v11, v3, v2
      },
      {O::V_CVT_PKRTZ_F16_F32});
  CodegenOptions fast = GetCodegenOptions();
  fast.fast_pkrtz = true;
  CodegenOptions slow = fast;
  slow.fast_pkrtz = false;
  const auto fast_run = RunBulk(vulkan, bulk, fast);
  const auto slow_run = RunBulk(vulkan, bulk, slow);
  static const char *const names[] = {"pkrtz(x,y)", "pkrtz(y,x)"};
  CompareBulk(name, bulk, fast_run, slow_run,
              [&](u32 o, u32 e) -> u32 {
                const auto lo = inputs[o][e];
                const auto hi = inputs[1u - o][e];
                return RefF32ToF16Rtz(lo) | (RefF32ToF16Rtz(hi) << 16u);
              },
              names);
  std::printf("[codegen] %-32s ok (%u pairs, SPIR-V %zu -> %zu words)\n", name, bulk.count,
              slow_run.spirv.size(), fast_run.spirv.size());
}

// Float-to-int conversions with the emitter's second saturation layer on and off.
void CheckFloatToIntSaturation(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenFloatToIntSaturation";
  using O = ShaderOpcode;
  std::vector<std::vector<u32>> inputs(1);
  inputs[0] = SpecialF32();
  // Around the integer range limits and the rounding points.
  const float edges[] = {2147483520.0f, 2147483648.0f, 4294967040.0f, 4294967296.0f,
                         -2147483648.0f, -2147483904.0f, 65535.0f,    65536.0f,
                         32767.0f,       32768.0f,       -32768.0f,   -32769.0f,
                         255.0f,         255.5f,         256.0f,      0.5f,
                         -0.5f,          1.5f,           -1.5f,       2.5f};
  for (const auto edge : edges) {
    const auto bits = std::bit_cast<u32>(edge);
    for (int delta = -2; delta <= 2; delta++) {
      inputs[0].push_back(bits + static_cast<u32>(delta));
    }
  }
  uint64_t seed = 0x5eed5eed5eed5eedull;
  while (inputs[0].size() < 65536u) {
    // Random magnitudes spread over the whole exponent range.
    inputs[0].push_back(RandomF32(seed));
  }
  const auto bulk = MakeBulkCase(
      name, 1, 7, inputs,
      [](std::vector<u32> *code) {
        code->push_back(EncodeVop1(0x07, 10, Vgpr(2))); // v_cvt_u32_f32
        code->push_back(EncodeVop1(0x08, 11, Vgpr(2))); // v_cvt_i32_f32
        code->push_back(EncodeVop1(0x0c, 12, Vgpr(2))); // v_cvt_rpi_i32_f32
        code->push_back(EncodeVop1(0x0d, 13, Vgpr(2))); // v_cvt_flr_i32_f32
        code->push_back(EncodeVop1(0x52, 14, Vgpr(2))); // v_cvt_u16_f16 (low half as f16)
        code->push_back(EncodeVop1(0x53, 15, Vgpr(2))); // v_cvt_i16_f16
        AppendVop3(code, 0x15e, 16, Vgpr(2), InlineU32(1), InlineU32(0)); // v_cvt_pk_u8_f32
      },
      {O::V_CVT_U32_F32, O::V_CVT_I32_F32, O::V_CVT_RPI_I32_F32, O::V_CVT_FLR_I32_F32,
       O::V_CVT_U16_F16, O::V_CVT_I16_F16, O::V_CVT_PK_U8_F32});
  CodegenOptions fast = GetCodegenOptions();
  fast.single_f2i_saturation = true;
  CodegenOptions slow = fast;
  slow.single_f2i_saturation = false;
  const auto fast_run = RunBulk(vulkan, bulk, fast);
  const auto slow_run = RunBulk(vulkan, bulk, slow);
  static const char *const names[] = {"cvt_u32", "cvt_i32", "cvt_rpi", "cvt_flr",
                                      "cvt_u16_f16", "cvt_i16_f16", "cvt_pk_u8"};
  CompareBulk(name, bulk, fast_run, slow_run, {}, names);
  std::printf("[codegen] %-32s ok (%u values, SPIR-V %zu -> %zu words)\n", name, bulk.count,
              slow_run.spirv.size(), fast_run.spirv.size());
}

// GET_LOD_STATS instrumentation of a pixel-shader sample: compile-only (the graphics harness has
// no mip-statistics buffer), SPIR-V validation and structure in both modes. Needs no GPU
// (--lod-stats-codegen-only).
void CheckLodStatsGate() {
  constexpr const char *name = "CodegenLodStatsGate";
  GraphicsCase test;
  test.name = name;
  AppendVMovLiteral(&test.fragment_code, 20, 0x3f000000u);
  AppendVMovLiteral(&test.fragment_code, 21, 0x3f000000u);
  test.fragment_code.push_back(EncodeMimg0(0x20, 0xf)); // image_sample v[0:3], v[20:21]
  test.fragment_code.push_back(EncodeMimg1(0, 20));
  test.fragment_code.push_back(EncodeExp0(0, 0xf, true, false, true));
  test.fragment_code.push_back(EncodeExp1(0, 1, 2, 3));
  AppendEnd(&test.fragment_code);
  test.user_data = MakeSampledTextureData(Prospero::BufferFormat::k32Float);
  test.has_user_data = true;
  test.opcodes = {ShaderOpcode::V_MOV_B32, ShaderOpcode::IMAGE_SAMPLE, ShaderOpcode::EXP,
                  ShaderOpcode::S_ENDPGM};
  CodegenOptions gated = GetCodegenOptions();
  gated.lod_stats_gate = true;
  CodegenOptions legacy = gated;
  legacy.lod_stats_gate = false;
  std::vector<u32> gated_spirv;
  std::vector<u32> legacy_spirv;
  {
    ScopedCodegenOptions scope(gated);
    gated_spirv = CompileFragmentCase(test).spirv;
  }
  {
    ScopedCodegenOptions scope(legacy);
    legacy_spirv = CompileFragmentCase(test).spirv;
  }
  ValidateSpirv(name, gated_spirv);
  ValidateSpirv(name, legacy_spirv);
  const auto count = [](const std::vector<u32> &spirv, const char *op) {
    return CountSpirv(spirv, op);
  };
  // Both modes: the finest level (subgroup UMin, one AtomicUMin) and the count, added only when a
  // lane's LOD is below the field's threshold (FOrdLessThan, subgroup UMax, one AtomicIAdd in its
  // own branch). The gate adds the has-counter and smaller-than-stored branches.
  Require(name, "instrumentation",
          count(legacy_spirv, "OpImageQueryLod") == 1u &&
              count(legacy_spirv, "OpAtomicUMin") == 1u &&
              count(legacy_spirv, "OpAtomicIAdd") == 1u &&
              count(legacy_spirv, "OpAtomicLoad") == 0u &&
              count(legacy_spirv, "OpGroupNonUniformUMin") == 1u &&
              count(legacy_spirv, "OpGroupNonUniformUMax") == 1u &&
              count(legacy_spirv, "OpFOrdLessThan") >= 1u &&
              count(gated_spirv, "OpImageQueryLod") == 1u &&
              count(gated_spirv, "OpAtomicUMin") == 1u &&
              count(gated_spirv, "OpAtomicIAdd") == 1u &&
              count(gated_spirv, "OpAtomicLoad") == 1u &&
              count(gated_spirv, "OpGroupNonUniformUMin") == 1u &&
              count(gated_spirv, "OpGroupNonUniformUMax") == 1u &&
              count(gated_spirv, "OpSelectionMerge") ==
                  count(legacy_spirv, "OpSelectionMerge") + 2u,
          "unexpected GET_LOD_STATS instrumentation structure");
  std::printf("[codegen] %-32s ok (SPIR-V %zu -> %zu words)\n", name, legacy_spirv.size(),
              gated_spirv.size());
}

// KYTY_LOD_STATS_PLAIN_VARIANT: the plain variant of an instrumented pixel shader keeps the
// mip_stats binding and every guest instruction but has no GET_LOD_STATS feedback: no LOD query,
// subgroup reduction or atomic, so no memory side effect that would force late depth tests.
void CheckLodStatsPlainVariant() {
  constexpr const char *name = "CodegenLodStatsPlainVariant";
  GraphicsCase test;
  test.name = name;
  AppendVMovLiteral(&test.fragment_code, 20, 0x3f000000u);
  AppendVMovLiteral(&test.fragment_code, 21, 0x3f000000u);
  test.fragment_code.push_back(EncodeMimg0(0x20, 0xf)); // image_sample v[0:3], v[20:21]
  test.fragment_code.push_back(EncodeMimg1(0, 20));
  test.fragment_code.push_back(EncodeExp0(0, 0xf, true, false, true));
  test.fragment_code.push_back(EncodeExp1(0, 1, 2, 3));
  AppendEnd(&test.fragment_code);
  test.user_data = MakeSampledTextureData(Prospero::BufferFormat::k32Float);
  test.has_user_data = true;
  test.opcodes = {ShaderOpcode::V_MOV_B32, ShaderOpcode::IMAGE_SAMPLE, ShaderOpcode::EXP,
                  ShaderOpcode::S_ENDPGM};
  const auto without = CompileFragmentCase(test);
  const auto with = CompileFragmentCase(test, true);
  Require(name, "only on request", without.spirv_plain.empty() && !with.spirv_plain.empty(),
          "the plain variant was emitted without being requested, or not at all");
  Require(name, "instrumented module unchanged", with.spirv == without.spirv,
          "requesting the plain variant changed the instrumented module");
  const auto &plain = with.spirv_plain;
  const auto count = [](const std::vector<u32> &spirv, const char *op) {
    return CountSpirv(spirv, op);
  };
  Require(name, "no feedback",
          count(plain, "OpImageQueryLod") == 0u && count(plain, "OpAtomicUMin") == 0u &&
              count(plain, "OpAtomicIAdd") == 0u && count(plain, "OpAtomicLoad") == 0u &&
              count(plain, "OpGroupNonUniformUMin") == 0u &&
              count(plain, "OpImageSampleImplicitLod") ==
                  count(with.spirv, "OpImageSampleImplicitLod") &&
              count(with.spirv, "OpAtomicUMin") == 1u,
          "the plain variant still records GET_LOD_STATS feedback, or lost the guest sample");
  // Same bindings: the mip_stats storage buffer stays declared, so both modules fit one layout.
  const auto has_name = [](const std::vector<u32> &spirv, const char *wanted) {
    for (size_t i = 5; i < spirv.size();) {
      const u32 words = spirv[i] >> 16u;
      if (words == 0u) {
        break;
      }
      if ((spirv[i] & 0xffffu) == 5u /* OpName */ && words > 2u &&
          std::strcmp(reinterpret_cast<const char *>(&spirv[i + 2]), wanted) == 0) {
        return true;
      }
      i += words;
    }
    return false;
  };
  Require(name, "binding kept", has_name(plain, "mip_stats") && has_name(with.spirv, "mip_stats"),
          "the plain variant dropped the mip_stats binding declaration");
  std::printf("[codegen] %-32s ok (SPIR-V %zu -> %zu words)\n", name, with.spirv.size(),
              plain.size());
}

// Buffer and scalar-buffer loads that straddle the end of a descriptor range whose size is not a
// multiple of 4 bytes, with the shader's dword bounds check and with the device's
// robustBufferAccess2 check. Out-of-range dwords (any byte past the range) must read 0.
void CheckRobustBufferLoads(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenRobustBufferLoads";
  using O = ShaderOpcode;
  if (vulkan->RobustStorageAlignment() != 1u) {
    std::printf("[codegen] %-32s skipped (robustStorageBufferAccessSizeAlignment %u)\n", name,
                static_cast<u32>(vulkan->RobustStorageAlignment()));
    return;
  }
  constexpr u32 Lanes = 16;
  constexpr u32 PerLane = 16;
  constexpr u32 Data = Lanes * PerLane;  // first data dword
  constexpr u32 RangeBytes = 4u * (Data + 8u) + 2u; // dword Data+8 is half inside
  TestCase test;
  test.name = name;
  auto &code = test.code;
  code.push_back(EncodeVop2(0x1a, 1, InlineU32(2), 0)); // v1 = lane * 4
  code.push_back(EncodeVop2(0x1a, 2, InlineU32(1), 0)); // v2 = lane * 2
  code.push_back(EncodeVop2(0x1a, 5, InlineU32(6), 0)); // v5 = lane * 64 (output base)
  const auto mubuf = [&](u32 opcode, u32 vdata, u32 vaddr, u32 offset) {
    code.push_back(EncodeMubuf0(opcode, offset));
    code.push_back(EncodeMubuf1(vdata, 0, vaddr));
  };
  mubuf(0x0c, 10, 1, 4u * (Data + 4u));        // dword: Data+4 .. Data+19
  mubuf(0x0a, 11, 2, 4u * (Data + 6u));        // ushort: 2-byte steps across the end
  mubuf(0x0b, 12, 2, 4u * (Data + 5u) + 2u);   // sshort
  mubuf(0x08, 13, 0, 4u * (Data + 8u) - 6u);   // ubyte: 1-byte steps
  mubuf(0x09, 14, 0, 4u * (Data + 8u) - 6u);   // sbyte
  mubuf(0x0e, 16, 1, 4u * (Data + 2u));        // dwordx4 v[16:19]
  AppendSmemLoadOpcode(&code, 0x0a, 20, 4u * (Data + 6u)); // s_buffer_load_dwordx4 s[20:23]
  AppendSmemLoadOpcode(&code, 0x08, 24, 4u * (Data + 8u)); // s_buffer_load_dword s24 (partial)
  AppendSmemLoadOpcode(&code, 0x08, 25, 4u * (Data + 7u)); // s_buffer_load_dword s25
  code.push_back(EncodeSopp(0x0c, 0));
  for (u32 s = 0; s < 6; s++) {
    code.push_back(EncodeVop1(0x01, 30 + s, 20 + s)); // v_mov_b32 v(30+s), s(20+s)
  }
  const u32 outputs[] = {10, 11, 12, 13, 14, 16, 17, 18, 19, 30, 31, 32, 33, 34, 35};
  for (u32 k = 0; k < std::size(outputs); k++) {
    code.push_back(EncodeMubuf0(0x1c, 4u * k)); // buffer_store_dword v, v5, s[48:51]
    code.push_back(EncodeMubuf1(outputs[k], 12, 5));
  }
  AppendEnd(&code);
  test.initial.assign(Data + 24u, 0u);
  for (u32 i = 0; i < Data + 24u; i++) {
    test.initial[i] = i < Data ? 0u : 0x80a0c0e0u + (i - Data) * 0x01010101u;
  }
  test.compute_info.threads_num[0] = Lanes;
  test.compute_info.threads_num[1] = 1;
  test.compute_info.threads_num[2] = 1;
  test.compute_info.thread_ids_num = 1;
  test.has_compute_info = true;
  test.storage_buffer_range_bytes = RangeBytes;
  test.opcodes = {O::V_LSHLREV_B32, O::BUFFER_LOAD_DWORD, O::BUFFER_LOAD_USHORT,
                  O::BUFFER_LOAD_SSHORT, O::BUFFER_LOAD_UBYTE, O::BUFFER_LOAD_SBYTE,
                  O::BUFFER_LOAD_DWORDX4, O::S_BUFFER_LOAD_DWORDX4, O::S_BUFFER_LOAD_DWORD,
                  O::S_WAITCNT, O::V_MOV_B32, O::BUFFER_STORE_DWORD, O::S_ENDPGM};

  // Reference: a dword reads its data when all four bytes are inside the range, else 0.
  const auto dword = [&](u32 index) -> u32 {
    return 4u * index + 4u <= RangeBytes && index < test.initial.size() ? test.initial[index]
                                                                          : 0u;
  };
  const auto sub = [&](u32 byte, u32 bits, bool sign) -> u32 {
    const auto word = dword(byte / 4u) >> ((byte % 4u) * 8u);
    const auto mask = bits == 8u ? 0xffu : 0xffffu;
    auto value = word & mask;
    if (sign && (value & (1u << (bits - 1u))) != 0u) {
      value |= ~mask;
    }
    return value;
  };
  std::vector<u32> expected(Data, 0u);
  for (u32 lane = 0; lane < Lanes; lane++) {
    const std::array<u32, 15> values = {
        dword(Data + 4u + lane),
        sub(4u * (Data + 6u) + 2u * lane, 16, false),
        sub(4u * (Data + 5u) + 2u + 2u * lane, 16, true),
        sub(4u * (Data + 8u) - 6u + lane, 8, false),
        sub(4u * (Data + 8u) - 6u + lane, 8, true),
        dword(Data + 2u + lane),
        dword(Data + 3u + lane),
        dword(Data + 4u + lane),
        dword(Data + 5u + lane),
        dword(Data + 6u),
        dword(Data + 7u),
        dword(Data + 8u),
        dword(Data + 9u),
        dword(Data + 8u),
        dword(Data + 7u)};
    std::copy(values.begin(), values.end(), expected.begin() + lane * PerLane);
  }
  CodegenOptions device = GetCodegenOptions();
  device.robust_buffer_loads = true;
  CodegenOptions shader = device;
  shader.robust_buffer_loads = false;
  std::vector<u32> device_spirv;
  std::vector<u32> shader_spirv;
  std::vector<u32> device_words;
  std::vector<u32> shader_words;
  {
    // The host binds storage buffers in whole dwords (descriptors.cpp NativeStorageBuffer):
    // NVIDIA returns data for a dword only partly inside the range despite reporting a 1-byte
    // robustness alignment. The shader-checked run keeps the odd range to show that rounding
    // down changes nothing for the shader's own checks.
    auto device_test = test;
    device_test.storage_buffer_range_bytes = RangeBytes & ~3u;
    ScopedCodegenOptions scope(device);
    device_words = RunCollect(vulkan, device_test, Data, &device_spirv);
  }
  {
    ScopedCodegenOptions scope(shader);
    shader_words = RunCollect(vulkan, test, Data, &shader_spirv);
  }
  RequireEqualWords(name, "shader-checked loads", expected, shader_words);
  RequireEqualWords(name, "device-checked loads", expected, device_words);
  std::printf("[codegen] %-32s ok (OpArrayLength %zu -> %zu, OpBranchConditional %zu -> %zu)\n",
              name, CountSpirv(shader_spirv, "OpArrayLength"),
              CountSpirv(device_spirv, "OpArrayLength"),
              CountSpirv(shader_spirv, "OpBranchConditional"),
              CountSpirv(device_spirv, "OpBranchConditional"));
}

// Random finite f32 with a moderate exponent, so products and sums stay normal.
u32 RandomModerateF32(uint64_t &state) {
  const auto bits = NextRandom(state);
  const auto exponent = 127u - 20u + (NextRandom(state) % 41u);
  return (bits & 0x807fffffu) | (exponent << 23u);
}

float RoundedF32(float value) {
  volatile float stored = value;
  return stored;
}

// V_MAD/MAC/MADMK/MADAK (unfused on PS5) and V_FMA (fused) in a compute shader under every
// MadMode: Exact must give round(round(a*b)+c), Fused and Position (compute has no position
// export) the fused result.
void CheckMadModes(VulkanHarness *vulkan) {
  constexpr const char *name = "CodegenMadModes";
  using O = ShaderOpcode;
  std::vector<std::vector<u32>> inputs(3);
  uint64_t seed = 0x3141592653589793ull;
  while (inputs[0].size() < 16384u) {
    for (auto &input : inputs) {
      input.push_back(RandomModerateF32(seed));
    }
  }
  static constexpr u32 MadmkConstant = 0x3fa66666u; // 1.3f
  const auto bulk = MakeBulkCase(
      name, 3, 6, inputs,
      [](std::vector<u32> *code) {
        AppendVop3(code, 0x141, 10, Vgpr(2), Vgpr(3), Vgpr(4)); // v_mad_f32
        code->push_back(EncodeVop1(0x01, 11, Vgpr(4)));         // v_mov_b32 v11, v4
        code->push_back(EncodeVop2(0x1f, 11, Vgpr(2), 3));      // v_mac_f32 v11, v2, v3
        code->push_back(EncodeVop2(0x20, 12, Vgpr(2), 4));      // v_madmk_f32 v12, v2, K, v4
        code->push_back(MadmkConstant);
        code->push_back(EncodeVop2(0x21, 13, Vgpr(2), 3));      // v_madak_f32 v13, v2, v3, K
        code->push_back(MadmkConstant);
        AppendVop3(code, 0x14b, 14, Vgpr(2), Vgpr(3), Vgpr(4)); // v_fma_f32
        code->push_back(EncodeVop1(0x01, 15, Vgpr(4)));         // v_mov_b32 v15, v4
        code->push_back(EncodeVop2(0x2b, 15, Vgpr(2), 3));      // v_fmac_f32 v15, v2, v3
      },
      {O::V_MAD_F32, O::V_MOV_B32, O::V_MAC_F32, O::V_MADMK_F32, O::V_MADAK_F32,
       O::V_FMA_F32});
  const auto k = std::bit_cast<float>(MadmkConstant);
  const auto reference = [&](u32 output, u32 e, bool fused) -> u32 {
    const auto a = std::bit_cast<float>(inputs[0][e]);
    const auto b = std::bit_cast<float>(inputs[1][e]);
    const auto c = std::bit_cast<float>(inputs[2][e]);
    const auto mad = [&](float x, float y, float z) {
      return fused ? std::fma(x, y, z) : RoundedF32(RoundedF32(x * y) + z);
    };
    switch (output) {
    case 0:
    case 1: return std::bit_cast<u32>(mad(a, b, c));
    case 2: return std::bit_cast<u32>(mad(a, k, c));
    case 3: return std::bit_cast<u32>(mad(a, b, k));
    default: return std::bit_cast<u32>(std::fma(a, b, c));
    }
  };
  static const char *const names[] = {"mad", "mac", "madmk", "madak", "fma", "fmac"};
  size_t differing = 0;
  for (const auto mode : {ShaderRecompiler::MadMode::Exact, ShaderRecompiler::MadMode::Fused,
                          ShaderRecompiler::MadMode::Position}) {
    CodegenOptions options = GetCodegenOptions();
    options.mad_mode = mode;
    const auto run = RunBulk(vulkan, bulk, options);
    const bool fused = mode != ShaderRecompiler::MadMode::Exact;
    for (u32 o = 0; o < bulk.outputs; o++) {
      const auto out = BulkOutput(bulk, run.words, o);
      for (u32 e = 0; e < bulk.count; e++) {
        const auto expected = reference(o, e, fused);
        if (out[e] != expected) {
          Fail(name, names[o],
               "mode " + std::to_string(static_cast<int>(mode)) + " element " +
                   std::to_string(e) + ": got " + Hex(out[e]) + ", expected " +
                   Hex(expected));
        }
        if (o == 0 && !fused && expected != reference(o, e, true)) {
          differing++;
        }
      }
    }
  }
  std::printf("[codegen] %-32s ok (%u elements, %zu where fused and unfused differ)\n", name,
              bulk.count, differing);
}

// A vertex shader with one MAD feeding the position and one feeding a parameter: which of them
// are fused, which carry NoContraction, and the Invariant position, per MadMode.
void CheckMadPositionSlice() {
  constexpr const char *name = "CodegenMadPositionSlice";
  std::vector<u32> code;
  AppendVop3(&code, 0x141, 4, Vgpr(0), Vgpr(1), Vgpr(2)); // v_mad_f32 v4 (position)
  AppendVop3(&code, 0x141, 5, Vgpr(0), Vgpr(2), Vgpr(1)); // v_mad_f32 v5 (parameter)
  AppendVMovLiteral(&code, 6, 0x3f800000u);
  code.push_back(EncodeExp0(12, 0xf, true)); // exp pos0 v4, v4, v4, v6 done
  code.push_back(EncodeExp1(4, 4, 4, 6));
  code.push_back(EncodeExp0(32, 0xf, false)); // exp param0 v5, v5, v5, v6
  code.push_back(EncodeExp1(5, 5, 5, 6));
  AppendEnd(&code);
  const auto compile = [&](ShaderRecompiler::MadMode mode) {
    CodegenOptions options = GetCodegenOptions();
    options.mad_mode = mode;
    ScopedCodegenOptions scope(options);
    std::array<u32, 16> user_data{};
    ShaderVertexInputInfo vertex;
    ShaderRecompiler::CompileOptions compile_options;
    compile_options.stage = ShaderType::Vertex;
    compile_options.user_data = user_data;
    compile_options.input_info.vertex = &vertex;
    auto translated = ShaderRecompiler::TranslateProgram(code, compile_options);
    auto plan = ShaderRecompiler::IR::ExtractResourcePlan(translated.program);
    ShaderRecompiler::IR::ResourceSnapshot resources;
    ShaderRecompiler::IR::ResourceSpecialization specialization;
    const ShaderRecompiler::IR::SrtRuntime runtime{
        .user_data = compile_options.user_data,
        .shader_base = reinterpret_cast<uint64_t>(code.data()),
    };
    Require(name, "resource materialization",
            ShaderRecompiler::IR::MaterializeResources(plan, runtime, resources,
                                                       specialization),
            "vertex resources could not be materialized");
    auto result = ShaderRecompiler::CompileProgram(std::move(translated), compile_options,
                                                   specialization);
    ValidateSpirv(name, result.spirv);
    return result.spirv;
  };
  struct Expectation {
    ShaderRecompiler::MadMode mode;
    size_t fma;
    size_t no_contraction;
    size_t invariant;
  };
  for (const auto &expect : {Expectation{ShaderRecompiler::MadMode::Exact, 0, 4, 1},
                             Expectation{ShaderRecompiler::MadMode::Position, 1, 2, 1},
                             Expectation{ShaderRecompiler::MadMode::Fused, 2, 0, 0}}) {
    const auto spirv = compile(expect.mode);
    const auto fma = CountSpirv(spirv, "Fma ");
    const auto no_contraction = CountSpirv(spirv, "NoContraction");
    const auto invariant = CountSpirv(spirv, "Invariant");
    Require(name, "decorations",
            fma == expect.fma && no_contraction == expect.no_contraction &&
                invariant == expect.invariant,
            "mode " + std::to_string(static_cast<int>(expect.mode)) + ": Fma " +
                std::to_string(fma) + ", NoContraction " + std::to_string(no_contraction) +
                ", Invariant " + std::to_string(invariant));
  }
  std::printf("[codegen] %-32s ok\n", name);
}

std::vector<u32> CompilePixelShader(const char *name, const std::vector<u32> &code,
                                    const ShaderPixelInputInfo &pixel_info) {
  auto pixel = pixel_info;
  // CompileOptions::user_data is a span: keep its backing array alive for the whole
  // translation, materialization and compilation below.
  const auto user_data = MakeNativeUserData(nullptr);
  ShaderRecompiler::CompileOptions options;
  options.stage = ShaderType::Pixel;
  options.input_info.pixel = &pixel;
  options.user_data = user_data;
  auto translated = ShaderRecompiler::TranslateProgram(code, options);
  auto plan = ShaderRecompiler::IR::ExtractResourcePlan(translated.program);
  ShaderRecompiler::IR::ResourceSnapshot resources;
  ShaderRecompiler::IR::ResourceSpecialization specialization;
  const ShaderRecompiler::IR::SrtRuntime runtime{
      .user_data = options.user_data,
      .shader_base = reinterpret_cast<uint64_t>(code.data()),
  };
  Require(name, "resource materialization",
          ShaderRecompiler::IR::MaterializeResources(plan, runtime, resources, specialization),
          "pixel resources could not be materialized");
  auto result =
      ShaderRecompiler::CompileProgram(std::move(translated), options, specialization);
  ValidateSpirv(name, result.spirv);
  return result.spirv;
}

// V_INTERP_P2 reads through different hardware I/J pairs: a perspective-center input, a
// linear-center input and an input read at the centroid and at the center. KYTY_INTERP_MODES
// decorates per input; the legacy path made every input NoPerspective because LINEAR_CENTER is
// enabled and ignored the centroid pair.
void CheckInterpolationModes() {
  constexpr const char *name = "CodegenInterpolationModes";
  std::vector<u32> code;
  code.push_back(EncodeVintrp(0, 10, 0, 0, 0)); // v_interp_p1_f32 v10, v0, attr0.x
  code.push_back(EncodeVintrp(1, 10, 0, 0, 1)); // v_interp_p2_f32 v10, v1, attr0.x
  code.push_back(EncodeVintrp(0, 11, 1, 0, 4)); // linear center pair v4/v5
  code.push_back(EncodeVintrp(1, 11, 1, 0, 5));
  code.push_back(EncodeVintrp(0, 12, 2, 0, 2)); // perspective centroid pair v2/v3
  code.push_back(EncodeVintrp(1, 12, 2, 0, 3));
  code.push_back(EncodeVintrp(0, 13, 2, 1, 0)); // attr2.y at the center
  code.push_back(EncodeVintrp(1, 13, 2, 1, 1));
  code.push_back(EncodeVintrp(0, 14, 3, 0, 2)); // attr3 only at the centroid
  code.push_back(EncodeVintrp(1, 14, 3, 0, 3));
  code.push_back(EncodeVop2(0x03, 13, Vgpr(13), 14)); // v_add_f32 v13, v13, v14
  code.push_back(EncodeExp0(0, 0xf, true, false, true));
  code.push_back(EncodeExp1(10, 11, 12, 13));
  AppendEnd(&code);
  ShaderPixelInputInfo pixel{};
  pixel.input_num = 4;
  for (u32 i = 0; i < std::size(pixel.interpolator_settings); i++) {
    pixel.interpolator_settings[i] = i;
  }
  pixel.ps_perspective_center_vgpr = 0;
  pixel.ps_perspective_centroid_vgpr = 2;
  pixel.ps_linear_center_vgpr = 4;
  pixel.ps_no_perspective = true;
  pixel.ps_system_input_base = 6;
  struct Counts {
    size_t no_perspective, centroid, interpolate_at_centroid;
  };
  const auto compile = [&](bool modes) {
    CodegenOptions options = GetCodegenOptions();
    options.interp_modes = modes;
    ScopedCodegenOptions scope(options);
    const auto spirv = CompilePixelShader(name, code, pixel);
    return Counts{CountSpirv(spirv, " NoPerspective"), CountSpirv(spirv, " Centroid\n"),
                  CountSpirv(spirv, "InterpolateAtCentroid")};
  };
  const auto modes = compile(true);
  const auto legacy = compile(false);
  Require(name, "per-input interpolation",
          modes.no_perspective == 1u && modes.centroid == 1u &&
              modes.interpolate_at_centroid == 1u,
          "modes: NoPerspective " + std::to_string(modes.no_perspective) + ", Centroid " +
              std::to_string(modes.centroid) + ", InterpolateAtCentroid " +
              std::to_string(modes.interpolate_at_centroid));
  Require(name, "legacy interpolation",
          legacy.no_perspective == 4u && legacy.centroid == 0u &&
              legacy.interpolate_at_centroid == 0u,
          "legacy: NoPerspective " + std::to_string(legacy.no_perspective) + ", Centroid " +
              std::to_string(legacy.centroid) + ", InterpolateAtCentroid " +
              std::to_string(legacy.interpolate_at_centroid));
  std::printf("[codegen] %-32s ok\n", name);
}

void RunAll(VulkanHarness *vulkan) {
  CheckInterpolationModes();
  CheckMadPositionSlice();
  CheckMadModes(vulkan);
  CheckLodStatsGate();
  CheckLodStatsPlainVariant();
  CheckMoveRelValueSet(vulkan);
  CheckFloatMinMax(vulkan);
  CheckFloatClampConstants(vulkan);
  CheckPackRtzExhaustive();
  CheckPackRtz(vulkan);
  CheckFloatToIntSaturation(vulkan);
  CheckRobustBufferLoads(vulkan);
}

} // namespace CodegenTests
