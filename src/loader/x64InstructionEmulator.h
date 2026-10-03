#ifndef KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_
#define KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_

#include <Zydis/DecoderTypes.h>
#include <cstdint>

namespace Loader::X64InstructionEmulator {

[[nodiscard]] bool IsReciprocalSquareRoot(const ZydisDecodedInstruction& instruction,
                                         const ZydisDecodedOperand* operands);
uint64_t           PatchReciprocalSquareRoots(uint64_t address, uint64_t size);
[[nodiscard]] bool TryEmulate(void* native_context);

// The "AMD CPU patch" (--amd-cpu, KYTY_AMD_CPU): patched VRSQRTPS executions emulated so far and
// the time spent emulating them inside the handler (the trap's dispatch comes on top).
struct ReciprocalSqrtStats {
	uint64_t traps      = 0;
	uint64_t emulate_ns = 0;
};
[[nodiscard]] ReciprocalSqrtStats GetReciprocalSqrtStats();

} // namespace Loader::X64InstructionEmulator

#endif /* KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_ */
