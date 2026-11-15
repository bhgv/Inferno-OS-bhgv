# Ordinary Command.init through Dis64 load

The selected `-64b` / `-64c` trial now compiles and executes the actual
`Command.init(ctxt: ref Draw->Context, argv: list of string)` declaration from
`module/sh.m`. `load` binds a module; the caller explicitly invokes `m->init`.
A library has no process entry point. Its ordinary RET returns to the caller.
This is the same distinction used by Limbo sh, which calls init after loading.

Save the following three blocks as `command.m`, `library.b`, and `client.b`.
The wrapper includes the existing declarations rather than restating an ABI.

```limbo
include "sys.m";
include "draw.m";
include "sh.m";
```

```limbo
implement Command;
include "command.m";
increment: fn(n: int): int;
validate: fn(ctxt: ref Draw->Context, argv: list of string, expected: int): string;
init(ctxt: ref Draw->Context, argv: list of string)
{
    count := increment(2);
    if(count != 3){ z := 0; bad := 1/z; }
    if(ctxt == nil || ctxt.display != nil) return;
    status := validate(ctxt, argv, 3);
    if(status != "checked"){ z := 0; bad := 1/z; }
    saved := ctxt;
    display := ctxt.display;
    screen := ctxt.screen;
    wm := ctxt.wm;
    ctxt.display = nil;
    ctxt.screen = nil;
    ctxt.wm = nil;
    ctxt.display = display;
    ctxt.screen = screen;
    ctxt.wm = wm;
    greeting := "Привет";
    if(len greeting != 6 || greeting[0] != 1055){ z := 0; bad := 1/z; }
    original := greeting;
    greeting += "!";
    greeting[0] = 1057;
    middle := greeting[1:6];
    if(original != "Привет" || greeting != "Сривет!" || middle != "ривет"){
        z := 0; bad := 1/z;
    }
    total := 0;
    while(argv != nil){
        part := hd argv;
        total += len part;
        argv = tl argv;
    }
    if(total != 11){ z := 0; bad := 1/z; }
    keep := saved;
}
increment(n: int): int
{
    return n + 1;
}
validate(ctxt: ref Draw->Context, argv: list of string, expected: int): string
{
    if(ctxt == nil || len argv != expected){ z := 0; bad := 1/z; }
    keep := argv;
    return "checked";
}
```

```limbo
implement Client;
include "command.m";
Client: module { init: fn(); };
init()
{
    m := load Command "library.dis64";
    if(m == nil) return;
    number := 42;
    text := string number;
    bytes := array of byte text;
    decoded := string bytes;
    capacity := 4;
    buffer := array[capacity] of byte;
    view := buffer[1:3];
    buffer = nil;
    view[0] = byte 65;
    value := int view[0];
    if(value != 65){ z := 0; bad := 1/z; }
    if(len view != 2){ z := 0; bad := 1/z; }
    numbers := array[3] of int;
    numbers[1] = 123456;
    selected := numbers[1:];
    numbers = nil;
    numberread := selected[0];
    if(numberread != 123456){ z := 0; bad := 1/z; }
    parsed := int decoded;
    if(parsed != number || len bytes != 2){ z := 0; bad := 1/z; }
    argv := "wm" :: "Привет" :: "a\0z" :: nil;
    ctxt := ref Draw->Context(nil, nil, nil);
    m->init(ctxt, argv);
    if(len argv != 3){ z := 0; bad := 1/z; }
    m->init(nil, argv);
    keep := ctxt;
}
```

With the newly built Limbo compiler and the repository module directory:

```sh
limbo -64b -I module -I . -o library.dis64 library.b
limbo -64c -I module -I . -o client.dis64 client.b
limbo -H -M Command -I module command.m > command.h
```

Place both images in the emulated namespace root, then run the rebuilt console:

```sh
emu -r /path/to/root -c 0 -X dis64-calls -d /client.dis64
emu -r /path/to/root -c 1 -X dis64-calls -d /client.dis64
```

The expected result is an empty Client exit string and normal root retirement.
The default policy refuses these opt-in images. Starting the entryless library
with `-d /library.dis64` also refuses. A failed load returns nil; it does not run
init. No special loader-triggered init hook is involved.

## Types, hashes and frames

`Sys` or `Command` in a load expression is an interface descriptor. The path
expression is a module path, or a built-in name such as `$Sys`. Limbo translates
that expression to Dis; the interpreter consumes Dis instructions. This series
adds a file-backed Command binding, not another Sys implementation.

