# Checked module links, exports and cleanup

## Resolve before changing VM state

mcall indexed Modlink.links without checking nlinks. Both mcall and mframe
narrowed the VM WORD to int and indexed negative export references directly;
large WORD64 values could become another index, and out-of-range exports
could read beyond the name-terminated table. Forming -MIN_WORD was also not
a defined way to compute the export ordinal.

A shared internal modulelink resolver now keeps the full WORD operand. It
checks nonnegative imports against nlinks, rejecting negative counts, and
walks the export sentinel for negative operands. The unsigned complement of
the original negative WORD gives the zero-based ordinal without negation.
Absent Module/export tables are rejected. mframe retains `invalid mframe`;
mcall uses `invalid mcall`; H module operands retain exModule.

mcall resolves and rejects empty/H entry addresses before writing Frame links,
switching R.FP/R.M or incrementing the module reference. mframe resolves and
rejects empty/H frame Types before changing the stack or destination. This
preserves pending call/frame state on these lookup errors. Existing valid
import/export and runtime-module behavior is retained. The helper does not
validate arbitrary non-nil pointers or tables missing their required sentinel.

Export lookup is linear in the requested ordinal, bounded by the actual
sentinel. No ABI field or stored export-count metadata was added.

## Guard the native fast paths

Nonnegative immediate mcall/mframe still use native IA-32/ARM paths, but now
first check the operand against H, compare nlinks with the index, and reject
nil/H entry or Type slots. Invalid cases use the checked VM helper through
punt. The guard precedes pending Frame writes and Type dereferences. The
helper bridge publishes the error PC and provides the established NEWPC/
TCHECK behavior. Successful guards skip the helper and keep the native path.

The guard checks declared metadata bounds, not the actual allocation length
of a corrupt Modlink. Its native loads still assume valid VM objects. Signal
handling and full loader/unloader integration are separate concerns.

## Missing export and cleanup

linkm stopped scanning at the terminal Link but tested only whether the Link
pointer was nil. A missing name could therefore use the terminal record's
signature and fields; with signature zero it could even be accepted as an
empty import. The lookup now recognizes the name sentinel and an absent table,
reports the existing not-implemented diagnostic, and leaves the target slot
unchanged. Signature matching for a found name is preserved.

destroylinks also tolerates an absent table and clears Module.ext after freeing
the names and table. This makes subsequent cleanup harmless and prevents the
Module from retaining a freed export-table pointer.

## Checks and boundaries

```
python3 libinterp/tests/module-links.py
python3 libinterp/tests/jit-frames.py
```

- module-links runs 196,662 cases per WORD32/WORD64 operand model (393,324
  total). It extracts the real resolver, mcall, mframe, mspawn, ret, linkm, destroylinks and
  runtime kprotect; VM errors unwind through real emulator Proc frames.
  It covers every signed 16-bit invalid link, MIN/MAX WORD and values beyond
  int width, valid imports/exports, nil modules and absent exports, invalid
  entries/Types, exact stack-limit extension, runtime-module cleanup and
  reference balance, missing-name/signature diagnostics, and repeated cleanup.
  Lookup errors compare the full REG, pending Frame, stack bytes, destination
  and module reference count before/after. Allocation, frame initialization,
  runtime function and GC bodies are recording stubs. Struct/pointer layout is
  the host layout, with explicit WORD32/64 operands; this is not Dis64 enablement.
- jit-frames now executes 296 IA-32/ARM cases. Existing stack-limit/wrap,
  initialization/extend and non-nil module-call dispatch checks remain.
  Invalid link guards cover direct/module/indirect operands, zero/negative
  counts, count boundaries, index 32767 and nil/H slots. Memory-read hooks
  reject any slot read when the index is outside nlinks; pending Frames and
  canonical FP remain unchanged. It also exercises the successful Type guard.
  The guard's punt body is a recording boundary; jit-handoff separately runs
  the production helper bridge. The full native mframe lifecycle is not run.

Separate negative controls reproduce missing-export acceptance, WORD64-to-int
narrowing and native nil/call failure after disabling each architecture's guard.
The host optimizer folded a direct `-index-1` experiment into a complement;
a separate volatile intermediate made the MIN negation visible to UBSan.
The normal suites execute unsigned complement arithmetic under UBSan.

These checks do not boot Inferno, prove arbitrary object-pointer validity,
execute target C prologues/GC, or integrate the entire loader/unloader and
module return lifecycle. They do not benchmark the additional native guards
or export scanning. No target ABI or backend availability was changed.

All 34 libinterp Python suites pass. The 296 native frame/guard cases also
pass under __ARM_ARCH=4. Full xec/link/IA-32/ARM syntax checks pass with Linux
headers; git diff --check is clean.

Permanent RET continuations, frame/module release order and checked mspawn
are covered in [jit-return-audit.md](jit-return-audit.md).
