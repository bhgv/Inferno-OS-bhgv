# Packed Sys.pctl frames and descriptor lists

Run `python3 libinterp/tests/sys-pctl-capture.py` for both production wrappers,
WORD32/WORD64 host models and the complete capture/preflight matrices. ASan,
UBSan and `NDEBUG` are enabled by default. `CC` and `CFLAGS` remain overridable.
Explicit predicates, guarded byte images and exact native reference accounting
remain active when assertions are disabled. No assertion is used in this fixture.

The wrapper copies the complete fixed call frame and validates the full WORD
result span before selecting the program or releasing the VM. Original flags,
descriptor-list head and result address remain stable across native callbacks.
Both descriptor-group loops copy list links and scalar payloads from bytes,
allowing borrowed nodes at any byte alignment. Native descriptor width checks,
NEWFD/FORKFD selection, shared-old-group ownership and duplicate handling retain
their existing behavior.

A private, allocation-free Floyd traversal validates numeric node spans and
rejects nil/low/wrapping links and cycles before any program/native-group action.
Only NEWFD/FORKFD consume the list; flags that do not consume descriptors ignore
it, including malformed/cyclic unused lists. This adds a linear preflight pass;
it does not prove arbitrary memory mappings, node ownership/type metadata or
stability under concurrent mutation. The fixture mutates call frames, not list
contents while the wrapper is executing.

The fixture executes complete production wrappers, native-width helpers,
list/frame checks and real VM protection frames. Native groups/channels,
reference operations, scheduler transitions and allocation failures are bounded
adapters. All nested errors run on a joined pthread stack with ASan stack
instrumentation retained. Only LSan's unavailable process inspection is disabled;
no address instrumentation is disabled or manually unpoisoned.

Per emu/os and WORD32/WORD64 build:

- Capture: 645,120 descriptor-group scenarios across eight frame shifts, eight
  node shifts, six callback phases, eight result shifts, seven list profiles,
  three descriptor modes, five process ids and normal/failed native group
  creation. Callback phases are unchanged, program selection, release, group
  allocation, acquire and previous-group release.
- Preflight: 14 rejected-input families and two ignored-list families across
  NEWFD/FORKFD selection and all frame/node/result shifts. This is 14,336 early
  rejections and 2,048 successful ignored-list checks per build.
- Each run also executes 72 independent packed scalar/native-width checks.

Four builds therefore cover 2,580,480 capture scenarios, 57,344 early rejections
and 8,192 ignored-list cases, plus 576 scalar checks across the two families.
The lower-level default `sys-pctl.py` retains 1,680 ordinary scenarios plus
72 scalar checks per build (7,008 total executions).

Every normal/error path checks full result/frame/list images, scheduler/error-frame
balance, unchanged shared old descriptors, exact old/new group and channel
references, native close counts and high-bit descriptor rejection. Early failures
must leave native groups and result bytes untouched and must not even select the
current program. Invalid-input native group creation aborts in the adapter so
old-source cycle controls terminate with a predicate instead of hanging.

`PCTL_CAPTURE=1` enables the enlarged matrix. `PCTL_FRAME=0..7`,
`PCTL_NODE=0..7`, `PCTL_PHASE=0..5`, and the existing `PCTL_MODE`, `PCTL_PROFILE`,
`PCTL_OFFSET` selectors isolate cases. `PCTL_PREFLIGHT=1` selects the other family;
`PCTL_PREFLIGHT_MODE=0..15` isolates one bad/ignored-input class.
`PCTL_PLATFORM=emu|os`, `PCTL_WORD=32|64`, and `PCTL_SOURCE` select a build/source.

All 84 independently compiled controls against parent `3ba10d2` fail with an
active predicate or sanitizer diagnostic: four builds times a packed-frame
probe, a packed-node probe, five callback probes and fourteen preflight probes.
Compilation failures do not count as regression evidence.

Independent parent-source controls retain the old wrapper statements. Private
new helpers are supplied from current sources only when absent in the old file;
they do not replace or repair any old wrapper reads, group actions or writes.

These are descriptor-group tests. Untested namespace/environment/process-group
branches abort if entered. WORD32 retains native host pointers and emulator
error-frame declarations. Real kernel concurrency, ARM64 hardware execution and
a coordinated native 64-bit compiler/loader/frame/GC ABI remain outside this
fixture. Complete JIT dispatch and guarded native opcode percentages do not
change as a result of this C-handler repair.
