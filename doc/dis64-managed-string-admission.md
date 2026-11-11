# Managed String calls in Dis64

The first fixture covers two signatures from `module/string.m`.

| Export | Argument | Result |
|---|---|---|
| `unquoted` | `string` | `list of string` |
| `quoted` | `list of string` | `string` |

## Candidate signature representation

Keep existing version-1 scalar records. A separate signature version could use a
bounded type expression for each parameter and the result. A string is a leaf
type. A list has exactly one child type. For this first profile the only
admitted list expression is a list whose child is string. Keep scalar leaves
distinct if the version later admits mixed signatures. Comparing byte widths or
the number of mapped references would confuse the two rows above.

This proposal leaves version numbers, tags and byte encoding for joint review
with FlashVM. No file admission or registry entry is enabled by this document.

### Candidate record for review

Keep the enclosing import/export signature length. Inside that span, use a
big-endian version word, a parameter-count word, one result expression, then
exactly that many parameter expressions in order. Each expression begins with
one big-endian type tag. A leaf consumes one word. A list consumes its tag
followed by one element expression. For this pilot the parser can reject a
list immediately unless its child is the string leaf. This gives a maximum
depth of two without allocating a type graph.

| Symbolic record words | Signature |
|---|---|
| `managed-version, 1, List, String, String` | `unquoted` |
| `managed-version, 1, String, List, String` | `quoted` |

Each record consumes five words. The enclosing length must match exactly.
Reject truncated leaves, missing children, unknown tags, a list with an
element other than string, extra words, and counts other than one in this
pilot. Match the argument and result in order. A width or map comparison
would accept the swapped signature, since both records carry a single mapped
argument and return a reference.

The table uses symbolic words pending agreement. A reviewer can compare these
records and refusal cases without allocating any version or tag values.

## Admission and execution checks

1. Validate the full parameter and result expressions and match them to the
   named export before allocating the callee frame. Reject a swapped string
   and list even on a machine where both references occupy eight bytes. Reject
   a list with a different element type.
2. Verify the selected export's frame extent and map. The generated String
   frames have a borrowed result address at byte 32 and a mapped argument at
   byte 64. The borrowed address is neither a serialized pointer nor a GC root.
3. Retain the argument while publishing it to the mapped parameter. A caller
   result slot must be live, mapped, and initialized to VM nil before the call.
4. Keep an unpublished managed result rooted during any fallible work. Transfer
   its owner into the caller slot on success. An exception must retire the
   unpublished result and allow the VM to retire the callee argument and stack.
5. Validate these rules with nil and non-nil values, return and exception
   paths, GC while the callee is live, and both ordinary and grown stacks.

`utils/tests/dis64-numeric-gc.py --managed --limbo /path/to/limbo` exercises
production MFRAME, MCALL, stack teardown and GC with the generated declarations.
The callbacks are fixture implementations of the two ownership directions.
File-level managed signatures and execution of the Limbo String bodies require
separate tests.

Limbo's opt-in `LIMBO_NATIVE64_MANAGED` declarations now derive the two
supported symbolic shapes directly from `module/string.m`. The collector
fixture matches the generated entry, argument kind, and result kind before
constructing its call frames. A compiler control checks that a list of int
does not receive the list-of-string witness. These are private C symbols
and do not define file kind numbers.

The candidate reader in `include/dis64-managed-signature.h` takes the version
and type tags from an explicit schema. The managed collector fixture uses
local trial values and decodes both directions before matching the generated
export and allocating frames. It checks every one-bit mutation of both trial
records, every shorter length, a trailing byte, a nested list, and the
unchanged rejection of the trial records by the scalar reader. No normal
Dis64 reader or loader selects the trial schema.

## Shared trial fixtures

FlashVM's review accepts this record shape for the bounded pilot. For
independent fixture replay, use the following big-endian words. The values
`face0010`, `71` and `72` are test-only substitutions for the symbolic
version, String tag and List tag. They carry no registry assignment.

| Record | Hex words |
|---|---|
| Import `String.unquoted` | `0 1 14 face0010 1 72 71 71` |
| Export `unquoted` | `1 1 0 14 face0010 1 72 71 71` |
| Import `String.quoted` | `0 2 14 face0010 1 71 72 71` |
| Export `quoted` | `2 1 0 14 face0010 1 71 72 71` |

