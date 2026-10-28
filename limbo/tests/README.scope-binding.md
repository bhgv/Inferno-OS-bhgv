# Scope bounds, iterative global binding and checked label growth

Run `python3 limbo/tests/scope-binding.py`. It extracts actual global `gbind`,
`pushlabel`, scope push/repush/pop operations, symbol installation, undefined-name
construction and checked reference propagation. A frozen shallow oracle supplies
the historical global walk and label behavior. Type binding itself is a controlled
lexical callback: it records visibility, resolves a Tid through real symbol state
and can call the actual `undefed` constructor. This fixture does not replace tests
of the separate recursive `bindtypes` implementation.

Production changes:

- Scope arrays have `MaxScope` elements, with valid indices 0 through MaxScope-1.
  `pushscope` and `repushids` check the next index before incrementing. Starting
  at the final valid index now reports `scope too deep` before an array access.
- `gbind` uses checked, private continuations for sequences, pick phases and
  aggregate scope cleanup. Pick cleanup retains its captured declaration; sibling
  and aggregate fields are read at their original phase. Type declaration chains
  use tail iteration. Binding/cleanup order and undefined propagation are unchanged.
- Label growth rejects inconsistent depths, signed capacity overflow and native
  byte-size overflow before allocation/indexing. A successful realloc publishes
  its pointer/capacity together; duplicate-label diagnostics and reference resets
  retain their original order. The existing 64-entry growth quantum is unchanged.

Per WORD configuration and build profile:

| Family | Scenarios |
| --- | ---: |
| Mixed shallow globals, ADTs, modules, picks and generic scopes vs frozen walk | 3,000 |
| Deferred sibling read and captured pick declaration under callback mutation | 2 |
| Independent binding, shadowing and outward undefined-name propagation | 5 |
| Last legal aggregate nesting and first rejected nesting | 2 |
| All 64 starting indices for each scope push/repush API | 128 |
| 100,000-level sequence and type-declaration chains | 2 |
| Frozen label order/duplicates with relocating allocation provider | 1,000 |
| Invalid/ordinary/large label capacities and depths | 275 |
| Total | 4,414 |

WORD32/WORD64 and `-O1`/`-O0` ASan+UBSan plus `-O2` UBSan, all with `-DNDEBUG`,
yield **26,484 scenarios**. Optimized pointer/integer casts are errors. Native
pointers/ulong retain native width in both configurations. Deep ASTs run with a
512 KiB C stack. Large valid label growth is observed at the allocator boundary
and stopped by the fixture before requesting gigabytes; this is not a successful
huge-allocation claim or a native 32-bit pointer test.

Binding comparisons record callback/diagnostic order and scope, Type bindings,
symbol restoration, owned undefined declarations and scope heads/tails. Normal
completion must release every private continuation. The fatal nested-scope probe
recovers for inspection and explicitly frees its abandoned work in the fixture;
the compiler itself exits on fatal. Independent generic/shadowing cases supplement
the differential oracle. Callback mutation cases check both deferred sequence
reads and the declaration captured before a pick body runs.

`SCOPE_BIND_DECLS` and `SCOPE_BIND_CHECKER` select older production sources.
`SCOPE_BIND_CASE` isolates a family. On parent `1059049`:

- `scope` and `repush` reproduce UBSan index-64-out-of-bounds errors;
- `sequence` and `type` reproduce ASan stack overflows;
- `label-overflow` reproduces UBSan signed capacity overflow;
- `label-index` reproduces ASan heap-buffer overflow at a negative index.

These are runtime reproductions; failed builds are not counted. `shallow`,
`mutation`, `edges`, `nested`, `labels`, `labelbounds` select the other families.
The core suite includes this driver. Adjacent checks rebuild native tools and all
17 Limbo units, run declaration/group and reference-use regressions, and verify
JIT readiness. Dis encodings, JIT machine code and opcode coverage are unchanged;
full coordinated VM boot and physical ARM64 execution are outside these checks.
