# AMD64/ARM64 byte-string branches and process diagnostics

The later [Unicode/heap-query checkpoint](README.unicode-heap-queries.md)
removes this series' byte-only restriction. The text below records the earlier
checkpoint and its diagnostics changes.

This series prioritizes AMD64 and ARM64. Both backends now emit native paths
for BEQC, BNEC, BLTC, BLEC, BGTC and BGEC on byte strings and language nil.
They compare unsigned bytes lexicographically, respect explicit lengths and
embedded NULs, and use remaining lengths to order equal prefixes. Identical
owners take the equality path without reading their payloads. Rune strings,
C-null owners and invalid lengths defer to the existing C comparator/error
path before changing PC or user storage. A false predicate never loads the
branch-target slot. Native pointer widths remain independent of WORD widths.

The loops use caller-saved registers, no native stack, no helper calls, no
allocation and no FP instructions. Source headers and owner/reference state
stay unchanged. Emission fits the existing private buffers, including ARM64's
64-instruction limit, and retains ENDBR64/BTI entry instructions. This is
partial opcode coverage: full native Unicode comparison is not implemented.

```
LEAF64_CASE=bytebranch python3 libinterp/tests/leaf64-jit.py
STRING_BRANCH_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-branches.py
LEAF_RUNTIME_BACKEND=arm64 LEAF_WORD_BITS=32 python3 libinterp/tests/leaf64-runtime.py
LEAF_RUNTIME_BACKEND=arm64 python3 libinterp/tests/leaf64-runtime.py
```

The Unicorn matrix has 19474 checks, including sizing/capacity preflight,
complete disassembly, narrow/wide/nil/null inputs, INT_MIN lengths, arbitrary
byte contents, prefixes, embedded NULs, high native pointers, byte-aligned
slots, unchanged memory and unmapped targets for false conditions. Identical
owners with INT_MAX lengths verify that equality does not walk the payload.
RV64's string branches remain unsupported and are checked as such.

The actual native AMD64 fixture separately compares leaf behavior and the
production C handlers with an independent character-order oracle. Both WORD
models cover all 256-by-256 byte pairs for every branch, all source/middle/
target byte alignments, Rune fallback and malformed-owner VM errors. There
are 831492 native/C/oracle checks per model (1662984 total) with ASan/UBSan.
ARM64 is executed in Unicorn and checked for production module publication;
physical ARM64 system acceptance remains separate. No benchmark speedup is
claimed.

## Process diagnostic C helpers

Both emu and os implementations now snapshot Frame metadata through byte
copies in progsize, progstack, calldepth and builtin modstatus. Stack printing
stops at the root before reconstructing a nonexistent caller PC. Empty or
negative output counts return before reading registers or output storage.

Normal modstatus names are bounded formatted strings instead of copies of
exactly the output capacity from the source name allocation. Builtin caller
names retain the existing formatting and PC conventions with unaligned frame
inputs. Returned compiled PCs still use the current WORD representation.

progsize accumulates native allocator sizes in uvlong, converts to kilobytes
once, and saturates the existing int result at INT_MAX. It retains allocation
counting rules while avoiding truncation through an int accumulator. The
public diagnostic interface and VM pointer ABI are unchanged.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/prog-diagnostics.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

The new driver extracts actual functions from both sources and links the real
lib9 formatter. Real Frame/Module/Modlink/REG declarations are used in both
WORD models. Its 10240 scenarios cover all ordinary frame alignments, builtin
and normal names, every short output capacity, unchanged buffer guards, zero
capacity with unreadable output, three-frame walks with module switches,
extension roots, large native allocation totals and int-result saturation.
Allocator size queries and native-PC lookup remain controlled boundaries.

`PROG_DIAGNOSTIC_SOURCE` selects another devprog.c revision. With the parent
source, `PROG_DIAGNOSTIC_CASE=statuswide` reproduces an ASan heap-buffer-overflow
while reading beyond an exactly allocated short name. `PROG_DIAGNOSTIC_CASE=size`
fails the independent size oracle after truncating a large sum. `status` and
`walk` select the normal regression families. This is host regression coverage,
not a complete kernel boot or live process-debugger acceptance test.

## Opcode availability

Out of 173 active Dis commands, AMD64 and ARM64 each now have 109 native paths
and 64 interpreter-only paths. RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I
87/86. Invalid INOP and unused IECLR slots are excluded. Guarded paths can still
return to C, and remaining complex operations can keep optimized C helpers
when integrated into native blocks. Full block JIT execution and a coordinated
64-bit compiler/loader/frame/GC ABI remain separate work.
