# Checked relocation tables for ARM and 386

The ARM and 386 compilation preflights now validate tables used by `case`,
`casec`, `casel` and computed `goto` before either generation pass can mark
counts or index the PC patch table. The shared `jit-tables.h` checks:

- Module-data addressing, nonnegative/aligned offsets and the full header span.
- Nonnegative counts, bounded row/default extents, and instruction targets.
- Eight-byte alignment of long-case rows.
- Partial overlaps and aliases with conflicting table kinds or extents.

Exact same-kind aliases remain valid: Limbo's optimizer can duplicate a case
instruction. Records are sorted by offset so overlap checking is not quadratic.
Allocation failure declines compilation before any module data changes. These
existing backends require 32-bit Dis words; unsupported layouts also decline.
This preflight does not validate string pointers, interval ordering, dynamic
runtime tables, or every other kind of malformed bytecode.

An absent case default is represented by -1 in Limbo compiler output. The
preflight accepts that sentinel, and both backends now preserve it instead of
reading `patch[-1]`. Row targets and non-sentinel defaults must still name a
real instruction. The change is confined to JIT preflight/relocation; it does
not add a whole-file verifier to the loader or bounds checks to the interpreter.

Run from the repository root:

```sh
python3 libinterp/tests/jit-tables.py
python3 libinterp/tests/386-publish.py
python3 libinterp/tests/arm-publish.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/jit-tables.py
```

The direct fixture runs 98289 checks with exact-sized module-data heaps. It
covers all four table kinds, count/offset extrema, extent boundaries, invalid
row/default targets, absent defaults, allocation failure, overlapping spans,
conflicting aliases, valid duplicate references and unchanged input bytes.
It uses target-width words and a host adapter for Plan 9's qsort callback;
allocation and sorting are the only external boundaries.

The production compilation fixtures add 21 rejected table configurations per
backend and compare both data and Dis instructions before restoring the fixture.
They also compile exhaustive word/string/long cases with -1 defaults and
repeated references, verifying that the sentinel survives relocation.
Pre-fix backends fail these positive cases with an out-of-bounds patch read
under ASan. Use `X86_PUBLISH_SOURCE` / `ARM_PUBLISH_SOURCE` to select them.
The full native call ABI and execution of complete compiled modules remain
outside these fixtures. MIPS, Power and SPARC are unchanged.
