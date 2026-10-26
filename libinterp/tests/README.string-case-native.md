Bounded native string case dispatch
==================================

AMD64 and ARM64 now publish an ICASEC leaf for empty tables and an exact
lower-label owner identity in a single-row table. This raises guarded opcode
availability to 164 of 173 active commands (94.8%), with nine interpreter-only
commands: ALT, NBALT, SPAWN, LOAD, MSPAWN, EXIT, SEND, RECV and RAISE.
It is not a general native string comparison/search implementation.

The native path reads declared IBY2WD cells independently of host WORD width.
Empty tables work with cell4/cell8. A nonempty table requires cell8 to contain
native pointers. Counts, defaults and selected destinations use the full cell
width; nonzero upper count bits cannot slip through a 32-bit limit check.
Negative/large counts, distinct owners, multiple rows and legacy compiled
modules decline without VM writes. C remains responsible for their comparison,
search or ordinary exception behavior.

Pointer equality alone does not bypass String validation. Equal owners reject
C-null, wrapped header spans, INT_MIN length and capacities below the absolute
length, matching stringcmp's checks before its identity return. H identities
skip header access. No character payload is read. An equal lower label selects
its row before inspecting the upper label, matching the C binary search even
for a poisoned upper bound. Empty tables do not inspect String headers.

The selected PC is checked against the live Module program/count and native
address arithmetic before publication. All guards precede REG.PC writes.
There are no runtime callbacks, stack frames, allocations, GC/refcount writes
or FP instructions. Native arithmetic guards do not establish that arbitrary
addresses name mapped allocations. The declared compiler/runtime table width
must match; this does not migrate bytecode or complete the NC64 VM ABI.

AMD64 case guards now use checked full displacements to a shared rejection
return. Extra String checks and distant private fields no longer make a short
rejection transfer exceed its range. Numeric case regression tests cover this
shared change. The production 128-unit code capacity remains unchanged.

Validation
----------

* `python3 libinterp/tests/string-case-native.py`: 8,408 host checks across
  independent WORD32/64 and cell4/8 models, comparing production C with real
  AMD64 RX execution where applicable. This includes identity/Rune/empty/H,
  distinct equal strings, invalid headers, byte misalignment, selected/default
  PC bounds, poisoned upper labels and existing C error probes.
* ASan/UBSan/NDEBUG successful subsets: 1,920 checks. The full nonlocal VM
  exception matrix uses UBSan. LeakSanitizer process inspection is unavailable
  in this sandbox; AddressSanitizer checks remain enabled.
* `python3 -O libinterp/tests/string-case-jit.py`: 12,672 AMD64/ARM64 instruction
  cases, plus sizing/canary probes for invalid String layouts and insufficient
  capacity. Compact/distant fields, full-width counts, complete data images,
  callee registers, stack and FP state are checked independently.
* Shared numeric tables retain their 50,624 two-architecture instruction cases
  and the existing targeted numeric-case instruction regression.

The host checkpoint includes the new native driver; --jit adds the instruction
matrix. Physical ARM64 execution and full Inferno build/boot remain separate.
