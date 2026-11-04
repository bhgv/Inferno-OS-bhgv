# Native64 series0130: private numeric Module construction and actual C/RX execution

2026-11-04, experiment. prepare64numeric builds an owned program, entry, copied
name/path and original ordinal Types over prepared mapped MP storage. It returns
a separate Dis64StorageOwner lease: the caller keeps it until Module/MP has an
independent collector root. Partial failure retires the actual Module and lease.
All image/role/map/native-lowering checks precede allocation. This is a private
native API, not a new readmod route, namespace/cache identity or file launch mode.

Only the MP-only numeric subset of the existing 43 roles is accepted. Every numeric
read/write/RMW/middle span excludes both halves of each mapped reference slot.
FP operands and AXINF are refused; the entry frame has no mapped locals. Nonempty
imports/exports, library/empty code, missing DATA for nonempty MP and unsupported
native lowering are refused. Empty interface tables and unused mapped Types remain
structurally legal. Managed MP slots start at runtime H and are never numeric
operands. Original indices, Frame64, default43 and explicit immutable dis64-calls
admission are unchanged. No new wire/feature/ABI IDs, managed instruction grants,
termination or static division safety promises are introduced.

Local O3/NDEBUG nonrecovering UBSan results:

* Independent raw numeric-map matrix: 1020 rows, 19293 active predicates; five
  exact semantic omission controls.
* Real Heap/Types/complete collector and unload construction closure: 8 successes,
  8 preflight refusals, 148 allocation refusals, 96 collector epochs and 125339
  active checks per WORD32 and WORD32-file; legacy refusal and four exact controls.
  Input bytes are poisoned; source lease and actual modules-list rooting are tested
  separately through retirement and every allocation failure restores baselines.
* Separate actual VM/C/native JIT closure: 15 raw images, 6 completed RX fast
  returns, 1 real decline to C for divide-zero, 12303 predicates. Machine function
  addresses must lie in actual RX mappings. Removing native entries still computes
  the C result but fails the exact RX-completion control after complete cleanup.
  This execution fixture does not run the collector; the collector proof above
  must not be combined into a claim of GC during guest execution or scheduler roots.
* Existing runtime regression: 744 images, 1667800 predicates, legacy refusal.
  Classic mk rebuilt actual load64.o successfully.

Native ARM64/x64 O3 ASan+UBSan positive/control receipt follows publication;
ordinary leak detection remains enabled. No full workflow matrix, Android/AWM,
physical RV64, managed imports/frame-result roots or arbitrary callback/concurrent
collector acceptance is claimed. Native fast counts remain ARM64/x64 166/173,
RV64M 103/173 and C dispatch 173/173. These are opcode counts, not port readiness.

Read coordination portions of FLASHVM-0131/0132/0133 through
 d0dbbc5cac9cc83e41799f27d0070a14eeab134f. Acknowledge the peer's independent
0129 replay separately from hosted evidence. Preserve incoming journal verbatim.
The typed25 baseline stays 7 passes/12 refusals/4 unsupported/2 malformed; Python
module-wide DATA/import/unused-type semantics remains outstanding. Peer guarded
constructors/tails and concurrency stay peer-local. Request review of this private
numeric construction boundary; no shared format change or foreign code transplant.

Next integrated work remains actual launch/import ownership, managed code/frame
roots and graphical target startup. Native console ARM64 already has earlier
build/execution evidence; a working Android/AWM APK has no honest fixed date yet.
