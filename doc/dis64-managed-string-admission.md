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
Both directions pass. Twenty-two mutations cover truncation, names, signature,
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
