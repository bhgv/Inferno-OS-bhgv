# Limbo Data Types and Writing C Extensions

[Russian version](limbo-data-types-ru.md)

These examples use Inferno’s standard Limbo compiler. The final section covers experimental Dis64 separately. Run the shell commands from the repository root. In the examples, `nil` is the empty value of a reference type, and `len` gives the length of a string, array, or list.

## 1. Numbers, strings, and composite values

Save the example as `types.b`. Running `types.dis` in Inferno invokes `init` through the `Command` interface.

```limbo
implement Types;
include "sys.m";
include "draw.m";

Types: module {
    init: fn(ctxt: ref Draw->Context, args: list of string);
};

Point: adt {
    x, y: int;
};

Row: adt {
    name: string;
    points: array of Point;
};

Scene: adt {
    rows: array of Row;
    origin: Point;
    selected: ref Point;
};

Value: adt {
    name: string;
    pick {
    Count =>
        n: int;
    Text =>
        text: string;
    Shape =>
        points: array of Point;
    Empty =>
    }
};

describe(v: ref Value): string
{
    pick p := v {
    Count => if(p.n > 0) return p.name;
    Text => return p.text;
    Shape => if(len p.points > 0) return p.name;
    Empty => return "empty";
    }
    return "unknown";
}

init(nil: ref Draw->Context, nil: list of string)
{
    sys := load Sys Sys->PATH;
    b: byte = byte 255;
    i: int = -42;
    wide: big = big 1 << 40;
    r: real = 1.25;
    s := "Привет";
    utf := array of byte s;
    words := s :: "Limbo" :: nil;
    (x, y) := (3, 4);

    p := Point(x, y);
    q := p;
    q.x = 9;
    shared := ref Point(7, 8);
    alias := shared;
    alias.x = 10;

    cells := array[] of {Point(1, 2), p};
    refs := array[] of {shared, ref Point(0, 0)};
    matrix := array[] of {array[] of {1, 2}, array[] of {3}};
    view := cells[0:1];
    view[0] = Point(5, 6);
    scene := Scene(array[] of {Row("first", cells)}, Point(0, 0), refs[0]);

    variants := array[3] of ref Value;
    variants[0] = ref Value.Count("items", 2);
    variants[1] = ref Value.Text("label", string utf);
    variants[2] = ref Value.Shape("shape", scene.rows[0].points);

    sys->print("%d %d %bd %g\n", int b, i, wide, r);
    sys->print("%s: %d runes, %d bytes\n", hd words, len s, len utf);
    sys->print("copy=%d/%d shared=%d view=%d matrix=%d\n",
        p.x, q.x, scene.selected.x, cells[0].x, matrix[1][0]);
    for(j := 0; j < len variants; j++)
        sys->print("%s\n", describe(variants[j]));
}
```

```sh
limbo -I module -o types.dis types.b
limbo -I module -S -o types.s types.b
```

The `copy` line should show `3/9`, `shared=10`, `view=5`, and `matrix=3`. The Russian word «Привет» occupies six runes and twelve UTF-8 bytes.

| Type | Storage and assignment |
|---|---|
| `byte`, `int`, `big`, `real` | An unsigned 8-bit integer, signed 32- and 64-bit integers, and a 64-bit floating-point value, respectively. Use explicit conversions such as `int b`, `big i`, and `real i`. |
| `string` | A sequence of runes. `s[i]` gives the rune value. `array of byte s` encodes the string as UTF-8 bytes, and `string utf` decodes them. |
| `(int, string)` | A tuple holds its field values. `(n, text) := pair` unpacks it. Copying a reference field preserves the reference to the same object. |
| `list of T` | A list ends in `nil`. `head :: tail` constructs a new head. Check for `nil` before using `hd` or `tl`. |
| `array of T` | An array reference. Assignment shares storage. A slice `a[lo:hi]` shares the original elements and excludes the upper bound. |
| `Point`, `ref Point` | Assigning an ADT value copies its fields. Assigning a `ref Point` copies the reference, so a field update is visible through both variables. |
| `array of Point`, `array of ref Point` | The first stores ADT values inline. The second stores references that can be replaced or set to `nil`. |
| `array of array of int` | The outer array stores references to rows. Rows can have different lengths, as in `matrix`. |
| `Row`, `Scene` | ADTs can contain arrays and other ADTs. Copying a `Row` copies `name` and the reference `points`, so the point elements remain shared. |
| `ref Value` | A reference to a tagged ADT. `Value.Count` and `Value.Text` carry different fields. A `pick` statement binds a variable to the matching variant. |

Embed ordinary ADTs by value and use references for recursive structures. Declare a field that can close a cycle with `cyclic`, as in `module/json.m`. A tagged ADT can model a tree whose nodes carry a number, a string, or an array of child nodes.

## 2. Channels, functions, modules, and special types

Add these declarations at module scope in `types.b`.

