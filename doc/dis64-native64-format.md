# Private typed native64 formatter

The shared runt.c parser now reads through xprinttake. Its public xprint wrapper
still selects legacy packed frame alignment/strides. Explicit
DIS_NATIVE64_FORMATTER includes native64-format.h after that shared core, adding
an internal native64format entry which reads only a constructed Native64Arguments
context. No Sys variadic registration or file-call admission is enabled.

A fresh cursor is created for each formatting attempt. Every consumed slot is
checked against the requested conversion: byte/int/fix supply four-byte integer
bits, big supplies eight-byte integer bits, real supplies exact binary64 bits,
String requires a reference slot with exact Tstring identity, and debug H accepts
a typed reference. Width stars also consume checked integer slots. Missing slots
raise exBounds; incompatible kinds raise exInval. All these checks remain active
when output fills. Extra unused slots retain ordinary printf semantics.

Eight-byte slots contain native-order values. Integer decoding narrows to UWORD
and copies its four-byte bit pattern, independently of host byte order; managed
references are converted from uintptr bits; real/big retain all eight bytes.
This adapter never constructs a host va_list or relies on legacy pointer strides.
The context is trusted constructed storage, immutable and pinned for the complete
synchronous attempt. Pattern/reference provenance and exact live Type identities
are established by the constructor's trusted schema callbacks. The formatter
cannot authenticate arbitrary C pointers passed as purported contexts.

The caller controls output retry/growth and result publication. The tests run the
actual shared parser and lib9 under native64argumentscall: a producer formats,
allocates an actual managed String and adopts it before collection; the resolver
publishes only on success. Sys_sprint/aprint/print/fprint still explicitly refuse
the native64 variadic path. Full startup remains blocked pending complete builtin
ABI and safe native64 registration/wiring. This private formatter is one part of
that work, not a complete replacement Sys interface.

## Reproduction and limits

```
python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
NATIVE64_CFLAGS='-O3 -DNDEBUG -g -fstrict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
```

Per emu/os and WORD32/WORD32-file combination, 1,728 formatter transactions cover
three collector phases, eight process states, nine format/type modes and eight
capacities from one to 128 bytes. Mixed signed/byte/fix/big/real values, star width,
Unicode characters, ASCII/Rune patterns and strings, H strings, embedded NUL,
repeat attempts, full buffers, wrong scalar/reference kinds and missing tail
arguments are tested. They retain the previous 456 construction/combined-call
transactions per combination and the frame-pin/Type-owner removal controls.
Another separately compiled control removes the typed conversion refusal and
must fail an actual result/error predicate. Real lib9 conversions, newstring/
newrunes, heap, Type, collector and protected-port bodies remain in the test;
bounded pool and deterministic native/process error text are fixture services.

Existing public packed-format, span, I/O and result-owner gates also run to check
the legacy reader after extraction. The complete runt.c translation unit compiles
with the native64 Sys header and formatter switch for NDK ARM64/x64. These are
compiler/hosted tests, not full emulator boot or physical device evidence. The
previous ASan failure during injected error unwinding remains open; this series
does not suppress or claim to resolve it. No Dis64 wire identifiers are assigned.

## Bounded growth and managed results

native64formatgrow constructs a Native64Formatted owner with an explicit initial
capacity and upper limit. It refuses invalid geometry and a nonempty output owner
before allocation. Each retry starts a fresh typed cursor. Capacity doubles only
when that cannot overflow or exceed the limit; the final step clamps to the exact
limit. The shared parser's UTF safety margin is retained conservatively. Reaching
the limit without proving complete output raises exNomem rather than publishing
possibly truncated output. Length counts bytes, including embedded NULs.

Mutable output ownership lives outside kprotect's jump frame. Every allocation
and parse failure releases acquired output, resets the owner and restores the
original VM diagnostic after free. Retirement detaches first and is repeatable.
The caller must keep arguments/pattern rooted and immutable throughout all retries.

native64formatstring and native64formatarray compose growth with actual c2string
and mem2array respectively. String results use normal VM UTF decoding and Rune
storage; byte arrays retain exact UTF bytes and NULs. The enclosing result guard
owns a value before fallible pinning. A second protected boundary retains native
output through conversion/pinning and frees it on all success/refusal/error paths.
These helpers are synchronous producers for an enclosing native64argumentscall
transaction; they do not publish a caller destination themselves or enable Sys.

