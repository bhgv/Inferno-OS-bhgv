# Maximum numeric case bounds without changing Dis tables

Run from the repository root:

```sh
python3 limbo/tests/case-max.py
```

Sparse ICASE/ICASEL tables use exclusive upper bounds. INT_MAX+1 cannot fit a
32-bit int table, and BIG_MAX+1 overflows the compiler's signed Long. Emitting
those values previously lost maximum matches and could lose the rest of a
maximum-ending range as well. Binary and textual initializers both had the
problem.

For a sparse numeric case ending at its type's maximum, casecom now snapshots
the case expression into a typed temporary and emits an existing IBEQW/IBEQL
branch to that label's body. The ordinary table covers the remaining values.
The shared caseend helper caps only the unrepresentable exclusive endpoint at
the maximum; disdatum and asminitializer use the same helper. The new snapshot
retains source locations and prevents an added second evaluation/read. Dense
int-to-IGOTO tables continue through their existing path.

The output uses existing moves, branches and table layouts. Existing native
backends need no new instruction or endpoint convention. **Already compiled
Dis files containing the old bad tables must be recompiled**; this change does
not reinterpret or repair their legacy tables in the loader/runtime.

The driver checks 1,184 value/target-layout combinations, plus mutation and
source-position assertions. It compiles:

* Production casecom, dense-table helpers, patch and findlab against actual
  compiler AST/label/declaration/instruction layouts.
* Production disvar/disdatum and all required data writers, canonical real
  conversion, plus actual textual asminitializer.
* Production VM move, comparison, subtraction, jump and ICASE/ICASEL/IGOTO
  bodies. An independent wire decoder materializes fixed-width table records.

Typed declarations, temporary/address lowering, simplification, source-body
compilation, symbol storage and sizing have bounded adapters. Body markers
continue through production common-exit jumps, checking against fallthrough.
A linear inclusive-range oracle checks sparse separated ranges, maximum
singletons, full-width ranges, default bodies and absent default bodies for
int/big, plus dense int controls. Both addressable and computed-input adapters
are tested. Source reads are counted; changing a source after its first read
must not change the selected body.

Execution covers interpreter instruction-index targets and relocated absolute
PC table targets, including the runtime fallback shape used by compiled big
cases. It does **not** execute generated native machine code. The VM slice uses
real 32-bit words and low-address mmap storage on a Linux LP64 host; the mapping
is a hint, never an overwrite of an unowned range. Narrow-pointer warnings are
suppressed only in this explicitly 32-bit host fixture. This does not establish
a native 64-bit VM ABI.

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 limbo/tests/case-max.py
```

`CASE_MAX_COM`, `CASE_MAX_DIS` and `CASE_MAX_ASM` select alternate production
sources independently. `CASE_MAX_CASE=word` or `big` limits the value model.
Old case generation and old word-table emission independently select incorrect
bodies; old big-table emission triggers signed-overflow UBSan. Old textual
word endpoints disagree with binary bounds and old big endpoints overflow.

The fixture is not a full parser/type-checker invocation, full temporary or
address allocator, emitted branch-byte encoder, Inferno boot or native JIT
execution. Complete changed C units also pass host-header syntax checks. The
Dis format, opcodes, instruction table layouts and runtime ABI are unchanged.