```limbo
addone(n: int): int
{
    return n + 1;
}

sender(out: chan of int)
{
    out <-= 42;
}

apply(f: ref fn(n: int): int, n: int): int
{
    return f(n);
}

channeltest(): int
{
    c := chan of int;
    spawn sender(c);
    n := <-c;
    f: ref fn(n: int): int = addone;
    return apply(f, n);
}

Eighths: type fixed(0.125);
BadIndex: exception(int);

Box: adt[T] {
    item: T;
};

specials(i: int): (real, string, int)
{
    x := Eighths(1.25);
    y := Eighths(0.5);
    b := Box[string]("hello");
    n := 0;
    {
        if(i < 0)
            raise BadIndex(i);
        n = i;
    } exception e {
        BadIndex => (n) = e;
        * => raise;
    }
    return (real (x+y), b.item, n);
}
```

- `chan of T` carries values of type `T`. The example spawns a sender and returns 43 after receiving and applying the function. Channels can also carry tuples, arrays, or ADT references.
- `ref fn(...): T` holds a function reference. Use `fn(...)` without a result type for procedures.
- In `load Sys Sys->PATH`, `Sys` supplies the interface and `Sys->PATH` supplies the built-in name `$Sys`. The variable `sys` holds a reference to the linked module.
- `nil` takes its type from context. Conditions use `int`, with zero representing false. Flags are often declared with `con 1<<iota`.
- `fixed` declares a fixed-point type with a scale. At the C boundary, use the generated representation and document the scale explicitly.
- `exception` declares a named exception with parameters. `raise` transfers control to an `exception` handler.
- `adt[T]` and `fn[T]` introduce type parameters. See `Table[T]` in `module/tables.m` and the generic list functions in `module/lists.m`. Use concrete types at the C boundary so Limbo can generate a call frame and reference map.

## 3. Build a C extension from a Limbo interface

Save this file as `module/typedemo.m`.

```limbo
Typedemo: module {
    PATH: con "$Typedemo";

    Point: adt { x, y: int; };
    Row: adt {
        name: string;
        points: array of Point;
    };
    Value: adt {
        pick {
        Number => n: int;
        Text => s: string;
        Rows => rows: array of Row;
        }
    };

    plusone: fn(n: int): int;
    greeting: fn(): string;
    bytes: fn(): array of byte;
    firstx: fn(points: array of Point): int;
    measure: fn(value: ref Value): int;
    firstcell: fn(rows: array of array of int): int;
};
```

```sh
limbo -a -I module module/typedemo.m > typedemo.h
limbo -t Typedemo -I module module/typedemo.m > typedemomod.h
limbo -T Typedemo -I module module/typedemo.m
```

`-a` generates C types and call frames. `-t` generates the export table with names, hashes, and reference maps. `-T` generates function stubs.

Save the implementation below as `libinterp/typedemo.c`. Add `typedemo.$O` to the object list in `libinterp/mkfile` and add these rules. Indent commands under targets with a tab.

```makefile
typedemo.$O: typedemo.h typedemomod.h

typedemo.h:D: ../module/typedemo.m
	limbo -a -I../module ../module/typedemo.m > $target.tmp && mv $target.tmp $target

typedemomod.h:D: ../module/typedemo.m
	limbo -t Typedemo -I../module ../module/typedemo.m > $target.tmp && mv $target.tmp $target
```

Add `typedemo` to the `mod` section of the selected `emu` or `emu-g` configuration and rebuild it. `mkdevc` generates the call to `typedemomodinit()`. Register the built-in with the same name declared in `PATH`.

```c
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "typedemo.h"
#include "typedemomod.h"

void
Typedemo_plusone(void *fp)
{
    F_Typedemo_plusone *f = fp;
    *f->ret = (WORD)((UWORD)f->n + 1U);
}

void
Typedemo_greeting(void *fp)
{
    F_Typedemo_greeting *f = fp;
    retstr("hello", f->ret);
}

void
Typedemo_bytes(void *fp)
{
    F_Typedemo_bytes *f = fp;
    Array *a, *old;
    char bytes[] = "abc";

    a = mem2array(bytes, sizeof(bytes)-1);
    old = *f->ret;
    *f->ret = a;
    destroy(old);
}

void
Typedemo_firstx(void *fp)
{
    F_Typedemo_firstx *f = fp;
    Typedemo_Point p;

    *f->ret = 0;
    if(f->points == H || f->points->len == 0)
        return;
    memmove(&p, f->points->data, sizeof(p));
    *f->ret = p.x;
}

void
Typedemo_measure(void *fp)
{
    F_Typedemo_measure *f = fp;
    Typedemo_Value *v = f->value;
    Typedemo_Row row;
    Array *rows;

    *f->ret = 0;
    if(v == H)
        return;
    switch(v->pick){
    case Typedemo_Value_Number:
        *f->ret = v->u.Number.n;
        break;
    case Typedemo_Value_Text:
        if(v->u.Text.s != H)
            *f->ret = v->u.Text.s->len;
        break;
    case Typedemo_Value_Rows:
        rows = v->u.Rows.rows;
        if(rows == H || rows->len == 0)
            break;
        memmove(&row, rows->data, sizeof(row));
        if(row.points != H)
            *f->ret = row.points->len;
        break;
    }
}

void
Typedemo_firstcell(void *fp)
{
    F_Typedemo_firstcell *f = fp;
    Array *row;

    *f->ret = 0;
    if(f->rows == H || f->rows->len == 0)
        return;
    memmove(&row, f->rows->data, sizeof(row));
    if(row != H && row->len != 0)
        memmove(f->ret, row->data, sizeof(*f->ret));
}

void
typedemomodinit(void)
{
    builtinmod("$Typedemo", Typedemomodtab, Typedemomodlen);
}
```

