# Native64 Sys consumers and fixed-frame execution

Explicit DIS_NATIVE64_SYS_HEADER selects fresh limbo -H Sys declarations for
both libinterp/runt.c and emu/port/inferno.c. DIS_WORD32=1 is required. These two
complete owning translation units now compile with coherent natural int32/ref64
aggregates and fixed frames. Ordinary runt.h/sysmod.h builds are unchanged.
This is a consumer bridge, not complete Sys registration or system startup.

The first compile exposed missing module constants and an implicit dependency
on sysmod.h's Runtab typedef. The native64 producer now emits integral module
constants and checks their C names against all generated declarations before
output. Exact signed minima use -(magnitude-1)-1 expressions, avoiding host-long
narrowing or an oversized positive signed literal. Big constants use LL suffixes.
Byte constants retain their eight-bit value. String/real constants are not added
by this integral-constant change. The active probe checks int32 min/max, big
min/max and byte255, including actual C expression widths; a descriptor/constant
name collision is refused before output. Default -a output is unchanged.

runtab-legacy.h supplies the unchanged legacy constructor record only when the
experimental Sys header excludes sysmod.h. Its fixed map[16] is not used as a
native64 adapter. The real builtinmod64 path and complete map8 metadata remain
separate. No shared format, signature hash or bytecode ID changes.

## Variadic boundary

Sys has39 fixed functions and four variadic functions. Current -H declarations
only describe the variadic fixed prefix; no tail layout, dynamic root map or
structural variadic call contract has been agreed. Experimental Sys_print,
Sys_fprint, Sys_sprint and Sys_aprint therefore report an explicit VM error
without reading the supplied frame, allocating or touching result owners.
They are still absent from fixed callable specification/implementation tables.
The legacy implementations remain selected in ordinary builds.

Experimental sysmodinit also refuses before sysinit or legacy registration.
The existing startup guard remains. Do not set this header option on only one
object of a running VM and assume the remaining builtin closure is compatible.
All relevant runtime objects and generated declarations must be rebuilt coherently.

## Actual tests

```
python3 utils/tests/native64-sys.py --limbo /path/to/fresh/source-built/limbo
```

The driver generates actual Sys declarations, compiles complete runt.c and
emu/port/inferno.c and validates the whole Sys geometry and all39 fixed specs.
It then executes these actual handlers with the generated frames:

* read/readn/write/pread/pwrite:230,400 count/metadata/short-transfer/alignment
  cases per model, plus40 early-refusal cases. Exact owning function/helper
  bodies run with bounded native I/O/error transport. Execution is a source
  closure here because seterror belongs to the owning OS translation unit;
  the complete owning translation unit is independently compiled.
* utfbytes/byte2char/char2byte:1,115,061 scalar/bounds cases per model. These
  functions are linked from the actual complete runt.o, with real lib9/rune.c;
  only array/error boundaries use the existing bounded fixture.
* Four variadic and one registration refusal run from the actual owning objects
  with real kprotect and a bounded process/error transport. An invalid frame
  pointer1 cannot be read; module state, allocation count and error depth remain.

WORD32 and WORD32+file pass O3/NDEBUG/strict UBSan; checks stay active despite
NDEBUG. Full native64-layout now includes this gate and passes O1/strict UBSan,
including Math integration, complete geometry, maps, roots, constructor and
constant/collision negatives. Legacy Sys UTF and both emu/os I/O models pass
with active checks. Both owning Sys units also compile using NDK r28c for
Android ARM64/x64. Cross compilation does not establish target execution.

## AddressSanitizer limitation retained

The additional O3 address+undefined run passes I/O but fails the UTF error-unwind
part with a stack-redzone report, after ASan reports ignoring
__asan_handle_no_return because its stack top is zero. Isolated alignment,
scalar and malformed-input modes pass; bad-return/bounds modes reproduce it.
Symbolized local offsets identify the frame-copy path reached through those
setjmp/longjmp error cases. This is consistent with the existing optimized
unwind instrumentation limitation, but is not declared a proven false positive
or a completed ASan gate. No memory check is disabled to obtain a pass. Ordinary
and UBSan success do not erase the unresolved sanitizer report.

## Remaining work

Sys descriptor initialization/ownership, complete fixed-function registration,
variadic tails, shared structural reference/call signatures and remaining builtin
initializers still need integration. The fixture does not run a collector or
Dis MCALL/JIT transitions. No native64 full boot, physical target/APK readiness,
new managed-reference file admission or NC64 default change is claimed. Existing
43-op np0 file admission and JIT fast-path counts remain unchanged.
