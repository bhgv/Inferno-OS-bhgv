# Priority JIT layout arithmetic and bounded publication

Run `python3 libinterp/tests/emitter-layouts.py`. The driver compiles both real
AMD64/ARM64 emitters under ASan/UBSan and NDEBUG, using separate WORD32/WORD64
host-header models. Explicit predicates stay active. `CC` and `CFLAGS` remain
configurable. Only LSan process inspection is disabled; address/stack checks
remain enabled.

The series repairs compiler-internal layout validation:

- The shared Array overlap validator now bounds each complete field before
  adding its width. Negative/wrapping spans cannot be accepted or trigger signed
  overflow while the Array-creation validator is still checking its inputs.
- AMD64 array length/index leaves bound both field extents and the negative
  signed-immediate limits used by span guards before forming either expression.
- AMD64 span guards select compact compares through `extent <= 126`, avoiding
  an intermediate `extent+2` when the supported extent approaches INT_MAX.
- ARM64 INDC validates `stringmax` before using `stringmax+4` in data-offset tests.

Normal private layouts and opcode classification remain unchanged. Extreme
layouts model corrupted metadata and future port configurations; the public
bytecode format and the coordinated native reference/frame ABI are unchanged.

The baseline copies the actual compiler's complete LeafLayout initialization
and uses real runtime headers. Runtime helper/Type/GC addresses become distinct
inert identities: they are encoded into staged instructions, never called by the
synthetic-layout matrix. All 53 integer layout fields are varied independently
across sixteen values, including INT_MIN, encoding thresholds and several
values within sixteen bytes of INT_MAX, for every one of 173 active Dis opcodes.

Each of four builds checks 146,704 layout/opcode combinations with four caller
capacities: zero, one unit below the required size, the required size and LeafMax.
Every emitted result must match sizing, stay within both output guards, preserve
unused output words, and leave the complete input layout unchanged. Rejection
must leave the whole caller buffer unchanged. Unused layout fields may remain
irrelevant to a particular opcode; the matrix does not require blanket rejection
of every unusual value.

Separate wide-arithmetic Array oracles check 76 cases per build, including both
sides of field-overlap and signed-endpoint boundaries. On an AMD64 host, a further
90 cases per WORD model execute the real span guard in a private RW-then-RX
mapping. They test compact/full immediates, the largest supported extent and
owners immediately on both sides of the native pointer limit. Numeric owners
are compared without dereferencing them. Synthetic object-layout leaves are
never executed.

Totals on the verified AMD64 host: 2,347,264 bounded-publication probes, 304 Array
oracle cases and 180 real native span executions (2,347,748 checks). This measures
layout/publication cases, not additional native opcodes.

Selectors: `EMIT_LAYOUT_ARCH=amd64|arm64`, `EMIT_LAYOUT_WORD=32|64`, and
`EMIT_LAYOUT_CASE=array|publication|span`. `EMIT_LAYOUT_FIELD` selects the integer
field index and `EMIT_LAYOUT_OP` the numeric opcode; `EMIT_LAYOUT_ARRAY_FIELD`
and `EMIT_LAYOUT_ARRAY_VALUE` isolate the Array oracle. `EMIT_LAYOUT_HEADERS`
selects alternate emitter/shared headers for old-source controls. Span-only
execution requires an AMD64 host/backend.

Fourteen independent, successfully compiled parent-header controls against
`3523d0d` reproduce the old failures: negative Array spans and Array-overlap
arithmetic overflow on both backends/models, AMD64 array length/data span
arithmetic on both models, and ARM64 string-capacity arithmetic on both models.
Only the relevant old header is substituted in each control. The old standalone
span guard passes the valid-extent execution oracle on this compiler; that
compatibility result is not counted as a reproduced failure.

Adjacent regressions execute array-slot C/RX paths, Array commands in AMD64 and
ARM64 Unicorn, ARM64 character leaves, INDC C preflight, complete decoded JIT
dispatch and both compiler-publication bridges. Physical ARM64 execution, full
VM boot and a coordinated compiler/loader/frame/GC ABI remain separate work.
Native availability stays 173/173 JIT+C entries and 166/173 guarded native paths
for each priority architecture. RV64 and excluded legacy JIT sources are untouched.