`limbo -H -M Command` and the selected writer both obtain the existing `.m`
function hash through `sign(fn)`. For the current declarations it is `4244b354`.
The loader first matches full signature bytes, module and symbol, then passes
the **consumer's** hash to the normal `linkmod` export resolver. The hash checks
consistency; it is not authentication. Normal linking clones the provider MP.
The complete source module is registered before its temporary MP pin is dropped;
normal unload removes it. Allocation refusals retire all intermediate owners.

| Value | Native location | GC role |
| --- | --- | --- |
| ctxt | callee FP+64 | Context reference |
| argv | callee FP+72 | list of String |
| parameter prefix | 80 bytes total | map bytes `00 c0` |
| bare Draw.Context | 24 bytes | three mapped pointer fields, initially nil |

Arguments are copied into a normal prepared frame by MFRAME/MOVP/MOVP/MCALL.
The callee can consume its argv variable or copy ctxt without changing the
caller's variables. List cells and String objects remain shared managed objects.
The actual collector sees both caller and callee frames, including callee-private
cons cells. Existing C dispatch and JIT paths execute the same checked code.

`ctxt := ref Draw->Context(nil, nil, nil);` creates a bare record with nil display/screen/wm.
The selected profile now admits the three Context pointer fields at native offsets
0/8/16 (classic offsets0/4/8). Read a field into a typed local, then copy, clear,
restore or nil-test that reference. Each field has its own internal opaque role;
Display/Screen/wm values cannot be used as Context, String or ListString. The
actual collector traces the three fields and their mapped frame aliases. Field
writes use the existing MOVP reference-count/barrier behavior.

This is reference transport, not resource creation: nested field dereferences,
Display/Screen allocation, channel operations and indirect-to-indirect copies
remain refused. Direct `ctxt.display == nil` / `!= nil` tests and pointer identity
comparisons between compatible Context/list/field aliases are admitted. They use
full-width reference comparisons; String equality and ordered comparisons now use content, with signed Rune32
ordering and nil/empty content equality. Character indexing returns a Rune in
an int and preserves Unicode code points and embedded NUL. Short-circuit guards can avoid a nil Context dereference.

`ref Draw->Context(nil,nil,nil)` now works in local declarations, assignments,
loops and callee constructors. The selected compiler preserves Context temporary
types instead of recycling a String/List slot. Evaluate a constructor into a
local before calling init; a constructor inside prepared call setup still refuses. A non-nil record does not establish a display/window manager.
For exact evidence see [INFERNO-0189](dis64-inferno-client-literals-report.md).

## Exact private boundary

The opt-in private signature is exactly four big-endian words:
`face0186`, existing Limbo function hash, arity `2`, void result `0`.
This is a new **private trial schema**, not a public managed signature assignment.
No new public opcode, section or type-kind ID is assigned; prior scalar signature
v1 bytes remain unchanged. Other implementations must not infer general managed
signature interoperability from this fixed-shape experiment.

The admitted provider has one Command.init export at PC0/frame0, no imports,
bounded proven bodies and the existing private 24-byte MP prefix and mapped
literal roots. Up to eight local helpers use checked fixed-width parameters and
void or managed results; they have separate frames and an acyclic call graph.
The executable importer still has an argument-free void process entry. One
interface/path is admitted. Up to32 extra readonly String roots are admitted per
image, each at most255 Runes,1024 Runes in aggregate. General DATA initializers,
additional exports, arbitrary ADT fields and general ADT/array payloads remain refused.
Only the selected client reader admits the required MP-to-prepared-frame MOVP;
the general scalar instruction reader is unchanged.

Reproduce the source/GC matrix with `dis64-command.py`, complete-console execution
with `dis64-command-boot.py`, independent raw refusal checks with
`dis64-command-proof.py`, and compatibility with `dis64-command-producer.py`
(all in utils/tests, each accepts `--limbo`). The callee-frame omission control
must detect a reclaimed observed object before resuming execution.
Linux x64/ARM64 QEMU results do not establish Android/Bionic GUI/APK startup.

The String pool/MP convention reuses the earlier private managed-file trial:
extra records follow the binding names; their owned roots occupy MP24+8*i. MP
bytes/maps cover every root. If MP grows, descriptor2 independently describes
Context24/E0; INEW must use it, not the expanded MP descriptor1. Without literals,
the original two-descriptor image and all previous bytes are preserved. This is
still a private opt-in convention, not a public managed initializer ABI.

