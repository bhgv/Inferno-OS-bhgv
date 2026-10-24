# Array slice bounds and tokenization ownership checkpoint

The series is based on experiment commit
`6db2f64c42bc12b9fe9ad60c7633f7a4b0daaefd`, preserving the concurrent Limbo result
folding, emulator startup and tokenizer corrections already published there.
No duplicate tokenizer production implementation is installed.

## Runtime corrections

[slicea](../xec.c) now admits only `[0:0]` for nil arrays. Equal negative or
positive nonzero bounds raise exBounds without allocation or destination changes,
enforcing the [Limbo language bounds invariant](../../doc/limbo/limbo.ms).
Unequal nil bounds retain the existing exNilref status. This intentionally
changes acceptance of invalid slices in old Dis programs; recompilation is not
required to obtain the corrected runtime check. Parent-source substitution
reproduces the newly rejected invalid equal bounds.

Empty arrays and zero-sized-element views preserve nil data addresses without
performing C null-pointer arithmetic. Repeated slicing transfers/collapses real
root ownership and retains exact element-type references. Allocation failures
preserve the old view. Existing array layouts and bytecode encodings do not change.
The successful outputs alone need not distinguish the old null-plus-zero C
expression; this is a definedness correction with ownership regressions.

The [tokenization matrix](README.sys-tokenize-matrix.md) adds per-input failure
sweeps to the existing runtime implementation, with real VM declarations and
poisoned exact-capacity storage. Each WORD32/native-WORD64 model passes 8,322
scenarios under UBSan and ASan+UBSan. Old results can own input strings, errors
preserve both output fields, nil/empty delimiters retain whole strings and every
block/type reference balances. The WORD32 model retains native pointers; it is
not a complete ABI32 execution. The allocator, destructor and GC schedule are
bounded host adapters.

## Reproduce and validation limits

```
python3 libinterp/tests/slice-tokenize-suite.py
python3 -m pip install -r libinterp/tests/requirements-jit.txt
python3 libinterp/tests/slice-tokenize-suite.py --jit
```

The suite selects strict UBSan unless CFLAGS is explicitly supplied. The complete
28-driver selection passes: 24 runtime/compiler/startup drivers plus four x86/ARM
instruction-emulation drivers. The latter include 130,562 x86 byte div/mod
executions, 84 type-helper scenarios per architecture and 122 ARM VFP checks.
Compiler regressions include traps/effects, lazy branches and inline rewriting,
result types, integer/boolean identities, string conversions/lengths and numeric
case tables. These compiler production changes came from the preserved base;
this series adds runtime fixes and tests rather than replacing them.

[Heap/slice tests](README.array-copies.md) include 285 pointer-overlap cases,
nil bounds at full-WORD extrema and sixteen repeated empty-view/root transfers.
Full fault injection passes UBSan. Full ASan fault coverage is unresolved in
this sandbox: siglongjmp triggers an ignored __asan_handle_no_return warning and
then a stack-redzone report. That run is not counted as passing; instrumentation
is not disabled. Successful empty views pass ASan+UBSan separately:

```
HEAP_CASE=empty-views CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-bounds.py
```

LeakSanitizer process inspection is disabled; explicit allocation/reference
checks remain active. The runtime translation units pass host-header syntax
checks and diff whitespace validation. No generated test logs are versioned.

MIPS, SPARC and PowerPC JIT sources are unchanged. Existing ARM/x86 slice
fallback selection was reviewed; it calls the corrected runtime operation.
The emitter tests execute instruction subsets in Unicorn, not a complete native
VM. Native builtin/JIT calling conventions for these exact operations, complete
Inferno/compiler builds, concurrent GC and physical devices remain separate.
No new x86-64, arm64 or riscv64 JIT or calling convention is claimed; the fixes
avoid assuming four-byte pointers or performing invalid host address arithmetic.

## Focused VM and compiler work

Run `python3 libinterp/tests/slice-tokenize-suite.py --core` to omit GUI/Tk and
launcher-option checks. The current focused selection has 24 host drivers,
including mapped record/channel transfer and module type-table consumers.
`--core --list` lists that selection; `--core --jit` adds the existing optional
x86/ARM instruction-emulation drivers. Optional native-JIT acceptance limits
remain unchanged. See [mapped moves](README.mapped-moves.md) for current coverage.