The name pool is `String`, `unquoted`, `quoted` in that order. The provider
name is pool entry zero. Both exports select Dis-PC one in a two-instruction
extent and descriptor zero. The descriptor is 72 bytes, aligned to eight,
with map bytes `00 80`. These are independent record fixtures. They omit
module, code and table-count envelopes and make no claim about an executable
provider.

Expected refusals include truncating the import by a byte, selecting another
provider name, reducing the instruction extent to one, substituting the other
symbol, changing the frame ID to one, swapping the export signature, and
changing the descriptor map to `00 00`. Refusal must preserve the output and
take place before native allocation. The ordinary version-1 scalar reader
continues to reject the trial signature records.

The managed collector test also builds complete consumer and entryless provider
images with an independent Python writer. The import's module and symbol use
different string ordinals from the provider's name and export. The candidate
reader first checks the complete envelope, storage and record counts, then
matches the names by Rune contents and verifies the selected PC and frame.
Both directions pass. Refusal cases cover truncation, names, signature,
entry, map, duplicate export, the unentered prefix, opcode, indirect offset
and code trailers. The ordinary scalar storage reader still refuses each
managed consumer. The candidate gate returns checked coordinates and a borrowed
provider body to the isolated fixture.

The fixture also executes two minimal one-token providers in the Inferno VM.
It decodes the existing indirect destination operands and calls the provider
through MFRAME, managed LEA and MCALL. CONSP constructs a list containing
the supplied string and HEADP returns the first element of a supplied list.
RET, stack growth and a failing provider body run with the real collector.
These bodies exercise the ownership boundary for one token. They do not
implement the Limbo String module's quoting, whitespace or tokenization.
The successful bytecode runs take the provider body from those checked images.
The harness still constructs the caller in memory and injects a separate
fault body for unwind tests. The normal Inferno loader does not select this
trial gate.

The candidate caller now carries MFRAME, MOVP, LEA, MCALL and EXIT. It owns
the input at Frame64 byte 64, the result at byte 72, and the linked provider
at MP byte zero. The candidate gate checks the complete caller instruction
sequence and both maps before returning either body. The VM fixture decodes
both checked bodies and executes them with collection between quanta. The
fixture supplies the input and constructs the linked owners in memory. The
normal file loader still has no trial-schema entry point.

The paired execution fixture now hands the checked storage views to the
existing Inferno module constructor and rooted link constructor. It takes
the provider's selected PC and frame from the validated export, installs
the link in the caller's mapped MP slot, and collects before starting the
caller. The allocation-failure sweep covers the partial caller and provider
construction and root handoff. Input setup remains fixture-owned, and the
normal command-line loader still does not select the trial schema.

## Explicit console trial

The Linux native64 console can read the two checked trial images with
`emu -v -X dis64-managed-trial -d /consumer.dis64 -P /provider.dis64`.
This option requires both explicit files and is mutually exclusive with
sibling lookup and other experimental launch profiles. The argument starts
at VM nil. The one-token CONSP provider returns a list with a nil element.
The HEADP provider raises the VM's nil-reference error. Both paths retire
the process. The provider can use the JIT with `-c 1`, while the caller
uses the checked predecode result binding and interpreter. The test harness
also exercises non-nil inputs and allocation failures through the same
pair constructor. The ordinary scalar file modes reject these trial images.

## Compiling a trial provider

`limbo -64m -o provider.dis64 provider.b` selects the private trial producer.
It checks the resolved legacy instruction sequence and exports a profile-zero
provider. For example:

```limbo
implement String;
String: module { unquoted: fn(value: string): list of string; };
unquoted(value: string): list of string { return value :: nil; }
```

The other accepted source has `quoted: fn(value: list of string): string`
and returns `hd value`. The compiler preserves the exact symbolic direction.
It fuses the legacy temporary list into the mapped result and writes no
module-data reference into the output. The two bodies exercise a single
token. The full `appl/lib/string.b` library still needs general managed
lowering. Ordinary `-64` continues to use the scalar format.

The explicit `-64m` producer also accepts a matching consumer with 1 through 32 calls:

