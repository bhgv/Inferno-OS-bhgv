# Ordinary Command.init through Dis64 load

The selected `-64b` / `-64c` trial now compiles and executes the actual
`Command.init(ctxt: ref Draw->Context, argv: list of string)` declaration from
`module/sh.m`. `load` binds a module; the caller explicitly invokes `m->init`.
A library has no process entry point. Its ordinary RET returns to the caller.
This is the same distinction used by Limbo sh, which calls init after loading.

Save the following three blocks as `command.m`, `library.b`, and `client.b`.
The wrapper includes the existing declarations rather than restating an ABI.

```limbo
include "sys.m";
include "draw.m";
include "sh.m";
```

```limbo
implement Command;
include "command.m";
init(ctxt: ref Draw->Context, argv: list of string)
{
    saved := ctxt;
    total := 0;
    while(argv != nil){
        part := hd argv;
        total += len part;
        argv = tl argv;
    }
    keep := saved;
}
```

```limbo
implement Client;
include "command.m";
Client: module { init: fn(); };
init()
{
    m := load Command "library.dis64";
    if(m == nil) return;
    s := "library.dis64";
    argv := s :: s :: nil;
    ctxt := ref Draw->Context;
    m->init(ctxt, argv);
    if(len argv != 2){ z := 0; bad := 1/z; }
    m->init(nil, argv);
    keep := ctxt;
}
```

With the newly built Limbo compiler and the repository module directory:

```sh
limbo -64b -I module -I . -o library.dis64 library.b
limbo -64c -I module -I . -o client.dis64 client.b
limbo -H -M Command -I module command.m > command.h
```

Place both images in the emulated namespace root, then run the rebuilt console:

```sh
emu -r /path/to/root -c 0 -X dis64-calls -d /client.dis64
emu -r /path/to/root -c 1 -X dis64-calls -d /client.dis64
```

The expected result is an empty Client exit string and normal root retirement.
The default policy refuses these opt-in images. Starting the entryless library
with `-d /library.dis64` also refuses. A failed load returns nil; it does not run
init. No special loader-triggered init hook is involved.

## Types, hashes and frames

`Sys` or `Command` in a load expression is an interface descriptor. The path
expression is a module path, or a built-in name such as `$Sys`. Limbo translates
that expression to Dis; the interpreter consumes Dis instructions. This series
adds a file-backed Command binding, not another Sys implementation.

`limbo -H -M Command` and the selected writer both obtain the existing `.m`
function hash through `sign(fn)`. For the current declarations it is `4244b354`.
The loader first matches full signature bytes, module and symbol, then passes
the **consumer's** hash to the normal `linkmod` export resolver. The hash checks
consistency; it is not authentication. Normal linking clones the provider MP.
The complete source module is registered before its temporary MP pin is dropped;
normal unload removes it. Allocation refusals retire all intermediate owners.

| Value | Native location | GC role |
| --- | --- | --- |
| ctxt | callee FP+64 | opaque ADT reference |
| argv | callee FP+72 | list of String |
| parameter prefix | 80 bytes total | map bytes `00 c0` |
| bare Draw.Context | 24 bytes | three mapped pointer fields, initially nil |

Arguments are copied into a normal prepared frame by MFRAME/MOVP/MOVP/MCALL.
The callee can consume its argv variable or copy ctxt without changing the
caller's variables. List cells and String objects remain shared managed objects.
The actual collector sees both caller and callee frames, including callee-private
cons cells. Existing C dispatch and JIT paths execute the same checked code.

`ctxt := ref Draw->Context;` creates a bare record with nil display/screen/wm.
This trial treats the reference as opaque: allocation, copy and nil comparison
are supported. Fields, GUI handles and `ref Draw->Context(nil,nil,nil)` field
initialization are outside admission. A non-nil record does not establish a
working display or window manager.

## Exact private boundary

The opt-in private signature is exactly four big-endian words:
`face0186`, existing Limbo function hash, arity `2`, void result `0`.
This is a new **private trial schema**, not a public managed signature assignment.
No new public opcode, section or type-kind ID is assigned; prior scalar signature
v1 bytes remain unchanged. Other implementations must not infer general managed
signature interoperability from this fixed-shape experiment.

The admitted provider has one Command.init export at PC0/frame0, no imports or
helper calls, a bounded proven body and the existing private 24-byte MP layout.
The executable importer still has an argument-free void process entry. One
interface/path is admitted; arbitrary literals, general DATA initializers,
additional exports, context fields and general ADT/array payloads remain refused.
Only the selected client reader admits the required MP-to-prepared-frame MOVP;
the general scalar instruction reader is unchanged.

Reproduce the source/GC matrix with `dis64-command.py`, complete-console execution
with `dis64-command-boot.py`, independent raw refusal checks with
`dis64-command-proof.py`, and compatibility with `dis64-command-producer.py`
(all in utils/tests, each accepts `--limbo`). The callee-frame omission control
must detect a reclaimed observed object before resuming execution.
Linux x64/ARM64 QEMU results do not establish Android/Bionic GUI/APK startup.