Source `""` keeps classic nil-like representation at MP8. A serialized zero-Rune
record is a separately owned non-nil empty String; constructor gates distinguish
these references while String content comparison treats nil and empty equally.
ASCII may use byte storage; other literals use Rune storage. C extensions must
use String length/representation, never strlen for a value containing NUL.
String concatenation, local character stores and slices now use existing ADDC,
INSC and SLICEC handlers. Mapped local owners are required; shared literals/aliases
are preserved by copy-on-write. Index equal to length appends one character; a
larger or negative index raises bounds error. Slices use character indexes,
including Unicode and NUL. General payload construction remains outside this profile.


Numeric casts now use existing int/big/real String conversions. `array of byte s`
encodes UTF-8; `string bytes` decodes it. Lengths differ for non-ASCII text:
characters in String, bytes in the array; embedded NUL is preserved. These arrays
are owned runtime values with mapped roots, not a new general DATA initializer.
Byte-array conversion, length, copies, identity, zeroed construction, slices and
byte indexing are admitted here; other element types remain separate work.


Selected byte arrays now support `array[n] of byte` and slices such as
`buffer[1:3]`. This profile zeros new byte buffers. A slice retains its backing
array after the original variable is set to nil; its bounds and length count
bytes. An extra checked primitive descriptor is emitted only for constructors;
it does not replace the Context descriptor. General array payload initialization
and other element types remain outside this profile.

Byte indexing now supports `n := int buffer[i]`, `b := buffer[i]` and
`buffer[i] = byte n`, with an int local or constant index. Byte-to-int reads are
unsigned. Negative, nil and end indexes raise the existing array bounds error.
The compiler lowers the adjacent address/consumer pair to a reserved native
pointer cell; its array owner remains mapped through scheduler and GC pauses.
Branches cannot enter the consumer alone. This does not admit general array
initializers, non-byte elements or byte compound arithmetic.

The selected profile also supports zeroed `array[n] of int`, owning slices,
length, aliases and indexed reads/writes through `INDW` plus an adjacent `MOVW`.
Use an int local for a value that will be compared or used in arithmetic:
`value := numbers[i]; if(value != expected) ...`. The element is a signed32-bit
Limbo int; the temporary address is a native64 pointer. Byte and int arrays have
distinct checked classes and primitive descriptors, even in the same frame.
An empty `[0:0]` slice of nil remains nil; nonempty nil slices and out-of-range
accesses keep the existing runtime errors. Compound indexed operations, general
array initializers and other element types are still outside this profile.

Local helper declarations belong to the implementation file, as `validate` above
demonstrates. They are reached through ordinary FRAME/CALL/RET; only init is
exported. A helper takes zero to eight scalar parameters, optionally preceded
by `(ctxt: ref Draw->Context, argv: list of string)`. Scalars are byte/int/big/real.
A helper returns void, byte/int/big/real, String, list of String, an ADT
reference or a byte/int array. The optional prefix occupies mapped FP64/FP72; scalars start at FP64
without it, or FP80 after it, each aligned to its own width. The collector traces caller, prepared and active callee frames;
helper-local lists/strings/arrays and caller values survive nested calls.
Branches cannot enter call setup or cross function boundaries. Recursive graphs,
standalone managed parameter shapes, aggregate results and helper imports still refuse the
whole file. These limits describe the selected profile, not Limbo itself.

A managed result is written directly into a mapped caller root, using a full
native address bound through LEA to the callee's unmapped FP32 cell. Existing
values are released by the normal managed store. `return next(ctxt, argv)` can
forward the checked address through a chain; the ancestor root stays traced.
The reader derives result classes from the callee code and checks them against
caller use. Every normal return must have assigned its result. Bare pointers,
scalar slots for managed results, missing assignments and conflicting classes are rejected. Nil may
be returned without inventing a new wire type. Use an explicit local before
returning a computed operation when it is outside the selected opcode set.

The `expected: int` parameter above is passed by value. A helper may update its
parameter without changing the caller variable. Numeric argument slots must be
unmapped and fully written before CALL. Packed byte parameters remain one byte;
big and real share size, but retain distinct checked classes.

| Scalar parameter | Bytes | Alignment |
| --- | ---: | ---: |
| byte | 1 | 1 |
| int | 4 | 4 |
| big | 8 | 8 |
| real | 8 | 8 |

Every call of a helper must use the same prefix, arity and scalar-kind sequence.
Neither padding nor an unused local is an initialized parameter. The executable
entry and exported Command.init keep their original signatures.

Local helpers also return `byte`, `int`, `big`, and `real`. The `increment`
example computes directly into its caller-owned result. The reader derives the
exact class from stores; the binder checks an unmapped caller span of1/4/8/8
bytes. The result address itself always occupies8 bytes. Nested
`return helper(...)` forwards that address; every path to RET must assign the
result. Equal widths do not make `big` and `real` interchangeable.
