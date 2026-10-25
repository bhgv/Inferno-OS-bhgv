# Native IGOTO and fixed-width numeric dispatch tables

## New native opcode on AMD64 and ARM64

IGOTO now has a helper-free native path on AMD64 and ARM64 for ordinary Dis-PC
modules. It captures the VM-width source index, reads the preceding signed
32-bit table count, validates the selected signed 32-bit destination against
the real module's instruction count, checks native address arithmetic and
commits only REG.PC. Table and source slots can have any byte alignment.
Malformed metadata/indices/targets reject without register/data writes; the C
handler then supplies the VM exception. The unused middle operand is not read.

The compiler adapter supplies the actual Modlink.compiled offset in its private
LeafLayout, rather than assuming a particular VM word or pointer layout.
Already relocated native-PC tables decline before reading their table/source.
They retain the historical C path, which cannot use a normal count guard because
legacy JIT relocation replaces the GOTO count with zero.

Both backends retain the fixed 64-unit buffer. ARM64 PC/table guards share a
single rejection return, while MOVPC retains its existing guard behavior.
Code uses caller-saved integer registers, no stack frame, helper calls, GC
writes or frame relocation. Original Dis instruction PCs and compiled==0 leaf
publication remain intact. Public REG/Module/Modlink layouts do not change.

The sizing emitters now report 119 native-path and 54 interpreter-only active
opcodes on both AMD64 and ARM64, in WORD32 and legacy WORD64 models. RV64+M
remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86. The current interpreter-only
name list in README.jit-coverage.md is regenerated from actual emitter output;
it no longer lists earlier supported floating opcodes or IGOTO.

## Three C dispatch corrections

GOTO, CASE and CASEL previously used WORD* table loads. Their compiler-emitted
fields remain four bytes even when WORD is eight; CASEL keeps eight-byte bounds
inside 24-byte rows, not six native WORDs. Typed loads also required alignment
that VM slots do not guarantee. The handlers now use byte offsets and memmove:

| Command | Header before rows | Row/entry stride | Target field |
| --- | --- | --- | --- |
| GOTO | count at entries-4 | 4 | signed 32-bit destination |
| CASE | 4 | 12 | row+8, signed 32-bit destination |
| CASEL | 8, including padding | 24 | row+16, signed 32-bit destination |

CASE/CASEL retain binary search and exclusive upper bounds, then use a shared
checked PC resolver. They validate module/target metadata and table span
arithmetic before forming a Dis instruction pointer. Empty tables take their
default; an absent/default-invalid destination reports exBounds instead of
forming a pointer before/outside the program. Negative/oversized case counts
also report exBounds. These numerical searches remain C implementations.

Already relocated legacy tables retain raw four-byte native targets, including
high address bits. This is the existing 32-bit native-PC representation, not
an assertion that the complete native-PC VM has a 64-bit table/GC/frame ABI.
CASEC and its pointer-containing table ABI are not changed in this series.

## Validation

- Actual production C/native AMD64 IGOTO: 88,733 cases per WORD model, 177,466
  total, ASan/UBSan. A separate packed-table oracle checks counts, negative/high
  indices, entry/destination width, all source/table byte alignments, aliased
  source/header/entry spans, invalid module metadata, program-address wrap,
  unchanged REG/data images and relocated-table C fallback.
- Actual CASE/CASEL with an independent linear range oracle: 614,428 cases per
  model, 1,228,856 total, ASan/UBSan. It covers both source widths, integer/big
  bounds and gaps, empty/default/malformed tables, all byte alignments, invalid
  targets/program metadata and raw relocated native-PC values.
- Unicorn/Capstone: 73,756 IGOTO instruction/ABI/oracle checks and 35,340 MOVPC
  regression checks across both WORD models, AMD64/ARM64 and RV64 rejection.
  Malformed private-layout guards and private emission capacity are checked.
- The compiler/data-emission pipeline runs actual Limbo case generation,
  binary/text table emission and actual interpreter dispatch: 1,184 scenarios
  pass, including interpreted and relocated paths. Its VM adapter now carries
  actual module metadata required by the checked dispatch contract.
- All 64 core drivers pass. Complete native AMD64 runtime matrices pass
  ASan/UBSan in both WORD models. Runtime publication now includes 67 explicit
  instructions, including IGOTO; allocation/RX publication failure, retry and
  freeing are checked on AMD64, ARM64 and RV64 adapters.

Replaying the previous C handlers reproduces a wrong PC for a valid WORD64
GOTO table, UBSan unaligned reads in GOTO/CASE, and a CASEL crash from the wrong
header/stride interpretation. GOTO_SOURCE selects the earlier source;
GOTO_VALID_ONLY isolates valid rows, and GOTO_CASE_ONLY with GOTO_CASE_FAMILY=0
or 1 selects CASE or CASEL. Replays are deliberately failing regression checks.
No sanitizer instrumentation is disabled beyond the established hook for
unavailable leak-process inspection.

No Tk, security/crypto or MIPS/SPARC/PowerPC JIT source is modified. Hardware
ARM64 validation, native basic blocks/helper transitions and a coordinated
native 64-bit compiler/loader/frame/GC ABI remain separate acceptance work.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/goto-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=goto python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=movpc python3 libinterp/tests/leaf64-jit.py
python3 limbo/tests/case-max.py
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```
