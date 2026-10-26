# RV64 string and scalar-list address spans

`IINDC` and `ILENC` now decline owners in the final 16 GiB of the native
address space before loading string metadata. Below that boundary, every
int-sized Rune index plus every supported inline field offset fits without
unsigned address wrap. Null remains a fallback; H remains an empty string for
`ILENC` and a fallback for `IINDC`. Rejection does not write VM memory.

`IINDC` also rejects layouts whose inline payload overlaps the length field.
This validation happens before publishing code. `HEADB/HEADW/HEADL/HEADF` check both the owner-to-payload addition and
the payload-end addition before reading. No RvLayout fields or opcode IDs changed. The conservative address window leaves precise handling of
upper-window owners to C; it does not claim arbitrary pointers are readable.

Run from the repository root with Unicorn installed:

    PYTHONOPTIMIZE=1 python3 libinterp/tests/riscv-string-window.py
    python3 libinterp/tests/riscv-jit.py
    python3 libinterp/tests/riscv-runtime.py
    python3 libinterp/tests/riscv-features.py
    python3 libinterp/tests/jit-coverage.py

The new fixture checks 50,145 cases across WORD32 and WORD64: six opcodes,
ASCII and signed Rune values, negative and oversized indexes, INT_MIN length,
nil and upper-window owners, three inline layouts, all offsets from -8 through
2056, every insufficient emitter capacity, and destination aliases with either
input slot. Scalar-list cases include both offset wrap and access-end wrap. It compares complete VM/register/string images and watches writes
so even same-value stores on fallback fail. All checks remain active with
optimized Python. `EMITTER_DIR` can select another riscv-emit.h for regression
comparison. The parent emitter fails layout rejection; after applying only the
layout validation to that parent, it still faults on an upper-window read.

These are instruction-emulation and host compiler checks. Hardware RV64
acceptance is separate. Native opcode availability stays at 103/173 for M,
90/173 for Zmmul and 87/173 for RV64I; AMD64 and ARM64 stay at 142/173 each.
