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
bodies with source-derived tail plans, trusted C-provided values/decoded operands and production result-address binding; it
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


## Bind the result before entering the native callee

Ordinary WORD32 `LEA` stores a four-byte VM address. A trusted native64 call
owner must use `Sys_bindframe64` to prepare the complete eight-byte return
address at byte 32 of a pending frame. Do this after FRAME/MFRAME and before
MCALL, while `R.FP` is still the caller and the pending frame is exactly at the
top of the current extent. The original caller may be bytecode; it does not
need to be a native module. The entered `Sys_*frame64` gateway still requires
the actual native callee activation.

```c
static int bind_sprint_result(Prog *prog, void *pending, Type *callee, Type *caller,
    unsigned int result_offset, const Native64TailSlot *tail, unsigned int count)
{
    Native64SysFrameRequest request = {0};
    request.prog = prog;
    request.frame = pending;
    request.type = callee;
    request.caller_type = caller;
    request.result_offset = result_offset;
    request.slots = tail;
    request.count = count;
    request.initial = 16;
    request.limit = 4096;
    return Sys_bindframe64(&request, Native64SysSprint);
}
```

Use `Native64SysAprint`, `Native64SysPrint` or `Native64SysFprint` for the other
three trusted call-site kinds. These are private C operation selectors and are
never serialized. The call site supplies the declared result offset and kind;
the GC bitmap does not establish the complete language type of that field.

The binder shares the full-frame geometry and process/extent checks with the
entered gateway. It requires the exact caller Type, complete callee map and a
pending frame whose `mr` is still nil. It checks destination width, alignment,
extent and scalar/reference root role before writing any address bytes. A
refusal preserves all eight old address bytes and the reserved temporary area.
The successful operation performs one pointer-sized write, without allocation,
formatting, reference acquisition or callbacks. Argument canonicalization and
old String/Array result identity are checked later by the existing gateway.
Binding alone does not validate values or publish a new language result.

The 768 VM cases now use this production binder instead of a manual return
pointer store. Each case also checks 32 refused bindings (24,576 per port/model),
root-extent Type fallback and a caller marked with a non-nil bytecode `prog`
(the test still uses decoded C operands). GNU PIE runs verify result cells
above 4 GiB. The static Bionic fixture also seeds nonzero upper address bytes,
so a four-byte implementation cannot pass even when its actual cell is below
4 GiB. Two additional removal controls detect pointer-width truncation and
acceptance of an already-entered pending frame.

This trusted API does not widen ordinary LEA or authorize a serialized call.
The compiled call-site plan still needs its bytecode producer, owned runtime
metadata and loader binding before ordinary variadic Dis64 execution is ready.


## Give call-site recipes a module owner

`Sys_setframes64` installs a complete trusted C recipe table on a live Module.
It copies every source record and tail vector, and retains the exact caller,
callee and tail-reference Type identities. The complete maps live inside the
retained frame Types; Type layouts must remain immutable. Both frame Types must
already occur in that Module's Type table. This is a constructor for trusted
loader/C metadata, not a parser for JSON or a serialized Dis64 section.

```c
static int install_sprint_site(Module *module, Type *callee, Type *caller,
    unsigned int result_offset, const Native64TailSlot *tail, unsigned int count)
{
    Native64SysFrameSpec spec = {0};
    spec.operation = Native64SysSprint;
    spec.type = callee;
    spec.caller_type = caller;
    spec.result_offset = result_offset;
    spec.slots = tail;
    spec.count = count;
    spec.initial = 16;
    spec.limit = 4096;
    return Sys_setframes64(module, &spec, 1);
}
```

After success, the source `spec` and vector storage may be retired. The Module
owns the copied table until `Sys_clearframes64` or ordinary `freemod`. The latter
invokes the trusted teardown callback before releasing its Type table. The
private pointer/callback fields begin as nil in ordinary Module constructors;
they are never supplied by file bytes or flags. A failed installation publishes
nothing and releases all partial vectors and Type reservations, including when
an allocation throws or a duplicate Type would overflow its reference count.
The table is bounded to 256 sites and 4096 total tail slots.

