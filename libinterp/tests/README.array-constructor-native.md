# Native NEWA/NEWAZ owner reuse and destination preflight

AMD64 and ARM64 now have guarded native paths for INEWA and INEWAZ. Availability
rises from 147 to 149 of the 173 active Dis opcode IDs; 24 remain interpreter-only.
RV64 counts are unchanged. Availability is conditional on operands, not complete
native implementation of every allocation or element type.

Both commands resolve the current Module.type table with a full-WORD index.
The destination must already hold a unique builtin Array owner with the exact
requested element descriptor and element count, no backing root and np == 0.
NEWA preserves unspecified scalar bytes. NEWAZ zeros the validated payload.
Neither path allocates, changes reference counts, runs finalizers, recolors the
heap or changes profiling/collector state. Shifted owning storage remains valid
when its data begins after the array header and its declared span does not wrap.
Views, shared owners, mapped elements and incompatible destinations allocate
through the previously checked C heaparray path.

Native leaves bound both element width and total payload to 256 bytes, reject
nonpositive/saturated descriptors, and preserve the original constructor's
reference-reservation rules. The special case where the element descriptor is
Tarray itself retains C. The interpreter supports larger eligible reuse using
acheck's full allocation/WORD bounds. Both implementations capture count and
index before zeroing, and restore an output slot overlapping the scalar payload.

Nonempty owning data must follow the complete Array header, be different from H
and have a nonwrapping end. This prevents zeroing metadata through a malformed
data pointer. Empty and zero-width arrays do not inspect data: even INT_MAX
zero-width elements require only metadata operations. Validation detects bad
geometry and address arithmetic; arbitrary unmapped pointers remain outside the
valid VM object contract.

The C constructor now checks the old destination before allocation, including
nil, low/unaligned owners, zero heap references and invalid reusable data spans.
Previously an invalid owner could be discovered only after allocating/publishing
its replacement. Error paths now leave the destination, references and pool
unchanged. Allocation failures for ordinary non-reusable owners also preserve
all prior state. Type/reference/geometry error precedence is retained.

Evidence:

- array-reuse-native.py compares production C with actual published AMD64 RX
  code: 67,584 owner/type/zero/offset/alias/failure/FP cases across WORD32/native
  pointers and WORD64. Another 40 cases check destination errors before pool
  access, and 12 check metadata-only INT_MAX arrays with inaccessible data.
  The preflight oracle fails against the parent interpreter implementation.
- array-create-jit.py executes production AMD64/ARM64 instructions in Unicorn:
  357,276 type-table, owner, count, span, high-address, write-bound, saved-register,
  FP-state and atomic code-publication checks. Both WORD models and commands run.
- array-create-layouts.py performs 4,212,600 ASan/UBSan layout/capacity/canary
  checks per backend. Failed sizing or publication leaves caller storage intact.
- Successful C/RX resource paths also pass ASan/UBSan. Complete errors run with
  UBSan and NDEBUG. Explicit live counts and trailing allocation canaries cover
  cleanup alongside this container's ASan process/stack-discovery warnings.
- The existing array creation, descriptor-reserve, moving module-type, heap
  bounds, slice/view and native publication regressions remain in the core suite.

The fixed 128-unit private code budget is unchanged: current private layouts
need 119/123 units on AMD64 and 108/112 on ARM64 (raw/zero). Nearby rejection
returns keep the AMD64 sequence bounded. Actual compiler ABI publication is
covered by the C/RX fixture. ARM64 evidence is emulated; physical hardware and
complete Inferno runtime acceptance remain separate.
