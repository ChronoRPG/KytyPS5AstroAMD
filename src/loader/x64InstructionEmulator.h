#ifndef KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_
#define KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_

#include <cstdint>

namespace Loader::X64InstructionEmulator {

uint64_t           PatchReciprocalSquareRoots(uint64_t address, uint64_t size);
[[nodiscard]] bool TryEmulate(void* native_context);

} // namespace Loader::X64InstructionEmulator

#endif /* KYTY_LOADER_X64_INSTRUCTION_EMULATOR_H_ */
