# Fixed operand/frame preflight and qword raw-copy kernels

Based on experiment `02b7b7f96183d30f6c75b7415197ecc986e5d411`.

## Nine fixed commands

MULX, DIVX, CVTXX, MULX0, DIVX0, CVTXX0, MULX1, DIVX1 and CVTXX1 now validate
used decoded WORD slots in C and AMD64/ARM64. Each complete span rejects null,
H and native address wrap. Result checks cover zero shortcuts as well as ordinary
publication; captures remain byte-safe at every alignment and overlap placement.
The existing argument widths, frame layout, modular intermediates and signed
rounding/division rules are unchanged.

The shared fixedword reader bounds its offset and complete frame field before
forming an address. Nil/H frame identities keep exNilref; negative offsets and
wrapping field spans raise exBounds. Native frame/coefficient leaves likewise
bound the required frame prefix before additions or field loads. CVTXX uses no
frame, and CVTXX0/1 bound only their used coefficient field, not an unused Dtmp.
AMD64 span scratch preserves the captured multiplicand/divisor before later
checks; ARM64 span scratch remains separate from arithmetic registers.

Error/short-circuit order is explicit:

* MULX validates its frame scale before reading decoded scalar values.
* DIVX reads its frame and divisor first. A zero divisor wins before the dividend
  or result slots; nonzero division validates its scale and remaining slots.
* CVTXX bounds/reads the count first and rejects invalid scales before source
  value access. Minimum-width counts are compared without signed negation.
* Coefficient commands capture both inputs before frame parameters. Required zero
  coefficient/divisor errors retain their priority before destination checks. Existing
  zero-numerator shortcuts still ignore otherwise invalid scale values; rejected
  native scale paths leave C authoritative for those shortcuts.

Malformed native paths decline without writes or FP effects. Bounds arithmetic
cannot prove arbitrary mapping/allocation ownership. No frame/GC/helper ABI or
private emitter capacity is changed; RV64 fixed emitters are untouched.

## MOVM and HEADM native development

AMD64/ARM64 now copy complete eight-byte blocks and then the exact 0..7 byte
remainder. Destination-after-source copies descend from the ends; other copies
ascend. Each complete block is loaded before its overlapping store, preserving
memmove behavior even at one-byte displacement. No word is read until at least
eight bytes remain, and all nonempty span guards precede the first payload read.

The existing 0..256 byte native domain and larger-record C fallback stay intact.
Empty copies do not form/access payloads or touch destinations. Owner/count cells
are captured before copied bytes can overwrite them. RV64 keeps its byte loop.
AMD64's empty-copy skip now uses a patched rel32 branch because the two block/tail
kernels exceed the old short-jump reach. This is not a claim of atomic copy
publication or a hardware throughput benchmark.

## Independent validation

```
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 -O libinterp/tests/fixed-slot-native.py
python3 -O libinterp/tests/fixed-slot-jit.py
python3 -O libinterp/tests/raw-copy-edges-jit.py
python3 -O libinterp/tests/raw-copy-jit.py
python3 libinterp/tests/raw-copy-native.py
python3 libinterp/tests/raw-copy-layouts.py
python3 libinterp/tests/fixed-ops.py
python3 libinterp/tests/fixed-frame.py
python3 libinterp/tests/fixed-coeff.py
COEFFICIENT_NATIVE=1 python3 libinterp/tests/fixed-coeff.py
LEAF64_CASE=roundedcoefficient python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/jit-coverage.py
```

New registered fixtures use explicit checks under Python -O/NDEBUG:

* fixed-slot-native.py: 24,328 WORD32 and 26,568 WORD64 C/RX AMD64 cases under
  ASan/UBSan. Independent small-integer results, source/result/frame aliases,
  every byte alignment, malformed slots/frames, zero-divisor priority, invalid
  scales with poisoned inputs and zero-shortcut fallback images are checked.
* fixed-slot-jit.py: 15,726 cases per AMD64/ARM64 across both WORD widths. Whole
  images, REG/PC/FP preservation, poisoned sibling slots, ignored CVTXX frames,
  exact capacity/canaries and every insufficient code capacity are checked.
* raw-copy-edges-jit.py: 8,224 cases per architecture, covering every length
  0..256, both directions, start/end placement at isolated pages, MOVM/HEADM and
  both WORD widths. Exact read/store widths/order and whole-page canaries are
  independent oracles; unmapped neighbors catch payload overreads/overwrites.
* Existing raw-copy-jit.py retains 137,088 cases per architecture (411,264 total)
  and now requires the exact qword/byte store sequence on AMD64/ARM64. RV64's
  byte-store oracle remains unchanged. C/RX raw-copy checks pass ASan/UBSan in
  both WORD models; all raw-copy layout/publication/canary drivers pass.

The nineteen adjacent fixed/copy/compiler/VM drivers pass, including native
module publication in WORD32/WORD64, independent arbitrary-precision fixed
oracles and Limbo selection/folding/zero-division checks. Minimal extraction
fixtures declare the new bounds identity and native address type. This is a
focused checkpoint, not a full rerun of the combined 204-driver selection. An
additional COEFFICIENT_NATIVE=1 run passes ASan/UBSan in both WORD models,
including 4,390,928 native/C coefficient cases and 518,400 rounding cases per model.

FIXED_SLOT_SOURCE, FIXED_SLOT_EMITTER_DIR, FIXED_SLOT_OP and FIXED_SLOT_ARCHES
select explicit baselines. All 27 old command/backend probes reproduce invalid
memory reads. RAW_COPY_EDGE_EMITTER_DIR / RAW_COPY_EDGE_ARCHES select the old
byte-only emitters, which fail the new qword-access oracle on both architectures.
Only unsupported LeakSanitizer process enumeration is disabled; stack/address/
undefined instrumentation remains active, with the sandbox's known longjmp and
executable-name warnings. Physical ARM64/RV64 and full Inferno/compiler boots
remain separate checks.

## Dis availability

There are 175 enum slots, excluding invalid INOP and unused IECLR from the 173
active commands. Correcting nine existing paths and broadening two copy kernels
does not change opcode availability:

| Backend | Native path | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 164 | 9 |
| ARM64 | 164 | 9 |
| RV64 + M | 103 | 70 |
| RV64 + Zmmul | 90 | 83 |
| RV64I | 87 | 86 |

A native path may remain guarded/partial; this is not complete block-JIT coverage.
Tk, security/crypto and MIPS/SPARC/PowerPC JIT production sources are unchanged.
