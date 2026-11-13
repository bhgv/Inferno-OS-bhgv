# Serialized String arguments to the native Sys module

The private `-64l` file trial executes the real fixed `Sys->werrstr(string): int`
entry through `LOAD $Sys`, MFRAME and MCALL. The same fixed String-to-Word
group now admits `chdir` and `remove`, and can mix them with `millisec`, `sleep` and `dup`, with up to twelve distinct imports including `fildes` and `iounit`. This connects serialized String
storage to a builtin native frame and a Word result. It remains an explicit
trial; ordinary `-64`, variadic print, other builtin signatures and Android AWM
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
The entry is `init()` without input; an optional `-A` seed is ignored. The extended frame described below permits Word expressions and control flow.
`werrstr`, `chdir` and `remove` can share the same group; other methods and
mixed file/builtin calls remain outside this profile.

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

## Word results, conditions and loops

Save the following as `sys-loop.b`. It makes three calls inside the loop and one
after it; it does not print. Change `-c 0` to `-c 1` to request JIT mode.

```limbo
implement SysText;
include "sys.m";
SysText: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    n := 0;
    status := 0;
    while(n < 3){
        status = sys->werrstr("tick");
        if(status != 0)
            return;
        n++;
    }
    status = sys->werrstr("done");
}
```

```sh
/host/tools/limbo -I module -64l -o /build/sys-loop.dis64 sys-loop.b
/build/emu/Linux/o.emu-g -v -c 0 -r /build \
    -d /sys-loop.dis64 -P '$Sys' -X dis64-managed-trial
python3 utils/tests/dis64-pair-boot.py --root /build \
    --emu /build/emu/Linux/o.emu-g --limbo /host/tools/limbo \
    --sys-string-flow-only
python3 utils/tests/dis64-sys-flow-gc.py --limbo /host/tools/limbo
```

The extended producer accepts Word arithmetic/bitwise operations, comparisons,
conditional calls, loops with break/continue, early void returns, short-circuit
conditions and nested `werrstr` expressions. It preserves live call results
across later calls. Initial LOAD stays first; repeated LOADs retain their order
relative to scalar effects. The existing 80-byte straight-line images remain
byte-identical.

An extended caller uses 112 bytes with the same `{0x00,0x80}` pointer map:

| Cell | Meaning |
| --- | --- |
| FP64 | Owned String argument |
| FP80, FP84, FP88 | Three persistent Word cells |
| FP104 | Four-byte Word result |
| FP108 | Unmapped result padding |

Only declared scalar cells can be read or written as Words. The full-pointer
return binder accepts the checked 80/112-byte caller geometries and the checked
fixed native callee geometries below. It never treats a Word or its padding as a root.
The compiler and loader check definite initialization using a bounded CFG
intersection solver, including the zero-iteration path through a loop. Branches
can enter scalar steps, later LOADs or the start of a complete argument/call
block; they cannot skip argument publication, enter a pending frame, or reenter
the initial LOAD. Every encoded instruction must be structurally reachable.

The bounds are 32 static call sites, 64 scalar/LOAD events and three persistent
Word cells. A loop may execute a static call repeatedly. More live scalar
identities are rejected. String-local assignments and aliases in a function
with branches are conservatively rejected: the current pointer-identity proof
does not merge such assignments across CFG edges. Pass a literal or nil at a
conditional/looped call; existing straight-line String aliases still work.
The mixed fixed-signature extension below also admits numeric arguments; general and variadic Sys calls remain outside this trial.

The owning flow gate checks actual error-text event sequences and call counts,
32-bit wrap and bitwise expressions, branch outcomes, nested calls, normal/error
retirement and GC after every quantum. Independent raw CFG cases reject missing
join definitions, loop-only definitions, reads of roots/padding and branches
into call preparation before allocation. Active controls replace fact
intersection with union and omit native result publication; both must fail at
explicit semantic predicates. Native/heap constructor rollback and conversion
errors after entering the 112-byte caller path are checked separately.

## Fixed native String groups

