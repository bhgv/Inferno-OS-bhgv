# Serialized String arguments to the native Sys module

The private `-64l` file trial executes the real fixed `Sys->werrstr(string): int`
entry through `LOAD $Sys`, MFRAME and MCALL. This connects serialized String
storage to a builtin native frame and a Word result. It remains an explicit
trial; ordinary `-64`, variadic print, mixed builtin groups and Android AWM
are not enabled. [Русская версия](dis64-sys-string.ru.md).

Save as `sys-text.b`:

```limbo
implement SysText;
include "sys.m";
SysText: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    message := "Grüße 世界";
    saved := message;
    message = nil;
    status := sys->werrstr(saved);
    sys = load Sys Sys->PATH;
    status = sys->werrstr("second\0suffix");
}
```

With native host tools and a console built with the native64 managed-file and
Sys I/O profiles, put the output under the emulator root:

```sh
/host/tools/limbo -I module -64l -o /build/sys-text.dis64 sys-text.b
/build/emu/Linux/o.emu-g -v -c 0 -r /build \
    -d /sys-text.dis64 -P '$Sys' -X dis64-managed-trial
```

Repeat with `-c 1` to request JIT execution. Successful scheduler retirement is
checked in both modes; it does not mean every instruction ran in machine code.
`werrstr` changes the current process error text and returns zero. It does not
print. The second call preserves the complete String in VM storage, while the
existing C-string error API sees `second` before the embedded NUL. Oversized
error text retains the existing `kstrcpy` truncation behavior.

`Sys` is the Limbo interface and `Sys->PATH` supplies `$Sys`; the runtime consumes
only serialized Dis64. Limbo writes a private structural String-to-Word record.
The resolver then verifies the actual generated `limbo -H module/sys.m` entry,
including its function identity, legacy consistency hash, 72-byte frame and
`{0x00,0x80}` argument map. Hash equality alone never authorizes a call.

The caller frame is 80 bytes. FP64 owns its current String argument; FP72 holds
the four-byte Word result with an unmapped four-byte suffix. A checked callback
binds the full eight-byte return address into the pending native frame. It
validates the caller map, stack position and callee argument map. The mapped
argument stays alive across conversion and GC; the scalar result is never
interpreted as a pointer. Repeated calls replace the owned argument normally.

MP0 holds the loaded module, MP8 nil, MP16 the owned `$Sys` path and MP24+8*i
independent String literals. Up to 32 roots, 255 runes each, 1024 total runes
and 32 straight-line calls are accepted. Supported local aliases, nil and
repeated LOADs use the same producer checks as the named String/List trial.
The entry is `init()` without input; an optional `-A` seed is ignored. Consuming
or branching on the Word result, other Sys methods and mixed file/builtin calls
are still rejected by this profile.

Run the exact byte-oracle, full-console and owning runtime gates:

```sh
python3 utils/tests/dis64-sys-string.py --limbo /host/tools/limbo \
    --root /build --emu /build/emu/Linux/o.emu-g
python3 utils/tests/dis64-sys-string-gc.py --limbo /host/tools/limbo
```

The owning gate uses the actual `Sys_werrstr`, `string2c`, `kstrcpy`, resolver,
linker, frame binder and collector. It checks exact Unicode/NUL/nil/truncated
error text, four-byte result publication and untouched adjacent padding,
collection after every VM quantum, all constructor allocation failures and
conversion errors after MCALL has entered the native frame. Malformed code,
maps, identities and signatures fail before owner allocation. Active controls
remove the result store and literal publication and must fail explicit value
predicates. Cross runs use `CC`, `NATIVE64_ARCH`, `NATIVE64_RUNNER`; flags use
`NATIVE64_CFLAGS`.
