# Priority CALL/MCALL/RET spans and C fallback

AMD64 and ARM64 retain 166 guarded native commands and 173 complete JIT+C
entries. This series repairs three existing fast paths and their real C
handlers; it does not count error rejection as a new opcode implementation.
RV64 backend work remains deferred. The [per-command table](README.jit-readiness.md)
and [CSV](jit-readiness.csv) describe the narrower execution domains explicitly.

## Checks before publication

CALL now bounds the source cell, both frame-link writes and the target cell.
A contained nil/H frame still declines before inspecting an unused target.
The frame pointer and target remain captured before any aliased link is written.
Targets themselves retain the existing Dis-PC contract; this is not general
validation that a numeric target belongs to an executable allocation.

MCALL bounds decoded cells, the module prefix, caller mode field, frame links
and backward Heap origin. Its import entry is formed with carry detection before
the complete entry-field span is read. The index remains full WORD width until
it passes the signed import-count limit. Target, caller, modes, MP and reference
count are captured before the first store. Existing frame/operand aliases and
equal-mode cross-module calls retain their behavior. Named exports, builtins,
mode transitions, errors and complicated lifetime changes retain C.

RET bounds the complete used frame header, Type.np, saved/current module fields
and backward reference origin. The pointer-free fast path still declines before
cleanup that can free an extent, traverse maps or destroy the last module owner.
The longer AMD64 ordinary-return branch uses a checked full displacement instead
of depending on an incidental short-branch distance. Both emitters stay inside
the existing 128-unit production capacity. Two old isolated fixtures now use
that real capacity instead of their historical 64-unit limit.

RET also preserves the C ordering of root/invalid-link failures. A root FP, bad
return PC, H saved module, or required nil/H current module declines before
reading an otherwise unused Type. Guest probes place that Type in unmapped
storage, so moving the check after a descriptor load is observable.

## C agreement and lookup

CALL/MCALL reject malformed cells before memmove; frame-span failures occur
before link/register/reference stores. moduleentry validates complete prefixes
and inline import records, and copies import/export metadata without typed
unaligned accesses. It checks the Module and each named-export Link span while
retaining the existing sentinel and negative ordinal convention. A failed lookup
leaves the caller's Modl output unchanged. This also benefits MFRAME/MSPAWN.

RET checks an extent-header subtraction before forming SEXTYPE, captures np,
module modes and saved MP before cleanup, and validates the current Heap origin
before retaining its numeric reference value. The original root-frame VM error
and ordinary return with no saved module remain unchanged. C still owns all
mapped cleanup, destruction, stack extension and builtin behavior.

These checks bound numeric address arithmetic, not mapping, allocation ownership,
access permissions or every possible alias between separate VM objects. They do
not claim safe execution of arbitrary fabricated pointers or a completed VM port.

## Evidence

| New driver | Primary cases | Scope |
| --- | ---: | --- |
| control-transfer-native.py, WORD32 | 21,504 successful routes + 582 rejected routes + 2,608 lookup cases | Real C, AMD64 RX fast+C, complete cjitexec entries |
| control-transfer-native.py, WORD64 | 21,504 successful routes + 590 rejected routes + 2,608 lookup cases | Same checks with native-width WORD |
| control-transfer-jit.py, AMD64 | 14,720 | Guest span/carry/early-read/full-image/FP checks |
| control-transfer-jit.py, ARM64 | 14,720 | Same independent shifted-layout oracle |
| Main total | 78,836 | Primary cases, not individual predicates |

The C matrix covers all byte alignments of cells, frames, Type/module/export
metadata, exact frame-link aliases, imported/exported calls, ordinary/cross/self
returns, malformed spans and unchanged full images on error. Three routes are
compared: direct C, direct guarded fast plus C, and a complete native entry using
the real shared C executor. No synthetic handler substitutes for these opcodes.

The guest matrix uses independent shifted REG/frame/module/Type layouts, both
WORD models and operands above 4 GiB. It includes mapped canonical high module
prefixes whose entry addition or extent would wrap, exact output canaries for
all short capacities, no VM writes on refusal and unchanged FP control/status.
Unicorn x86 applies a 52-bit physical bus to canonical high addresses; the
fixture mirrors only the valid high prefix at that physical alias. Guest pointer
arithmetic remains full 64-bit. This is not a physical-address claim for hosts.

Twenty-two selected configurations pass, followed by the final RET-ordering
recheck. Adjacent regressions include 12,288 MCALL/RET round trips, 20,736 MFRAME
cases, existing call/return alias and ownership matrices, full module type/import
and stack/startup drivers, all-byte AMD64/ARM64 compiler binding, publication in
both WORD models and readiness/capacity reports. The complete production xec.c
compiles as a host object in both WORD models with implicit declarations rejected.

The parent 6f749bb fails 14 selected probes: eight real C/lookup families and
six architecture/opcode guest selections. The main C matrices pass UBSan with
NDEBUG; the guest drivers also pass with Python -O. ASan/UBSan passes successful
C/RX routes and pure failing lookups with CONTROL_NO_ERRORS=1. Explicit VM
longjmp cases remain in UBSan because of the documented sanitizer stack-shadow
limitation; no stack instrumentation is disabled. Physical ARM64 execution and
a full Inferno boot remain unverified. Tk, security/crypto and excluded JIT
architectures are unchanged.

```sh
python3 libinterp/tests/control-transfer-native.py
python3 -O libinterp/tests/control-transfer-jit.py
CONTROL_NO_ERRORS=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/control-transfer-native.py
python3 libinterp/tests/mcall-native.py
python3 libinterp/tests/return-native.py
python3 libinterp/tests/call-return-native.py
python3 libinterp/tests/jit-readiness.py --check
```