Use `python3 utils/tests/dis64-sys-groups.py --limbo /host/tools/limbo --root /build --emu /build/emu/Linux/o.emu-g`
and `python3 utils/tests/dis64-sys-groups-gc.py --limbo /host/tools/limbo` to reproduce
the group gates. They cover all six independently permuted import orders, exact
MFRAME/MCALL ordinal matching, distinct method identities despite equal structural
types, 80/112-byte callers, real namespace statuses and removal of a file created
inside a fresh temporary emulator-root directory. The compiler retains its
canonical import order; the independent wire permutations test the runtime.

The owning gate calls the actual `Sys_chdir`/`Sys_remove` wrappers with controlled
OS boundaries. It forces GC during release and native operations, retargets the
borrowed pending frame after the wrapper snapshot, and verifies the original
result address and exact four-byte status. Allocation-capable String conversion
now occurs while the VM is held, before release; failure enters no OS operation
and leaves release/acquire balanced. The existing scheduler/thread machinery is
reused. See [module loading](dis64-module-loading.md) for why LOAD never calls init.


## Mixing fixed String and numeric calls

One caller can import all twelve methods through the same `$Sys` module and linker.
No method-specific module loader or automatic `init` call is introduced.

| Methods | Arguments | Native frame | Pointer map |
| --- | --- | --- | --- |
| `millisec` | none | 64 bytes | `00` |
| `sleep` | one Word | 72 bytes | `00 00` |
| `dup` | two Words | 72 bytes | `00 00` |
| `werrstr`, `chdir`, `remove` | one String | 72 bytes | `00 80` |

All return a four-byte Word through the checked full-pointer result address.
Numeric arguments occupy native offsets 64/68; they never become GC roots.
Mixed callers use the 112-byte layout above, even with no String arguments.
The compiler and loader require each scalar argument to be initialized on every
incoming path. Writing an argument in a pending frame does not initialize a
caller cell. Generated entry identity, signature, size and pointer map must agree
before any owning module allocation.

Save as `sys-fixed.b` and compile/run with the same commands, replacing
`sys-text` by `sys-fixed`:

```limbo
implement SysFixed;
include "sys.m";
SysFixed: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    stamp := sys->millisec();
    status := sys->werrstr("start");
    status = sys->sleep(stamp-stamp);
    status = sys->dup(-1, -1);
    if(status < 0)
        status = sys->werrstr("invalid descriptor");
}
```

Run `utils/tests/dis64-sys-fixed.py` with the same `--limbo`, `--root`, `--emu`
(and optional `--runner`) arguments as the group gate. The owning gate is
`python3 utils/tests/dis64-sys-fixed-gc.py --limbo /host/tools/limbo`.
It calls the real C handlers with controlled OS boundaries, forces GC and
checks argument snapshots, exact Word results, import permutations, nested calls,
loops, reloads and constructor rollback. A deliberate dataflow corruption must
fail the admission predicate. These checks do not establish full shell/AWM
support or native JIT coverage for every opcode.


## Two String arguments in a mixed frame

The same loader and linker now admit `unmount(string, string): int` and
`bind(string, string, int): int`. Their generated native frames are 80 and 88
bytes, both with map `00 c0`. Strings occupy offsets 64/72; bind flags occupy
80. The 112-byte caller roots both Strings at FP64/FP72 with map `00 c0`.
Its Word cells FP80/84/88 and result FP104 remain unmarked.

```limbo
implement SysPair;
include "sys.m";
SysPair: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    status := sys->bind("/dis", "/dis", Sys->MAFTER);
    if(status == 0)
        status = sys->unmount("/dis", "/dis");
    status = sys->werrstr("namespace calls completed");
}
```

Save as `sys-pair.b` and use the commands above with `sys-pair` as the name.
The example adds and removes a binding inside the emulator namespace.
Both calls use the existing C wrappers. Both String conversions precede
`release`; failure of either conversion enters no OS operation and preserves
the result cell. The owning gate forces GC between instructions and at the OS
boundary, mutates borrowed arguments after the wrapper snapshot and checks the
original text, flags and result address. Publishing an argument pointer cannot
establish an initialized scalar-result fact.


## Managed FD results and arguments

