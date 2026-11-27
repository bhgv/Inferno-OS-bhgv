# Scalar library files and structural imports

[Русская версия](dis64-scalar-libraries.ru.md).

Limbo can now emit a complete scalar library, and the native64 constructor can
instantiate it from checked structural import requests. Construction never invokes
`init`. This is the provider-file half of the import path; general guest `LOAD`,
namespace/cache admission and a managed importing client remain separate work.

## Compile a library

`library.m`:

```limbo
Library: module {
    init: fn();
    mid: fn(n: int): int;
    leaf: fn(n: int): int;
};
```

`library.b`:

```limbo
implement Library;
include "library.m";
init() {}
mid(n: int): int {
    a := leaf(n);
    return leaf(a);
}
leaf(n: int): int { return n+1; }
```

```sh
limbo -64b -I . -o library.dis64 library.b
limbo -H -M Library library.m > library-native64.h
```

The `mid(7)` result is9. `init` is just another exported function here: a caller
may explicitly request and call it, but constructing the module/instance runs no
code. The generated header remains an ABI witness for native extensions; its
classic signatures still come from the Limbo compiler and `.m` interface.

| Producer | Metadata | Entry behavior |
| --- | --- | --- |
| `-64b` | Existing library profile0 | No entry, including when a function is named `init` |
| `-64c`, no `init` entry | Existing library profile0 | No entry |
| `-64c`, implemented `init` entry | Existing executable profile1 | Previous checked command rules |

Both scalar flags disable inlining; adding `-i` yields identical bytes. Classic
output and `-64`, `-64m`, `-64l` retain their meanings. A library is deliberately
refused by standalone `emu -X dis64-calls -d library.dis64`; command startup must
receive an executable. A shell normally chooses which function to call after
loading a module; the library constructor makes no such choice.

## Library proof

Libraries and commands share function translation, Frame64, the existing code
materializer and C/RX execution. The library metadata contains only name/profile/
data-size; the absent entry fields have no fabricated root or sentinel. Functions
remain in compiled order, and ordinal0 can have arguments/results, call another
function or be called by another function.

Every compiled function needs a declared export. All code belongs to checked,
nonoverlapping function intervals; unused exports are proved too, even when the
request table is empty. Scalar signatures support void/int/big/real. Call-free
leaves retain checked branches/loops and multiple returns. Call-containing bodies
are linear, terminate with RET and form an acyclic graph. Every nonvoid normal
return must store a definitely initialized value through its incoming result
pointer. EXIT is invalid in every library function.

Each caller binds its own outgoing result tail. Forwarded return expressions copy
the completed value back through the current incoming pointer. No caller result
pointer is passed through to the next local callee. Numeric conversions retain
the [scalar call-file rules](dis64-source-call-files.md).

Limits remain256 translated instructions,64 functions/types/parameters,1MiB frames
and producer image output. Pointer maps must be zero; module data, own imports,
managed references, byte signatures, handlers, recursion, call-containing branches
and nonexported compiled helpers refuse. This does not serialize a general managed
library or a library that itself imports another module.

The compiler and runtime independently invoke dis64calllibraryinfercheck over the
complete raw image. Final records/count/image publish atomically after full proof;
scratch can change on refusal. The old executable checker and its exact accepted
proof summaries are preserved. No new wire IDs are added: Dis64v1.0 ABI1,
Module/Code/Types/Exports/Strings and scalar signaturev1 are unchanged.

## Instantiate from existing import payloads

The native API is:

```c
Modlink *prepare64calllibrarylink(
    char *path, uchar *image, ulong bytes, int jit,
    const uchar *imports, ulong importbytes,
    const uchar *strings, ulong stringbytes);
```

`imports` and `strings` are complete existing Imports and Strings payloads. They
are immutable borrowed buffers, not native pointer tables. Use dis64signaturewrite,
disstringpoolwrite and dis64bindingswrite to create them from the intended scalar
interface. For example, the `mid` request has signaturev1, result int and one int
parameter; its module and symbol string IDs identify `Library` and `mid`.

