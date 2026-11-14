# Scalar interface requests and namespace library reads

[Русская версия](dis64-scalar-namespace.ru.md).

The scalar library path now joins compiler-generated `.m` requests to namespace
reading, ordinary file admission and whole-provider proof. The importing outer
program in this gate is native C; guest Limbo LOAD instruction admission and a
serialized managed importing client remain next.

## Generate the client interface

For the [Library example](dis64-scalar-libraries.md):

```sh
limbo -64b -I . -o library.dis64 library.b
limbo -H -R -M Library library.m > library-native64.h
```

`-R` is optional and requires `-H -M <module>`. Ordinary `-H` output is unchanged.
The additional output consists of:

- `limbo_native64_Library_scalar_imports`: complete existing Imports payload;
- `limbo_native64_Library_scalar_strings`: complete existing Strings payload;
- `LIMBO_NATIVE64_IMPORT_Library_mid` and analogous macros: request ordinals.

These are interface bytes and indices, with no provider PCs, frame maps or native
function addresses. Every interface function is requested, including unused
functions. Runtime matching resolves the entire table. A missing unused export
therefore fails the load; it is not silently dropped. Ordinals refer to this exact
generated table, independently of provider export order.

Signaturev1 preserves void/int/big/real kinds and parameter order. Equal-width
Big and Real remain distinct. Limits are64 functions,64 arguments per function
and255 ASCII bytes per module/function name. Empty request tables are representable.
Managed/byte/fixed/by-value aggregate/variadic interfaces are outside this scalar
producer. The ordinary native64 header generator retains its wider existing type
support when `-R` is absent. Unsupported scalar-request semantics are rejected
before C output. Existing reserved C-name prefixes remain protected.

The existing `.m` hash constants and native frame/layout witnesses remain in the
header. No32-bit hash is guessed, replaced or used as the scalar file's structural
signature. The header is client metadata, not a provider implementation. Its two
payload arrays are not a standalone module file. No public wire ID changes.

## Read and instantiate

The embedding API is:

```c
Modlink *read64calllibrary(
    char *path, int jit,
    const uchar *imports, ulong importbytes,
    const uchar *strings, ulong stringbytes,
    int sync);
```

Select the existing `dis64-calls` startup policy before the first namespace use.
The reader seals/checks that policy; it never selects it from a filename or file
contents. A default or disabled reader refuses before namespace/constructor work.
The feature is compiled with DIS64_PILOT_LOADER=1 and the compatible WORD32/ref64
model. Paths are nonempty, at most1023 bytes and use ordinary namespace lookup.
Builtin `$...` requests belong to their existing separate runtime path.

After including the generated header, under the required VM context:

```c
ml = read64calllibrary("/lib/library.dis64", 1,
    limbo_native64_Library_scalar_imports,
    sizeof(limbo_native64_Library_scalar_imports),
    limbo_native64_Library_scalar_strings,
    sizeof(limbo_native64_Library_scalar_strings), 1);
if(ml == nil)
    return 0;
/* Transfer ml to a mapped VM root with ordinary reference/barrier rules. */
/* Then remove its one temporary root: ptrdel(D2H(ml)). */
```

Use `LIMBO_NATIVE64_IMPORT_Library_mid` with the existing VM MFRAME/MCALL protocol.
The selected link contains a proved VM PC and the provider's actual frame Type,
not a directly callable C function. Existing Frame64/RET/JIT rules remain intact.
Library construction invokes no function, including one named init.

Input native path/request/name buffers must remain accessible and immutable
throughout the call, including any I/O release. With sync=1, the caller must be
in a VM context allowed to release: the ordinary release/isave and acquire/irestore
boundaries surround only the protected file read. Existing synchronous caller-lease
release guards still apply. With sync=0, exclusion stays held throughout I/O.
The API returns with exclusion held.

The reader owns its Dir, image buffer and FD, joins bounded short reads and checks
all returned counts against the remaining extent. Positive lengths are bounded by
the existing constructor image limit before allocating the image. The FD is closed
before admission. Native kclose consumes its owned descriptor on every status.
I/O/protected callback failures retire the temporary resources.

After acquire, the existing mustbesigned predicate runs before any library proof
workspace or VM owner. The library constructor then matches full structural
requests and proves all functions before creating Types/code/JIT and the rooted
instance. Current host dontcompile policy is checked after I/O, including changes
while exclusion was released. Input buffers can be discarded after success.

Success returns one Modlink reference and one external GC pin. Failure returns
nil, retaining no partial instance or pin. To discard an unpublished success under
exclusion, call ptrdel(D2H(ml)), then destroy(ml). Existing native constructor and
lifetime contracts are described in [scalar libraries](dis64-scalar-libraries.md).

## Fresh admission and scope

Each load opens, reads, admits, proves and creates a fresh unregistered instance.
The bridge uses no module cache or legacy hash lookup; an old same-path module
cannot supply its admission. No filesystem identity/provenance capability is
attached for later cache reuse. Normal filesystem mutation/permission semantics
remain those of the existing namespace and admission boundary.

The opcode handler for guest ILOAD is unchanged. This API provides the file half
needed by that future admitted caller; it grants no new guest opcode capability,
managed graph serialization, builtin import path or legacy/native ABI mixing.
No Module/Frame layout, public Dis64/ISA ID, signature version or vendor pin changes.

## Reproduce

```sh
python3 utils/tests/dis64-scalar-requests.py --limbo /host/limbo
python3 utils/tests/dis64-library-namespace.py --limbo /host/limbo
for control in admission close acquire profile; do
    python3 utils/tests/dis64-library-namespace.py --limbo /host/limbo --control "$control"
done
python3 utils/tests/dis64-library-namespace.py --limbo /host/limbo --disabled
```

Use CC/NATIVE64_ARCH/NATIVE64_RUNNER/NATIVE64_CFLAGS for ARM64/Linux QEMU.
For host ASan/UBSan, pass sanitizer flags in NATIVE64_CFLAGS and select
`--materialize-only`; this runs the read/admission/construction/failure matrices
without VM instruction execution. The known runtime-ASan unwind limit remains.

Fourteen actual interfaces/providers pass672 scheduler roots per x64/ARM64 target:
four client/provider C/RX choices, fitting/growing stacks, quanta1/3/1000 and both
I/O modes. Exact scalar bits, genuine accepted RX on normal runs, input poisoning,
no implicit init and explicit faults are checked. Fault-only code can use C fallback.
Independent `.m` changes add an unused export or substitute Real for Big; both
loads refuse before execution. Normal `-H` bytes match the prior compiler for all
14 interfaces plus five limit fixtures. Five producer boundaries,12 well-typed
unsupported interfaces, two reserved-prefix declarations and three invalid flag
combinations are checked independently.

Both runtime gates check every read/constructor allocation failure, final root
failure, denied/throwing admission, nine I/O failures, invalid/full-width lengths,
malformed file/request data, builtin paths, stale cache aliases and host JIT policy.
Four active controls require precise semantic exit10 and full fixture cleanup when
admission, FD cleanup, acquire ordering or startup policy is removed. A missing
acquire is repaired only by the test boundary before raising its diagnostic;
malformed modules never execute.

The reader and scheduler bodies are production code. Namespace transports,
admission predicate, pin/GC and OS boundaries are bounded fixture support. These
are not positive new full-OS guest LOAD or concurrent filesystem/collector proofs.
Full console compatibility is separately rebuilt/booted. Android/Bionic GUI, AWM
and a working APK remain unverified. [INFERNO-0180 report](dis64-inferno-namespace-report.md).
