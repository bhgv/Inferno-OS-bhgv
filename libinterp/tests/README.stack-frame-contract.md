# Frame map, stack extent and captured allocation contracts

The shared C stack path now enforces the same complete native-pointer map shape
as FRAME/MFRAME. Maps cannot mark the four control-link fields in `Frame` as
heap references: initialization used to overwrite those fields, and cleanup
could treat code/stack/Type pointers as heap owners. Maps must fit their byte
extent and every marked native pointer must fit fully inside the frame.
`framefits` rejects control-link bits before any initialization or publication.
Native FRAME/MFRAME retain their pointer-free fast paths; mapped initialization
and growth stay in C through the existing complete JIT entries.

Stack operations also validate:

- Type prefix alignment, field/map spans and map length before typed reads;
- native Modlink/Module prefixes before reading stack-size hints;
- Prog output storage and decoded frame-pointer slots before access;
- complete source/current Frame headers and frame payload spans;
- backward root-extent arithmetic, native root alignment and the root marker
  before reading saved cursors or releasing an extent.

Ordinary byte-aligned frames and pointer slots remain supported through byte
copies. Root extents must retain the alignment provided by their native
allocator. Invalid metadata leaves registers, inputs and allocation ownership
unchanged. These are numeric span and descriptor checks; they cannot establish
that an arbitrary fabricated pointer belongs to a mapped VM allocation.

`newstack` captures the frame size before allocation, copies and clears the
private header, then publishes the child registers. `extend` captures the size
and parent SP/TS/EX before allocation. An allocation observer changing those
scalar values cannot redirect a saved parent cursor or enlarge the copy after
allocation was bounded. This does not support concurrent mutation of pointer
maps or establish a general mutable-Type contract.

`unframe` validates both current and source descriptors before publishing SP or
freeing the source extent. `unextend` validates root identity and descriptor
shape before restoring cursors or releasing mapped owners. The checks preserve
existing valid stack and argument-transfer ownership semantics.

## Verification

Run from the repository root:

```sh
python3 libinterp/tests/stack-spans.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/stack-lifecycle.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/frame-allocation-native.py
python3 libinterp/tests/stack-startup.py
python3 libinterp/tests/stack-unwind.py
python3 libinterp/tests/frame-allocation-jit.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The guest frame test requires the existing Unicorn/Capstone test environment.
The C stack/frame and actual AMD64 RX checks pass ASan/UBSan with NDEBUG;
no assertions or disabled address/undefined instrumentation are used by the new
oracles. Inherited lifecycle checks are now explicit predicates as well. Their
small frame-construction image uses stable storage across error unwinds. The
valid temporary-extent fixture supplies its real Type owner, as runtime extents
do; it no longer relies on an uninitialized descriptor that old unframe ignored.

| Matrix | Per WORD model | Both WORD models |
| --- | ---: | ---: |
| Invalid frame maps across four stack operations | 4032 | 8064 |
| Pointer geometry and invalid native owners | 856 | 1712 |
| Captured sizes and saved parent cursors | 512 | 1024 |
| FRAME/MFRAME C and complete-JIT header-map rejection | 8192 | 16384 |
| Existing lifecycle copy/pop/extent/error checks | 3045 | 6090 |
| Stack unwind, packed maps and destructor callbacks | 98304 | 196608 |

The actual FRAME/MFRAME C/RX suite additionally retains 54336 successful image
routes per model and its earlier error/extent cases. The guest native-code
matrix passes 16320 cases for AMD64 and 16320 for ARM64. Full startup/spawn/frame
checks pass for both emulator/kernel implementations and both scalar WORD
models. Complete dispatcher and priority compiler checkpoints remain passing.

The old source independently fails map, geometry and captured-allocation
families in both WORD models, plus the FRAME/MFRAME header-map family: seven
parent probes. Replay uses `LIFECYCLE_SOURCE`, `LIFECYCLE_WORD_BITS=32|64`,
`STACK_SPAN_CASES=maps|geometry|capture`, `FRAME_ALLOCATION_SOURCE` and
`FRAME_ALLOCATION_SECTION=headers`.

## Packed diagnostics and captured teardown roots

`memchk` and `destroystack` now copy `Type.np` through bytes, matching the
existing byte-safe pointer-map helpers. Diagnostic bitmap access uses byte
addresses as well. This removes typed accesses through packed read-only Type
prefixes without changing map order, recursion limits, frame links or owner
release counts. It does not establish a general mutable-Type/callback contract.

`destroystack` captures each extension's parent and root Type before running
mapped-owner destructors. A destructor changing saved EX or TR can no longer
redirect or truncate the remaining cleanup, or suppress the root frame's mapped
owners. Saved frame links and module owners remain captured before cleanup.

The unwind fixture varies eight Type byte shifts in addition to both ordinary
frame shifts, all module-switch masks and extension-root forms. Three destructor
profiles leave metadata alone, overwrite the current extension's parent, or
replace its saved root descriptor. Expected owner releases and extension order
are independent of those overwritten fields. Assertions were replaced by
explicit predicates, so NDEBUG preserves the full oracle.

The latest matrix passed 98,304 cases per WORD model (196,608 together) under
ASan/UBSan, NDEBUG and strict compiler warnings. Header-only unknown pragmas and
an inherited unused parameter require `-Wno-unknown-pragmas` and
`-Wno-unused-parameter` with `-Wall -Wextra -Werror`. Eight independent preceding
source probes fail as expected: packed diagnostic maps, packed teardown maps,
redirected parents and replaced root Types, each in WORD32/WORD64. The first two
reach UBSan alignment diagnostics; the latter two fail active ownership checks.
Use `UNWIND_WORD_BITS`, `UNWIND_TYPE_SHIFT`, `UNWIND_CALLBACK_MODE` and the
existing `UNWIND_XEC_SOURCE`/`UNWIND_CHECK_SOURCE`/`UNWIND_VALID_SOURCE` controls
for isolated replay. This matrix runs native C cleanup on AMD64; it does not
constitute a physical ARM64 acceptance run.

## Priority JIT status

| Backend | Complete JIT + C entries | Guarded native fast paths | C-only |
| --- | ---: | ---: | ---: |
| AMD64 | 173/173 (100%) | 166/173 (95.95%) | 7 |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7 |

[Per-command domains and percentages](README.jit-readiness.md) are unchanged.
The native frame/reference ABI work described in
[the native tool report](../../utils/tests/README.native-bootstrap.md) is still
required before a full 64-bit VM boot. ARM64 guest execution is not a physical
ARM64 acceptance run. RV64 is deferred; Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are unchanged.
