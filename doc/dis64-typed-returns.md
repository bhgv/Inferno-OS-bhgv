# Typed value returns on native64

[Русская версия](dis64-typed-returns.ru.md). This extends the earlier
[complete void bodies](dis64-typed-bodies.md), using the existing ISA.

## Limbo examples

These functions exercise scalar and managed results:

```limbo
count(n: int): int {
    total := 0;
    while(n > 0) { total += n; n--; }
    return total;
}
identity(text: string): string { return text; }
wide(x: big): big { return x; }
fraction(x: real): real { return x; }
octet(x: byte): byte { return x; }
items(xs: list of string): list of string { return xs; }
```

Declare the functions in the module interface as usual. The complete compilable
module and runtime cases are in `utils/tests/dis64-typed-body.py` and
`utils/tests/native64-typed-body.c`. Run from the repository root:

```sh
python3 utils/tests/dis64-typed-body.py --limbo /path/to/limbo --awm
python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/limbo \
    --omission typed-return-width --omission typed-return-map
```

The frontend writes through the classic return address at FP[16]+0. Lowering
maps that address to the full native pointer at FP[32]. Byte results retain one
byte, int/fix four bytes, big/real eight bytes; managed references occupy eight
bytes and use the existing reference move handler. No conversion of int to a
pointer-sized integer is introduced. Every reachable RET must have a defined
result on every incoming path. The worklist intersects definition facts at
joins; a loop or a single initialized arm cannot hide an undefined return.
Tuple/ADT results and other unsupported operations refuse the complete body.

## Trusted C binding

`include/native64-typed-return.h` binds two distinct owning heap buffers through
existing rooted `Native64Result` guards. The caller's result slot starts after
the 64-byte reserved prefix. Bind validates bounds, alignment, width, GC role,
heap types and the empty callee return pointer before publishing the address.
Resolution repeats these checks and requires the stored pointer and requested
width/reference role to match. Refusal performs no allocation or result write.
Clear removes a valid binding and is idempotent.

The binding borrows the guards: it acquires no independent ownership or pin.
Both guards, their values, types and maps must remain immutable, rooted and live
until clear. Use is synchronous, nonescaping and under existing VM exclusion.
This trusted C contract provides no pointer authentication, permanent revocation,
ABA defense or concurrent lifetime protocol.

Tests use staged owning caller/callee buffers and actual VM handlers with GC
between instructions. They cover nil, Unicode/NUL, old and same-object reference
replacement, actual lists, callee retirement, scalar canaries and invalid binding
refusals. These buffers are not active Prog stack frames: general production
module CALL/RET scheduling and module image admission remain unfinished.

## Private compiler report

`LIMBO_DIS64_TYPED=1` emits schema `inferno-typed-body-v2`. Each body adds
`result: [compiler_kind, native_bytes]`. Code rows contain six values:
`[opcode, mode, source, middle, destination, return_contract]`. The last value is
zero for ordinary instructions; for a result store it is byte width OR 16 for
a managed reference. Reference width eight therefore encodes 24. This is a
private checked-IR report and replay contract, not an assigned Dis64 file format.
Existing bytecode, file admission and public opcode numbers are unchanged.

The AWM source matrix now accepts 14 complete bodies and refuses 346. This is
source analysis, not AWM execution, a GUI APK or an Android/Bionic JIT result.