The extension adds 384 growth/result transactions per port/model (1,536 total).
Sixteen scenarios across three collector phases/eight process states cover early,
intermediate and final growth allocation refusal; hard output limit; String and
byte-array heap/pin/resolver failures; late conversion mismatch; embedded NUL;
exact UTF-to-Rune conversion; and byte-preserving UTF arrays. Output exceeds the
initial capacity and exercises repeated parsing. A separate successful probe
checks the non-power-of-two 400-byte clamp, refusal to overwrite a live buffer,
invalid geometry without allocation and repeated retirement. Native frees
intentionally alter the error text; the original success/failure diagnostic must
survive. A separately compiled control removes final native-output retirement
and must fail allocation-balance predicates, alongside the existing kind/pin/Type
controls. Complete actual c2string and mem2array bodies are included in the gate.
The earlier ASan unwind finding remains open and is not suppressed here.

## Generated variadic prefix schemas and target execution

The four typed Sys gateways now consume `LimboNative64Prefix` records emitted
by fresh `limbo -H module/sys.m`, selected by `LIMBO_NATIVE64_PREFIXES`.
Each schema holds the module-relative name, existing Limbo hash, fixed argument
count, full fixed-frame size and map8, with a nil function pointer. It is separate
from `LimboNative64Entry` implementation tables. The 39 fixed Sys exports remain
unchanged; no variadic prefix becomes a callable builtin entry.

The gateways validate the generated schema against their C prefix and then lend
its immutable fixed record to the existing typed-tail constructor. They no longer
construct a second handwritten fixed record. Name/hash/count, C extent and maps
must all agree. Formatter/type checks, complete tail maps and protected result
ownership still apply. See [the prefix/C gateway tutorial](dis64-native64-prefixes.md).

`native64-prefix.py` verifies nine schemas with 159 active predicates per model:
all four Sys prefixes, qualified ADT methods, mixed byte/int/big/real/String
prefixes, inline ADTs, an all-variadic module and a 150-reference fixed prefix.
It checks complete tail-map composition, malformed schemas, refusal to bind a
prefix as a fixed implementation, deterministic output and explicit opt-in.

The full formatter/constructor/gateway suite now accepts `NATIVE64_RUNNER`, and
runs on GNU x64, ARM64 and RV64. Its `--android-ndk` option selects ARM64 NDK clang,
Android port headers and static Bionic. For example:

```sh
NATIVE64_RUNNER=qemu-aarch64 python3 utils/tests/native64-arguments.py \
  --formatter --limbo /path/to/host-tools/limbo \
  --android-ndk /path/to/android-ndk-r27c
```

The emu/os protected-body fixtures and WORD32/WORD32-file combinations each run
456 constructor/combined-call, 384 growth, 1,728 formatter, 1,104 Sys result and
1,200 Sys write transactions, plus eight root/Type/removal controls. Android
allocator aliases remain test boundaries; real heap/collector/format/gateway
bodies are retained. The Android GUI library can also link all four exported
`Sys_*64` gateways using `--native64-formatter`; bytecode variadic calls and AWM
startup are still not admitted by that switch.

Current x64 O2 and O3/NDEBUG strict-aliasing ASan+UBSan runs pass the complete
formatter suite and its removal controls. The historical optimized injected-
unwind report above did not reproduce in those runs; its original root cause
has not been established by a revision/toolchain bisect. The separate historical
Sys UTF error-unwind finding is not reclassified by formatter success. A new
native x64/ARM64 ASan+UBSan and RV64/QEMU CI matrix preserves this coverage.
Process-inspection leak scanning remains outside the bounded collector fixture;
its explicit allocation/refcount/root balance predicates stay active.

## Active VM frame bridge

`Sys_sprintframe64`, `Sys_aprintframe64`, `Sys_printframe64` and
`Sys_fprintframe64` consume a trusted active native frame through
`Native64SysFrameRequest`. A complete frame Type must exactly match the generated
fixed prefix plus an explicit immutable typed-tail recipe. The source is the
current MCALL frame in REG, not an arbitrary raw data pointer. Both callee and
caller boundaries are found by walking the current extent; full maps, Type
identities, process/module identity and the caller result span are checked before
payload reads. The return-address cell at offset 32 must equal that exact result
field. Scalar WORD results require an unmarked four-byte field; managed results
require a complete marked pointer field. The trusted call site supplies the
language result kind; map bits alone do not establish it.

Before constructing the owned argument copy the bridge publishes live REG to
Prog for collector visibility. Result resolution checks the active frame and
map again after formatting/conversion and, for print/fprint, after reacquisition.
Kill flags, process identity, guarded epoch changes and conflicting caller leases
cause refusal. Old results survive refusal; owned temporary frames, metadata and
produced results still retire through the existing guards. This interface borrows
the active VM process and descriptors, and does not authorize reset, reentry,
stack retirement or escaped asynchronous use. Older caller extents are refused.

