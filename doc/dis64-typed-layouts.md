# Typed storage plans for the general 64-bit compiler

The compiler now computes Word32/ref64 storage directly from Limbo types and
declarations, beyond the hand-assigned frames of the executable private pilots.
The new planner is in `limbo/dis64-layout.c`; it does not yet emit a general
managed Dis64 module. [Русская версия](dis64-typed-layouts.ru.md).

## Inspect a real AWM frame

Build host Limbo normally, then request the independent layout diagnostics:

```sh
LIMBO_DIS64_LAYOUT=1 /host/tools/limbo -I module \
  -o /tmp/awm.dis appl/wm/awm.b 2>/tmp/awm-layout.log
```

Each `DIS64_LAYOUT` JSON line identifies an MP bank or a function's FP bank by
its final legacy PC and descriptor ordinal. Accepted records contain the old
extent, target extent, complete map8 and leaves of the form
`[legacy_offset, native_offset, legacy_bytes, native_bytes, private_kind]`.
For the current AWM `init`, the first two leaves are:

```json
[[32, 64, 4, 8, 12], [36, 72, 4, 8, 10]]
```

These are `ctxt: ref Draw.Context` and `argv: list of string`: independent
eight-byte roots at 64 and 72. An `int` remains four bytes. A nested ADT's fields
are placed recursively using natural alignment; multiplying an old offset by
two is not a layout algorithm. Field and object padding have no readable leaf.
Current AWM `init` needs 592 bytes including locals and compiler temporaries.
The numeric kind is a private compiler diagnostic, not a public Dis64 type ID.

The ordinary `.dis` file above remains a legacy file. The opt-in environment
variable does not change instructions, descriptors, hashes, initialization or
target selection. Using it with `-64l` reports the same layouts before the
existing general-managed refusal and produces no admitted AWM image.

## Supported storage and address roles

The planner handles byte/int/fix/big/real, String, ref, array, list, channel,
module and opaque polymorphic reference storage; nested tuple/ADT/exception
records; parameters; globals; locals; and compatible shared local slots.
References stop recursion: an array variable is a root, not an inline array
payload. Its element layout can be requested separately with `dis64layouttype`.
Recursive references therefore terminate; by-value cycles refuse.

Each concrete picked ADT variant has its own tag/common-field/payload plan.
The unspecialized pick base refuses: merging all variants' roots would allow GC
to interpret a scalar alternative as a pointer. These plans do not implement
pick allocation, tag dispatch or heap descriptor installation in the loader.

Compiler tables retain explicit roles:

| Storage | Native width | GC root |
| --- | --- | --- |
| Direct `alt` channel/array reference | 8 | Yes |
| `alt` address of a channel cell or payload | 8 | No |
| Numeric/string case branch PC | 4 | No |
| String case bound | 8 | Yes |
| Encoded import-name table | Preserved bytes | No |

Borrowed addresses, code coordinates and opaque import records cannot pass the
ordinary scalar/reference address resolver. Case target relocation and import
record writing still need dedicated compiler paths. Preserving their storage
does not make these tables executable under the new ABI.

## Compiler interface and refusal behavior

`dis64layouttype` plans a type; `dis64layoutstorage` combines actual parameter,
local or global declarations and verifies every root against the old descriptor.
`dis64layoutaddress` resolves one complete typed read or scalar subspan. Its
role is 0 for scalar, 1 for managed reference, -1 for borrowed native address.
It refuses padding, partial pointers, cross-field reads and wrong address roles.
The frame header, result address and reserved VM temporaries are not inferred
from user declarations. They remain the calling convention's responsibility.

Plans own their leaves and borrow the AST. Zero-initialize a plan and call
`dis64layoutfree` after success or refusal. A refused partial plan carries a
reason and cannot resolve addresses. Source types, offsets, maps and signatures
are never rewritten. Bounds cover object size (1 MiB), leaves (65536), recursive
depth (128), and shared-alias comparison work. Overlaps, inconsistent aliases,
wrong scalar widths and unexplained descriptor roots refuse.

## Reproduce the checks

```sh
python3 utils/tests/dis64-layout.py --limbo /host/tools/limbo
```

This independently decodes the emitted legacy Dis descriptors and validates
376 storage banks across all 16 AWM startup sources. Ordinary output is compared
byte for byte; `-64l` must still refuse with identical diagnostic plans. Optional
`--baseline /previous/limbo` compares against the previous compiler too.

The C fixture compares native `sizeof`/alignment/`offsetof`, nested fields,
aliases, picked variants, alt roles and case tables; checks malformed metadata
and complete allocation retirement; and requires three deliberate omissions to
fail their semantic assertions. Use `--c-only`, `CC`, `NATIVE64_CFLAGS` and
`NATIVE64_RUNNER` for cross execution. AMD64 ASan/UBSan and ARM64 UBSan/QEMU pass.
LeakSanitizer cannot run in this hosted ptrace environment; fixture-owned
allocation balance is checked explicitly with ASan leak scanning disabled.

The next integration is opcode-aware use of these relocations, caller/callee
result binding, payload descriptors, initializer/table writers and general
managed loader admission. Exceptions and variadic tails remain additional work.
This milestone does not execute AWM or produce a working Android GUI APK.
