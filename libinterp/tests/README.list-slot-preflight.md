# List, raw-copy and Type-check decoded-slot checkpoint

The C interpreter and AMD64/ARM64 emitters now preflight decoded slots for eight
existing native commands: HEADB, HEADW, HEADL, HEADF, LENL, HEADM, MOVM and TCMP.
The checks reject null/sentinel slots and spans whose exclusive end would wrap
before issuing the corresponding load or store. They check native pointer widths
independently of WORD32/WORD64 scalar widths.

Scalar heads and LENL validate the result first, so an invalid output cannot
trigger an otherwise unused list read. HEADM validates its List-owner slot and
owner identity before checking its count slot. MOVM validates its count before
consulting source/destination data. Zero counts still ignore data/output
identities, and negative counts fail before accessing data. HEADM still needs a
nonnull, non-H List identity even for zero counts. TCMP preflights both owner
slots while retaining its existing H-source and backward Type-field rules.

These arithmetic checks cannot establish that an arbitrary nonwrapping address
belongs to a mapped allocation. They do not change the native copy-size budget,
list traversal budget, ownership behavior or the remaining interpreter paths.

Validation:

* All 129 selected host core drivers passed in one complete checkpoint run.

* `list-slot-native.py`: 9,604 cases across WORD32 and WORD64, including exact VM
  errors, complete REG/image preservation on failure, zero/negative copies,
  wrapped List fields, and successful unaligned/overlapping scalar heads.
  Passed with NDEBUG, ASan and UBSan; the host sanitizer emitted its existing
  no-return stack warning around the setjmp fixture.
* `list-slot-jit.py`: 12,188 cases across AMD64/ARM64 and both WORD models.
  Independent read allowlists catch premature metadata/data reads; store hooks
  catch even same-value writes on decline and stores outside successful result
  spans. Complete images, REG bytes, return addresses, FP control/sticky flags,
  overlap direction and native copy-budget boundaries are checked.
* Emitter output/sizing agreement, every insufficient output capacity and the
  exact-capacity trailing canary are checked for all eight commands/models.
* The existing typecheck/headcopy/listlen/head/copy leaf matrices passed 41,762
  instruction/ABI/alias/oracle cases across AMD64, ARM64 and RV64. The incoming
  optimized owner checkpoint also passed both priority backends.
* All eight per-command C baseline probes and all sixteen per-command/backend
  JIT baseline probes fail on the previous implementation, confirming that the
  new checks detect the original defects. Predicates stay active under NDEBUG
  and Python -O.

Both new drivers are included in `slice-tokenize-suite.py`. Physical ARM64 and a
complete Inferno boot remain separate acceptance checks. Opcode availability
remains 164 native paths and nine interpreter-only commands out of 173 active
commands on both AMD64 and ARM64; these paths remain guarded/partial.

The overlap-copy read oracle admits both byte tails and complete eight-byte
blocks wholly inside the requested source span. Optimized copy kernels must
not be rejected merely for using qword loads; crossing either source boundary
remains forbidden, and refused copies still admit no payload reads or writes.
