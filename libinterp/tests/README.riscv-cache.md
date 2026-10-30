# RISC-V sidecar reuse and compiler preflight

The compiler previously returned success for every non-nil Module.jit. A cached
IADDW entry could therefore point at IMOVW and still be accepted by compile.
A fresh mixed program containing both a native opcode and a reserved opcode
also passed: unsupported valid operations and invalid opcode bytes were not
distinguished. These are compiler admission/reuse defects; the new gate does not
claim execution of an invalid program through the complete VM loop.

comp-riscv.c now validates the Module prefix before typed reads, bounds the
complete byte-addressed Inst span, and rejects all reserved/invalid opcodes before
allocation or cache reuse. Packed instruction storage is read by byte address.
A negative emitter result is an error, never an unsupported-operation sentinel.

A cached sidecar must have the expected instruction count, code extent/alignment,
entry callback and allocator ownership callback. Every native entry must point
at its canonical opcode position in the mapping and contain the exact current
emitter output for this model/ISA profile. RISC-V's unused complete-dispatch table
must remain empty. All code bytes must be accounted for, and each opcode in the
current program that has a native implementation must have its corresponding
entry. Valid operations without a native fast path retain ordinary C dispatch.
No code, callbacks, module fields or allocations change on refusal.

## Host gate

```
python3 libinterp/tests/riscv-cache.py
CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/riscv-cache.py
CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/riscv-cache.py
```

The actual compiler/emitter is built for WORD64 and WORD32, each with I, Zmmul
and M selections, using both embedded allocation/cache services and real Linux
RW-to-RX mappings. Twelve configurations run31896 active predicates per full
pass. Each compiles all173 valid opcodes; the supported native counts remain
87/90/103. Tests change table entries to other valid entries, offsets within
code, absent entries and inappropriate complete-dispatch entries; they also
change code bytes, callback identities, extents and module opcode bytes.
Allocation failures and embedded publication failure retain atomic cleanup.
Generated RV instructions are not executed by this host cache gate.

RISCV_CACHE_SOURCE selects an alternate compiler source. RISCV_CACHE_CASE=binding
and RISCV_CACHE_CASE=opcode isolate independently compiled regression controls.
The original compiler at ee39013 reaches an active predicate/SIGABRT for each;
the current compiler passes both controls across all12 configurations. Neither
control depends on a compile error, sanitizer diagnostic or invalid-pointer fault.
The fixture disables only unavailable LeakSanitizer process inspection; address
and UB checks remain. Restricted-host executable-name warnings can remain.

The adjacent riscv-runtime.py now uses active if-based checks instead of assert,
so NDEBUG cannot remove calls to compile from its predicates. Its79 checks cover
allocation/cache/VM-error cleanup, loader integration, opcode mutation, scheduler
quanta and module switches using C stand-ins for native execution. The actual
encoder execution gate riscv-jit.py passes2174 Unicorn checks across RV32/WORD32,
RV64/WORD32 and RV64/legacy-WORD64. Feature selection I/Zmmul/M is checked separately.
No physical RISC-V machine, full engine boot or new native opcode is claimed.
