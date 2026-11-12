# Self-contained String callers in the managed file profile

The explicit `-64l` compiler profile now accepts a void `init()` and up to 32 materialized String
literals. The compiler places the literals in the existing Rune32 string pool.
The loader creates an owned VM String for each root and publishes it in mapped module data
before scheduling. A console `-A` input is unnecessary. This remains the private
String/List trial, selected with `-X dis64-managed-trial`; ordinary `-64` and
general managed linkage are unchanged. It does not start Android AWM.

This small provider demonstrates ownership and round trips; it implements a
subset of the String interface, rather than the full String library algorithms.
Save it as `literal-provider.b`:

```limbo
implement String;
String: module {
    unquoted: fn(value: string): list of string;
    quoted: fn(value: list of string): string;
};
unquoted(value: string): list of string
{
    return value :: nil;
}
quoted(value: list of string): string
{
    return hd value;
}
```

Save the caller as `literal-caller.b`:

```limbo
implement Caller;
include "string.m";
Caller: module { init: fn(); };
init()
{
    lib := load String "/literal-provider.dis64";
    items := lib->unquoted("Grüße 世界");
    value := lib->quoted(items);
    items = lib->unquoted("second\0line");
    value = lib->quoted(items);
    lib = load String "/literal-provider.dis64";
    items = lib->unquoted("Grüße 世界");
    value = lib->quoted(items);
}
```

From the repository root, use native host tools and a console emulator built
with the native64 managed-file profile. Both outputs belong under the root
passed to `emu -r`:

```sh
/host/tools/limbo -64m -o /build/literal-provider.dis64 literal-provider.b
/host/tools/limbo -I module -64l -o /build/literal-caller.dis64 literal-caller.b
/build/emu/Linux/o.emu-g -c 0 -v -r /build \
    -d /literal-caller.dis64 -P /literal-provider.dis64 \
    -X dis64-managed-trial
```

The caller returns normally after both imports; it does not print the String.
Repeat with `-c 1` to check the requested JIT mode. That request does not prove
that this caller completed in machine code. The serialized LOAD path must
match the supplied provider path exactly. Embedded NUL in a LOAD path is
refused instead of silently truncating it; NUL inside literal data is preserved. `-64m` retains its prelinked behavior;
source literals are admitted only by the explicit file profile.

The same literal can be assigned to a local, aliased, passed repeatedly and
reloaded from module data after another call. Nil remains a separate mapped
reference, and `quoted(nil)` still faults. Empty source constants and their
local aliases retain the nil representation selected by the Limbo frontend.
An independently serialized non-nil empty pool record creates an empty owned
String; the constructor tests distinguish that object from a nil reference.

The bounds are 32 distinct materialized literals, at most 255 runes each and
1024 runes in total, two matching imports/exports and 32 calls. Empty source
constants still use nil and do not consume a materialized root. The
argument/result cells remain at FP64/FP72. MP0 holds the loaded module, MP8 the
nil reference, MP16 the LOAD path and MP24+8*i literal i. The pointer map spans
all 3+n references, including additional map bytes. For 32 roots the data
extent is 280 bytes and the map is `ff ff ff ff e0`.

The loader checks every literal index, map bit and pool record before creating
objects. Every materialized root must be used by the checked code. The loader
copies Rune32 directly into independent owned rune storage, including embedded
NUL and all signed32 bit patterns; equal serialized values also receive
independent objects. No UTF-8 round trip, public initializer, section or
signature tag is added. Older literal-free and single-root images keep their
layout and bytes. MP and FP operands are separate address spaces, even when
their numeric offsets coincide.

Reproduce the focused complete-emulator gate:

```sh
python3 utils/tests/dis64-pair-boot.py --root /build \
    --emu /build/emu/Linux/o.emu-g --limbo /host/tools/limbo \
    --managed-literal-only
python3 utils/tests/dis64-numeric-gc.py --managed --limbo /host/tools/limbo
```

The first gate checks compiled callers without input seeds, with an ignored
external seed, normal/nil-error retirement, one/two imports and the 32-call
boundary. It also rejects malformed maps, missing/oversized pool records,
invalid MP operands and String-to-List type mismatches before scheduling.
The second uses actual constructors, String allocators and the production
collector to check exact Rune32 values, root survival and every allocation
rollback. It also executes unseeded C/JIT cases with collection after every
VM quantum and checks the final String/list-head identity against the original
expected MP literal and verifies all roots after collection. An active control removes literal-root publication and must fail
at its explicit value predicate. Cross execution uses `CC`, `NATIVE64_ARCH` and `NATIVE64_RUNNER`; sanitizer flags use
`NATIVE64_CFLAGS`.

`dis64-managed-multiliteral.py` also compares complete emitted CODE and type
maps with independent byte oracles derived from source argument order. Cases
cover aliases, alternating strings, nil, Unicode/NUL, repeated LOADs, every map
byte boundary through 32 roots, the exact 1024-rune limit and malformed upper
indices/maps/pools.

See [named user-defined modules](dis64-managed-modules.md) / [пользовательские модули](dis64-managed-modules.ru.md) for the same checked shapes with arbitrary interface and method names.
