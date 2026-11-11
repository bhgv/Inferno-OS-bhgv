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

## Consume a live VM frame

The four `Sys_*frame64` functions bridge an already prepared native VM frame to
the typed C gateways. They are intended for trusted native callbacks whose call
site already owns a complete fixed-plus-tail frame descriptor. They do not add
a variadic export to the ordinary Sys table or infer a tail from a printf string.

The following helper assumes an active MCALL callback and borrowed, immutable
call-site metadata. The `fp` argument is its exact current callee frame. The
caller and callee must occupy the same current stack extent, and their complete
Type descriptors must remain live through the synchronous call. The result
field offset and language kind are supplied by the trusted call site; a GC map
alone cannot distinguish String from Array, or a WORD field from Real/padding.
The callee's eight-byte return address at offset 32 must already point exactly
to that field. Tail values must already occupy canonical eight-byte slots.

```c
static int render_vm_frame(Prog *prog, void *fp, Type *callee, Type *caller,
    unsigned int result_offset, const Native64TailSlot *tail, unsigned int count)
{
    Native64SysFrameRequest request = {0};
    request.prog = prog;
    request.frame = fp;
    request.type = callee;
    request.caller_type = caller;
    request.result_offset = result_offset;
    request.slots = tail;
    request.count = count;
    request.initial = 16;
    request.limit = 4096;
    return Sys_sprintframe64(&request);
}
```

The bridge checks the live process/module/frame identities, walks complete
current-extent frame boundaries, matches the entire fixed-plus-tail GC map and
checks the result address before reading payload. It then lets the existing
gateway copy and pin the arguments. Every late result resolution repeats the
frame and map checks, including process cancellation, guarded epoch and live-REG
identity. A mismatched result address or invalidated activation preserves the
old result. Print/fprint reacquire the VM and resolve again after the native
write. Cancellation can therefore leave an already-issued write visible while
withholding its scalar result.

This is a borrowed native-activation interface, not a new scheduler lifetime
lease. The caller must not reenter the VM, destroy/reset its process or retire
its stack/Types while the call is active. The bridge refuses an existing caller
lease, foreign process/frame identities and caller frames in older extents.
Reference provenance is the VM/C caller's obligation: numeric bounds cannot
authenticate arbitrary native pointers or repair an already-corrupt VM frame.

`native64-arguments.py --formatter` now executes 768 real FRAME/MFRAME/MCALL cases per
port/model in addition to the previous gateway suite. It uses production opcode
bodies with source-derived tail plans, trusted C-provided values/decoded operands and result-address binding; it
does not decode a serialized variadic Dis64 image, extend LEA admission or execute
JIT code. The cases cover all four operations, 150-slot frames, root-extent Type
fallback, malformed metadata/cells, cancellation, epoch invalidation, GC and
complete cleanup. Separate removal controls must fail if the exact return-address
check or full frame-map comparison is deleted.


## Obtain plans from actual Limbo calls

`limbo -Dq -I module -o example.dis example.b` emits one `NATIVE64_CALL`
JSON diagnostic for each compiled variadic call. It reads the original `.m`
function signature and the instantiated, typechecked argument list after legacy
argument offsets have been assigned. The plan contains the fixed-prefix extent,
complete native64 frame extent/map, original result kind/fixed argument count
and ordered eight-byte tail slots. Slot kinds
are names, never the runtime's private numeric C tags. The recorded legacy
argument offsets are provenance for the diagnostic, not native64 addresses.

For example, inside an ordinary Limbo function:

```limbo
sys := load Sys Sys->PATH;
text := sys->sprint("%d|%s", -1, "ok");
```

The `Sys_sprint` plan has a 72-byte fixed prefix, an 88-byte complete frame,
map `[0, 0xa0]`, and `int`/`string` tail slots at offsets 0 and 8. Bit 0x80
roots the format at byte 64; bit 0x20 roots the string at byte 80. The scalar
at byte 72 is not a root. The signature is computed from the original `sys.m`
declaration, including its variadic flag, rather than the expanded call type.

The initial producer accepts fixed scalar/reference fields and tails containing
byte/int/fix/big/real/String kinds. A by-value fixed aggregate or another managed
tail kind produces an explicit `accepted:false` record. An opaque managed
reference needs an exact runtime Type witness before the C bridge can consume
it; a GC bit alone is insufficient. The diagnostic is opt-in, deterministic and
leaves ordinary legacy bytecode byte-for-byte unchanged.

`native64-callsites.py` compiles real source calls for all four Sys operations,
2-slot and 150-slot tails, empty and mixed numeric tails, and an opaque-reference
refusal. It also checks the 4096-slot boundary and 4097-slot refusal from real
source while preserving legacy output. `native64-arguments.py --formatter` consumes those generated plans as C
slot descriptors, checks their signatures/maps against the generated Sys prefix,
and runs both production FRAME/MCALL and MFRAME/MCALL paths. FRAME resolves the
call-site descriptor from the active module's type table, as ordinary variadic
Limbo lowering does; MFRAME remains a separate fixed-export transport control.

This is the compiler-side plan producer and a decoded-C VM integration test.
The source is still compiled to legacy `.dis` for the unchanged-output check;
the test supplies runtime values, Type identities and the result-address binding
from trusted C. It does not yet compile that source to an executable variadic
`.dis64`, serialize the diagnostic, change file admission or execute JIT code.