Once the module is built in, save this program as `calltypedemo.b`, compile it, and run it in that Inferno configuration.

```limbo
implement Calltypedemo;
include "sys.m";
include "draw.m";
include "typedemo.m";

Calltypedemo: module {
    init: fn(ctxt: ref Draw->Context, args: list of string);
};

init(nil: ref Draw->Context, nil: list of string)
{
    sys := load Sys Sys->PATH;
    demo := load Typedemo Typedemo->PATH;
    if(demo == nil){
        sys->print("load Typedemo: %r\n");
        return;
    }
    Point, Row, Value: import demo;
    points := array[] of {Point(7, 8), Point(9, 10)};
    rows := array[] of {Row("first", points)};
    matrix := array[] of {array[] of {11, 12}, array[] of {13}};
    sys->print("%d %s %s %d %d %d\n",
        demo->plusone(2), demo->greeting(), string demo->bytes(),
        demo->firstx(points), demo->measure(ref Value.Rows(rows)),
        demo->firstcell(matrix));
}
```

```sh
limbo -I module -o calltypedemo.dis calltypedemo.b
```

Expected output is `3 hello abc 7 2 11`.

### Reading composite objects in C

| Limbo object | C representation and access |
|---|---|
| `string` | Use `String *` and convert to UTF-8 with `string2c`. Return strings with `retstr` or use `retnstr` when the byte length is known. |
| `array of Point` | Use `Array *`. The element stride is `a->t->size`. The generated `Typedemo_Point` describes an inline element for the matching ABI. |
| `array of ref Point` | Use `Array *` with reference elements. Check for `H` before dereferencing an element. |
| `array of array of int` | Each outer element is an `Array *`. Check the outer index, then the row reference, then the inner index. |
| `array of Row` | Each element contains the fields of `Row`, including string and array references. Copying the bytes into a temporary C struct gives borrowed references that remain valid while their owner is live. |
| `ref Value` | Use the Limbo-generated tag to select the variant layout. When allocating a value, use the descriptor for the specific variant so the GC can trace its reference fields. |
| List, channel, module | Use the runtime types `List`, `Channel`, and `Modlink` and their VM operations. The runtime defines their layouts. |

When creating an array with `heaparray`, pass the **element** descriptor, including its reference map. For an array of strings or an array of arrays, the element map describes a reference. For an array of ADTs, it describes the reference fields within each element, including fields inside ADTs embedded by value.

To store a borrowed reference, first retain the incoming object and mark it for GC using the runtime’s ownership mechanism. Publish the new value, then release the previous one. For a fresh object, you can transfer its owned reference to the slot. Account for calls between allocation and publication that may trigger GC or raise an exception. `H` is the VM’s empty reference value, and `nil` is the C null pointer. See `retstr` in `libinterp/runt.c` and `newmp` and `heaparray` in `libinterp/heap.c`.

## 4. What carries over to Dis64

Ordinary Dis builds its import table from `.m` declarations and links the group by name and hash. Dis64 also needs a representation for serialized values, a native frame layout, and reference ownership rules. A matching interface hash checks the Limbo type. Slot widths, GC maps, and the result ABI need separate checks.

| Area | Status in the experimental branch |
|---|---|
| Numbers | `limbo -64` and scalar execution profiles support a bounded subset. Support depends on the operation and admission profile. |
| Strings | The Rune32 pool stores names and strings in the file format. Constructing a `String *` and passing a managed string through an import are separate runtime operations. |
| Modules | A restricted paired-module loader and an explicit `$Sys` path for `millisec` exist. AWM uses a broader set of imports. |
| Arrays, ADTs, tagged variants, and nesting | The examples above compile to ordinary Dis. Dis64 execution needs compatible reference maps, operations, ownership rules, and admission for the relevant profiles. |
| C calls | The branch uses `limbo -H` to generate native64 declarations for selected interfaces. Check registration and the startup profile for each function. |

Use the ordinary C frames from `-a` with their matching runtime ABI. For ARM64, check the native declarations, reference maps, and profile tests. JIT emission checks, file validation by FlashVM’s independent reader, and execution on a target architecture establish different properties.

Further reading includes the [Limbo manual](../man/1/limbo), [language addendum](limbo/addendum.ms), [tagged variants in JSON](../module/json.m), [Dis64 pilot profile](dis64-pilot.md), and the articles on developing Limbo modules in C ([part 1](https://habr.com/ru/articles/118665/) and [part 2](https://habr.com/ru/articles/118696/)).
