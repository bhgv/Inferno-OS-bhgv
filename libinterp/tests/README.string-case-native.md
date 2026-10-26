Bounded native string case dispatch
==================================

AMD64 and ARM64 now publish an ICASEC leaf for empty tables and an exact
lower-label owner identity or equal empty values at the first binary-search
midpoint. This retains guarded opcode availability to 164 of 173 active commands (94.8%), with nine interpreter-only
commands: ALT, NBALT, SPAWN, LOAD, MSPAWN, EXIT, SEND, RECV and RAISE.
It is not a general native string comparison/search implementation.

The native path reads declared IBY2WD cells independently of host WORD width.
Empty tables work with cell4/cell8. A nonempty table requires cell8 to contain
native pointers. Counts, defaults and selected destinations use the full cell
width; nonzero upper count bits cannot slip through a 32-bit limit check.
The count span is checked for its full cell width before the load. Nonempty
cell4 tables decline before any row/default load, including at a mapping edge.
Tables up to 4,095 rows probe the same midpoint as C. Negative/larger counts,
distinct nonempty owners, unmatched midpoint labels and legacy compiled modules decline without VM writes. C remains responsible for their comparison,
search or ordinary exception behavior.

Pointer equality alone does not bypass String validation. Equal owners reject
C-null, wrapped header spans, INT_MIN length and capacities below the absolute
length, matching stringcmp's checks before its identity return. H identities
skip header access. Distinct empty owners require zero length and nonnegative
capacity independently; H also compares equal to an ordinary empty String.
No character payload is read. An equal lower label selects
its row before inspecting the upper label, matching the C binary search even
for a poisoned upper bound. Empty tables do not inspect String headers.

The selected PC is checked against the live Module program/count and native
address arithmetic before publication. All guards precede REG.PC writes.
There are no runtime callbacks, stack frames, allocations, GC/refcount writes
or FP instructions. Native arithmetic guards do not establish that arbitrary
addresses name mapped allocations. The declared compiler/runtime table width
must match; this does not migrate bytecode or complete the NC64 VM ABI.

AMD64 case guards and the empty-table transfer now use checked full displacements to a shared rejection
return. Extra String checks and distant private fields no longer make a short
rejection transfer exceed its range. Numeric case regression tests cover this
shared change. The production 128-unit code capacity remains unchanged.

Validation
----------

* `python3 libinterp/tests/string-case-native.py`: 8,552 host checks across
  independent WORD32/64 and cell4/8 models, comparing production C with real
  AMD64 RX execution where applicable. This includes identity/Rune/empty/H,
  distinct equal strings, invalid headers, byte misalignment, selected/default
  PC bounds, poisoned upper labels and existing C error probes.
* ASan/UBSan/NDEBUG successful subsets: 1,952 checks. The full nonlocal VM
  exception matrix uses UBSan. LeakSanitizer process inspection is unavailable
  in this sandbox; AddressSanitizer checks remain enabled.
* `python3 -O libinterp/tests/string-case-jit.py`: 25,856 AMD64/ARM64 instruction
  cases, plus sizing/canary probes for invalid String layouts and insufficient
  capacity. Memory-read hooks reject character/header overreads; count-wrap and
  mapping-edge probes exercise preflight ordering. Compact/distant fields, full-width counts, complete data images,
  callee registers, stack and FP state are checked independently. Multirow probes
  use poisoned nonmidpoint owners/targets and forbid upper-label reads.
* Shared numeric tables retain their 50,624 two-architecture instruction cases
  and the existing targeted numeric-case instruction regression.

The host checkpoint includes the new native driver; --jit adds the instruction
matrix. Physical ARM64 execution and full Inferno build/boot remain separate.

C String preflight
------------------

Production stringcmp checks both complete len/max header spans before any field
read, including equal-owner comparisons. This brings C fallback validation into
line with the JIT. It does not prove an arbitrary pointer names mapped memory.
Production stringdup rejects underflowing heap-owner addresses and wrapping
headers before refcount/length access, then checks the complete ASCII or Rune
source span before allocating the destination. Nil/H behavior stays unchanged.

`python3 libinterp/tests/string-header-spans.py` exercises 764 cases across
WORD32/64: real extreme pointers, a reduced 16-bit arithmetic model backed by
real host storage, invalid lengths/capacities/references, exact copies and zero
allocations on rejection. The full matrix also passes ASan/UBSan/NDEBUG.
Replaying the previous string.c fails the header and payload probes separately.
The reduced domain changes only guard arithmetic, not payload/heap pointers.

The C comparison/branch regressions pass 313,344 span cases and 876,732
Unicode/alignment/PC cases across both WORD models. Native AMD64 string
branches additionally pass 1,666,236 C/RX cases. The span oracle now checks
header wrap even for identities and empty prefixes. These checks and the new
driver remain active under NDEBUG; no new assert is used.
