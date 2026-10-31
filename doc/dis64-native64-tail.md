# Internal typed-tail preflight and dynamic-root checkpoint

FLASHVM-0038 explicitly confirms the representations and staged ownership in
INFERNO-0033. This checkpoint implements descriptor/value preflight and map
composition for that trusted internal prototype. It does not invoke a formatter,
construct a complete variadic call context, admit file calls or enable Sys's
four variadic functions. No wire kind IDs, byte order or feature bits are assigned.

## Private API

Include native64-tail.h after ordinary lib9/interp declarations. Native64TailSlot
contains a private kind, exact contiguous tail offset, extent8 and a trusted Type
identity for managed references. Scalar descriptors must have nil Type. Reference
Type must be neither nil nor H. The C enum's numeric values are never serialized.

native64tailplan checks the shared native64 C model, a callback-free fixed-prefix
specification, all descriptors, private resource limits and total size before
publishing a plan. Offsets are relative to the aligned end of that prefix.
Malformed plans leave output unchanged; oversized counts/extents can be refused
before reading a supplied slot/value span. The caller supplies readable, aligned,
immutable C metadata; this is not arbitrary-address authentication.

native64tailvalues first validates the entire descriptor and exact value-buffer
extent. It then checks all canonical scalar/nil bits before any live-reference
checker is called. Byte zero-extends8; int/fix sign-extend32; big accepts signed64
bits; real accepts exact binary64 bits, including signed zero and NaN payloads.
References accept H or a trusted live native64 VM pointer; nil0 is refused. A
trusted pure, non-allocating callback checks exact Type identity. This service
never retains values, writes a frame, mutates a result or invokes a target.
Readable inputs and Type identities remain caller-owned/rooted during preflight.

native64tailmap derives the full fixed+tail map8 from kinds, preserving fixed roots
and clearing new/padding bits. Frame links/result/reserved bytes remain excluded.
Scalar(-1) is not mapped even though its bits equal H. Capacity/refusal checks
precede writes; bytes beyond the logical map are untouched. Output storage must
be disjoint from borrowed input records/maps. No allocation or ownership transfer
is performed by these helpers.

Limits reuse the private4096-entry/1MiB frame bounds. They are implementation
bounds, not wire ranges. No callback pointer or layout/hash-only structural
signature is accepted from a file.

## One shared C model

native64-model.h now centralizes scalar widths, exact IEEE-double geometry,
natural argument alignment and Frame64 link offsets. Both builtinmod64 and the
private native64frametype descriptor adapter use it; typed-tail preflight uses
the same gate. This avoids diverging packed-layout/WORD-model checks. The shared
checker also requires native pointer-sized uintptr and exact64-bit slot storage.
The existing constructor geometry/refusal behavior remains covered by its gates.

## Active evidence

```
python3 utils/tests/native64-tail.py --limbo /path/to/fresh/source-built/limbo
```

The metadata gate runs152 active checks per compatible model: canonical scalar
bits, exact real bit preservation, live/H/nil reference policy, wrong identities,
late invalid descriptors, overflow/resource limits, truncated buffers/maps and
unchanged outputs/canaries. Wrong WORD64 and packed models refuse before reading
intentionally invalid descriptor pointers. No Type or heap callback is needed
to establish descriptor refusal. Input lifetime/readability remains a precondition.

The dynamic-root gate uses the actual collector, dtype/freetype, heap arrays,
incmem/freeptrs/initmem and external ptradd/ptrdel roots. Only process/pool services
and unrelated generated callbacks reuse bounded native64-gc fixtures. It executes
24 mixed fixed/tail graphs per model across process states and collector phases,
with1,562 active predicates per model. The composed descriptor spans1,272 bytes
and20 map bytes; its final tail reference is in byte20, beyond legacy map[16].
Actual dtype failure and map-copy independence are checked. Before publication,
actual initmem sets every mapped field to H. Existing caller owners remain rooted
while values are checked/copied, then actual incmem acquires frame references.
A nested allocation forces collection; caller roots/owners are retired and later
collections must preserve both objects solely through the composed frame map.
Ordinary freeptrs and descriptor retirement release every acquired object owner.
Scalar H-like bit patterns remain unmarked. Removing either the fixed root or
last tail root reaches the expected active GC predicate failure in a separate
negative-control process; a permissive collector would not pass this gate.

Both WORD32 and WORD32+file pass O1/strict UBSan and O3/NDEBUG/strict UBSan. The
new gate also passes address+undefined instrumentation with explicit
NATIVE64_NO_LSAN for unavailable process leak inspection. Existing bounded GC
services retain their leak-scan limitation; executable-name warnings remain.
This does not resolve the previous optimized UTF/error-unwind sanitizer report.
Full fresh native64-layout includes this gate and complete Math integration and
passes O3/NDEBUG/strict UBSan. Metadata probes also compile with NDK r28c for
Android ARM64/x64; cross compilation is not target execution.

## Remaining call-context work

Preflight/map helpers borrow metadata; they do not yet own the vector/context
through a protected C call. This valid-graph test does not implement cancellation,
nonlocal error unwinding, moving destination resolution or result commit. Existing
protected-result/collector tests remain separate evidence, not proof that the new
variadic call path is complete. Those combined tests and a real typed formatter
adapter are required before removing Sys's refusals. Full fixed Sys registration,
remaining builtins and whole native64 boot also remain pending. Existing Frame64/
map8 file agreements,43-op np0 profile, NC64/SDK defaults and JIT counts stay.

The next [late-result-resolution checkpoint](dis64-native64-resolved.md) combines
valid typed argument roots with producer/resolver error, cooperative cancellation
and mapped-owner relocation. Generic fallible argument-context construction and
real typed formatter execution remain pending.