A trusted VM owner uses `Sys_bindsite64(module, index, prog, pending)` before
MCALL, then `Sys_callsite64(module, index, prog, active)` inside the native
callback. These functions derive requests from the owned record instead of
borrowing the compiler's temporary vector. Before binding, the active Modlink
must belong to that exact Module. After entry, the retained caller Modlink in
`Frame.mr` must belong to it. The table also records its exact owning Module;
a shallow copy is not a second owner. The active caller link keeps Module
lifetime valid during the call. The APIs require VM exclusion and the existing
no-reentry/immutable-metadata contract.

Retirement and recursive use are refused while an owned call is active. A
protected error, cancellation or native write failure clears that active state
before returning, so later retirement is possible. Clearing an idle table
detaches its fields before freeing native storage and Type owners; clearing it
again succeeds without releasing anything twice.

The runtime gate now uses owned recipes for normal/150-slot calls, canonical
value refusal, native write failure, cancellation, changed return addresses and
epoch invalidation. It overwrites the original source records/vectors, checks
foreign-caller refusal and retirement from allocating callbacks, injects both
nil-returning and throwing allocations after partial acquisition, checks Type
reference overflow, and executes the production `freemod` body. Independent
removal controls detect lost vector copies, caller-owner checks and retirement.
The values/decoded operands are still C-supplied; no serialized producer, PC-to-
site mapping, loader admission, JIT execution or ordinary variadic export is
created by this storage API.


## Locate the actual compiled call

The opt-in `-Dq` output now gives every `NATIVE64_CALL` an `id` and emits a
matching `NATIVE64_CALL_PC` record after instruction, descriptor and import
resolution. IDs are unique across a multi-file compiler invocation; temporary
IR notes are retired between modules. Nested calls are matched by ID, not by
assuming source order equals execution order.

| Field | Meaning |
| --- | --- |
| `module`, `id` | Caller module name and matching diagnostic plan identity. |
| `pc_space` | Always `legacy-dis`; these are not Dis64 coordinates. |
| `caller_pc`, `caller_descriptor` | Original caller entry and frame descriptor, resolved after function lowering. |
| `frame_pc`, `frame_descriptor`, `frame_slot` | Final FRAME instruction ordinal, legacy descriptor ordinal and FP frame-pointer slot. |
| `result_pc` | Final LEA ordinal, or -1 when there is no result address. |
| `result_space`, `result_offset`, `result_indirect` | Actual legacy FP/MP addressing operands. |
| `call_pc`, `call_opcode`, `binding`, `module_space`, `module_offset` | Final call instruction and resolved import/module operands. |
| `located`, `linked` | Captured instructions survived and satisfy the local synchronous FRAME/LEA/MCALL operand relation. |

The local relation checks the same prepared-frame register at allocation,
return-address binding and MCALL, including the legacy return-header offset.
It is not a proof of general control flow or an authorization to execute a
native call. A void MSPAWN can be located while `linked` remains false. Missing
or rewritten instructions are refused instead of guessing replacement PCs.
The diagnostic lookup is bounded to 65,536 notes and 1,048,576 instructions.

An independent classic Dis reader in `native64-callsites.py` decodes the actual
written image and checks all reported operands/PCs. This covers all four Sys
operations, 2/150/4096 slots, the 4097 refusal, nested FP/MP destinations, a void
spawn and a multi-file compiler invocation. Tracing leaves the legacy bytes
unchanged, and existing `-64`/`-64m` refusals remain in force.

The native frame geometry in the plan and these legacy coordinates describe
different representations. In particular, legacy descriptor ordinals, result
offsets and four-byte argument positions cannot be passed directly to the
native64 site constructor. The next producer must lower the values/instructions,
construct native caller/callee Types and emit an agreed owned PC/site mapping;
this diagnostic assigns no wire records or managed signature IDs.


## Lower canonical argument stores

`-Dq` also emits `NATIVE64_CALL_LOWER`, matched by the same diagnostic ID.
The initial pass proves a complete region of direct argument writes between
the captured FRAME and LEA. Each declared field must be written exactly once;
no branch, unproved address, computation or temporary cleanup is silently
skipped. Unsupported regions produce `lowered:false` with no partial program.

For `sys->sprint("%d|%s", number, text)`, the native destinations are:

| Value | Operation | Destination |
| --- | --- | --- |
| Fixed format String | MOVP | Byte 64 |
| Int tail argument | CVTWL | Byte 72 |
| String tail argument | MOVP | Byte 80 |