```limbo
implement Caller;
include "string.m";
Caller: module { init: fn(value: string); };
init(value: string)
{
    lib := load String String->PATH;
    result := lib->unquoted(value);
    result = lib->unquoted(value);
    result = lib->unquoted(value);
}
```

Compile both sides with `limbo -64m -I module` and pass both output files
to the explicit console trial. The compiler checks the original LOAD path,
the import and entry types and the entire lowered call sequence. It emits a
prelinked import for the file-pair loader. The GC suite uses the compiled
pairs for non-nil execution and keeps independent byte fixtures for refusal
tests. By default the console starts with a nil argument. Pass `-A text`
to seed a string for `unquoted` or a one-element list for `quoted`.

```sh
emu -c 1 -X dis64-managed-trial -d /caller.dis64 -P /provider.dis64 -A 'Grüße 世界'
```

The trial option accepts up to 4096 bytes of UTF-8, including an empty
string. The seed belongs to the managed startup frame and is traced by
the same pointer map as the call result.

## Repeated managed calls

The private caller may repeat the same imported function with its original
input, assigning each result to the same local. The compiler checks the legacy
module, pending-frame and result slots rather than assuming one fixed placement.
It emits the existing MFRAME/MOVP/LEA/MCALL sequence for each call, with an
ordinary MOVP of VM nil into the result slot before every subsequent call.
MOVP releases the old result through the normal ownership path. The managed
result-address binder still requires an empty, mapped result cell.

The caller frame remains 80 bytes: input at 64 and result at 72, both mapped.
A repeated caller has 16 bytes of mapped MP: the provider at zero and an
immutable nil source at eight. DATA initializers are forbidden, so that nil
cell cannot be replaced by an image payload. A one-call caller retains its
original eight-byte MP. The checker verifies every call and cleanup byte,
including exact ordering, map bits, source/destination cells and the 32-call
limit, before constructing either owner. Missing cleanup, wrong cleanup
operands, inconsistent MP maps/extents and a 33rd call are refused.

Independent wire fixtures cover 1, 2, 3 and 32 calls in both directions. The
compiler corpus compares emitted call/cleanup bytes with those fixtures and
executes Unicode and empty-string inputs in C/JIT provider modes. Production
GC tests execute independently serialized and compiler-produced 32-call pairs
with collection after each instruction, including stack growth. On x64,
ARM64 and RV64 the fixture completes 264 paired calls, 132 native provider
completions, 5859 collection epochs and the constructor allocation-failure
sweep. The caller continues to use the interpreter's managed address binder.

The `-64m` path remains an explicitly selected, prelinked String trial with
its private schema. The `-64l` path below adds bounded filesystem loading.
Full String quoting/tokenization and arbitrary managed signatures remain
requirements for ordinary AWM modules.

## Filesystem LOAD with generated interface hashes

`limbo -64l` is the explicit managed-file producer. Unlike `-64m`, it retains
an ILOAD in CODE and records the absolute ASCII load path in STRINGS. No new
signature tags or assigned Dis64 kinds are introduced. The existing String
trial shape and the 32-call limit still apply.

For example, use the provider and Caller above, compile the provider to the
path declared by `String->PATH`, and compile Caller in file mode:

```sh
mkdir -p "$ROOT/dis/lib"
limbo -64m -o "$ROOT/dis/lib/string.dis" provider.b
limbo -64l -I module -o "$ROOT/caller.dis64" caller.b
emu -r "$ROOT" -c 1 -X dis64-managed-trial \
    -d /caller.dis64 -P /dis/lib/string.dis -A 'Grüße 世界'
```

The explicit `-P` path remains required for this trial and must exactly match
the serialized path. Admission validates both images but does not construct
or prebind a provider Modlink. It installs the checked import table and path
in the caller. The ordinary VM ILOAD reads the path, calls readmod/parsemod,
checks the file admission policy, publishes its validated provider in the
normal module cache, and calls linkmod. MFRAME and MCALL then use that link.
The input seed is selected from the checked import before ILOAD executes.

