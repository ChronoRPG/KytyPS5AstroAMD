void TestRayTracingDispatchDetection() {
  namespace D = ShaderRecompiler::Decoder;
  const auto check_decoded = [](std::span<const uint32_t> code, bool expected,
                                const char *message) {
    D::Program decoded;
    D::DecodeProgram(code, decoded);
    Check(decoded.has_bvh == expected, message);
    if (expected) {
      const auto &last = decoded.instructions.back();
      Check(last.family == D::Family::MIMG &&
                (last.opcode_id == 0xe6u || last.opcode_id == 0xe7u),
            "decoder continued past the first BVH instruction");
      const auto result = ShaderRecompiler::TranslateProgram(
          code, MakeCompileOptions(ShaderType::Compute));
      Check(result.skip_dispatch && result.program.blocks.empty() && result.cfg_dump.empty(),
            "BVH compute shader reached IR translation");
    }
  };
  const auto check = [&](std::initializer_list<uint32_t> code, bool expected,
                         const char *message) {
    check_decoded(std::span<const uint32_t>{code.begin(), code.size()}, expected, message);
  };

  check({0xf1989f01u, 0x00081014u, EncodeSopp(0x01)}, true,
        "captured contiguous BVH instruction did not skip dispatch");
  check({0xf1989f07u, 0x00071001u, 0x3d3e3c15u, 0x38095851u, 0x00003a39u,
         EncodeSopp(0x01)}, true,
        "captured NSA BVH instruction did not skip dispatch");
  check({0xf19c9f01u, 0x00081014u, EncodeSopp(0x01)}, true,
        "RDNA2 BVH64 instruction did not skip dispatch");
  check({0xbe8e210eu, 0xf1989f01u, 0x00081014u, EncodeSopp(0x01)}, true,
        "unsupported SWAPPC before BVH prevented dispatch detection");
  check({0xf1989f01u, 0x00081014u, 0xc0000000u, EncodeSopp(0x01)}, true,
        "unknown instruction after BVH prevented dispatch skipping");

  for (uint32_t opcode : {0xe6u, 0xe7u}) {
    for (uint32_t nsa = 0; nsa < 4; ++nsa) {
      for (uint32_t slc = 0; slc < 2; ++slc) {
        std::array<uint32_t, 6> code{
            EncodeMimg0(opcode, 0xf, true) | (nsa << 1u) | (slc << 25u),
            EncodeMimg1(0, 0, 0, 0)};
        code[2 + nsa] = EncodeSopp(0x01);
        check_decoded(std::span(code).first(3 + nsa), true,
                      "BVH cache flags or NSA length prevented dispatch detection");
      }
    }
  }
  for (uint32_t opcode : {0x66u, 0x67u, 0xe4u, 0xe5u, 0xe8u, 0xe9u}) {
    check({EncodeMimg0(opcode, 0xf), EncodeMimg1(0, 0, 0, 0), EncodeSopp(0x01)},
          false, "non-BVH image opcode was incorrectly skipped");
  }

  check({EncodeSMovB32(0, 255), 0xf1989f01u, EncodeSopp(0x01)}, false,
        "scalar literal was mistaken for a BVH instruction");
  check({EncodeMimg0(0x20, 0xf) | 6u, EncodeMimg1(0, 0, 0, 0),
         0xf1989f01u, 0xf19c9f01u, 0u, EncodeSopp(0x01)}, false,
        "NSA address words were mistaken for BVH instructions");
  check({EncodeMimg0(0x20, 0xf), 0xf1989f01u, EncodeSopp(0x01)}, false,
        "MIMG operand word was mistaken for a BVH instruction");
  check({EncodeSopp(0x01), 0xf1989f01u, 0x00081014u}, false,
        "data after the shader end was mistaken for a BVH instruction");
  check({EncodeMimg0(0x20, 0xf), EncodeMimg1(0, 0, 0, 0), EncodeSopp(0x01)},
        false, "ordinary image shader was incorrectly skipped");

  // An end instruction on one branch does not end another branch's code.
  check({EncodeSopp(0x04, 1), EncodeSopp(0x01), 0xf1989f01u, 0x00081014u,
         EncodeSopp(0x01)}, true,
        "BVH at a branch target after an early end was not detected");

  const std::array bvh_code{0xf1989f01u, 0x00081014u, 0xf19c9f01u, 0x00081014u,
                            EncodeSopp(0x01)};
  const std::array ordinary_code{EncodeSMovB32(0, 255), 0xf1989f01u, EncodeSopp(0x01)};
  D::Program reused;
  D::DecodeProgram(bvh_code, reused);
  Check(reused.has_bvh && reused.instructions.size() == 1,
        "decoder did not stop at the first BVH instruction");
  D::DecodeProgram(ordinary_code, reused);
  Check(!reused.has_bvh, "decoder retained stale BVH metadata");
  for (const auto stage : {ShaderType::Compute, ShaderType::Vertex, ShaderType::Pixel}) {
    const auto options = MakeCompileOptions(stage);
    Check(!ShaderRecompiler::TranslateProgram(ordinary_code, options).skip_dispatch,
          "BVH-shaped literal marked an ordinary shader for skipping");
    const auto compiled = RecompileForTest(ordinary_code, options);
    Check(!compiled.spirv.empty(), "ordinary shader did not compile");
    CheckSpirvBinaryValidates(compiled.spirv);
  }
}

