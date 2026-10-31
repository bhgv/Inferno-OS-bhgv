#ifndef INFERNO_HOST64_FPU_H
#define INFERNO_HOST64_FPU_H // Native environment layout shared by hosted assembly and its callers.

#include "native64-target.h"
#if defined(__riscv) && (!defined(__riscv_flen) || __riscv_flen < 64 || !defined(__riscv_float_abi_double))
#error hosted RV64 FPU contexts require the double-float ISA and ABI
#endif

/* C ABI calls preserve live register values; this context owns the thread's FP environment. */
typedef struct FPU FPU;

struct FPU
{
#if defined(__x86_64__)
	uint x87[7]; // Complete 28-byte x87 environment; control and sticky status are both retained.
	uint mxcsr; // SSE control, rounding, sticky status and denormal policy.
#elif defined(__aarch64__)
	uint fpcr; // AArch64 control register, including rounding and exception enables.
	uint fpsr; // AArch64 sticky exceptions and cumulative status.
#elif defined(__riscv) && __riscv_xlen == 64
	uint fcsr; // RV64 floating-point rounding mode and sticky exception flags.
#else
#error unsupported hosted 64-bit FPU architecture
#endif
};
#endif
