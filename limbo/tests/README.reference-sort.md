# Checked reference propagation and full-width local ordering

Run `python3 limbo/tests/reference-sort.py` on a native Linux host with eight-byte
C `long`. The driver extracts the actual `checkrefs`, `popscope`, `freeloc`, local
comparison/merge/sort functions and their private arithmetic helper. Both WORD32
and WORD64 configurations retain native host pointers and native compiler metadata.

Reference propagation now checks each sum at vlong precision before narrowing to
the int-sized `Decl.refs`. The covered paths are ADT members, module members and
nested members, interface destinations, string-constant owners, temporary `:=`
reference adjustments and imported declarations on scope exit. Unrepresentable
counts produce a compiler fatal rather than signed overflow or a silently narrowed
module total. Each individual checked assignment leaves its destination unchanged
on rejection; a compiler fatal can follow earlier successful metadata updates in
the same traversal. Other compiler reference increments are outside this change.

Local comparison keeps Type sizes as native `long` and compares sizes/reference
counts by relational signs instead of subtraction. Reversing the arguments to
`strcmp` retains ascending names without negating its result. Group merge order,
reuse priority, the small-slot preference, reference priority and stable ties are
unchanged. Oversized/negative Type sizes in comparator-only tests exercise native
width and ordering; accepting their order does not accept them as valid VM layouts.
The existing checked frame-layout code still governs representable frame extents.

Per WORD model and profile:

| Family | Scenarios |
| --- | ---: |
| Python arbitrary-precision expectations through nine real propagation paths | 1,164 |
| Independent lexicographic ordering keys through actual pair merges | 40,000 |
| Frozen ordinary propagation/scope cleanup, counts and diagnostic order | 3,000 |
| Frozen stable whole-group sorting, ordinary/boundary/stress lists | 1,001 |
| Total | 45,165 |

The nine propagation families include explicit decrement and restoration probes;
ordinary paired runs preserve warning selection/order, symbol restoration, imported
ADT bindings and local-slot cleanup. Invalid vectors require the exact overflow
fatal, and check unchanged destination counts except for the multi-step `:=`
restoration path. Integer expectations are calculated in Python, not by repeating
C int arithmetic.

Pair ordering uses independent Python keys for linked/unlinked groups, small-slot
category/size, descending references, size and name. Vectors cross native long and
int boundaries, both link states and stable name ties. Group tests retain all
continuations, compare complete node identity/metadata order against the historical
sort and include a 1,000-member group in a 4,096-group list. All run with a 512 KiB
C stack. The recursive sort itself remains logarithmic and is unchanged.

Across WORD32/WORD64 and `-O1`/`-O0` ASan+UBSan plus `-O2` UBSan, all with
`-DNDEBUG`, there are **270,990 scenarios**. Optimized pointer/integer casts are
errors. The core suite includes this driver.

`REF_SORT_DECLS` and `REF_SORT_COM` select old production sources. `REF_SORT_CASE`
selects `adt`, `module`, `member`, `nested`, `nestediface`, `string`, `dasdec`,
`dasrestore`, `import`, `refsorder`, `sizeorder`, or the complete `refs`, `pairs`,
`shallow`, `groups` families. On parent revision `49d996b`, the eleven isolated
families reproduce eight UBSan signed overflows, two missing module-count
rejections and one wrong wide-size order. The latter three fail explicit runtime
predicates. Build failures are not counted as reproductions.

Adjacent verification includes native tool bootstrap and all 17 Limbo compilation
units, deep declaration/group handling, WORD32/WORD64 frame layout, and emitted
native-stub tables under strict C99. This change leaves JIT instruction coverage
and machine-code emission unchanged; full VM boot and physical ARM64 execution
are not measured here.