class ScopedRtStub {
public:
  explicit ScopedRtStub(bool enabled)
      : m_saved(ShaderRecompiler::GetCodegenOptions()) {
    auto options = m_saved;
    options.rt_stub = enabled;
    ShaderRecompiler::SetCodegenOptions(options);
  }
  ~ScopedRtStub() { ShaderRecompiler::SetCodegenOptions(m_saved); }
  ScopedRtStub(const ScopedRtStub &) = delete;
  ScopedRtStub &operator=(const ScopedRtStub &) = delete;

private:
  ShaderRecompiler::CodegenOptions m_saved;
};

void TestRayTracingStub() {
  namespace D = ShaderRecompiler::Decoder;
  using O = D::Opcode;
  const auto decode = [](std::initializer_list<uint32_t> words) {
    const std::vector<uint32_t> code(words);
    D::Instruction inst;
    D::DecodeInstruction(code, 0, inst);
    return inst;
  };
  const auto a16 = [](const D::Instruction &inst) {
    return (inst.image_sample_flags & D::ImageSampleFlagA16) != 0u;
  };

  // Captured contiguous form: image_bvh_intersect_ray v[16:19], v[20:30], s[32:35].
  const auto contiguous = decode({0xf1989f01u, 0x00081014u});
  Check(contiguous.opcode == O::IMAGE_BVH_INTERSECT_RAY && contiguous.word_count == 2 &&
            contiguous.image_address_components == 11 && contiguous.data_dwords == 4 &&
            contiguous.dmask == 0xfu && contiguous.image_r128 && !a16(contiguous) &&
            contiguous.dst.kind == D::OperandKind::Vgpr && contiguous.dst.reg == 16 &&
            contiguous.src0.kind == D::OperandKind::Vgpr && contiguous.src0.reg == 20 &&
            contiguous.src1.kind == D::OperandKind::Sgpr && contiguous.src1.reg == 32,
        "captured contiguous BVH instruction decoded wrongly");
  Check(D::InstructionToString(contiguous).find("IMAGE_BVH_INTERSECT_RAY") !=
            std::string::npos,
        "BVH instruction text lost its opcode name");

  // Captured NSA form: node pointer in v1, then v21, v60, v62, v61, v81, v88, v9, v56-v58.
  const auto nsa = decode({0xf1989f07u, 0x00071001u, 0x3d3e3c15u, 0x38095851u, 0x00003a39u});
  constexpr uint32_t nsa_regs[] = {21, 60, 62, 61, 81, 88, 9, 56, 57, 58};
  bool nsa_ok = nsa.opcode == O::IMAGE_BVH_INTERSECT_RAY && nsa.word_count == 5 &&
                nsa.image_nsa_dwords == 3 && nsa.image_address_components == 11 &&
                nsa.src0.reg == 1 && nsa.dst.reg == 16 && nsa.src1.reg == 28;
  for (uint32_t i = 0; i < std::size(nsa_regs); ++i) {
    nsa_ok = nsa_ok && nsa.image_nsa_addr[i] == nsa_regs[i];
  }
  Check(nsa_ok, "captured NSA BVH instruction decoded wrongly");

  // Address dword counts from RDNA2 ISA Table 48.
  const auto bvh64 = decode({0xf19c9f01u, 0x00081014u});
  const auto a16_32 = decode({0xf1989f01u, 0x40081014u});
  const auto a16_64 = decode({0xf19c9f01u, 0x40081014u});
  Check(bvh64.opcode == O::IMAGE_BVH64_INTERSECT_RAY && bvh64.image_address_components == 12 &&
            a16_32.opcode == O::IMAGE_BVH_INTERSECT_RAY && a16(a16_32) &&
            a16_32.image_address_components == 8 &&
            a16_64.opcode == O::IMAGE_BVH64_INTERSECT_RAY && a16(a16_64) &&
            a16_64.image_address_components == 9,
        "BVH address dword counts do not follow the ISA");

  // Encodings the ISA forbids are rejected rather than guessed.
  for (const auto &[word0, word1, nsa_word, text] :
       {std::tuple{0xf1989701u, 0x00081014u, 0u, "DMASK=0x7"},
        std::tuple{0xf1981f01u, 0x00081014u, 0u, "R128=0"},
        std::tuple{0xf1999f01u, 0x00081014u, 0u, "TFE=1"},
        std::tuple{0xf19a9f01u, 0x00081014u, 0u, "LWE=1"},
        std::tuple{0xf1989f01u, 0x80081014u, 0u, "D16=1"},
        std::tuple{0xf1989f03u, 0x00081014u, 0x04030201u, "one NSA dword"}}) {
    const auto inst = decode({word0, word1, nsa_word});
    if (inst.opcode != O::UNSUPPORTED) {
      std::fprintf(stderr, "BVH encoding with %s was accepted\n", text);
      Check(false, "invalid BVH encoding was accepted");
    }
  }

  // decode_bvh=true decodes through BVH instructions to the end of the shader.
  const std::array code{0xf1989f01u, 0x00081014u, 0xf19c9f01u, 0x00081014u,
                        EncodeSopp(0x01)};
  D::Program program;
  D::DecodeProgram(code, program, true);
  Check(program.has_bvh && program.instructions.size() == 3 &&
            program.instructions[0].opcode == O::IMAGE_BVH_INTERSECT_RAY &&
            program.instructions[1].opcode == O::IMAGE_BVH64_INTERSECT_RAY &&
            program.instructions[2].opcode == O::S_ENDPGM,
        "BVH decode mode did not decode the whole shader");
  D::DecodeProgram(code, program, false);
  Check(program.has_bvh && program.instructions.size() == 1 &&
            program.instructions[0].opcode == O::UNSUPPORTED,
        "default decode did not stop at the first BVH instruction");

  // KYTY_RT_STUB: BVH shaders translate and compile in every stage instead of being skipped.
  const std::array compute_code{0xf1989f01u, 0x00081014u, EncodeSopp(0x01)};
  {
    const ScopedRtStub stub(false);
    Check(ShaderRecompiler::TranslateProgram(compute_code,
                                             MakeCompileOptions(ShaderType::Compute))
              .skip_dispatch,
          "BVH compute shader was not skipped without KYTY_RT_STUB");
  }
  const ScopedRtStub stub(true);
  for (const auto stage : {ShaderType::Compute, ShaderType::Pixel}) {
    const auto options = MakeCompileOptions(stage);
    Check(!ShaderRecompiler::TranslateProgram(compute_code, options).skip_dispatch,
          "KYTY_RT_STUB still skipped a BVH shader");
    const auto compiled = RecompileForTest(compute_code, options);
    Check(!compiled.spirv.empty() && compiled.program.info.uses_bvh,
          "KYTY_RT_STUB program did not compile or lost its BVH flag");
    CheckSpirvBinaryValidates(compiled.spirv);
  }
  const auto ordinary = RecompileForTest(std::array{EncodeSopp(0x01)},
                                         MakeCompileOptions(ShaderType::Compute));
  Check(!ordinary.program.info.uses_bvh, "program without BVH was marked as using BVH");
}