The build generates `string-native64.h` with `limbo -H -M String` from the
real `module/string.m`. Generated `LIMBO_NATIVE64_SIGNATURE_*` constants
supply the legacy hashes used by both import and export tables. Structural
String/List signatures, exported PC/frame, pointer maps and the complete
one-token provider body are still checked independently of those hashes.
The parser admits only the explicitly selected provider path and this bounded
provider shape. A native-only cache flag prevents reuse across launch modes;
it is not a serialized type or a substitute for validation.

The file caller owns three mapped MP cells: provider at 0 (initially nil),
cleanup nil at 8, and load path at 16. DATA initializers remain forbidden.
Tests compare compiler output with independent ILOAD fixtures, execute both
symbolic directions in C/JIT on all three targets, and reject path/map/body
mutations. The 48-case allocation-failure sweep covers both the file export owner and
the unlinked caller's LDT/path construction, including cleanup of partial tables.
This adds real filesystem loading for the bounded String trial. General
managed libraries, multiple signatures and full AWM remain separate work.

## Repeated filesystem LOAD and cache ownership

File mode also accepts reassigning the same module variable between completed
calls, using the same absolute path and imported interface:

```limbo
    lib := load String String->PATH;
    result := lib->unquoted(value);
    lib = load String String->PATH;
    result = lib->unquoted(value);
```

The compiler preserves each LOAD position. The code checker consumes complete
call islands, verifies the declared instruction count, and permits an optional
reload only after result cleanup and before the next MFRAME. There are at most
32 calls and at most one LOAD before each call. Partial frames, consecutive/dangling
LOADs, changed paths, wrong reload operands and a 33rd call are refused. The
existing single-LOAD and prelinked formats remain accepted. Pointer maps and
signature tags are unchanged.

Every successful ILOAD creates a new Modlink. Ordinary readmod reuses the same
Module and JIT mapping when file identity is unchanged; linkmod builds the new
instance before the destination releases the old one. A changed QID causes
rereading, independent validation and replacement. The old Module remains live
until its existing instance is released. Failed reads or admission publish VM
nil through the ordinary ILOAD behavior; the following MFRAME refuses it.

The collector fixture executes the actual ILOAD, readmod, managed Dis64 parser,
linkmod and VM handlers with controlled namespace I/O. It compares Module and
JIT identities across cache hits, checks new Modlink identity, and exercises a
changed file, replacement on every LOAD, malformed replacement, short read, open failure and signing refusal.
The namespace callbacks provide file bytes/metadata; they do not supply a cached
Module or a linking result. Compiler-produced 32-LOAD callers run in both symbolic
directions, C/JIT modes and with stack growth, collecting after every instruction.
Across 56 cases, the fixture executes 832 LOADs and 400 native completions, and
verifies all module/instance ownership is released. Together with the existing
managed corpus this covers 25083 collection epochs. Complete console tests also
execute independent wire images and compiled sources on x64, ARM64 and RV64,
including sparse reload positions between repeated calls.

## Two imported functions and typed result chains

The trial now supports one or both String methods in one provider and one
import group. Both export bodies and every requested signature are validated,
including an unused export. Binding uses Rune names and complete structural
signatures, so import order, export order and code order need not agree.

A minimal two-method provider is:

```limbo
implement String;
String: module {
    unquoted: fn(value: string): list of string;
    quoted: fn(value: list of string): string;
};
unquoted(value: string): list of string { return value :: nil; }
quoted(value: list of string): string { return hd value; }
```

A matching caller can use a result as the next argument:

```limbo
implement Caller;
include "string.m";
Caller: module { init: fn(value: string); };
init(value: string)
{
    lib := load String String->PATH;
    items := lib->unquoted(value);
    value = lib->quoted(items);
    lib = load String String->PATH;
    items = lib->unquoted(value);
    value = lib->quoted(items);
}
```

Compile the provider with `-64m` and the caller with `-64l`, then run the
explicit file trial as above. Without the second LOAD, `-64m` also accepts
the caller for prelinked execution. The same 32-call limit applies. The two
callable entries have independently selected PCs and generated legacy hashes;
the actual import table is sentinel terminated and may contain two entries.
Startup input is selected from the first called ordinal, which may be one.

