# Allocated byte formatting, kernel parity and Rune endpoints

Run `python3 lib9/tests/allocated-format.py` from the repository root.
ALLOC_FMT_LIBRARIES selects lib9 and/or libkern. ALLOC_FMT_CASE selects growth,
failures, views, flush or rune. ALLOC_FMT_VSMPRINT/FMTSTR/SMPRINT and
ALLOC_FMT_RUNEVSEPRINT select alternate production sources for isolated replay.

## Production changes

The allocated byte formatter now uses the same source in lib9 and libkern.
The former kernel copy cast capacity pointers directly to int, lacked the host
size guard and computed a cursor difference after moving realloc had freed the
old allocation. Both implementations now:

- validate byte mode, positive bounded capacity, exact reserved-NUL endpoint and
  the current cursor before growth or narrowing a pointer difference;
- capture the cursor before realloc, retain the first 256-byte expansion from the existing
  32-byte initial allocation and grow geometrically thereafter, and cap the byte allocation at INT_MAX;
- publish a new base/cursor/capacity only after successful growth;
- free the old owner and clear dead coordinates after failed realloc, retaining
  the existing consume-on-allocation-failure contract;
- reject nil/H format identities before allocating or dispatching, and release
  the owned result on ordinary formatting or final-termination failure.

fmtstrinit rejects a nil Fmt before initialization. Byte fmtstrflush checks mode
and ordered non-null start/to/stop coordinates before its NUL store. Invalid
public flush views retain caller ownership and receive no writes. This differs
from vsmprint's private failure path, which releases the buffer it allocated.

The Rune endpoint formatter now compares native integer coordinates rather than
relationally comparing unrelated typed pointers. It rejects nil/H buffers or
formats, empty/reversed ranges and misaligned Rune endpoints before pointer
formation, formatter callbacks or output. Valid truncation and terminating-Rune
behavior remain unchanged.

These are state, numeric geometry and lifetime guarantees for ordinary formatter
usage. They do not establish allocation ownership from arbitrary forged pointer
values or sandbox custom formatter functions that corrupt their owner pointer.
No partial-result rollback for arbitrary callback effects is introduced.

## Evidence

The fixture links the real formatting engines from both libraries. The Rune
endpoint implementation is the lib9 source, also exercised with the kernel
format engine where the kernel has no corresponding source file. Its allocator
tracks exactly one owned string and always moves successful reallocations while
preserving the old block on injected failure. Complete strings are compared with
independently generated libc output and each scenario must release its owner.

| Family | Cases per library/profile |
| --- | ---: |
| Exact output and logarithmic growth bound | 5,235 |
| Every allocation failure, bad format and late cursor failure | 13 |
| Private growth view/capacity refusal | 10 |
| Public flush refusal/ownership | 8 |
| Bounded Rune output and invalid spans | 25 |
| Total | 5,291 |

Lengths cover every value 0..1024 plus explicit boundaries through 65,536 bytes,
with five integer values including INT_MIN/INT_MAX. The allocation bound is
2*bit_length(result_bytes+1)+4, deliberately looser than the implemented doubling
sequence so the oracle does not mirror one size policy. It rules out repeated
fixed-size growth on long output; old formatters can produce correct bytes while
failing this complexity criterion.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass with NDEBUG: 31,746 scenarios
across both libraries. Explicit predicates remain active. LSan process inspection
is disabled while selected address/undefined instrumentation and owner accounting
remain active. The optimized profile additionally treats native-pointer conversion diagnostics
as errors. This is AMD64 host execution, not a kernel boot.

Nine old-source selections against e438394 reproduce the intended runtime or
complexity failures: growth/failure/private-view/public-flush on each library,
plus the Rune endpoint. Compile failures are not counted as runtime regression
proof. Moving allocation checks verify cursor preservation; capturing offsets
before realloc separately removes the kernel source's dangling-pointer arithmetic.

Adjacent quote-bound and Unicode formatting tests pass on both engines, including
140,580 UTF vectors per engine. Native formatter arithmetic checks, readiness and
bootstrap pass; bootstrap builds real host tools, regenerates the parser and
compiles all 17 Limbo units. The new driver is registered in the interpreter suite.

No JIT production source changes are included. AMD64/ARM64 entry availability
remains 173/173 complete native+C entries and 166/173 guarded native fast paths.
This does not establish coordinated 64-bit compiler/loader/frame/GC ABI or full
VM boot acceptance. Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources remain
unchanged.
