# Dis64 prototype fixtures

These fixtures describe the jointly reviewed map8 and typed-table direction.
No magic, ABI, section or opcode registry IDs are assigned. The active compiler,
loader, interpreter and JIT do not include these prototype headers yet.

Run from the repository root:

```sh
python3 libinterp/tests/dis-layout.py
python3 libinterp/tests/dis-control.py
CFLAGS='-O1 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis-layout.py
CFLAGS='-O3 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis-control.py
```

`dis-layout.c` uses always-on `if` checks. It covers ABI32/map4 and ABI64/map8,
int32/ref width separation, explicit frame slots, nested records, int-only
stride4, bounded array extents, atomic failures, and 2,304 single-bit map cases.
Full maps reject missing/excess bytes and marked incomplete references.
This prototype's full ABI32 maps do not redefine existing classic Dis files.

`control-golden.json` contains hand-written BE payload bytes and expected valid
or invalid classifications. They include signed MIN/MAX inclusive ranges,
string ranges whose ID order differs from content order, embedded U+0000,
Latin-1/Rune/supplementary values and goto order. The string fixtures use a
resolved pool; they do not choose the future pool encoding or a nil sentinel.

`dis-control.py` compiles the real candidate C helper and compares 6,112
complete/truncated/mutated/PC-bound cases to an independent Python oracle.
It also verifies that rejection leaves the output view unchanged. The helper
checks payloads without constructing VM heap objects or native-PC caches.

These are helper tests on the current compiler host, not executable new-ABI
modules, host32 cross builds, physical ARM64 runs or GC/ref-result acceptance.
For design/open decisions see `doc/dis64-section-matrix.md` and the two journals.

## Descriptor construction and target lookup

`disabmaprefs` builds complete descriptor maps from repeated reference offsets.
It preflights existing map bits, alignment, count/stride and extent before any
write; nested record references can be enumerated separately from int fields.
Zero stride sets one shared slot even for a large count. The C fixture checks
397,800 independent byte-geometry constructions, existing-bit preservation,
invalid-map rejection and large-count failure atomicity.

`discontrolcase` binary-searches a validated immutable view and returns a Dis-PC;
`discontrolgoto` preserves index order and rejects an unavailable index without
changing the caller output. String ID comparison remains a supplied callback,
so no Rune ordering or pool encoding is silently chosen. The Python oracle now
also checks 17,338 case/goto selections, including sparse generated tables with
0..64 rows. This is lookup helper execution, not interpreter/JIT integration.


## Lossless Rune32 records

`include/dis-rune.h` implements the non-nil count:u32 + BE u32 Rune record
accepted in FLASHVM-0002. `dis-rune.py` checks hand-written `rune-golden.json`,
388,404 record/capacity/truncation/index cases and guard/output preservation.
Empty records contain a zero count header. Nil uses a separate, unassigned
reference initializer. No Unicode scalar filter or Rune ordering is introduced.
Input arrays to the writer must not overlap output storage.

The existing `string-view-native.py` additionally executes extracted production
INSC/INDC on all seven requested boundary Runes and signed-order comparisons.
Those tests preserve and document existing behavior; they are not new-ABI
module acceptance. Full Rune32 ordering is signed32, explicitly agreed in FLASHVM-0003 / INFERNO-0006.


## Agreed signed32 ordering and production parity

`disrunecompare` orders full Rune32 bit patterns as signed32 keys, using sign-bit
flips without unsigned-to-signed casts. Prefix length decides equal prefixes;
a null view explicitly represents nil content and compares equal to empty.
The wire nil-reference ID remains unassigned. `rune-order-golden.json` records
hand-written equality/order groups; the Python oracle independently subtracts
2^32 for high-bit Runes. `dis-rune.py` checks all pairs of fixture and generated
strings, as well as the existing codec matrix.

`dis-rune-case.py` combines actual Rune decoding/comparison with the actual
typed-table validator and binary search: 12,180 range/ID/lookup combinations.
Pool indices here are test-local, not a proposal for nil-reference encoding.

`string-view-native.py` compares production stringcmp, wire ordering and a
wide-integer oracle for every mixed byte/Rune pair and packed-header alignment.
Production INDC is checked against explicit mathematical signed32 values.
These checks cover current runtime semantics on WORD32/64 models, while the
shared wire helpers remain disconnected from the active module loader.

## Accepted envelope and independent FlashVM fixture

`dis-envelope.py` compiles `include/dis64.h` and the standalone inspector. The
hand-written `envelope-golden.json` uses opaque M/C/T markers, not executable
payloads. An independent Python parser checks 45,348 reader/feature/unaligned
cases; 1,283 writer cases check exact bytes, capacity and unchanged outputs.
The inspector's rewrite is verified byte-for-byte, with explicit no-execution
status. Registry and limits are documented in `doc/dis64-envelope.md`.

`libinterp/tests/flashvm-rune-order.py` is imported byte-for-byte from
`bhgv/flashVM` commit `757ea0d952bb8ace5fa37f54c468f660a688dcc1`, path
`disVM/tests/rune_order_crosscheck.py`, blob
`5f9109ad4c1fdb060cc96faeb6e36b690007ecc3`. It checks this repository's actual
header independently: 29,032 signed-list/extent/immutable-alias roundtrip checks.
Run `python3 libinterp/tests/flashvm-rune-order.py .` from the repository root;
its explicit source-root argument also supports invocation from other directories.
The source provenance is preserved rather than rewriting its independent oracle.

`dis-string-pool.py` covers the agreed count+concatenated non-nil Rune records:
exact bytes, limits, duplicate contents under distinct IDs, empty pools/strings,
ordinal lookup, signed comparisons, capacity failure and unchanged outputs.
Writer inputs are complete encoded records; all validation precedes writes.
`dis-rune-case.py` now builds that real serialized pool before validating and
selecting typed string-case targets. A fixture-only offset array no longer
stands in for the pool wire payload. No nil ID or VM heap is introduced.