MOVW alone is insufficient for an int/fix tail: it writes four bytes, while the
canonical slot must contain the sign-extended eight-byte value. The pass selects
CVTWL. Big values use MOVL or an existing WORD-to-LONG conversion; real values
use MOVF or the existing WORD/LONG-to-REAL conversions. MOVP retains and marks
references through the production handler. Fixed fields and packed tail offsets
come from captured compiler declarations; native offsets are not obtained by
doubling legacy argument offsets. Byte and other unproved store forms remain
outside this initial pass.

Each store names its original legacy PC, source mode/value, field ordinal,
canonical operation and native destination. `source_space:legacy-operands`
explicitly means that source FP/MP offsets are still legacy operands. These
records are not ready-to-serialize Dis64 instructions. Caller storage layout,
source relocation, initializers, control flow and owned PC/site binding still
need their native64 producer/admission path.

The independent classic decoder checks the original write and derives the
expected opcode/destination from its declared type. It covers 2/150/4096-slot
calls, empty/mixed tails, literal immediates and raw big/real copies. Arithmetic
arguments and the outer call of a nested expression currently refuse lowering
when their temporary operations are not covered; their ordinary legacy code
continues to compile unchanged.

The runtime gate executes the generated store choices through actual MOVP,
CVTWL, MOVL, MOVF and CVTWF bodies. The 768 FRAME/MFRAME/MCALL cases no longer
manually pack their prefix/tail values. Additional cases check full signed32
extension, exact float conversion, signed64/IEEE bits (including signed zero,
infinity and NaN payloads), packed source slots and guard bytes. Removal controls
must detect replacing CVTWL with a four-byte move or MOVP with a raw pointer
copy. The gate still supplies trusted C source cells/values and decoded operand
addresses; it does not run the complete source program or a variadic Dis64 file.


## Relocate source reads into a checked bank

The store program now includes a `bank` layout and a `source_cell` /
`source_offset` for each non-immediate read. Original FP/MP coordinates remain
in the record as provenance. Native reads use the bank offset, rather than
reinterpreting a four-byte legacy reference cell as an eight-byte pointer.

| Read class | Original span | Native span | Slot stride | Root |
| --- | --- | --- | --- | --- |
| WORD, including conversion inputs | 4 bytes | 4 bytes | 8 bytes | No |
| LONG or REAL | 8 bytes | 8 bytes | 8 bytes | No |
| String or fixed reference | 4 bytes | 8 bytes | 8 bytes | Yes |
| Immediate WORD | Operand value | 4 bytes | No cell | No |

The compiler checks source bounds against the original caller descriptor or
module descriptor zero, and checks every original word's GC role. Identical
typed reads share a native storage cell; overlapping reads with different
widths or classes refuse the whole program. Each native cell starts at
`64 + 8*index`. The bank carries its complete extent/map, including zero
header and scalar bits. Root role comes from the operation and compiler type
witnesses, with the original map used as a consistency check; it is not inferred
from value bits or the printf pattern.

For the 150-tail-slot test, repeated reads of the local number and text need
only three input cells (including the format), while all 150 output arguments
still have their own canonical destination slots. An immediate does not reserve
a cell or a root. A String used both as format and as a string argument shares
one source cell and still acquires both destination owners.

`evaluation:per-read` is deliberate. Sharing a storage slot does **not** permit
caching mutable FP/MP values or changing the order of original reads. A future
producer must gather/refresh the original source at the corresponding read
point, or separately prove it immutable under the execution contract. Original
PCs and operand provenance are retained for that work.

The runtime gate seeds C input values once per unique native source, creates
actual source/bank Types from generated maps and pins the managed storage.
Every generated store obtains its operand from the native bank after gathering
its current source value, as described below. An unchanged alias returns
`-1|%d|%s`: the String argument reads the same source as the format instead of
receiving an unrelated per-destination C value. Packed scalar tests use both
native origin and bank offsets, including a Big cell shared by MOVL and CVTLF.
Guard bytes, root maps and per-generation pool allocation/free counts verify
complete retirement.

