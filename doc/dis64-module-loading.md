# Module loading and explicit init calls

Inferno uses the same module model for a command and for a library. Limbo `load`
becomes Dis LOAD: the runtime loads a module and links the requested interface.
It does not invoke an export named `init`. The shell performs a separate call.
The actual sequence is visible in `appl/cmd/sh/sh.b` around the `load Command`
and following `mod->init(ctxt.drawcontext, argv)` calls (generated from `sh.y`).

`Sys` is an interface; `$Sys` is the internal load path of a registered builtin.
Native exports resolve to C functions and Dis exports to bytecode PCs, through
the existing export tables and linker. The new String group uses those same
native Sys functions and the existing generated `limbo -H` frames. Host C
`sysmodinit` registration is separate from calling a Limbo export named `init`.
Existing thread/scheduler ownership code remains the basis of native calls.

The private Dis64 module metadata currently distinguishes presence of a bootstrap
entry. That is a startup hint for the explicit console launcher, not another
library ABI and not an instruction for LOAD to run `init`. The bounded provider
producer now checks its complete export bodies before considering the compiler's
optional entry hint. An export named `init` is accepted as an ordinary callable
export when its type/body fits the supported profile.

This runnable private-profile example makes that call explicit. Save `initlib.m`:

```limbo
InitLib: module {
    init: fn(value: string): list of string;
    unpack: fn(values: list of string): string;
};
```

Save `initlib.b`:

```limbo
implement InitLib;
include "initlib.m";
init(value: string): list of string { return value :: nil; }
unpack(values: list of string): string { return hd values; }
```

Save `caller.b`:

```limbo
implement Caller;
include "initlib.m";
Caller: module { init: fn(); };
init()
{
    lib := load InitLib "/initlib.dis64";
    values := lib->init("explicit call");
    text := lib->unpack(values);
}
```

```sh
/host/tools/limbo -I . -64m -o /build/initlib.dis64 initlib.b
/host/tools/limbo -I . -64l -o /build/caller.dis64 caller.b
/build/emu/Linux/o.emu-g -v -c 0 -r /build \
    -d /caller.dis64 -P /initlib.dis64 -X dis64-managed-trial
```

Repeat with `-c 1` for the requested JIT mode. The example returns without printing.
Its `InitLib->init` type comes from its own `.m`; it is not the shell's `Command`
interface. The owning collector gate checks that the first actual LOAD creates
no List and leaves the provider bootstrap entry unset before the explicit call.
`dis64-managed-module.py` compiles and executes the `InitLib` case alongside
arbitrarily named exports.

This corrects module-role selection within the existing bounded String/List
producer. It does not yet admit the complete `Command.init(Draw.Context, list of
string)` signature or run the Limbo shell/AWM in Dis64. General types and call
signatures remain the work needed to reach that goal.
