# Array operand guards and shared native copy kernels

This checkpoint hardens INDB, INDW, INDL, INDF, INDX and LENA in the C
interpreter and both AMD64/ARM64 emitters. It also shares the overlap-safe
qword/byte-tail generator between MOVM, HEADM, MOVMP and HEADMP.

## Array contracts

All used decoded operand spans are checked before reading operand bytes or
publishing results. Index commands check the owner-pointer, index and result
slots. LENA checks only its owner and result slots; R.m remains unused.

Indexing rejects negative indices before reading the array header. Native code
still rejects nil/H owners before loading the index, retaining its earlier
nonaccess behavior. Length errors read no descriptor or backing pointer. Only
INDX reads Type.size; its complete field prefix is checked before access.
Header and descriptor fields in C are captured with memmove, including synthetic
unaligned inputs. These checks reject null, H and wrapping spans; they do not
attempt to establish whether an arbitrary nonwrapping address is mapped.

Positive strides retain the signed-32-bit byte-offset limit and pointer-wrap
checks. Zero-stride generic records preserve arbitrary backing identities,
including nil/H, without narrowing a WORD64 index. LENA preserves nil/H owners
as zero and reads only the length field for other owners, including stored
negative lengths. Sources are captured before aliased result publication.

## Copy contracts

The compile-time x64copykernel/a64copykernel generators emit full eight-byte
loads/stores followed by exact byte tails. Both directions preserve overlapping
memmove results, unaligned pointers and zero-count nonaccess. They make no
runtime calls and preserve stack, FP state and the register holding REG.

MOVMP/HEADMP now use these kernels for the existing pointer-free 0..256-byte
domain. Live descriptor lookup, map rejection, list reference checks and HEADMP
R.s publication order are preserved. Larger or pointer-containing records still
use C. The production LeafMax stays 128 u32 units. Mapped test fixtures now use
that existing capacity instead of their earlier 64-unit limit; every short
capacity must decline without output writes. AMD64 uses a rel32 empty-copy skip.
Extracting the previous raw-copy generators leaves all eight MOVM/HEADM emitted
images unchanged (two WORD models, two commands, two architectures).

## Validation

New drivers use explicit checks, active with Python -O and C -DNDEBUG:

| Driver | Cases | Execution |
| --- | ---: | --- |
| array-slot-native.py | 1,094,912 | Extracted production C and actual AMD64 RX, WORD32/64, ASan/UBSan |
| array-slot-jit.py | 43,812 | AMD64/ARM64 Unicorn, WORD32/64 |
| mapped-copy-edges-jit.py | 16,576 | AMD64/ARM64 Unicorn, WORD32/64 |
| New total | 1,155,300 | Complete images, aliases, failures and boundary accesses |

Array drivers cover malformed decoded spans, header/descriptor wrap, unaligned
fields, poisoned unused operands, negative indices with inaccessible headers,
zero strides, result aliases and full register/FP preservation. Top-address
Unicorn cases check exactly which metadata prefixes may be read. The x86 model
maps the masked physical page used by Unicorn while retaining full-width
virtual pointer arithmetic.

Mapped page-edge tests record exact payload read/write widths and order,
HEADMP reference reads and R.s publication, whole-page canaries, both directions,
and sizes -1 through 257. Existing mapped-copy-jit.py also verifies exact qword
write sequences across 119,240 cases. mapped-copy-layouts.py passes 421,056
capacity/layout cases under ASan/UBSan with -DNDEBUG.

Against parent 80f859e, 34 isolated probes reproduce 18 decoded-slot memory
failures, six C unaligned-header diagnostics and ten native negative-index
header faults. The old C negative-index short circuit was already correct.
Nineteen adjacent driver configurations pass: array-indices, descriptor-index,
mapped-copy-native, raw-copy-native/layouts/jit/edges-jit, jit-coverage,
leaf64-runtime in both WORD models, leaf64-jit array/descriptor/copy/headcopy,
array-view-native, array-copy-source, array-create, Limbo layout-bounds and
Limbo descriptor-maps. This is a focused checkpoint, not a full suite/VM boot.

## Opcode availability

Counts remain unchanged. Native availability can be guarded or partial; it does
not mean every input executes natively. INOP and IECLR are excluded from the
175 enum slots, leaving 173 active commands in either WORD model.

| Backend/profile | Native | Interpreter |
| --- | ---: | ---: |
| AMD64 | 164 | 9 |
| ARM64 | 164 | 9 |
| RV64 + M | 103 | 70 |
| RV64 + Zmmul | 90 | 83 |
| RV64I | 87 | 86 |

Physical ARM64/RV64 execution and full Inferno/compiler boot are outside this
checkpoint. Tk, security/crypto and MIPS/SPARC/PowerPC JITs are untouched.
