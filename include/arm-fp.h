#ifndef INFERNO_ARM_FP_H
#define INFERNO_ARM_FP_H

/* Keep C, lib9 assembly and emulator context assembly on the same target.
 * USEVFP is retained for older toolchains; an explicit zero disables VFP. */
#ifndef ARM_FPSCR_VFP
#ifdef USEVFP
#define ARM_FPSCR_VFP USEVFP
#elif defined(__ARM_FP) && (__ARM_FP != 0)
#define ARM_FPSCR_VFP 1
#else
#define ARM_FPSCR_VFP 0
#endif
#endif

/* Math exposes five IEEE exceptions and rounding, not IDC/IDE, NZCV, QC,
 * FZ, DN, AHP or deprecated vector controls. Preserve those other fields. */
#define ARM_FPSCR_ENABLE_MASK 0x00001f00
#define ARM_FPSCR_ROUND_MASK 0x00c00000
#define ARM_FPSCR_STATUS_MASK 0x0000001f

#endif