The complete formatter gate adds 384 real MFRAME/MCALL cases per emu/os and
WORD32/WORD32-file combination. Normal and 150-slot calls exercise the actual
opcode C bodies, heap, collector and Sys gateways. Negative cases cover full-map
mismatch, frame bounds/identity, canonical kinds, result roots/width/address,
invalid process state, cancellation during result allocation or after a write,
epoch invalidation and independent old-result preservation. Module references,
input owners, output owners, Type/vector allocations and error depth must balance.
The two new removal controls independently delete the return-address equality
check and full-map comparison, then require a semantic predicate failure.

These tests set up decoded operands/result addresses in trusted C. They do not
claim a Limbo variadic source producer, serialized variadic file admission, new
LEA semantics, JIT execution or AWM startup. All existing four packed-entry
refusals and the 39-entry fixed Sys registration remain unchanged.


The next compiler/VM integration stage uses `limbo -Dq` to derive private
call-site plans from original variadic declarations and instantiated argument
types. Tests feed the generated 2/150-slot descriptors into production
FRAME/MFRAME/MCALL, with 768 GC/cancellation/refusal cases per port/model.
The producer records the original signature and result kind, complete map and
canonical tail geometry; 4096/4097 descriptor-count boundary tests retain
legacy output unchanged. This diagnostic JSON is not a wire section or a
managed signature assignment. Values and result-address binding remain
trusted C inputs, and ordinary source-to-Dis64 variadic admission is pending.


Trusted pending-frame result binding is available as `Sys_bindframe64`. It
validates the active caller, exact pending frame/map and declared result-cell
role before writing the full pointer at byte 32. The entered gateways repeat
all late resolution checks. Runtime tests use this production operation rather
than a manual return-pointer store, with 32 refusal cases per VM transaction
and pointer-width/pending-state removal controls. This adds no file metadata,
new LEA semantics, signature IDs or ordinary variadic exports.


Trusted C call-site recipes can now be owned by a Module. Installation copies
ordered tail metadata and retains exact frame/reference Types, then publishes
the table atomically. Binding and entered dispatch require the actual caller's
Module identity, and active calls block explicit table retirement. Ordinary
`freemod` retires this metadata before its Type table. The private fields/callback
are never parsed from file flags. There is still no agreed serialized site/PC
record or general variadic file admission; compiler diagnostic plans cannot be
installed as executable bytecode merely by parsing their JSON.


Compiler call-site provenance now includes final legacy FRAME/LEA/MCALL PCs,
descriptor ordinals, prepared-frame registers and resolved result/import/module
operands, correlated to source type plans by diagnostic identity. An independent
classic decoder checks the written bytes, including nested and multi-file calls.
The trace explicitly names the legacy PC space and does not convert those PCs,
WORD32 offsets or descriptors into executable native64 code. Owned Dis64 PC/site
mapping and general serialized variadic lowering/admission remain separate.


The compiler's private argument-store pass now selects canonical native writes
from proven legacy argument fields: CVTWL for eight-byte sign-extended int/fix,
MOVL/MOVF or existing conversions for big/real, and MOVP for reference ownership.
It publishes only complete direct-write regions. Independent decoding validates
the original operands; VM fixtures execute the generated choices rather than
manually packing values. The source operands are explicitly still legacy FP/MP
coordinates, with C-provided mappings in the runtime gate. This introduces no
serialized instruction/profile admission and does not bypass unsupported
arithmetic or temporary cleanup regions.


Private store programs now carry typed source-bank relocation. WORD reads keep
four-byte width, LONG/REAL keep eight, and reference reads expand from legacy4
to native8. Original bounds/root maps are checked; identical typed reads share
storage while conflicting overlaps are refused. The bank has a full native
map/extent and retains original per-read provenance. Its explicit `per-read`
policy does not authorize value caching. VM tests read a real rooted/pinned
bank at generated offsets with GC between reads, rather than substituting C
operand addresses per operation. Bank values remain immutable C fixture inputs;
this does not establish general caller/MP relocation or serialized admission.

The private `-Dq` store plan now includes complete native source FP/MP layouts
and each typed read's origin offset. The runtime fixture seeds those managed
source extents, gathers at every original read through production MOV handlers,
and exercises a String alias changed between format and tail reads with GC.
This is a source-storage transport test; fixture C still supplies initial
values, and serialized caller/initializer/result lowering remains pending.
See `dis64-native64-prefixes.md` for the layout and ownership boundaries.
