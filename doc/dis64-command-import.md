# Command with its own dependency

The selected `-64b` profile supports a Command that exports ordinary `init` and loads one file module itself. `load` binds the module; the caller invokes `init` explicitly.

Save these blocks in order as `library.m`, `dependency.b`, `command.b`, `client.b`. Command comes from `module/sh.m`; the compiler calculates its signature hash.

```limbo
Library: module {
    inc: fn(n: int): int;
};
```

```limbo
implement Library;
include "library.m";
inc(n: int): int
{
    return n + 1;
}
```

```limbo
implement Command;
include "sys.m";
include "draw.m";
include "sh.m";
include "library.m";
init(ctxt: ref Draw->Context, argv: list of string)
{
    if(ctxt != nil || len argv != 2){ z := 0; bad := 1/z; }
    m := load Library "dependency.dis64";
    if(m == nil){ z := 0; bad := 1/z; }
    n := len argv;
    value := m->inc(n);
    if(value != 3){ z := 0; bad := 1/z; }
}
```

```limbo
implement Client;
include "sys.m";
include "draw.m";
include "sh.m";
Client: module { init: fn(); };
init()
{
    m := load Command "library.dis64";
    if(m == nil){ z := 0; bad := 1/z; }
    argv := "wm" :: "abc" :: nil;
    m->init(nil, argv);
    m->init(nil, argv);
    if(len argv != 2){ z := 0; bad := 1/z; }
}
```

```sh
limbo -64b -I module -I . -o dependency.dis64 dependency.b
limbo -64b -I module -I . -o library.dis64 command.b
limbo -64c -I module -I . -o client.dis64 client.b
emu-g -r . -c 1 -X dis64-calls -d /client.dis64
```

These commands require a prepared 64-bit emu with Dis64 enabled; `-r` selects the root containing all three output files. Use `-c 0` for the C interpreter. The client calls Command twice with two arguments; Command checks Context and the list, calls `Library.inc(2)`, and checks that the result is 3. A missing dependency triggers an intentional check failure.

The library has no process entry point. Its RET returns to the client. Outgoing Imports and incoming Exports are separate tables using existing Dis64 sections. The loaded module owns its request and name buffers.

This stage accepts one import path/group, scalar dependency signatures or fixed Command.init, and one public init. Private helpers work, but only root init may import. General ADTs, arbitrary public signatures, multiple paths, exception handlers and mutable MP globals remain outside this stage. This does not establish Android GUI/AWM execution.
