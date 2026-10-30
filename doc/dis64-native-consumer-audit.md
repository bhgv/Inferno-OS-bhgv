# Native-consumer layout audit: representative blockers, not ABI certification

The bounded scalar constructor/scheduler/vmachine gates do not establish that
native module frames and aggregate descriptors share the proposed Dis64 ABI.
`libinterp/tests/native-consumer-layout.py` compiles actual declarations and
reports their sizes/offsets separately from successful scalar object compilation.
It changes no generator, header, format constant or runtime admission.

## Reproduce

```sh
python3 libinterp/tests/native-consumer-layout.py
python3 libinterp/tests/native-consumer-layout.py --require-coherent
# The second command currently returns2 after successful measurement.
# Regenerate the default legacy native declarations with an explicit Limbo tool:
limbo -a -I module module/runt.m > /path/to/generated-runt.h
python3 libinterp/tests/native-consumer-layout.py --header /path/to/generated-runt.h
```

The optional header is copied into private scratch, never installed over runt.h.
CC/CFLAGS are explicit host inputs. Compiler or child failures propagate as errors;
a measured incompatibility under --require-coherent has the distinct status2.
Default JSON output is a report, not a successful readiness test.
`full_native_abi_verified` is always false: these representative measurements
cannot establish callable ABI, all maps, ownership or generated-consumer coherence.

## Observed Linux/AMD64 layout, 2026-10-30

WORD32 and WORD32+private file-routing selections both yield:

| Actual declaration | C measurement | Generated descriptor / proposed Frame64 |
| --- | ---: | ---: |
| WORD / pointer / Frame links | 4 /8 /32 | 4 /8 /32 |
| Sys_Qid size |16|16|
| Sys_Dir size |80|64|
| Sys_Connection size |24|12|
| F_Sys_sleep result offset |16|32|
| F_Sys_sleep first argument offset |36|64|

The checked-in header and a freshly regenerated default `limbo -a` header both
show these blockers. Regeneration alone does not select a new ABI. The default
generator describes legacy ref4/layout; selecting WORD32 in C does not change
its aggregate sizes, pointer maps, frame/result/temporary/argument rules.
This is an unclosed integration boundary, not a newly introduced production
wire defect. The map8 and Frame64 contract must be implemented consistently by
generator, runtime consumers, allocations and GC before enabling native calls.
Do not repair this by changing only `_size` constants or suppressing diagnostics.

## Broader whole-object compilation

`word32-build.py` now additionally compiles complete runt.c, loader.c,
math.c, emu/port/exception.c and devprog.c. The selected headless closure builds25
actual translation units per model:75 objects. An explicit `--with-geometry`
adds geom.c for a complete Inferno checkout, yielding26 units/model or78 objects.
Both selections were exercised. The default must not silently depend on an
unselected Draw/Point/Rect implementation in a downstream headless snapshot. The existing
actual scalar/frame probes and invalid selector checks remain. Successful object
compilation is separate from descriptor/layout compatibility and runtime linkage.
Draw/Tk/Prefab/security/crypto implementation or full emulator boot is not added.

Next: explicitly selected generated native ABI with agreeing full consumer builds,
then calls/dependencies, Frame64 roots/results/temporaries, ownership/unwind and
live GC. Keep default NC64 and general inspector readiness unchanged meanwhile.
