# String branch operand preflight

BEQC, BNEC, BLTC, BLEC, BGTC and BGEC share corrected operand ordering on
AMD64/ARM64 and in C. Both decoded String-owner slots must fit before either
comparison input is loaded. The native path checks both C-nil owners before
reading either header, matching stringcmp's early nil error even if the other
owner is unmapped or malformed. H remains the language's empty/nil String value.

Target-slot checks occur only after the predicate is true. A false branch does
not validate or dereference an unused nil, H, wrapped, unmapped or mapped target
slot. A taken branch bounds the complete native target slot before loading its
value or writing REG.PC. Source/comparison errors retain priority over target
errors. Target instruction values retain their existing semantics; these
arithmetic guards do not prove arbitrary addresses identify mapped allocations.

The ASCII/Rune comparison, signed Rune keys, identity and empty cases, private
field adapters, conservative owner window and alias behavior remain covered by
existing tests. No FP instructions, execution helpers or new allocation paths
are introduced. Fixtures now use the backend's existing 128-unit private code
capacity without changing production LeafMax or the layout ABI.

Validation across independent WORD32/64 models with native host pointers:

* `STRING_BRANCH_NATIVE=1 string-branches.py`: 1,666,236 existing Unicode,
  alignment and PC checks plus 2,640 new preflight cases. The complete C/AMD64 RX
  matrix passes ASan/UBSan/NDEBUG. Explicit normalized predicates replace asserts
  so allocation/setup and error checks stay active in release-mode fixtures.
* New C/RX probes cover every predicate, byte alignment, both source slots,
  taken/untaken target errors, false unmapped targets and both nil-owner orders.
  Rejected/taken-error cases preserve complete REG, source and slot images.
* `python3 -O string-compare-jit.py`: 79,488 existing window/predicate/image/
  write/FP cases plus 2,592 preflight cases. Read hooks prohibit premature String
  reads and false-target reads, including mapped targets whose loads would not
  otherwise fault. Nil-owner probes forbid all metadata reads.
* `string-compare-layouts.py`: 4,212,600 production sizing/publication/canary
  checks, covering six predicates, both architectures and malformed/distant
  fields. Insufficient capacities must publish nothing.

Previous C and emitters independently fail source-slot and taken-target probes.
Replaying the intermediate emitters before the joint nil check independently
exposes premature first-header reads on both architectures. COMPARE_PROBE_ONLY
and COMPARE_PROBE_FAMILY select instruction probes; BRANCH_PREFLIGHT_ONLY and
BRANCH_PROBE_KIND select C/RX cases. Existing false-target nonaccess remains
verified rather than being replaced with unconditional target validation.

Guarded opcode availability stays AMD64/ARM64 164/173. These are partial native
paths; complete NC64 acceptance, full Inferno boot and physical ARM64 execution
remain separate from host tests and instruction emulation.