The caller still owns only the mapped cells at 64 and 72. Before passing the
latest result, MOVP retains it in the argument cell and releases the previous
argument. Only then does the ordinary cleanup clear the result cell. Independent
flow validation tracks String versus List[String] through both cells and checks
each MFRAME/MCALL ordinal. Missing, doubled or incorrect transfers, incompatible
signatures and duplicate names are refused before either owner is constructed.
The compiler also tracks legacy value identities, so an overwritten or no longer
available source local cannot silently select a stale native cell.

Generated `LIMBO_NATIVE64_ARGUMENT_*` and `LIMBO_NATIVE64_RESULT_*` macros now
preserve the `.m` argument/result kinds independently of optional C function
entries. The file bridge checks these kinds and uses the generated frame sizes
and map8 values alongside the hashes. Unsupported or changed interfaces cannot
silently inherit a hand-written frame specification.

The expanded fixture checks 192 independent/compiled wire pairs, both ordinal
orders and both provider body orders. Its 80 additional C/JIT/GC scenarios cover
864 LOADs and 536 native completions, including prelinked execution, stack growth,
cache replacement and a replacement file missing either requested method. String
pointer identity survives the complete round trip. A 52-case allocation-failure
sweep checks partial two-export and two-import table cleanup. Together with the
previous managed tests, collection runs through 53211 epochs on the checked
64-bit targets. Complete console tests execute Unicode, empty and nil inputs.

These are still the bounded one-token String bodies, not full tokenization or
general managed signatures. Arrays, ADTs, channels and the remaining AWM module
interfaces require further lowering; no new public wire tags are assigned here.

## Local names and pointer assignments

The caller may copy String and List[String] values between local variables before
LOAD and between complete calls. For example, this body uses the same provider
and compiler/run commands as the preceding example:

```limbo
init(value: string)
{
    original := value;
    lib := load String String->PATH;
    items := lib->unquoted(original);
    nextitems := items;
    lib = load String String->PATH;
    result := lib->quoted(nextitems);
    nextvalue := result;
    items = lib->unquoted(nextvalue);
}
```

The compiler tracks value identities through local pointer assignments, frame
writes and returned results. A local name is not a native root of its own: when
a call uses the latest result, that value replaces the previous argument root.
Using an alias of that older argument after replacement is still refused. Likewise,
an alias of an older result cannot be used after another call has replaced the
result root. Supporting those uses needs more mapped roots; copying a local name
does not silently add one. Nil assignments, branches and module aliases remain
outside this bounded caller profile.

Direct and aliased source versions must emit the same complete CODE bytes.
Console tests execute both versions, both initial kinds, prelinked/file modes,
4/32 calls, Unicode/empty/nil inputs and C/JIT execution. The 32-call aliased
versions also run through the actual ILOAD/cache/GC matrix. Eight compiler
refusals cover stale original and stale intermediate aliases, in both initial
kinds and both compiler modes, without producing a partial output file.

## Provider locals and equivalent method bodies

Provider validation now follows pointer value identities rather than requiring
one exact legacy instruction sequence. For example, the two-method provider may
use these bodies:

```limbo
unquoted(value: string): list of string
{
    copy := value;
    value = nil;
    items := copy :: nil;
    result := items;
    return result;
}
quoted(value: list of string): string
{
    copy := value;
    value = nil;
    result := hd copy;
    answer := result;
    return answer;
}
```

The proof starts with the typed argument, tracks nil and local aliases, and
requires exactly one CONS or HEAD of that original argument and exactly one
write of its result through the return address. CONS must start with a known
nil tail. Local frame slots are checked against that function's frame size;
unknown values, another effect, a replaced result, other instructions and an
invalid function boundary are refused. Clearing a local or the original argument
after saving its value does not lose the saved identity. The generated Dis64
provider remains byte-for-byte identical to the direct-body version.

Compiler checks compare 24 complete images: four body variants, either function
order, and one or both exports. Six other valid Limbo providers must fail the
experimental compiler without leaving output: two CONS operations, returning nil
instead of the constructed list, no construction, HEAD after TAIL, replacing a
HEAD result with nil, and two HEAD operations. Ordinary Limbo compilation of each
negative source is checked first. The aliased/reassigned providers also run in
the existing prelinked and file LOAD C/JIT/GC scenarios on all three targets.
This proof still admits one-token bodies only; it does not compile the complete
String implementation or enable AWM's remaining managed interfaces.