The explicit file trial also admits `fildes(int): ref Sys->FD` and
`iounit(ref Sys->FD): int` through the same `$Sys` module and generated entries.
The caller remains 112 bytes. FP96 is a separate full-pointer FD owner; FP104
still holds only a four-byte numeric result. Its map is `00 88`, or `00 c8` when
both String roots are needed. The callee frames are both 72 bytes: `fildes` has
map `00 00`, and `iounit` has `00 80`.

Save as `sys-fd.b` and use the compile/run commands above with `sys-fd`:

```limbo
implement SysFD;
include "sys.m";
SysFD: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    fd := sys->fildes(1);
    unit := sys->iounit(fd);
    saved := fd;
    fd = nil;
    unit = sys->iounit(saved);
    saved = nil;
    fd = sys->fildes(2);
    unit = sys->iounit(fd);
    fd = nil;
    status := sys->werrstr("FD calls completed");
}
```

This duplicates the emulator's existing descriptors. Dropping the last local
alias clears the FD root and closes the duplicate; retirement also releases it.
The bounded producer retains one current FD identity, with aliases of that
identity. Reading an older alias after a later `fildes`, `open` or `create` replaces it is refused;
multiple independent live FDs and general ADT field access remain pending.
Conditional pointer assignments retain the earlier conservative restrictions.
Numeric inputs may be precomputed in Word locals. Returns from `fildes` inside
branches and loops are supported, including paths with zero loop iterations.

The private signature carries an explicit ref/one-field-ADT/int tree, rather
than an integer return or a pointer-width guess. Its private `0x73`/`0x74` tags
are not public format assignments or general ADT admission. Compiler and loader
track FD availability separately from Word initialization. A managed return
cannot initialize FP104, and pointer argument stores cannot invent scalar facts.

The owning gate runs actual `Sys_fildes`, `Sys_iounit`, `mkfd`, `freeFD`, heap,
collector, frames and linker against controlled OS descriptor boundaries. It
checks replacement, aliases, last-owner nil, normal/error retirement, rollback
before publication and allocation failure after duplication (with and without
an old owner). `iounit(nil)` raises the existing native descriptor exception in
the complete emulator; its wrapper now reacquires the VM before rethrowing.
The exceptional result remains unpublished and owned descriptors still close.
These two fixed FD signatures do not enable general file I/O or full AWM startup.


## Opening and creating files

The same managed-result path admits `open(string, int): ref FD` and
`create(string, int, int): ref FD`. Both generated callee frames are 80 bytes
with map `00 80`: the path is rooted at native offset 64, mode occupies 72,
and create permissions occupy 76. The caller keeps its String at FP64 and
its FD result at FP96; these are distinct from Word locals and FP104.

Save as `sys-open.b` and use the commands above with `sys-open`:

```limbo
implement SysOpen;
include "sys.m";
SysOpen: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    fd := sys->create("/open-example.txt", Sys->OWRITE, 8r600);
    unit := sys->iounit(fd);
    fd = nil;
    fd = sys->open("/open-example.txt", Sys->OREAD);
    unit = sys->iounit(fd);
    fd = nil;
    status := sys->remove("/open-example.txt");
}
```

Run it in a writable emulator root with that example path available for use.
The file is created, closed, reopened, closed and removed. A failed open/create
returns nil; as with the previous example, this bounded snippet assumes those
operations succeed before calling `iounit`.

String conversion happens while the VM is held, before `release`. The existing
C wrappers retain their owner-replacement behavior: they clear the result and
release its old FD before attempting the new operation. If native opening fails,
the result stays nil. If `mkfd` fails after a successful native operation, its
real cleanup closes the new descriptor; the old descriptor is already released.
Neither failure publishes a half-constructed FD. The same current-FD/alias bounds
apply to all three creators.

Create permissions are a 32-bit Limbo bitmask. They are passed through `UWORD`
to the native `ulong` parameter so `DMDIR` retains its bit pattern without sign
extension on LP64. The gate covers this high-bit case, and a deliberate removal
of that conversion must fail the native argument predicate. It also checks both
scalar offsets independently, Unicode conversion failure, replacement success
and failure, nil paths, loops, aliases, reloads and all twelve import identities.
Full array-based read/write and general shell/AWM admission remain pending.
