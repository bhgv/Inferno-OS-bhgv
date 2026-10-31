#include "lib9.h"

#define NativeIEEEFlags 0x3du // x87/SSE IEEE flags; the denormal-operand bit is not part of Math's API.


/* Replace only represented controls; unrelated native denormal and exception fields survive. */
void
setfcr(ulong value)
{
#if defined(__x86_64__)
	uint environment[7], simd;

	/* FNSTENV masks pending x87 traps before the waiting environment-load instruction. */
	__asm__ volatile("fnstenv %0" : "=m"(environment));
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	environment[0] = (environment[0] & ~(NativeIEEEFlags|FPRMASK|FPPMASK)) |
		((~value) & NativeIEEEFlags) | (value & (FPRMASK|FPPMASK));
	simd = (simd & ~((NativeIEEEFlags << 7)|0x6000u)) |
		(((~value) & NativeIEEEFlags) << 7) | ((value & FPRMASK) << 3);
	__asm__ volatile("ldmxcsr %0" : : "m"(simd) : "memory");
	__asm__ volatile("fldenv %0" : : "m"(environment) : "memory");
#elif defined(__aarch64__)
	ulong control;

	__asm__ volatile("mrs %0, fpcr" : "=r"(control));
	control = (control & ~(ulong)(FPRMASK|0x1f00u)) | (value & (FPRMASK|0x1f00u));
	__asm__ volatile("msr fpcr, %0\n\tisb" : : "r"(control) : "memory");
#elif defined(__riscv) && __riscv_xlen == 64
	ulong control;

	/* RMM/reserved modes are not values of Math's four-mode rounding API. */
	if((value & FPRMASK) > (3u << 5))
		return;
	__asm__ volatile("frcsr %0" : "=r"(control));
	control = (control & ~(ulong)FPRMASK) | (value & FPRMASK);
	__asm__ volatile("fscsr %0" : : "r"(control) : "memory");
#else
#error unsupported native64 FP control target
#endif
}


/* Read actual hardware controls; no software shadow pretends unsupported RV traps exist. */
ulong
getfcr(void)
{
#if defined(__x86_64__)
	unsigned short control;
	uint simd;

	__asm__ volatile("fnstcw %0" : "=m"(control));
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	return ((~(simd >> 7)) & NativeIEEEFlags) |
		((simd >> 3) & FPRMASK) | (control & FPPMASK);
#elif defined(__aarch64__)
	ulong control;

	__asm__ volatile("mrs %0, fpcr" : "=r"(control));
	return control & (FPRMASK|0x1f00u);
#else
	ulong control;

	__asm__ volatile("frcsr %0" : "=r"(control));
	return control & FPRMASK;
#endif
}


/* Observe IEEE sticky status without conflating it with trap-enable controls. */
ulong
getfsr(void)
{
#if defined(__x86_64__)
	unsigned short status;
	uint simd;

	__asm__ volatile("fnstsw %0" : "=am"(status));
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	return (status|simd) & NativeIEEEFlags;
#elif defined(__aarch64__)
	ulong status;

	__asm__ volatile("mrs %0, fpsr" : "=r"(status));
	return status & 0x1fu;
#else
	ulong status;

	__asm__ volatile("frcsr %0" : "=r"(status));
	return status & 0x1fu;
#endif
}


/* Update represented sticky flags without changing rounding, enables or other native status. */
void
setfsr(ulong value)
{
#if defined(__x86_64__)
	uint environment[7], simd;

	__asm__ volatile("fnstenv %0" : "=m"(environment));
	environment[1] = (environment[1] & ~NativeIEEEFlags) | (value & NativeIEEEFlags);
	__asm__ volatile("fldenv %0" : : "m"(environment) : "memory");
	__asm__ volatile("stmxcsr %0" : "=m"(simd));
	simd = (simd & ~NativeIEEEFlags) | (value & NativeIEEEFlags);
	__asm__ volatile("ldmxcsr %0" : : "m"(simd) : "memory");
#elif defined(__aarch64__)
	ulong status;

	__asm__ volatile("mrs %0, fpsr" : "=r"(status));
	status = (status & ~(ulong)0x1fu) | (value & 0x1fu);
	__asm__ volatile("msr fpsr, %0" : : "r"(status) : "memory");
#else
	ulong status;

	__asm__ volatile("frcsr %0" : "=r"(status));
	status = (status & ~(ulong)0x1fu) | (value & 0x1fu);
	__asm__ volatile("fscsr %0" : : "r"(status) : "memory");
#endif
}
