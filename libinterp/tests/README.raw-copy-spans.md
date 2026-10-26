# Raw byte-copy spans on AMD64, ARM64 and RV64

Latest AMD64/ARM64 qword kernels and page-edge access checkpoint: [README.fixed-slots-copy-chunks.md](README.fixed-slots-copy-chunks.md).

MOVM and HEADM now reject invalid nonempty source/destination byte ranges before
memmove or the first native store. Nil/H starts keep VM nil-reference errors;
negative counts and overflowing native end addresses raise bounds errors. HEADM
checks the list owner before reading its count, then validates the native payload
offset without forming l->data through an overflowing borrowed pointer.

Zero-length MOVM still ignores both byte pointers. Zero-length HEADM still
requires a nonnil/non-H list owner but never forms a payload address or touches
the destination. Count/source-owner operands are captured before copied bytes
can overwrite their slots. These are native geometry checks, not validation of
every arbitrary inaccessible address or complete list-allocation extents.

All three 64-bit backends extend raw-copy native loops from 128 to 256 bytes.
Larger valid records use C memmove. Both forward and backward loops run only
after payload-offset/start/end guards succeed. Nil/H or wrapped native pointers
decline without writes. HEADM offset validation remains target-specific: 0..4095
on AMD64/ARM64 and 0..2047 on RV64. Layout fields and VM ABI are unchanged.
Production staging bounds remain intact; see the guarded HEADM capacity checkpoint below. RV64 uses
base integer instructions, so I/Zmmul/M profiles share the same expanded paths.

Validation:

- raw-copy-jit.py executes 137088 independent image/write/span/FP cases per
  backend (411264 total). Both WORD widths, 0/1/128/129/255/256/257 boundaries,
  negative/MIN/MAX counts, null/H/wrapped starts, overflowing head offsets,
  all source byte alignments, unaligned/overwritten count slots and overlap
  directions are checked. Successful AMD64/ARM64 stores follow exact directional
  qword/byte-tail order; RV64 retains byte order. Every native decline is write-free.
- RISC-V FCSR is seeded/read through actual CSR instructions after enabling FS,
  with an independent seed readback. Unicorn's CSR register API is unsupported
  here. No emulated FP-state check is silently skipped.
- raw-copy-native.py checks 161280 actual C/RX cases per WORD model (322560
  total), including error identity/precedence, unchanged rejected images and
  FP rounding/sticky status. Both models pass ASan/UBSan. Parent replay with
  RAW_COPY_PROFILE=5/RAW_COPY_HEAD=0 reproduces a null memmove destination;
  profile 4/head 1 reproduces overflowing list-payload pointer formation.
- raw-copy-layouts.py passes 740340 sizing/capacity/canary probes per backend
  (2221020 total) under ASan/UBSan. AMD64/ARM64 short-capacity declines leave
  caller buffers untouched. The existing RV emitter permits a bounded partial
  prefix when capacity is insufficient; its canaries verify no writes beyond
  that capacity. Production publication still sizes before emission.
- Updated legacy copy/head selections pass 4852 and 3556 cross-backend checks;
  the guard selection passes 1171 ABI/instruction checks. All three compiler
  publication/fallback/metadata/failure/retry/unmap fixtures and RISC-V runtime
  tests pass. The core suite passes all 97 host drivers.

There are still 173 active Dis commands: AMD64/ARM64 141 guarded paths / 32
remaining; RV64 M 103/70, Zmmul 90/83, I 87/86. This broadens two existing
opcodes rather than adding IDs. Native paths remain partial; ARM64/RV64
instruction execution is emulated, with physical target acceptance separate.

## Guarded HEADM image capacity

The copy emulator fixture uses the existing 128-unit private emitter capacity.
The list-slot guards added in 85cd4e5 make the ARM64 HEADM image exceed the old
64-unit test buffer. A zero result at that insufficient capacity was expected
emitter behavior, not an unavailable instruction. This change adjusts only the
fixture; it does not increase production LeafMax or change the native ABI.

For each architecture, WORD model and MOVM/HEADM variant the fixture checks null
sizing, every insufficient capacity, exact-capacity image equality and a trailing
canary. AMD64/ARM64 short publication must remain completely write-free; RV64
retains its existing prefix-publication boundary check. Guest instruction cache
invalidation covers the complete 128-unit staging window. The unchanged memory,
overlap, access-span, write-order and FP oracles pass 411,264 cases under Python
-O across AMD64, ARM64 and RV64.
