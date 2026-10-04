#ifndef KYTY_COMMON_ABI_H_
#define KYTY_COMMON_ABI_H_

#include "common/common.h"

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define KYTY_MS_ABI __attribute__((ms_abi))

// Guest code can call a SysV host entry with RSP misaligned. On Windows the entry realigns the
// stack, so the Windows x64 calls it makes (16-byte aligned spills) do not fault.
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS && (defined(__x86_64__) || defined(_M_X64))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define KYTY_SYSV_ABI __attribute__((sysv_abi, force_align_arg_pointer))
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define KYTY_SYSV_ABI __attribute__((sysv_abi))
#endif

#endif /* KYTY_COMMON_ABI_H_ */
