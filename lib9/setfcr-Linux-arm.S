#include "../include/arm-fp.h"
#ifdef __thumb__
#define FN(x) .type x,%function; .global x; .thumb_func; x
#else
#define FN(x) .type x,%function; .global x; x
#endif
#define RET bx lr

	.syntax unified
	.text
#ifdef __thumb__
	.thumb
#else
	.arm
#endif
#if ARM_FPSCR_VFP
	.fpu vfp
#endif

/* Update only the fields represented by Math.FPcontrol. */
FN(setfcr):
#if ARM_FPSCR_VFP
	vmrs r1, fpscr
	bic r1, r1, #ARM_FPSCR_ROUND_MASK
	bic r1, r1, #ARM_FPSCR_ENABLE_MASK
	and r2, r0, #ARM_FPSCR_ROUND_MASK
	and r0, r0, #ARM_FPSCR_ENABLE_MASK
	orr r0, r0, r2
	orr r0, r0, r1
	vmsr fpscr, r0
#endif
	RET

FN(getfcr):
#if ARM_FPSCR_VFP
	vmrs r0, fpscr
	and r1, r0, #ARM_FPSCR_ROUND_MASK
	and r0, r0, #ARM_FPSCR_ENABLE_MASK
	orr r0, r0, r1
#else
	mov r0, #0
#endif
	RET

FN(getfsr):
#if ARM_FPSCR_VFP
	vmrs r0, fpscr
	and r0, r0, #ARM_FPSCR_STATUS_MASK
#else
	mov r0, #0
#endif
	RET

/* Do not erase rounding/traps or the VFP comparison flags when clearing
 * accumulated exceptions. Unrepresented native status bits are preserved. */
FN(setfsr):
#if ARM_FPSCR_VFP
	vmrs r1, fpscr
	bic r1, r1, #ARM_FPSCR_STATUS_MASK
	and r0, r0, #ARM_FPSCR_STATUS_MASK
	orr r0, r0, r1
	vmsr fpscr, r0
#endif
	RET
