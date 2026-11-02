# Series0075: prepared scalar-call state and strict joins

Source: `71a259ab494a0301661231018c886c2fcd4e3159` on `experiment`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41440788038

## Concrete change

The new private `include/dis64-local-state.h` defines executable abstract
transitions for preparation, exact typed argument stores, result-address binding,
successful call return, result reads/stores, abandonment, return and failure.
It keeps the current function's incoming result-assignment fact separate from
its pending/completed outgoing call result. A new preparation invalidates the
previous result. Nonvoid calls require a binding and every argument established.
Kinds are exact; width equivalence cannot substitute bool/int or big/real.

Bind checks the accepted future scalar convention: dedicated final caller cell,
prepared callee destination byte32 and address width8. No ordinary LEA semantics
are changed. Abandon/failure cannot expose partial results, and terminal paths
cannot resume. Joins compare complete live state, including site/callee,
initialized argument set, binding, outgoing kind and incoming assignment.

State is fixed unsigned32 storage independent of VM WORD, with64 arguments in
two masks and preparation identities bounded65536. Facts and state shape are
rechecked before each operation. No heap allocation, recursion, managed pointer
or format registry ID is introduced. Output remains unchanged on refusal;
step supports in-place output, and joins can alias either input.

## Independent oracle

The Python reference uses immutable owners and argument SETS instead of C masks
and a mutable candidate. It supplies98032 records:20250 accepted and77782 refused,
including57330 joins. Every record checks disjoint guarded and in-place outputs;
joins also alias their right input. Coverage includes all scalar result kinds,
argument counts0/1/2/31/32/33/63/64, both mask halves, repeated typed stores,
missing setup, old-result reuse, bool0/1 boundaries, each changed state field,
terminal/repeated retirement, geometry/descriptor limits and deterministic traces
that continue after refused events.

Thirteen semantic removal controls weaken argument completion, binding, result
invalidation, argument kind, bool range, return assignment, terminal exclusion,
join identity, canonical unused fields, single preparation, address destination/
width, frame identity and result kind. Each must fail an active predicate with
status10; compiler, signal and sanitizer failures do not count. Local optimized
nonrecovering UBSan passes the oracle and all controls. The classic native64-port
matrix includes the same gate/control set on native ARM64/x64 and RV64/QEMU.

## Shared format boundary

FLASHVM-0064 reviews this exact source, independently replays the1540 flow and
98032 state records with all five/thirteen controls under strict UBSan, and
accepts both trusted abstract layers as the conservative initial subset policy.
This peer-local replay is separate from our target CI evidence. The initial
format choices remain those accepted in FLASHVM-0063. This is a private executable
state MODEL, not decoded operands, a whole-CFG verifier or actual CALL execution.
Call assumes a verified successful callee result; Store assumes an established
source value. The event's concrete bool value is checked, but this does not
inspect arbitrary dynamic arithmetic. Canonical supplied state alone is not
proof of entry-seeded history.

Next: derive facts from the accepted catalog and checked operand roles; propagate
states over owned CFGs; prove local spans/definite assignment and setup-interior
entry exclusion; compose callee return guarantees. Whole-file C/RX positive/
refusal goldens must precede activation. Shared file admission stays43/173 scalar
opcodes (24.86%); file FRAME/CALL/RET/LEA remain closed. Ordinary Dis, complex C
handlers and JIT fast-path counts are unchanged.

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

These counts describe guarded opcode entries, not full runtime or file-format
completion. This is a Linux scalar console checkpoint, not an Android APK/GUI
acceptance or general managed/imported application boot.

## Cross-target validation and downloads

All three CI jobs passed for the source SHA above: native Linux ARM64/x64 and
Linux RV64 under QEMU. Each executes98032 state records (20250 accepted/77782
refused,57330 joins) and all13 semantic removal controls, plus the preceding
628 catalog cases/two controls and1540 control-flow cases/five controls. The
x64 C fixtures use optimized nonrecovering UBSan.

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41440788038/artifacts/13140126877) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41440788038/artifacts/13140683919) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41440788038/artifacts/13140361079) |

Every archive was downloaded; ZIP SHA256 matches GitHub metadata and extracted
consoles are ELF64 machines183/62/243. Logs retain241 helper sources/482 whole
C/JIT processes,21504 fresh-message cases,5376 caught-handler cases and896
terminal-call cases per target. The x64 packaged loop/case/procedures additionally
replay locally with JIT off/on: all six report expected module, empty exception
and empty scheduler after retirement.

Packaged procedures.dis64 bytes agree on all three targets:
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
GitHub retains these artifacts for14 days. Each archive contains the Linux
console tarball, existing restricted-profile examples and complete gate logs.
No new file-call opcode admission or Android APK/GUI acceptance follows.
