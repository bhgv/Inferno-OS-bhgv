# Generated variadic prefixes and typed C calls

A variadic function has two different descriptions: the fixed parameters declared
in its `.m` interface and the ordered types of one particular call's tail.
Limbo can generate the first description without knowing any calls. The caller
must supply the second explicitly. Neither a GC map nor a printf pattern can
recover the tail's scalar/reference types.

For example, `Sys.fprint(fd, pattern, *)` has an 80-byte fixed native64 prefix:
Frame64 occupies bytes 0–63, `fd` is at 64 and `pattern` at 72. Both parameters
are managed references, so the fixed map is `{0, 0xc0}`. The tail starts at 80.
`sprint`, `aprint` and `print` have one fixed String at 64, a 72-byte prefix and
map `{0, 0x80}`.

## Generate and inspect a schema

Use a source-matched compiler:

```sh
limbo -H -M Sys module/sys.m > sys-native64.h
```

In a C consumer, include the ordinary port declarations before selecting metadata:

```c
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_PREFIXES
#include "sys-native64.h"
#include "native64-tail.h"

static const LimboNative64Entry *fprint_prefix(void)
{
    static const uchar map[] = {0, 0xc0};
    return native64tailprefix(&limbo_native64_Sys_fprint_prefix,
        "fprint", LIMBO_NATIVE64_SIGNATURE_Sys_fprint,
        2, sizeof(F_Sys_fprint), map, sizeof(map));
}
```

A successful check returns the generated fixed schema; a mismatch returns nil.
`LimboNative64Prefix.fixed.function` is always nil. This record cannot satisfy
`native64bindrecord(record, 1)` or a fixed implementation-table binding. The
existing fixed Sys table still contains 39 entries, excluding all four variadic
functions. Selecting prefix metadata does not register a function or authorize
bytecode execution.

The generator also handles qualified methods, inline ADT fields and wide maps.
For example, a prefix with 150 String parameters has size 1264 and a complete
20-byte map. Adding one reference tail slot and one scalar slot gives size 1280;
the map retains all fixed references and marks only the added reference.

## Call the existing typed sprint gateway

A trusted C caller may use the existing synchronous gateway. In this example,
`pattern` contains `%d %s`. The caller must already keep `pattern`, `text`, the
logical destination owner and `context` live under the VM's root discipline.
`resolve(context)` returns the current mapped String result cell; returning nil
cancels publication. It may allocate or throw, so the gateway resolves it after
formatting and allocation rather than caching the earlier address.

```c
#include "native64-sys-call.h"

static int render(Prog *prog, String *pattern, String *text,
    void *(*resolve)(void*), void *context)
{
    Native64TailSlot slots[] = {
        {Native64TailInt, 0, 8, nil},
        {Native64TailRef, 8, 8, &Tstring},
    };
    uvlong values[] = {(uvlong)(vlong)-3, (uvlong)(uintptr)text};
    Native64SysFormatRequest request = {0};

    request.prog = prog;
    request.pattern = pattern;
    request.slots = slots;
    request.count = 2;
    request.values = values;
    request.value_bytes = sizeof(values);
    request.resolve = resolve;
    request.argument = context;
    request.initial = 16;
    request.limit = 4096;
    return Sys_sprint64(&request);
}
```

Each canonical tail slot is eight bytes. Int/fix values are sign-extended from
32 bits, byte values are zero-extended, and big/real values retain all 64 bits.
Managed empty values use `H`, not a C null pointer. Reference slots carry the
exact expected trusted `Type` identity. The constructor composes fixed and tail
maps, initializes mapped cells, acquires owners and pins the frame before calling
the formatter. The previous result survives refusal or error; successful result
publication transfers one owner into the resolved cell. Destructors must obey
the existing nonthrowing synchronous cleanup contract.

`Sys_aprint64` returns exact UTF bytes in an array, including embedded NULs.
`Sys_print64` and `Sys_fprint64` use `Native64SysWriteRequest`, resolve a four-byte
WORD result cell and issue one write after formatting. The fprint request also
borrows a caller-rooted actual Sys_FD. Their release/write/acquire path retains
the frame and output until cleanup completes.

## Build and verify

```sh
python3 utils/tests/native64-prefix.py --limbo /path/to/fresh/limbo
python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
```

Set `CC`, `NATIVE64_CFLAGS` and `NATIVE64_RUNNER` for GNU ARM64 or RV64 execution.
For Android ARM64/Bionic, use `--android-ndk /path/to/ndk` with the formatter gate
and `NATIVE64_RUNNER=qemu-aarch64`. The Android GUI build's
`--native64-formatter` option additionally verifies that the four C gateways
link into the complete NativeActivity library.

These are trusted C metadata and call interfaces. Their type identities are not
serialized. The existing Dis64 variadic bytecode admission and ordinary AWM
startup restrictions remain in place until the compiler/runtime tail contract
is connected separately.
