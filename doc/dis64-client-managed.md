# Strings and lists inside an importing Dis64 client

[Русская версия](dis64-client-managed.ru.md).

The existing file-client writer now emits mapped String and `list of string`
locals. The loader checks their classes, copies their maps into runtime Types,
and executes the existing reference/list handlers and JIT. Calls to the provider
still have scalar signatures; the client entry remains argument-free and void.
This is a managed heap/caller step toward ordinary `Shell.init(context, argv)`,
not admission of that managed exported signature.

## A complete list-walking client

`library.m`:

```limbo
Library: module {
    PATH: con "/demo/library.dis64";
    inc: fn(n: int): int;
};
```

`library.b`:

```limbo
implement Library;
include "library.m";
inc(n: int): int { return n+1; }
```

`client.b`:

```limbo
implement Client;
include "library.m";
Client: module { init: fn(); };
init() {
    m := load Library Library->PATH;
    if(m == nil)
        return;
    s := Library->PATH;
    words := s::s::nil;
    n := 0;
    while(words != nil) {
        part := hd words;
        n += len part;
        words = tl words;
    }
    expected := 2*len s+1;
    last := m->inc(n);
    if(last != expected) {
        zero := 0;
        bad := 1/zero;
    }
    keeps := s;
    keepwords := words;
}
```

Create `/demo` below your emulator root and run:

```sh
limbo -64b -I . -o /path/to/root/demo/library.dis64 library.b
limbo -64c -I . -o /path/to/root/demo/client.dis64 client.b
emu -r /path/to/root -X dis64-calls -c 1 -d /demo/client.dis64
```

Use `-c 0` for the interpreter. Successful execution returns silently. A missing
provider takes the ordinary nil guard; a wrong result trips the source check.
The filename's actual length is used, so relocating the example does not depend
on a hard-coded path length.

## Values and lifetime

This selected profile admits String roots holding its already owned filename or
nil, and flat lists of those Strings. Extra String literal DATA, concatenation,
list-valued imports/exports, nested lists, arrays and ADT graphs remain outside
this file writer. The existing one-path/import-table and code/frame limits remain;
64 proof slots now count scalar spans and mapped roots together.

| Source operation | Behavior |
| --- | --- |
| `alias := s`, `alias := words` | Retains the same managed owner |
| `s = nil`, `words = nil` | Releases the previous owner |
| `len s`, `len words` | Nil length is zero |
| `s::words` | Allocates a pointer-valued list node and retains its head/tail |
| `hd words` | Produces a retained String head |
| `tl words` | Produces a retained list tail |
| `words == nil`, `words != nil` | Existing full-width equality branches |

The list nil test is a pointer comparison, not a Word32 truncation. `hd nil` and
`tl nil` raise the existing `dereference of nil` VM error and unwind normally.
String equality/concatenation and arbitrary reference comparisons are not added.
Native Type identities and the existing Tptr list payload descriptor stay the
runtime's responsibility; no C struct is serialized into the file.

Every mapped local starts as runtime H through ordinary frame initialization.
The checker seeds only those root facts; scalar reads still require definitions
on every incoming CFG path. It propagates class equalities across MOVP aliases
before checking String/List uses. Unknown root class means an initially nil root
or nil-only aliases until an operation constrains it; it grants no arbitrary
pointer payload or descriptor permission. String and list classes cannot alias.
All allowed producers are closed: filename/nil, MOVP, CONSP, HEADP and TAIL.

The frame prefix and reserved scalar result tail cannot be mapped. Numeric reads
or writes cannot overlap mapped roots, and managed instructions cannot access
unmapped cells. A mapped caller selects the existing typed result-address binder,
which separately checks the unmapped result cell and prepared callee frame. The
result address is native-only constructor metadata; wire LEA and scalar signatures
remain unchanged. Scalar-only clients retain their prior binder and exact bytes.

## Collector and rejection gates

```sh
python3 utils/tests/dis64-scalar-client.py --managed --gc --limbo /path/to/limbo
python3 utils/tests/dis64-scalar-client-managed-proof.py --limbo /path/to/limbo
python3 utils/tests/dis64-scalar-client-managed-producer.py \
  --limbo /path/to/limbo --baseline /path/to/prior/limbo
python3 utils/tests/dis64-scalar-client-boot.py --managed \
  --limbo /path/to/limbo --root /path/to/root --emu /path/to/emu
```

The actual collector runs three complete color epochs between nonterminal VM
quanta. The bounded arena is sweepable; an independent observer snapshots the
reachable frame/list graph before collection and verifies survival before the
next instruction. Omitting frame marking or list traversal must lose an observed
object and produce semantic exit10. Neither a crash nor a sanitizer report counts
as a successful omission control. Reference counts, descriptors, modules, stacks
and all arena slots must retire. Transport/namespace seams remain fixture-backed;
complete-console execution is a separate gate.

`CC`, `NATIVE64_ARCH` and `NATIVE64_RUNNER` select a cross target; boot uses
`--runner`. Native-port automation includes the managed GC matrix and omission
controls. Host ASan/UBSan covers independent raw checking and constructor-only
execution; whole-VM ASan and concurrent collection are not new claims. These
Linux/QEMU checks do not establish Android GUI/Bionic/device startup.
