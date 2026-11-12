# Self-contained String callers in the managed file profile

The explicit `-64l` compiler profile now accepts a void `init()` and one materialized String
literal. The compiler places the literal in the existing Rune32 string pool.
The loader creates an owned VM String and publishes it in mapped module data
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

The bounds are one distinct materialized source literal, at most 255 runes, two matching
imports/exports and 32 calls. The argument/result cells remain at FP64/FP72.
For literal callers, MP0 holds the loaded module, MP8 the nil reference, MP16
the LOAD path and MP24 the literal; all four references are covered by map
`f0`. The loader copies Rune32 values directly into owned rune storage,
including embedded NUL and all signed32 bit patterns. It performs no UTF-8
round trip. These are private profile conventions, not new public initializer,
section or signature tags. Older literal-free images retain their layout.

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
rollback. It also executes 32 unseeded C/JIT cases with collection after every
VM quantum and checks the final String/list-head identity against the original
MP literal. An active control removes literal-root publication and must fail
at its explicit value predicate. Cross execution uses `CC`, `NATIVE64_ARCH` and `NATIVE64_RUNNER`; sanitizer flags use
`NATIVE64_CFLAGS`.
