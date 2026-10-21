#include "../../include/arm-fp.h"
	.file	"asm-Linux-arm.S"
	.text
	.arm

/*
 * ulong umult(ulong m1, ulong m2, ulong *hi)
 */

        .align  2
        .global umult
        .type   umult, %function
umult:
        @ args = 0, pretend = 0, frame = 12
        @ frame_needed = 1, uses_anonymous_args = 0
        mov     ip, sp
        stmfd   sp!, {fp, ip, lr, pc}
        sub     fp, ip, #4
        sub     sp, sp, #12
        str     r0, [fp, #-16]
        str     r1, [fp, #-20]
        str     r2, [fp, #-24]
        ldr     r1, [fp, #-16]
        ldr     r2, [fp, #-20]
        umull   r0, r3, r1, r2
        ldr     r1, [fp, #-24]
        str     r3, [r1]
        ldmea   fp, {fp, sp, pc}
        .size   umult, .-umult

/* Dis switches between C/JIT calls: FP values obey the C ABI, but FPSCR
 * belongs to the Dis thread. The first aligned FPU word holds the whole
 * register, including rounding, exception enables/status and comparison flags. */
#if ARM_FPSCR_VFP
	.fpu vfp
#endif
	.syntax unified
#ifdef __thumb__
	.thumb
#define FPFN(x) .type x,%function; .global x; .thumb_func; x
#else
#define FPFN(x) .type x,%function; .global x; x
#endif

FPFN(FPsave):
#if ARM_FPSCR_VFP
	vmrs r1, fpscr
#else
	mov r1, #0
#endif
	str r1, [r0]
	bx lr

FPFN(FPrestore):
#if ARM_FPSCR_VFP
	ldr r1, [r0]
	vmsr fpscr, r1
#endif
	bx lr
