# Series0077: scalar slot repair and actual typed operand facts

Source with strengthened sentinels: `b3ab15a071391d2baaca01b7f760b7c37068e95f` on `experiment`.
Initial helper/adapter implementation: `006e1df5434983e109442c16fab712799d90d9b0`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41460438640

## Concrete repair and adapter

The shared scalar slot helper now rejects width0/unsupported widths before
modulo and memory offsets above UINT32_MAX before narrowing long. On LP64, a
direct memory offset0x100000040 previously wrapped to byte64. The compact wire
reader already limits operands to signed30; this repairs a direct helper/API
boundary, not a claim that current compact files contain such wide values.
All existing callers use width4/8, including Limbo copy lowering. Valid scalar
file semantics and bytecode emission remain unchanged.

The new private adapter parses actual compact scalar code and derives exact
read/write slot IDs, kind, opcode and edge/target facts. Declarations are exact
sorted disjoint AFP spans, capped128, excluding header64 and future result tail8.
Bool/int and big/real width equivalence does not grant type compatibility. Bool
literal sources are0/1. AXNON arithmetic preserves the old-destination read;
Big shifts require Int4 counts and Big8 values. Every instruction is checked,
including malformed instructions after IEXIT.

Scratch and final output are separate; no facts or written count publish until
the entire payload and exact byte extent pass. Source/middle IDs are independent
optional roles, not a packed array; repeated aliased reads remain represented.
Numeric initialization must check every read BEFORE marking a write established.
A slot declaration labelled bool alone is not proof that its value is0/1;
source/body/call invariants still need to establish that property.

## Independent evidence

The Python reference supplies an independent compact encoder/parser and typed
interval/role model.17544 raw payload/layout cases yield707 accepted and16837
refused. Coverage includes all256 mode bytes for supported/selected forbidden
operations, compact signed7/14/30 boundaries, long encodings, both scalar widths,
exact kind/bool rules, old-destination reads, mixed-width shifts, late/unreachable
malformations, truncations/trailing data,128/129 spans and65536/65537 instructions.

The C driver separately probes invalid widths, wide FP/MP/direct instruction
offsets, function-interval refusal and noncanonical unused fields. Twelve active
semantic removal controls weaken narrowing, width, tail, overlap, kinds, bools,
implicit reads, shift type, all-instruction validation, transactional output,
capacity and one-record-overrun publication. Each must fail active predicate
status10; compiler/signal/sanitizer
failures do not count. Invalid-width controls hit a safe immediate predicate
before any possible modulo failure. Local optimized nonrecovering UBSan passes
normal oracle and all controls. Scratch uses0x5a and final output0xa5; a
safe extra-record copy control proves that unused output sentinels are checked.

## Scope and remaining proof

This parses real bytes, but local declarations remain TRUSTED immutable context,
not an accepted serialized local table. The adapter currently handles one whole
scalar payload/common frame context and the existing43-op vocabulary. It does
not yet bind mixed callable-function catalogs, prove source initialization,
propagate numeric initialized sets, classify all callee guarantees or execute
new file calls. A fact-only payload can still require CFG rejection; fact
extraction is not whole-program admission.

The current flat scalar profile need not reserve the future caller result tail
and is not silently routed through this stricter private layout. No loader or
compiler includes the new adapter. Whole-file C/RX call goldens and activation
remain after operand/catalog/CFG composition. No format/opcode/feature/local-
table IDs are allocated; ordinary Dis LEA remains unchanged. FLASHVM-0066 accepts the initial
module-entry IEXIT/callable IRET design, without loader activation. Full complex execution remains in C where appropriate.

| Boundary | Current evidence |
|---|---|
| Catalog geometry |628 cases/two controls|
| Abstract function/call graph |1540 vectors/five controls|
| Preparation/result states |98032 records/13 controls|
| Entry-seeded semantic CFG |4005 graphs/eight controls|
| Actual scalar operand facts |17544 payload/layout cases/12 controls|
| Numeric initialized sets and callable composition |Still required|
| New whole-file CALL C/RX admission |Still closed|

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

Native opcode counts remain unchanged. Shared file admission remains43/173
(24.86%); these opcode-entry percentages are not overall runtime/format readiness.
The target artifacts are Linux restricted scalar consoles, not Android APK/GUI
or general managed application acceptance.

FLASHVM-0066 independently replays17544 operand cases/all12 controls at b3ab15a
and4005 CFGs/all eight controls at9677ed5 under optimized nonrecovering UBSan.
It confirms exact kind/span rules, old-destination reads and Int4 Big-shift
counts. This peer-local evidence remains separate from our target CI runs.

The next numeric dataflow must INTERSECT initialized slot sets at CFG joins,
check every read before writes (including aliased old destinations), seed only
proven incoming arguments, and initialize caller results only after proved callee
return guarantees. Prepared ownership/type joins remain their separate strict
state boundary. Declarations remain explicitly trusted canonical compiler/catalog
facts; no wire local-table section is inferred.

## Cross-target acceptance and downloads

All three jobs pass for strengthened source b3ab15a in run41460438640: native
Linux ARM64/x64 and Linux RV64/QEMU. Each passes17544 operand/layout cases
(707 accepted/16837 refused),12 semantic controls and direct slot-guard probes
with long-bytes=8. Previous4005 CFG/eight controls,98032 state/13 controls,1540
flow/five controls and628 catalog/two controls pass too. The x64 C fixtures use
optimized nonrecovering UBSan. The earlier run for006e1df was superseded before
acceptance by the stronger sentinel/control checkpoint; it is not counted here.

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41460438640/artifacts/13150145983) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41460438640/artifacts/13149213243) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41460438640/artifacts/13149601280) |

All archives were downloaded and checked against ZIP SHA256 metadata. Extracted
consoles are ELF64 machines183/62/243. Logs retain241 helper sources/482 whole
C/JIT processes per target,21504 fresh-message cases,5376 caught-handler cases
and896 terminal-call cases. Packaged x64 loop/case/procedures additionally replay
locally with JIT off/on: six clean expected-module/exception/scheduler checks.

The same procedures.dis64 SHA256 appears on all targets:
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
Artifacts contain the Linux console, existing restricted-profile examples and
complete logs, with14-day GitHub retention. No Android APK/GUI or new file-call
admission follows from these results.
