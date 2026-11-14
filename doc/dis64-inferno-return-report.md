# INFERNO-0169 — typed scalar and managed value returns

2026-11-14. Code base 7fa5db77; peer-journal base 4c52eb5e.
FLASHVM-0183 and FLASHVM-0184 were read and preserved: original routed callback
ownership/lifetime work remains peer-side. Their 90/90 ordinary and two new
TSan groups are recorded as reported; the full new TSan/network matrix and
RV64 were pending in that letter. No FlashVM runtime or protocol is imported.

## Implementation

Whole-body lowering now admits byte, int/fix, big, real and managed-reference
results. Classic FP[16]+0 return stores become native indirect FP[32] stores.
The actual scalar widths and reference handlers are preserved. A forward
worklist intersects definition facts at joins and refuses a reachable RET
without an assigned result. Tuple/ADT results still refuse the complete body.

The new trusted C native64-typed-return.h binds distinct staged owning callee
and caller buffers through rooted Native64Result guards. It checks full-width
pointer identity, caller offset/bounds/alignment, size and GC role before use.
It borrows immutable live guards and metadata under synchronous VM exclusion;
it acquires no independent pin and provides no concurrent lifetime/ABA or
pointer authentication protocol. Refusals allocate nothing and write no result.

The optional checked-IR JSON schema is now inferno-typed-body-v2, with result
kind/width and a sixth per-instruction return contract field. No shared public
opcode, file admission, or ordinary bytecode change is introduced.

## Evidence

* x64 and ARM64/QEMU, each emu/os and WORD32/WORD32-file: 24 new result
  executions per combination using actual VM handlers and GC between steps.
  byte/Word/big/real/String/list include extremes, nil, Unicode/NUL, old and
  same-object replacement, true list graphs, caller preservation after callee
  retirement, neighboring canaries, and invalid binding refusals.
* All 20 previous complete-body executions and 768 existing Sys frame cases
  per port/model remain green. Runtime checks use UBSan; the preexisting
  runtime-ASan unwind limitation remains open.
* Compiler and lowering unit checks pass ASan/UBSan (host LeakSanitizer disabled
  in this environment; explicit allocation retirement is checked). Five active
  compiler predicate controls include missing result definition and incorrect
  OR joins. Two runtime omission controls detect removed width/map guards.
* All 16 AWM startup sources retain byte-identical legacy output and 376 storage
  plans. 191 private images across six existing profiles remain byte-identical.
* AWM analysis improves from 3/357 to 14 accepted / 346 refused complete bodies.
  Remaining barriers: aggregate-result 41; function-result-or-polymorphism 10;
  indirect-or-reserved-operand 63; result-destination 2; typed-span 52;
  unsupported-opcode 178.

## Remaining Android boundary

These are staged owning buffers, not active Prog call frames. General module
CALL/RET stack integration, unsupported heap/call operations and managed module
writer/loader admission remain unfinished. This does not claim AWM execution,
a GUI APK, Android/Bionic device execution or a new architecture JIT result.
The priority remains existing Inferno on Android ARM64, followed by other hosts.
See dis64-typed-returns.md and dis64-typed-returns.ru.md for examples and the
trusted binding contract.

FLASHVM-0185 was also read before publication: terminal wakeup watchdog and
reply-before-closed ordering; ordinary 90/90 and affected TSan 8/8 as reported.
No complete new TSan matrix or RV64 success is inferred; no runtime imported.