Removal controls detect a destination offset used as a source, a missing bank
root map and repeated initialization of a shared immutable fixture input. This
closes the decoded-C operand-address substitution step. Initial source values
remain C-supplied. The private layout/gathering plan below still needs complete
caller instruction/initializer lowering, serialized PC/site metadata and
ordinary Dis64 execution. The bank is a private lowering representation, not a
new wire section or an assigned managed type signature.

## Gather each read from native FP/MP storage

The private lowering plan now describes the complete source descriptors used by
its reads. `bank.sources` has two entries, FP then MP. A used entry includes
`legacy_bytes`, native `bytes`, a complete map8 and the `word4-ref8` policy;
an unused entry has zero extent and an empty map. Every source cell adds an
`origin_offset` in the corresponding native source extent. The independent
classic Dis reader reconstructs these layouts from the serialized caller and
module descriptors, rather than trusting the diagnostic offsets/maps.

The layout reserves 64 bytes before FP locals (replacing the legacy 20-byte
header), starts MP at zero, preserves four-byte scalar words, and aligns each
widened reference to eight bytes. Padding is unmarked. Packed LONG/REAL reads
must still span two adjacent scalar words; a split span refuses the complete
plan. Extents are rounded to eight bytes. Source descriptors are bounded to
1 MiB before allocating relocation scratch. These are private storage layouts,
not Types installed in an executable Module, and do not change existing frame
instructions or the wire format.

The VM transport gate now creates actual managed native FP and MP extents from
these maps and seeds each unique source there. Before **each** original read,
it obtains the source through the generated space/offset, refreshes its bank
cell using MOVP/MOVW/MOVL, and then executes the canonical argument operation.
The test keeps all three extents in one pinned managed allocation with their
maps combined at aligned offsets. GC runs between source initialization,
gathering and argument writes. Source initialization remains fixture C code;
it is not execution of a module's serialized initializers.

A mutable-alias case stores a String as both format and tail source. After the
format read, it changes the source to `"ok"`, runs GC, and verifies `"-1|ok"`.
The pending frame must retain the old format while the later read sees the new
String. The unchanged-alias case still returns `"-1|%d|%s"`. Removal controls
must reject reading the unfilled bank instead of FP/MP and caching the first
reference read. All earlier canonical-store, source-map and ownership controls
remain active.

This closes the private source-layout and per-read gathering step. Compiling
all caller operations and source initializers into those layouts, lowering
result/module operands, admitting owned serialized PC/site metadata and
executing an ordinary variadic Dis64 module remain required before AWM.

## Preserve source MP constants

`-Dq` now carries an `initial` record for an exact supported top-level MP
initializer. The lookup uses the resolved global declaration and its legacy
offset; the value is then associated with the already checked native origin
cell. It never interprets source FP contents as constants.

| Initial record | Meaning |
| --- | --- |
| `null` | No supported initializer proof; not a zero or nil value. |
| `string`, explicit byte vector | Original UTF-8 bytes, including embedded NUL; no `strlen` conversion. |
| `word`, high/low words | Four-byte int/fix representation, with zero high word. |
| `long`, high/low words | Complete signed64 bit representation. |
| `real`, high/low words | Canonical IEEE binary64 bits from `dtocanon`. |
| `nil-string` | Canonical nil String in an uninitialized mapped MP cell; no heap allocation. |

The independent classic reader now parses flat data records after the type
table. It compares every emitted initializer to the actual String/scalar bytes
at that offset. Explicit nil String records require absent initializer data and
a previously checked String root role. Unsupported arrays/aggregate data are
outside this bounded data reader; the producer does not pretend that unknown
initializers were handled. Ordinary `.dis` output remains byte-identical.

A module-load initializer is **not** a proof of the value at a later call. A
mutable MP variable still requires the intervening instructions and per-read
refreshes. These records remain private compiler diagnostics, not a new Dis64
section or permission to install source storage as an executable Module.

The transport fixture constructs its format Strings from those compiler bytes.
The 150-argument calls now use the entire original 449-byte format and verify
all 449 result bytes for String, array and native-write results. The observer
buffer and this case's explicit output limit were enlarged to hold that result;
the smaller refusal cases retain their existing limits. A literal String tail
also comes from its original MP initializer. FP local values and FD creation
remain fixture inputs and still need caller instruction lowering.