// Development check on real guest code (not part of the default run): each file is a raw
// shader binary (code first; data after its end is ignored). Decodes it with the BVH decoder,
// then translates it as a compute shader with KYTY_RT_STUB. Returns the number of failures.
int TranslateBvhShaderFiles(int argc, char **argv) {
  namespace D = ShaderRecompiler::Decoder;
  const ScopedRtStub stub(true);
  int failures = 0;
  for (int arg = 2; arg < argc; ++arg) {
    std::FILE *file = std::fopen(argv[arg], "rb");
    if (file == nullptr) {
      std::fprintf(stderr, "%s: cannot open\n", argv[arg]);
      failures++;
      continue;
    }
    std::vector<uint32_t> code;
    uint32_t word = 0;
    while (std::fread(&word, sizeof(word), 1, file) == 1) {
      code.push_back(word);
    }
    std::fclose(file);
    D::Program program;
    D::DecodeProgram(code, program, true);
    uint32_t bvh = 0;
    uint32_t bvh64 = 0;
    uint32_t unsupported = 0;
    for (const auto &inst : program.instructions) {
      bvh += inst.opcode == D::Opcode::IMAGE_BVH_INTERSECT_RAY ? 1u : 0u;
      bvh64 += inst.opcode == D::Opcode::IMAGE_BVH64_INTERSECT_RAY ? 1u : 0u;
      if (inst.opcode == D::Opcode::UNSUPPORTED) {
        unsupported++;
        std::fprintf(stderr, "%s: %s\n", argv[arg], D::InstructionToString(inst).c_str());
      }
    }
    const auto translated = ShaderRecompiler::TranslateProgram(
        code, MakeCompileOptions(ShaderType::Compute));
    const bool ok = unsupported == 0 && !translated.skip_dispatch &&
                    !translated.program.blocks.empty();
    std::printf("%s: instructions=%zu bvh=%u bvh64=%u unsupported=%u translated=%s\n",
                argv[arg], program.instructions.size(), bvh, bvh64, unsupported,
                ok ? "yes" : "NO");
    failures += ok ? 0 : 1;
  }
  return failures;
}
