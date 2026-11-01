# Numeric source cases in the scalar Dis64 backend (series 0056)

`limbo -6` now lowers small numeric case statements to the existing scalar
comparison and jump opcodes. This is compiler translation, not file admission
of CASE, CASEL or GOTO. No shared section/opcode/kind/feature ID, wire type,
module profile or native-call ABI changes. The shared np0 subset remains43 ops.
The generic VM table handlers and complex C services are unchanged.

## Accepted source and bounds

The existing source language supplies int cases with inclusive ranges, discrete
Big labels, defaults and implicit defaults. Dense int cases may already have been
lowered by the frontend to GOTO. Up to eight labels in the final numeric IR table
are accepted. This count is after the ordinary frontend's dense-table expansion,
so a short source range can still exceed the bound. String/managed cases and
larger tables remain refused; ordinary Dis compilation retains its old behavior.
This does not extend the frontend's range language to Big ranges.

The backend accepts only inaccessible compiler-generated .c/.g global declarations
with numeric Case metadata. It validates the corresponding opcode/type, key role,
label bounds/targets and default before lowering. Entry locals remain exclusively
int/big/real, and the entry frame map must remain pointer-free. General aggregates,
exported module data, helper calls and managed imports remain outside admission.

For sparse tables each label becomes `key < low` to the next label followed by
`key <= high` to its arm, then one default jump. Dense, already normalized GOTO
keys use equality comparisons with ordinal bounds. The inherited frontend's
normalization and out-of-range guards remain. Label endpoints are inclusive;
no high+1 is computed at signed maxima. Small linear lowering is explicitly
bounded and does not replace the generic C table/search algorithms.

Numeric bound cells are appended after the unchanged original scalar MP extent.
Original compiler table holes remain zero-initialized unused bytes; no native
pointer or raw legacy jump-table payload is serialized. Bounds use the agreed
int/big scalar initializer kinds and full-width canonical bits. All appended
middle operands fit the existing unsigned16-bit middle-slot address contract;
otherwise compilation refuses. Combined original and appended initializers are
limited by the existing1024-record cap. Object/code/image limits remain active.

## PC identity and source locations

A first pass maps every old resolved Dis PC to its translated PC and checks the
complete expanded instruction count. A second pass rewrites ordinary branch
targets and emits all case-arm/default targets through that map. No original
Inst, Decl.offset, descriptor, Case, Label or legacy output structure is changed.
All emitted instructions pass the existing scalar role gate; the full immutable
image still passes the shared pilot gate before output publication.

The private diagnostic trace now uses translated PCs/opcodes and the borrowed
originating IR source span. Several comparisons may refer to one source case.
Every row still corresponds to exactly one serialized instruction. Debug PCs
remain decoded instruction indices, never native addresses or byte offsets.
This does not define a shared debug section or native callable field witness.

## Example

```
limbo -6 -o case.dis64 limbo/tests/dis64-scalar/case.b
./emu -v -r "$PWD" -c 1 -d /case.dis64
```

Run from the root containing case.dis64 with the experimental native64 scalar
console. Expected module Native64Case retires cleanly after checking4097 loop
iterations, int ranges/dense dispatch and the largest signed Big label. Use
`-c 0` for the interpreter. Existing defaults and ordinary runtime trees remain
separate from this restricted entry/profile.

## Verification

`utils/tests/dis64-cases.py` uses67 real source programs and independently
computes expected arms in Python. It covers signed extrema, sparse ranges,
dense tables/gaps, wide Big labels, nested cases, temporary expression keys,
implicit defaults, long loops and an active wrong expected answer. Every image
executes with interpreter/JIT, requiring late retirement:134 full processes per
architecture. String and oversized cases must first compile as ordinary Dis,
then refuse scalar output with no leftover image.

The ordinary assembler supplies an independent old-PC/table-count product for
checking every ordinary branch after expansion. The GOTO count includes its
extra default cell; the oracle does not assume the C lowering's private nlab.
A stale but numerically valid old target is injected into decoded output and
must be caught:67 active controls. Ordinary Dis remains byte-identical before
and after scalar compilation. Complete diagnostic rows must match every emitted
PC/opcode. An optional external reader must admit each image; it does not execute
them or claim general Dis64 execution.

Native fast-path and complete JIT+C counts remain unchanged. Full shell/imports,
file CALL/MCALL and managed argument/result graphs still require separate work.
