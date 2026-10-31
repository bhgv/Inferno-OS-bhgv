#ifndef INFERNO_HOST64_FP_H
#define INFERNO_HOST64_FP_H // Override shared declaration headers with the selected CPU's real FP encoding.
#if defined(__x86_64__) || defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
#undef FPINEX
#undef FPUNFL
#undef FPOVFL
#undef FPZDIV
#undef FPINVAL
#undef FPRNR
#undef FPRZ
#undef FPRPINF
#undef FPRNINF
#undef FPRMASK
#undef FPPEXT
#undef FPPSGL
#undef FPPDBL
#undef FPPMASK
#undef FPAINEX
#undef FPAOVFL
#undef FPAUNFL
#undef FPAZDIV
#undef FPAINVAL
#if defined(__x86_64__)
#define FPPEXT (3u << 8) // x87 extended-precision selector.
#define FPPSGL (0u << 8) // x87 single-precision selector.
#define FPPDBL (2u << 8) // x87 double-precision selector.
#define FPPMASK (3u << 8) // SSE operand precision remains instruction-specific.
#define FPINEX (1u << 5) // Logical enable bit, inverse of the native x87 mask.
#define FPUNFL (1u << 4) // IEEE underflow; do not conflate denormal operands with underflow.
#define FPOVFL (1u << 3) // IEEE overflow.
#define FPZDIV (1u << 2) // IEEE divide by zero.
#define FPINVAL (1u << 0) // IEEE invalid operation.
#define FPRNR (0u << 10) // Shared x87-style public rounding representation.
#define FPRZ (3u << 10) // Round toward zero.
#define FPRPINF (2u << 10) // Round toward positive infinity.
#define FPRNINF (1u << 10) // Round toward negative infinity.
#define FPRMASK (3u << 10) // The setter also updates the corresponding SSE field.
#define FPAINEX FPINEX // Sticky status uses the corresponding native flag bit.
#define FPAOVFL FPOVFL // Sticky overflow.
#define FPAUNFL FPUNFL // Sticky underflow, independent of denormal operand status.
#define FPAZDIV FPZDIV // Sticky divide by zero.
#define FPAINVAL FPINVAL // Sticky invalid operation.
#else
#define FPPEXT 0 // Instruction operands determine precision on these CPUs.
#define FPPSGL 0 // There is no x87-style global single-precision control.
#define FPPDBL 0 // There is no x87-style global double-precision control.
#define FPPMASK 0 // No precision-control field is writable.
#if defined(__aarch64__)
#define FPINEX (1u << 12) // FPCR inexact exception enable, when implemented by hardware.
#define FPUNFL (1u << 11) // FPCR underflow exception enable.
#define FPOVFL (1u << 10) // FPCR overflow exception enable.
#define FPZDIV (1u << 9) // FPCR divide-by-zero exception enable.
#define FPINVAL (1u << 8) // FPCR invalid-operation exception enable.
#define FPRNR (0u << 22) // Round to nearest, ties to even.
#define FPRZ (3u << 22) // Round toward zero.
#define FPRPINF (1u << 22) // Round toward positive infinity.
#define FPRNINF (2u << 22) // Round toward negative infinity.
#define FPRMASK (3u << 22) // FPCR rounding-mode field.
#define FPAINEX (1u << 4) // FPSR inexact sticky status.
#define FPAOVFL (1u << 2) // FPSR overflow sticky status.
#define FPAUNFL (1u << 3) // FPSR underflow sticky status.
#define FPAZDIV (1u << 1) // FPSR divide-by-zero sticky status.
#define FPAINVAL (1u << 0) // FPSR invalid-operation sticky status.
#else
#define FPINEX 0 // RISC-V has no hardware floating-point exception-enable field.
#define FPUNFL 0 // Unsupported trap enables must never be reported as active.
#define FPOVFL 0 // Sticky status remains available independently of traps.
#define FPZDIV 0 // Division exceptions do not trap through FCSR.
#define FPINVAL 0 // Invalid operations report sticky NV, not a trap enable.
#define FPRNR (0u << 5) // FRM RNE: round to nearest, ties to even.
#define FPRZ (1u << 5) // FRM RTZ: round toward zero.
#define FPRPINF (3u << 5) // FRM RUP: round toward positive infinity.
#define FPRNINF (2u << 5) // FRM RDN: round toward negative infinity.
#define FPRMASK (7u << 5) // FCSR FRM field; the public API selects four IEEE modes.
#define FPAINEX (1u << 0) // FFLAGS NX: inexact.
#define FPAOVFL (1u << 2) // FFLAGS OF: overflow.
#define FPAUNFL (1u << 1) // FFLAGS UF: underflow.
#define FPAZDIV (1u << 3) // FFLAGS DZ: divide by zero.
#define FPAINVAL (1u << 4) // FFLAGS NV: invalid operation.
#endif
#endif
#endif
#endif
