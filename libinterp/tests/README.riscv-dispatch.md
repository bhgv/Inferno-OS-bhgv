# RV64 complete JIT tails

RV64 now publishes a complete Jit.dispatch entry for every active opcode used by
a compiled module, even when there is no native fast body. A module containing
only C operations can receive a sidecar. RV32 keeps its original dispatcher and
code-generation path. Fast availability is unchanged: 87/173 for I, 90/173 for
Zmmul, 103/173 for M. Complete RV64 entries are 173/173 in all profiles.

The 15-word bridge passes the decoded REG, opcode and optional fast pointer to the
actual cjitexec. It uses only RV64I, leaves sp/ra intact and tail-jumps to C. It
never returns into disposable JIT storage after a C handler. Full-width trusted
pointers live in a private literal pool; two aligned 32-bit loads assemble each pointer
so four-byte-aligned entries do not require an unaligned LD. Odd function pointers,
reserved opcodes and bad capacities are refused before output. Literal data is
not serialized Dis64 metadata or a wire calling signature.

Compiler sizing/emission owns fast bodies followed by complete tails in one
mapping. Cache validation regenerates both bodies and exact tail/pointer bytes,
checks canonical bindings, rejects missing tails, and requires every current
opcode to have a complete entry. Publication/cache fencing and freecode ownership
remain atomic. Linux uses the existing private RW-to-RX mapping; embedded code
keeps its existing allocator/cache API. The decoded VM still prefers direct fast
entries and retains ordinary C fallback on their refusal; C-only operations enter
the complete tail. Original Inst PCs, frame/root maps and compiled=0 remain.

## Hosted compiler/cache/VM checks

```
python3 libinterp/tests/riscv-cache.py
python3 libinterp/tests/riscv-runtime.py
python3 libinterp/tests/jit-readiness.py --check
```

The cache gate exercises actual compiler/validator code in 12 combinations:
WORD32/WORD64, I/Zmmul/M and embedded/page allocators. Each verifies173 complete
tails, native entries, code/pointer corruption, missing/aliased bindings, reserved
opcodes and unchanged ownership on failure. Three mutations separately corrupt
a C-only tail instruction and its executor/fast literal words. The hosted VM gate
uses actual xec/cjitexec/compile bodies with C stand-ins for target instructions;
complete-only handlers switch/return modules through the actual VM loop. It also
checks compilation of C-only modules and existing compiler/VM-error cleanup.

## Actual target execution

```
python3 libinterp/tests/riscv-dispatch.py \
  --cc 'clang --target=riscv64-linux-gnu -fuse-ld=lld' \
  --runner qemu-riscv64
```

The freestanding Linux binaries run the production encoder and complete actual
cjitexec body, including its real fast/C selection, on RV64I, RV64IM and RV64GC
with WORD32/WORD64 layouts. Text/data are deliberately linked above 4 GiB. Each
configuration executes 1,038 all-opcode C-only/declining/accepting outcomes across
two entry alignments, plus high-bit pointer capture, real emitted integer leaf,
reserved/capacity checks, absent-handler boundary and page retirement. That is
6,228 complete dispatch outcomes across six configurations, not a speed benchmark.
A final C handler unmaps the actual bridge page before returning; C then returns
to the original caller, proving no JIT return address remains. Separate target
mutations replace the tail jump with a link or truncate the fast pointer and must
fail. Code is generated RW and executed RX, with explicit instruction fencing.

Only minimal Linux startup/syscalls, synthetic REG/handlers and compiler support
are supplied. The actual executor uses REG identity rather than field layout;
the native-leaf layout is independently supplied to the encoder. Target error
publication is recorded rather than a full VM nonlocal unwind. Thus these target
checks establish ABI/instruction/lifetime behavior, not whole Inferno boot,
physical hardware, scheduler/GC correctness or all native runtime libraries.
The Dis64 file loader remains at its 43-op scalar admission profile; complete
classic decoded-op tails do not enable references, variadics or new wire IDs.
Previous optimized UTF/injected-unwind ASan findings remain open independently.

NATIVE64_TMPDIR may name an existing external scratch directory, matching the
peer review gates. Generated sources, linker scripts and binaries then stay
outside the selected source tree; all production reads still use ROOT.
