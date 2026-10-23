# Dense int-case expansion at signed endpoints

Run from the repository root:

```sh
python3 limbo/tests/case-ranges.py
```

The fixture compiles production dogoto and fillrange with actual limbo.h AST,
label, instruction and case layouts. Constant-node construction and compiler
allocation are bounded adapters. Its 7,781 scenarios use an independent linear
range oracle to check optimizer selection, expanded values, preserved source
locations, exact allocation sizes, assigned targets and default gaps.

Dense int ranges use Long for span/endpoints and inclusive loop counters.
Density arithmetic widens n before multiplying by three, and expansion checks
its label allocation fits the compiler's signed allocation bound before
narrowing the resulting count. Values around INT_MIN and INT_MAX can therefore
be expanded and assigned without wrapping the initial predecessor, the final
loop increment or a 32-bit fill counter. Small/sparse cases retain their prior
optimization decisions and data when no goto table is selected.

The matrix combines nine bases, counts 1..16, widths 1..6 and gaps 0..8. Separate
cases exercise a three-value range beginning at INT_MIN, one ending at INT_MAX,
a maximum singleton target fill, empty labels and sparse full-width endpoints.
After expansion, every value around the table span is checked against its
original range's target or the default. Both allocation-free dense tables and
expanded tables are covered. The two expansion endpoints and maximum singleton
fill independently trigger UBSan signed overflow against the old helpers.

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 limbo/tests/case-ranges.py
```

`CASE_RANGE_SOURCE` selects an alternate com.c. `CASE_RANGE_CASE` selects
`minimum`, `maximum`, `fill`, `sparse`, or `grid`. Default compilation uses strict
UBSan; the supplementary ASan/UBSan run also passes. Large label allocations
are bounded rather than materialized by the fixture.

These checks concern dense int-to-IGOTO optimization only. They do not validate
full case instruction generation,
front-end parsing/type checking, compiled program execution or new 64-bit JIT
backends. Valid typed int endpoints are an input precondition of these internal
helpers. No VM opcode, table layout or runtime ABI is changed.
Sparse maximum-bound generation/serialization now has separate coverage in
[README.case-max.md](README.case-max.md).
