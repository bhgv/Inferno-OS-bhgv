# Pointer-free record slice copies on AMD64 and ARM64

SLICELA now copies records with positive strides from 1 through 256 bytes in
both backends. The complete copy must fit 256 bytes and the destination byte
end must fit INT_MAX, matching arrayspan. Counts and indexes remain in elements
until descriptor validation; byte products are bounded before payload access.
The shared overlap-safe qword/tail kernel preserves memmove semantics, including
partial payload/operand-cell aliases. Every decline precedes the first store.
Zero-stride records, pointer maps, larger copies and complex ownership retain C.
Nil sources and zero-length copies retain their existing early exits.

The C arrayspan preflight now rejects negative/excessive map byte counts and
set bits whose complete native pointer exceeds the element size. Both view
creation (SLICEA) and mapped copying (SLICELA) consume this validation before
allocation, pointer visitors or ownership publication. Empty records preserve
their existing payload-free behavior. This does not validate arbitrary unmapped
host memory or introduce a new VM pointer ABI.

The regression matrix executes emitted AMD64/ARM64 code under Unicorn in both
WORD profiles: 325,920 full-memory, access-span, overlap, alias, FP and decline
checks, including 95,744 added record-stride checks. Actual RX AMD64 code is
compared with production C over 5,432,140 cases per WORD profile. This includes
748 new transactional descriptor checks per profile; they also pass ASan/UBSan
with NDEBUG. The parent C implementation aborts the new descriptor control.
ASan emits the sandbox's executable-name and no-return/stack warnings.
ARM64 results are emulated execution and compiler checks, not physical hardware.

Additional passing validation: heap-bounds in both WORD profiles, 389,328 array
constructor/view/error routes, 354,304 complete C/RX dispatch routes, 12,430 ARM64
compiler-bridge cases, dis2c loader/runtime differentials and regenerated readiness.

```
python3 libinterp/tests/slice-copy-jit.py
python3 libinterp/tests/slice-copy-native.py
SLICE_COPY_DESCRIPTORS=1 python3 libinterp/tests/slice-copy-native.py
python3 libinterp/tests/heap-bounds.py
python3 libinterp/tests/array-spans-native.py
python3 libinterp/tests/jit-readiness.py --check
```

Coverage stays at 173/173 JIT+C entries and 166/173 guarded native commands on
both AMD64 and ARM64. This series expands SLICELA's accepted native states; it
does not count one existing opcode as a newly supported command. The per-command
percentages and explicit domains are in README.jit-readiness.md and jit-readiness.csv.
