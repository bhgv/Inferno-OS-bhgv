# Canonical signed words and complete priority JIT caches

This series fixes the input path shared by native fast execution and C fallback,
and the shared AMD64/ARM64 compiler's cached-entry validation. It does not change
the Dis wire format or claim that the native VM frame/reference ABI is finished.

## Loaded integers

DEFW carries a signed 32-bit integer. Previously, assigning unsigned `disw()`
output directly to LP64 `WORD` turned `0xffffffff` into positive 4294967295.
The loader now extends the canonical sign mathematically into WORD32 or WORD64.
Unsigned metadata and signatures continue to use `disw()` unchanged.

The signed operand decoder now constructs the signed 14/30-bit value within
`int` range instead of converting an out-of-range unsigned value to `int`.
Public parsing also rejects nil/H/wrapping image extents, invalid metadata
storage and nil/H paths before allocation or typed reads. Encoded-span checks
use native unsigned address differences rather than unrelated pointer ordering.

## Complete JIT caches

A mixed program containing both a valid instruction and INOP, IECLR or an
out-of-range opcode previously passed compilation when another instruction
produced code. The cached path skipped opcode scanning altogether. Both paths
now validate every instruction before allocation or reuse, retaining bytewise
reads for supported unaligned instruction images.

Cached mappings must have native instruction alignment and whole-word lengths.
Fast entry addresses must belong to the mapping and name a supported fast
opcode. Complete tail entries must be aligned and fit their entire emitted
extent inside the mapping; reserved dispatch slots are rejected. Every opcode
used by the current program must have a complete dispatch entry. Rejection
preserves the existing owner and allocates nothing. These are geometry checks;
they do not prove that an arbitrary address range is mapped or authenticate the
machine-code contents.

## Verification

```sh
python3 libinterp/tests/dis-numeric-literals.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis-numeric-literals.py
python3 libinterp/tests/dis-loader.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
python3 libinterp/tests/vm-widths.py
```

The same fixture exhausts all 128 one-byte and 16384 two-byte encodings,
including nonminimal encodings, then checks 50000 deterministic four-byte
patterns. All 66512 cases per WORD model check signed values, exact cursor
advancement and unchanged encoded bytes.

The numeric integration fixture parses 4114 canonical pairs per WORD model
using the real parser, Type creation, initialized array data and error-frame
cleanup. Values cover sign boundaries and deterministic full 32-bit patterns.
It executes production MOVW/ADDW/SUBW/MULW/ANDW/ORW/XORW handlers and genuine RX
AMD64 leaves, comparing complete operand images against independent unsigned
modular oracles for three source/result alias arrangements. There are 172788
C/RX image comparisons per model (345576 total) and 174 storage rejection cases
per model on this host. All checks survive NDEBUG. Combined ASan/UBSan passes.
The numeric fixture retains native pointer width in both scalar WORD models;
this is not a complete WORD32/native-pointer ABI port or a full VM boot.

The ordinary parser fixture retains 120 truncation and 30720 mutation cases,
allocation failures, initialized maps, nested arrays and cleanup checks; its
combined ASan/UBSan run passes. The new numeric driver is in the core checkpoint.

Each priority backend passes 5001 compiler checks in each WORD model, including
355 added mixed-opcode/cache-geometry cases. All four compiler profiles also
pass combined ASan/UBSan. The AMD64 full dispatch fixture
also passes 177152 real C/RX routes and 262 VM-loop integration cases per model.
ARM64 compiler checks size and bind actual ARM64 code but do not execute it on
the x64 host. The numeric driver selects AMD64/ARM64 machine code from the
actual host architecture; only its AMD64 execution was verified here.

The prior source independently fails the literal sign case and storage span
case. Four compiler defect families (mixed uncached opcodes, mixed cached
opcodes, mapping alignment/length, and table entry geometry/completeness) fail
independently on both backends and both WORD models: 16 compiler parent probes.
Replay controls are `DIS_LOAD_SOURCE`, `DIS_NUMERIC_CASE=literals|storage`,
`BRIDGE_COMPILER_SOURCE`, `BRIDGE_BACKEND`, `BRIDGE_WORD_BITS=32|64`,
`BRIDGE_COMPILER_CASE=opcodes|geometry`, `BRIDGE_OPCODE_PHASE=uncached|cached`
and `BRIDGE_GEOMETRY_PHASE=alignment|entries`. Foreign compiler replay requires
`BRIDGE_PREFLIGHT_ONLY=1`.

Opcode availability is unchanged:

| Backend | Complete JIT + C entries | Guarded native fast paths | C-only commands |
| --- | ---: | ---: | ---: |
| AMD64 | 173/173 (100%) | 166/173 (95.95%) | 7 |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7 |

The per-command [readiness table](README.jit-readiness.md) describes the native
domains. Seven complex operations retain C: ALT, NBALT, SPAWN, MSPAWN, LOAD,
EXIT and RAISE. The remaining compiler/runtime layout blockers are documented in
[the native tool report](../../utils/tests/README.native-bootstrap.md). RV64,
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources remain outside this series.


## Complete fast-entry extents

Cached native fast entries must fit their entire emitted body, just like their
dispatch bridges. An aligned entry in the last four bytes of a mapping is not
sufficient for IADDW. Cache validation sizes the fast body with the same complete
LeafLayout as fresh compilation and rejects a truncated span without allocation
or publication. The native dispatch regression now includes that endpoint in
both WORD32 and WORD64 models; the parent accepted it.
