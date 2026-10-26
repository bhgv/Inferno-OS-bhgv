# Checked scalar list and length views on AMD64/ARM64

Six existing native paths now check address geometry before reading:
HEADB, HEADW, HEADL, HEADF, LENL and LENC. Scalar heads validate the complete
owner-to-payload span, LENL validates every next-link span, and LENC validates
the signed length field. A failed guard returns to C without writing VM state.
H still terminates lists and denotes an empty string; null retains its error.

C performs the corresponding checks before pointer arithmetic or reads.
Scalar heads and length fields use memmove after validation, avoiding aligned
structure-member accesses on malformed unaligned owners. These checks detect
address wrap; they do not certify that arbitrary nonwrapping addresses are mapped
or that an owner has a valid heap lifetime.

The native LENL budget also increases from 256 to 1024 nodes on AMD64 and ARM64.
Brent checkpoints retain early rejection of cycles, one next-link read per
visited node and a single final result store. Longer lists still receive their
complete result from C. RV64 retains its earlier 256-node budget. This series preserves the incoming 80-word emitter capacity and string-type
layout field. Native pointers, Dis table formats and opcode IDs are unchanged.

Validation from the repository root:

    PYTHONOPTIMIZE=1 python3 libinterp/tests/scalar-view-spans-jit.py
    python3 libinterp/tests/scalar-view-spans-c.py
    python3 libinterp/tests/list-length-jit.py
    python3 libinterp/tests/list-length-native.py
    LEAF_RUNTIME_BACKEND=arm64 python3 libinterp/tests/leaf64-runtime.py
    python3 libinterp/tests/slice-tokenize-suite.py --core
    python3 libinterp/tests/jit-coverage.py

The new instruction fixture runs 45,288 ARM64 and 45,144 AMD64 cases with UBSan
on the emitter. It covers both WORD widths, all six operations, upper native
addresses, the exact last nonwrapping owner, first rejected owner, inline field
offsets through 32760, nil/H, INT_MIN, signed/real payload bits, every input byte
alignment, slot/payload aliases, complete memory images, exact writes, FP state
and every insufficient code capacity. LENL includes an ordinary first node
leading to an upper-address second node. Checks remain active with optimized
Python. EMITTER_DIR and VIEW_ARCHES select a regression emitter/backend.

The C fixture extracts the actual instructions and changes only span-check
arithmetic to a 16-bit domain for 38,808 boundary cases per WORD width. Payload
pointers retain native width. It separately restores native address arithmetic
and compares actual C against published x64 RX code for 33,600 cases per WORD
width. Both modes run with ASan/UBSan and NDEBUG by default. The native mode
includes errors before inaccessible payload reads, overlapping stores and
malformed second links. VIEW_XEC_SOURCE and VIEW_STRING_SOURCE select older
sources; VIEW_NATIVE_ONLY runs just the real-address/RX cases.

The preceding emitters accept a rejected upper-address scalar head. The
preceding C implementation fails the reduced span/alignment fixture. Expanded
list graph tests cover 1023/1024/1025-node boundaries, nil links, cycles, exact
read budgets, all input/output alignments, FP state and aliased link storage.

Opcode availability stays at 142/173 on both AMD64 and ARM64. These are guarded
native paths, not full operand coverage. ARM64 execution is emulated; physical
target acceptance is separate.

After integrating the incoming pointer-temporary fix, the coverage sizing fixture
also supplies the required nonnull exact String identity. Missing fixture metadata
must not be reported as lost production opcode support.
