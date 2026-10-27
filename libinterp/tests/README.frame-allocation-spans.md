# Priority FRAME/MFRAME allocation: native fast path plus C

AMD64 and ARM64 keep 166 guarded native commands and all 173 complete JIT+C
entries. This series repairs FRAME/MFRAME rather than counting checks as new
native opcodes. Native allocation stays pointer-free and inside the current
extent; mapped initialization, named exports and stack growth remain in C.
RV64 backend changes remain deferred. See the generated
[per-command table](README.jit-readiness.md) and [CSV](jit-readiness.csv).

## Native preflight

Both emitters now bound source/index/output cells before access or publication,
check the module/link prefix, and validate the entire selected Type-table cell
or Modl import record. Indexed address formation detects unsigned carry before
any record read. Type size/np fields must fit and the Type must have native
pointer alignment, matching the C descriptor contract. Operand cells and stack
frames themselves may be byte-aligned.

All checks precede SP, Frame.t, Frame.mr or output stores. The exact-capacity and
atomic short-buffer contract is unchanged. Bodies remain inside the existing
128-unit production budget; the old standalone FRAME/MFRAME fixtures now use
that real limit instead of their historical 64-unit cap.

The strict extent threshold is preserved: Type.size must be smaller than the
remaining bytes. Equality follows C extension. Metadata, count and destination
are captured before writes, including the deliberately aliased REG/frame case.
Unused frame bytes and unrelated VM state remain unchanged on a fast success;
a refusal makes no VM writes and leaves the ordinary handler authoritative.

## C descriptor and publication fixes

framefits bounds aligned Type fields before typed reads. It uses uintptr values
for stack ordering/subtraction, avoiding subtraction of unrelated pointers or
an unrepresentable signed pointer difference. Missing/H/reversed endpoints,
undersized descriptors and negative maps retain the VM error boundary.

Mapped descriptors are checked before any allocation, cursor change or initmem:
map-byte count must fit the frame, map address arithmetic must not wrap, and
every marked native pointer must fit completely within Type.size. Partial final
pointer cells cannot be initialized across the end of the frame.

FRAME/MFRAME validate decoded cells and capture the output address before
growth or header writes. Inline headers use memmove instead of typed Frame
stores, including on unaligned stack addresses. Captured size and np are not
reread after aliased stores. The output capture is observable when Frame.t
replaces REG.d and Frame.mr replaces REG.s: publication must still use the
original decoded destination, not the newly overwritten register value.

These are numeric span and descriptor-shape checks. They cannot prove that an
arbitrary fabricated address is mapped, owned by a VM object or writable; they
do not validate every possible alias or finish the overall VM/Dis64 ABI port.

## Evidence

| New matrix | Primary cases | Scope |
| --- | ---: | --- |
| C/RX WORD32 | 54,604 | 54,336 successful routes, 190 errors, 78 integer comparisons |
| C/RX WORD64 | 54,620 | 54,336 successful routes, 206 errors, 78 integer comparisons |
| AMD64 guest | 16,320 | Both WORD models, shifted layouts, carry, alias, canary and FP checks |
| ARM64 guest | 16,320 | Same independent oracle and both Modl frame positions |
| Total | 141,864 | Cases, not individual predicates |

Successful C cases sweep all byte alignments of decoded cells and stack frames,
output/source aliases with headers, mapped output cells, REG-field aliases,
negative export ordinals, map initialization and strict extension fallback.
The driver extracts the real initmem body; an instrumented extension boundary
models callback selection only. Actual allocation, copying, failure unwind and
stack ownership remain covered by stack-startup.py and stack-lifecycle.py.

The integer comparison matrix does not dereference synthetic stack addresses.
Guest probes use operands above 4 GiB, mapped high prefixes and carry cases,
all insufficient capacities, complete images and no even-same-value stores on
refusal. Canonical high x86 addresses are mirrored at Unicorn's 52-bit physical
alias, as in the control-transfer checkpoint. This is an emulator fixture detail,
not a claim about physical addressability on a target machine.

Nineteen selected configurations pass, including the existing FRAME/MFRAME
native/guest matrices, module types, actual emu/os stack startup, stack lifecycle,
previous CALL/MCALL/RET regressions, all-byte native/complete binding, publication
in both WORD models and readiness. The full production xec.c compiles as a host
object in both WORD models with implicit declarations treated as errors.

Parent 47b4954 fails 23 selected probes: nine C invalid-input families, unaligned
frame stores, destination capture, and six guest selections per architecture for
slots, entry carry and alignment. Main synchronous C/error matrices pass
UBSan/NDEBUG. ASan/UBSan passes successful C/RX routes, real mapped initialization
and pure integer comparisons with FRAME_ALLOCATION_NO_ERRORS=1. VM-longjmp
error cases remain in UBSan because of the documented sanitizer stack-shadow
limitation; no stack instrumentation is disabled. Physical ARM64 execution and
a full Inferno boot remain unverified. Tk, security/crypto and excluded JIT
architectures are unchanged.

```sh
python3 libinterp/tests/frame-allocation-native.py
python3 -O libinterp/tests/frame-allocation-jit.py
FRAME_ALLOCATION_NO_ERRORS=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/frame-allocation-native.py
python3 libinterp/tests/frame-native.py
python3 libinterp/tests/mframe-native.py
python3 libinterp/tests/stack-startup.py
python3 libinterp/tests/stack-lifecycle.py
python3 libinterp/tests/jit-readiness.py --check
```