Additional tests materialize negative int/big and real initializer bits through
the actual gather/conversion handlers, and construct Unicode/embedded-NUL
Strings with the production length-aware `c2string`. They separately recognize
the classic empty-literal nil representation. Removal controls must detect a
hardcoded short format and loss of the initializer's high 32 bits. Existing GC,
mutable-alias, binding, owner-retirement and refusal controls remain active.

## Prove direct local FP definitions

A source cell may now carry a separate `setup` recipe. This is distinct from
`initial`, which still describes module-load MP data only. A recipe preserves
the defining legacy PC, actual opcode, source space/value and optional input
seed. The input of an immediate definition is the operand itself; the input
seed for an MP read describes **initial** module data, not a proof that a
mutable MP value still has those bits when the definition executes. A real
lowering must perform the source read at the original definition point.

The producer searches backward from FRAME through a suffix of direct FP
moves/conversions. It accepts only the latest exact-width definition with a
supported immediate/MP source. Calls, address-taking, indirect writes,
branches, unknown operations, overlaps and unsupported FP-to-FP dependencies
stop the proof. MP input spans and root roles are checked against descriptor
zero. An incoming branch may target the defining instruction, but may not
skip it and enter the remainder of the suffix. Computed dispatch/alt/case
instructions and exception tables conservatively disable these recipes.
Missing proofs remain explicit `setup:null`; no zero or constant is guessed.

The independent classic reader verifies the defining opcode/operands, suffix
writes, absence of intervening overwrites, incoming branch targets and any
input seed against the actual serialized instructions/data. Tests cover a
conditional skip, an intervening call, the latest of two assignments and
corrupt definition PC/opcode/value controls. Legacy output and existing
Dis64 refusals remain unchanged.

The runtime source fixture now places direct `number = -1` and `text = "ok"`
assignments immediately before each of its eight principal Sys calls. Their
FP cells must have compiler-produced recipes and inputs. The transport uses
those values for native FP storage and uses the actual MOV handlers to install
the String/integer owners before the established per-read gather path. It also
executes selected local big/real definitions and checks all eight output bits.
Removal controls detect ignoring the integer definition and truncating a
full-width big assignment.

This proves preparation of a bounded straight-line local suffix in the
transport. It does not execute the complete caller, arbitrary control flow,
FD construction or mutable MP history. Unknown cells still use explicit C
fixture inputs; executable initializer/caller/result lowering and serialized
PC/site admission remain required before AWM.

## Replay local definitions against current managed MP storage

FP recipes now carry `source_offset` and `source_bytes` in native MP storage
(or zero offset/four operand bytes for an immediate). `bank.preparation` lists
recipe-bearing source cells in increasing original definition-PC order. The
classic reader checks each native input span against the original MP operand,
complete map/relocation layout, and scalar/root width. It independently checks
the full preparation order and rejects a reversed vector.

The transport no longer installs a known FP value directly from its flattened
seed. It first initializes managed MP inputs, retaining identical aliases once.
It then executes the generated MOV/CVT recipes in definition-PC order, reading
current MP memory into mapped FP cells. An immediate is supplied as the decoded
operand. Only after preparation does the existing per-read bank gathering and
argument-store path run. GC epochs occur between these stages. Unknown recipe
inputs and FD setup remain explicit C fixture inputs.

A byte-span installation ledger verifies complete source initialization and
refuses partially overlapping or inconsistent duplicate seeds. Replaying a
known recipe is mandatory before its FP cell can be gathered. Managed root
writes use MOVP and retain/release through the actual heap/collector machinery;
reference source bits are never copied with a raw pointer assignment.

The new mutation case changes an MP String **after initialization but before
its defining MOVP**. The local FP value and eventual result must see the new
String (`"-1|%d|%s"`), rather than the old initializer (`"ok"`). This differs from
the earlier alias test, which changes MP between argument reads. Removal
controls detect substituting the original seed for the current MP input and
reversing preparation. Existing integer-value, full-width, root-map, GC,
call-site ownership and result checks remain active.

These are ordered recipes in a C-driven VM transport with preallocated native
storage and an already prepared synthetic frame. They do not replay every
instruction relative to FRAME allocation, implement source-language allocation
failure ordering, or execute an ordinary serialized caller. Full instruction,
initializer, result and owned-site admission remain backend work before AWM.
