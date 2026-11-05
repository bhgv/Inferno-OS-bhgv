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
