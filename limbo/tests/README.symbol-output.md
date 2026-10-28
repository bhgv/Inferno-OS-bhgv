# Bounded SBL output and module-local symbol ownership

The Limbo compiler's `.sbl` writer now walks nested Types and declarations using
private continuations rather than mutually recursive C calls. Arrays, lists,
channels, references, function arguments/results, tuples, exception payloads,
ADT fields, pick tags and module globals retain their existing serialization.
Nominal Type references are still registered before their definitions are written.
Adjacent tags sharing a layout still omit the repeated field section.

The stack uses 64 local actions, grows with checked byte arithmetic and is
released after each top-level section. Unary wrapper chains reuse the current
action. Source-delta callbacks retain their original order, so file/line delta
state and all bytes for valid legacy inputs remain unchanged.

The same series fixes independent symbol-output defects:

- Function-pointer layout rendering restores the caller's previous `Tany`
  visibility instead of unconditionally clearing it. Temporary visibility still
  exposes the function-pointer layout's otherwise hidden `any` field.
- Beginning another module releases private file-list cells from the preceding
  module, retaining borrowed filename strings and stable first-use numbering.
- File indices and visible-declaration counts reject overflow before increment.
- Instruction metadata rejects negative/oversized block dimensions, invalid block
  indices and mismatched emitted instruction counts before writing a header.
  Skipped INOOP instructions remain excluded from counts and block validation.
- Missing source ends reject an INT_MAX start position before adding one or
  mutating the instruction's span.
- New block IDs are staged before publishing output or changing the global count;
  negative/exhausted counts reject instead of wrapping. The final legal block ID
  is accepted and advances the count exactly to INT_MAX.
- Type/function vectors validate count, byte span, nil entries and missing Types
  before reading/publishing their section or nominal indices.

There is no `.sbl` format version change, `.dis` layout change, VM ABI change or
new JIT opcode in this compiler series. It does not make other recursive compiler
passes stack-independent or detect malformed anonymous Type cycles.

## Verification

```
python3 limbo/tests/symbol-output.py
python3 utils/tests/native-bootstrap.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
```

The new driver compiles the entire real `sbl.c` against both WORD32 and WORD64
compiler headers and the real Plan 9 formatter. It uses ASan/UBSan, NDEBUG and
explicit predicates; unavailable process-based leak inspection is disabled,
not address or stack instrumentation. Sandbox symbolization/no-return warnings
may appear. Private allocations are counted independently of the output buffer.

Ten thousand randomized shallow Type graphs per model compare every output byte,
source-delta state, callback count and final visibility with a frozen pre-change
writer. Additional exact comparisons cover mutually recursive nominal ADTs,
ADT pick references, qualified function records, locals and instruction span
repairs. Deep wrapper/function/tuple/tag families each contain 30,000 levels and
execute with a 256 KiB native stack; an independent flat grammar checks their
entire output, including all return/newline suffixes. File-lifetime tests repeat
100 module transitions. Invalid metadata must reject before output publication;
large invalid dimensions are intercepted rather than allocated.

Pre-change source 40785b3 passes the shallow differential oracle but independently
fails all four deep families, caller-visibility preservation, module file-cell
ownership and six metadata families. Replay one at a time with:

```
SBL_SOURCE=/path/to/parent-sbl.c SBL_WORD_BYTES=4 SBL_CASE=functions python3 limbo/tests/symbol-output.py
```

`SBL_CASE` accepts `wrappers`, `functions`, `tuples`, `tags`, `visibility`,
`lifetime`, `blocks`, `source`, `vectors`, `counts`, `cardinality` and `blockids`
for those parent failures. Parent controls use UBSan and
`-fno-optimize-sibling-calls`; failures occur during execution, not compilation.
The theoretical multi-billion-entry file/declaration counter boundaries are
checked in production but not materialized by this fixture.

Native bootstrap still builds ELF64 mk/iyacc/decgen, regenerates the grammar and
decoder, and compiles all 17 compiler units. Actual x86-64 JIT/C tests retain
177,152 routes, 6,183 compiler cases and 295 VM-loop checks per WORD model;
ARM64 compiler preflight retains 6,215 cases per model. Complete JIT entries
remain 173/173, with 166/173 guarded fast paths (95.95%) for AMD64 and ARM64.
See the [per-command table](../../libinterp/tests/README.jit-readiness.md).
These are not a complete native VM boot or physical ARM64 execution result.
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.
