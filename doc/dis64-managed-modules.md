# User-defined modules in the private managed file profile

The existing String/List file trial now accepts user-defined module and method
names. It still admits only the checked two-function profile below. Ordinary
`-64`, general managed calls, arrays, ADTs and Android AWM are outside this
profile. [Русская версия](dis64-managed-modules.ru.md).

Save this interface as `box.m`:

```limbo
Box: module {
    pack: fn(payload: string): list of string;
    unpack: fn(items: list of string): string;
};
```

Save the provider as `box-provider.b`:

```limbo
implement Box;
include "box.m";
pack(payload: string): list of string
{
    return payload :: nil;
}
unpack(items: list of string): string
{
    return hd items;
}
```

Save the caller as `box-caller.b`:

```limbo
implement Caller;
include "box.m";
Caller: module { init: fn(); };
init()
{
    lib := load Box "/box-provider.dis64";
    items := lib->pack("Grüße 世界");
    text := lib->unpack(items);
    items = lib->pack("second\0line");
    text = lib->unpack(items);
}
```

Compile with native host tools and run with a console emulator built with the
native64 managed-file profile. The two output files must be under `/build`:

```sh
/host/tools/limbo -H -M Box box.m > box-native64.h
/host/tools/limbo -I . -64m -o /build/box-provider.dis64 box-provider.b
/host/tools/limbo -I . -64l -o /build/box-caller.dis64 box-caller.b
/build/emu/Linux/o.emu-g -c 0 -v -r /build \
    -d /box-caller.dis64 -P /box-provider.dis64 \
    -X dis64-managed-trial
```

The caller returns without printing. Repeat with `-c 1` to exercise the requested
JIT mode; this is not proof that the whole caller runs as machine code. The LOAD
path must exactly match `-P`. The generated header is an ABI witness; these
Limbo provider bodies do not require a hand-written C extension.

`limbo -H` computes the type signatures from the `.m` interface. The runtime
checks structural signatures, frames, maps and provider code, then matches
module and exported names exactly. Renaming a provider without recompiling its
caller fails even when the types match. These shapes use 72-byte frames with
the managed argument at offset 64 and pointer map `{0x00,0x80}`. The generated
header exposes their actual signature constants; the names do not determine
the argument type.

The optional console `-A` seed is classified from a checked signature for an
input-taking caller. A String argument receives a String; a list argument
receives a one-element list containing a String. This remains true if the
functions are named `quoted` and `unquoted` in the opposite direction. Without
`-A`, list input is nil and `hd` faults. The void caller above needs no seed.

The profile permits one export per direction, at most two exports/imports,
and the checked `payload :: nil` / `hd items` bodies, including the supported
local/alias/nil cleanup forms. Module and function identifiers are bounded to
255 ASCII characters. The loader owns copied names and metadata; temporary
catalog buffers do not escape into live links. `-64m` callers retain the
canonical prelinked String profile; named file callers use `-64l`.

The existing [literal limits and ownership rules](dis64-managed-literals.md)
apply: at most 32 materialized literals, 255 runes per literal, 1024 runes total
and 32 calls. This does not admit mixed builtin calls or a general GUI program.

Reproduce the named-interface and owning collector gates:

```sh
python3 utils/tests/dis64-pair-boot.py --root /build \
    --emu /build/emu/Linux/o.emu-g --limbo /host/tools/limbo \
    --managed-module-only
python3 utils/tests/dis64-numeric-gc.py --managed --limbo /host/tools/limbo
```

They compile actual `.m` headers and C frame assertions, compare complete CODE
and import signatures, exercise arbitrary/reversed names and 255-character
identifiers, and reject nominal mismatches and invalid names before scheduling.
The collector gate checks exact values and seed heap types, forced collection,
allocation rollback, and active controls for missing roots and name-based seed
classification. Cross execution uses `CC`, `NATIVE64_ARCH`, `NATIVE64_RUNNER`;
sanitizer flags use `NATIVE64_CFLAGS`.
