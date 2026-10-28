# Native owner GC layout and builtin identity preflight

AMD64 and ARM64 MOVP, SELF, HEADP and TAIL now share leafownergclayout before
emitting their guarded native bodies. It validates:

- complete, aligned, disjoint backward Heap.ref (8 bytes) and Heap.color (4 bytes)
  fields within the existing bounded metadata window;
- native eight-byte reference counters and the existing propagator color domain;
- two non-null, aligned, nonwrapping four-byte GC cells for mutator and nprop;
- distinct mutator and nprop addresses, so marking cannot overwrite its own color.

The previous backend checks rejected nil GC addresses but accepted H, wrapping
cells, unaligned/aliased cells and overlapping reference/color metadata. Valid
layouts generate the same machine code; invalid layouts decline before caller
output is written. These are numeric layout checks, not executable/mapped-memory
ownership or allocation validation.

The shared byte-conversion validator now rejects H as well as nil for builtin
String/Array identities. CVTAC/CVTCA cannot accept a malformed Heap Type sentinel
as a reusable builtin owner. Both numeric-format emitters likewise reject H as
the required String identity for CVTWC/CVTLC/CVTFC. Pointer identity comparisons
remain sufficient here: these identity constants are not dereferenced by those
comparisons, and arbitrary alignment restrictions have not been introduced.

## Verification

```sh
EMIT_LAYOUT_CASE=owners python3 libinterp/tests/emitter-layouts.py
python3 libinterp/tests/owner-native.py
python3 libinterp/tests/owner-jit.py
python3 libinterp/tests/emitter-layouts.py
python3 libinterp/tests/jit-readiness.py --check
```

The new layout matrix has 13,928 cases across AMD64/ARM64 and WORD32/64. It uses
an independent wide-coordinate interval oracle, metadata boundaries, all tested
GC address pairs, every supported/adjacent propagator byte, reference widths,
and nil/H builtin identities. It checks the shared validator, actual sizing,
unchanged input layout, output canaries and fully unchanged rejected output.
`EMIT_LAYOUT_OWNER_CASE=metadata|cells|identities` isolates the families.

Eight old-source probes reproduce semantic predicate failures: metadata overlap,
GC cells, numeric-format identity and shared byte-conversion identity on each
backend. Old emitter probes borrow the new standalone helper declarations so the
test can compile its direct oracle check; the unchanged old backend does not call
that helper. Old shared-validator probes append that helper for the same reason.
Compilation failures are not counted as reproduced bugs.

Native AMD64 owner checks pass 385,856 C differential cases plus 3,160 preflight
cases in each WORD model. The AMD64/ARM64 Unicorn owner matrix passes 193,896
cumulative checks. Adjacent integer/real formatting, byte/string conversion,
complete dispatch, bootstrap and [inline graph checks](../../limbo/tests/README.inline-graphs.md)
provide additional regression coverage as recorded for this checkpoint.

| Architecture | Complete native+C entries | Native fast-path availability | C-only entries |
| --- | ---: | ---: | ---: |
| x86-64 | 173/173 (100%) | 166/173 (95.95%) | 7 |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7 |

The [per-command table](README.jit-readiness.md) remains unchanged. Entry
availability does not measure full operand-domain acceleration or bootable VM
completion. Physical ARM64 and coordinated compiler/loader/frame/GC ABI acceptance
remain outstanding. Tk, security/crypto, MIPS/SPARC/PowerPC JIT and RV64 production
sources are unchanged. Incoming cold-host-fixture correction 2bd9f3c is retained.