The API checks exact module name, exact function name and the complete structural
signature, including parameter/result kinds. Big and Real do not match merely
because both occupy8 bytes. Reordered requests receive links in request order.
Duplicate exact requests retain existing codec semantics. Zero requests and up to
64 requests are supported;65 refuses before allocation. Conflicting duplicates,
missing/late mismatches and malformed metadata refuse without partial matching
output or a VM owner.

After complete library proof, the constructor owns copied code, Types/maps,
module/path labels and JIT state. It uses production mklinkmod and fills positive
ordinal links from proved export coordinates. MFRAME uses the actual provider
frame, including locals, rather than an importer-supplied argument bound. MCALL
and RET use the existing runtime implementation. Source image/request/name buffers
can be destroyed after return.

This path does not invent32-bit hashes or populate legacy Module.ext with fake
hashes. Classic `.m` hashes and the separate [trusted owned-import constructor](dis64-owned-imports.md)
keep their contracts. Structural matching alone is not execution authority: the
runtime additionally proves every function before materializing this instance.

## Ownership and failure

Call under VM exclusion with allocation monitoring disabled and a compatible
WORD32/ref64 native caller. Success returns one Modlink reference and one external
GC pin. Transfer it to a mapped VM cell using ordinary reference/barrier rules, then call `ptrdel(D2H(ml))`. To discard an
unpublished result under the same exclusion:

```c
ptrdel(D2H(ml));
destroy(ml);
```

Nil means refusal/failure. Partial Modules, Types/maps, code/JIT, Modlinks and a
failed final root acquisition are retired. Successful root acquisition is the
last fallible construction step. Pure libraries have no MP data. The instance
retains the Module while links/frames use its code and Types.

The API reads no namespace path, registers no cache entry and admits no guest
LOAD instruction. `path` is a copied label. It does not authenticate arbitrary
serialized C manifests, mix legacy/native instances or provide concurrent
revocation. Dynamic C module initialization and managed import graphs are outside
this scalar path.

## Reproduce

```sh
python3 utils/tests/dis64-library-files.py --limbo /host/limbo
python3 utils/tests/dis64-library-check.py --limbo /host/limbo
for control in module signature entry atomic; do
    python3 utils/tests/dis64-library-files.py --limbo /host/limbo --control "$control"
done
```

Both gates accept CC/NATIVE64_ARCH/NATIVE64_RUNNER/NATIVE64_CFLAGS. Constructor-only
host ASan/UBSan:

```sh
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
python3 utils/tests/dis64-library-files.py --limbo /host/limbo --materialize-only
```

Fourteen whole libraries cover empty/void, single functions, ordinal0 in both call
directions, nested calls, branches/loops, int/Big/Real, mixed aligned arguments and
errors. Each x64 and ARM64/Linux QEMU gate executes336 scheduler roots across all
four client/provider C/RX combinations, real stack growth and quanta1/3/1000.
Normal runs require genuine accepted RX dispatch; fault-only leaves may use C fallback, including when a constant zero divisor has
no emitted RX entry. Requested JIT mode does not mean every instruction runs as
machine code. Exact result bits, copied-input independence, every
constructor allocation failure and final pin failure are checked. The importing
outer client is trusted fixture code; the whole provider comes from its file.

Ten unsupported sources emit no payload. The raw checker covers74 positive/
negative/resource cases with an independent Python caller/leaf/DFS oracle and
atomic output guards. Three raw omissions detect lost ordinal0 return coverage
and self/mutual cycle checks. Four constructor/matcher omissions detect missing
module identity, full signature equality, no-entry policy and atomic matching.
Host ASan/UBSan construction/raw checks and x64/ARM64 runtime UBSan pass. Bounded
fixture collector/OS seams and the existing runtime-ASan unwind limitation remain.

Rebuilt full consoles retain56 executable-file OS roots each and refuse28 library
standalone selections each. That is compatibility/refusal evidence, not a new
positive full-console library LOAD run. AWM, Android/Bionic GUI and a working APK
remain unverified. See [INFERNO-0179](dis64-inferno-scalar-library-report.md).

The next [compiler-interface and namespace layer](dis64-scalar-namespace.md) now
emits requests directly from .m with -H -R and reads/admit/proves provider files
through an explicit uncached native API. Guest ILOAD/managed importer admission
remains separate.
