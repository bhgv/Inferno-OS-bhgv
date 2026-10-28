# Checked declaration uses and temporary holds

Run `python3 limbo/tests/reference-uses.py`. The driver extracts the actual
`declrefsum`, declaration/node constructors, `mkdeclname`, import-chain `increfs`,
`modrefable`, `globalsconst`, `talloc`, and temporary acquire/release/zeroing
functions. Symbols, descriptors and zeroing emission are controlled callback
fixtures; the reference-counting and lifetime algorithms are production source.

The checked count helper is now a compiler API declared in `fns.h`. All 25
remaining direct `Decl.refs++` sites in six compiler units use it. All ten direct
increments/decrements of temporary `Decl.tref` use it too, including deferred
zeroing holds. Source auditing rejects reintroduced direct arithmetic at those
fields. Initialization, intentional resets and copies remain ordinary assignments.
No new validation policy for negative metadata is introduced: the change prevents
int arithmetic overflow while retaining the existing null, zero and double-release
checks. Compiler-fatal paths can follow earlier successful metadata changes.

Per WORD model and build profile:

| Family | Scenarios |
| --- | ---: |
| Name construction, imports, modules, strings; 11 boundary counts each | 44 |
| Acquire, release, immediate/deferred free; 11 counts each | 44 |
| Temporary allocation/reuse, six ordinary paths and four exhausted counters | 10 |
| Multiple deferred owners and repeated empty-queue draining | 1 |
| 100,000 imports, normal and failure halfway through | 2 |
| Shared-owner lifetime sequences, 100 model steps each | 10,000 |
| Null/zero guards, fresh string, small-temp and already-marked module cases | 5 |
| Total | 10,106 |

Across WORD32/WORD64 and `-O1`/`-O0` ASan+UBSan plus `-O2` UBSan, all with
`-DNDEBUG`, the matrix has **60,636 scenarios**. Optimized pointer/integer casts
are errors. The 60,000 lifetime sequences cover 6,000,000 model steps: shared
borrowed declarations, live aliases, immediate free, deferred holds and queue
draining. The model checks every owner's holds and unchanged use-count metadata,
source Node links, queue membership and exact zeroing callbacks after each step.
All constructor allocations are released on scenario reset. The long import
failure additionally verifies the exact updated prefix, unchanged saturated
counter and untouched suffix under a 512 KiB C stack.

All fatal probes require the checked-count diagnostic. Ordinary temporary reuse
checks both scalar and aggregate paths, supplied-candidate/list reuse, fresh
allocation, descriptor callbacks and references that intentionally do not change
on scalar reuse. A fresh string checks actual constructor ownership and symbol
installation. Already-marked modules must not increment again, even when the
stored declaration count is exhausted.

`REF_USE_NODES`, `REF_USE_TYPECHECK`, `REF_USE_TYPES`, `REF_USE_ECOM` and
`REF_USE_GEN` select earlier producer sources; the checked provider remains the
current declaration implementation. `REF_USE_CASE` isolates a family. On parent
`577940f`, twelve probes independently trigger UBSan signed overflow:

- `name`, `imports`, `module`, `string`;
- `acquire`, `release`, `free`, `later`;
- `allocate-small-hold`, `allocate-big-hold`, `allocate-big-refs`,
  `allocate-list-refs`.

The four allocation probes are also included in the default `allocate` family.
Build failures do not count as reproductions. The source audit is skipped only
when explicit producer overrides are supplied for these historical probes.

Extracted-source drivers for node chains, grouped declarations, reference sorting
and big constants now include the real checked provider. The core suite includes
the new driver. Adjacent checks build native tools and all 17 Limbo units, execute
those four affected test drivers, and verify JIT readiness. WORD configurations
retain native pointers; these checks do not boot a coordinated 64-bit Inferno VM
or execute on physical ARM64. Machine-code emission and opcode coverage do not
change in this series.
