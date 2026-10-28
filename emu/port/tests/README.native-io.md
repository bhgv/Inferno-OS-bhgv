# Native-width I/O replies at int-result library and module-loader boundaries

Run `python3 emu/port/tests/native-io.py` and
`python3 libinterp/tests/readmod-io.py` from the repository root.
The first driver tests the real libread/libreadn/libwrite bodies from both
emu/port/discall.c and os/port/discall.c. NATIVE_IO_PLATFORM selects emu and/or os;
NATIVE_IO_SOURCE selects alternate source; NATIVE_IO_CASE selects reply, stream
or span. NATIVE_IO_OP and NATIVE_IO_REPLY isolate a reply-matrix operation/vector.
The module driver supports READMOD_IO_SOURCE, READMOD_IO_CASE=wide and an optional
READMOD_IO_REPLY index for isolated native-result replay.

## Production changes

The library ABI returns int counts, while kread/kwrite return native long counts.
The single-operation bridges previously narrowed the result before returning it.
A nonrepresentable negative service result could therefore become a successful
positive result, and an oversized positive reply could appear valid after
narrowing. libreadn also accepted a long request even when its int return type
could not represent a successful complete transfer.

Both runtime copies now use the same checks. A nonnegative request must fit the
int-result ABI, and a nonempty buffer must have a usable complete native span.
Invalid requests return -1 with a diagnostic before releasing the VM lock or
calling a native service. Zero-count behavior remains unchanged: libread/libwrite
still call the native service, while libreadn performs its balanced lock boundary
without a native read; nil/H buffer identities remain usable at zero count.

Native results remain long until reacquisition and validation. Representable
negative errors are retained; unrepresentable negative errors become -1, never a
positive count or EOF. Positive results larger than the requested amount return
-1 with a diagnostic. libreadn advances only by checked positive replies, retains
ordinary EOF/error prefixes and rejects an oversized reply even after a valid
prefix. Complete and partial valid results remain representable as int.

readmod likewise keeps its native reply as long and compares every positive
count with the remaining image before int narrowing and advancement. Invalid
positive counts abort image acquisition with a diagnostic. No incomplete image
reaches parsemod. Descriptor/image/stat cleanup, lock transitions and the existing
cached-module unload policy remain unchanged.

Well-behaved native services already return a positive count no larger than the
request. The new full-width reply tests deliberately inject contract violations
and nonrepresentable errors; they do not imply that ordinary OS reads return
such values. The oversized long request to libreadn is independently rejected
because the library's actual public result ABI is int. These checks establish
numeric span geometry, not allocation/mapping ownership of arbitrary pointers.

## Verification

| Family | Cases per runtime copy/profile |
| --- | ---: |
| Reply width, 0..64-byte lengths and all eight byte alignments | 15,600 |
| Fragmented reads, each prefix, EOF/errors and oversized later replies | 26,880 |
| Invalid requests and valid zero-count identities | 25 |
| Total | 42,505 |

The two runtime copies yield 85,010 scenarios per profile. Both drivers use real
public native-width declarations and explicit predicates enabled under NDEBUG.
The wrapper fixture checks complete payload/canary images, exact native request
addresses/counts, write prefixes, service call counts, error publication and
balanced release/acquire on every path. Service stubs write bytes only for
in-contract replies, so injected oversized counts do not themselves write beyond
the test allocation.

The module driver retains its 3,267 original fragmentation/failure/cache/dynamic
scenarios and adds 40 full-width reply cases: each tests both
lock modes, new/replaced modules and empty/partial image acquisition. The old
assert predicates have been replaced by active explicit checks and the touched
fixture functions are expanded for readability.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass: 255,030 wrapper scenarios plus
9,921 module scenarios, 264,951 total. The optimized profile treats native
pointer/int conversion diagnostics as errors. LSan process inspection is disabled;
address instrumentation and explicit temporary-owner tracking remain active.
These are LP64 host fixtures, not a booted kernel or a full emulator build.

Eight before-source probes against fb53532 reproduce actual predicate failures:
negative-wide read/readn/write replies in each runtime copy, and positive/negative
wide module replies. Compile errors are excluded from runtime regression proof.
The production bodies are compiled with actual public headers; surrounding native
services, filesystem, parser and dynamic-loader boundaries are controlled fixtures.
Native bootstrap and readiness pass; bootstrap regenerates the real parser/decoder
and compiles all 17 Limbo units. Both drivers are registered in the core suite.

JIT encoder coverage is unchanged: AMD64/ARM64 each retain 173/173 complete JIT+C
entries, 166/173 guarded native fast paths and seven C-only entries. Full 64-bit
VM ABI/boot acceptance remains separate. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are unchanged.
