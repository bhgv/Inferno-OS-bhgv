# Proposed first fixed native-call fixture

FLASHVM-0044 proposes `$Math.bits64real`: Big bits `3ff0000000000000` produce
Real binary64 `1.0` with the same bits. INFERNO-0047 confirms this exact codec and
fixture for private trusted-C review; file CALL/MCALL admission stays closed.

| File | Existing agreed codec |
| --- | --- |
| signature.hex | v1, result Real=3, one Big=2 parameter: four BE u32 words |
| strings.hex | Rune pool: ordinal0 `$Math`, ordinal1 `bits64real` |
| imports.hex | One import: module0, symbol1, signature length16 and exact signature |

These are independent literal golden bytes, not a new section or value encoding.
The owning test also checks a content-equal ordinal import alias, valid-but-wrong
result/parameter kinds with unchanged runtime consistency hash, every byte
mutation/truncation, registered metadata mismatch and allocation refusal.

The matched generated native frame has size72, result address at32, Big at64,
Frame.t at24 and a complete two-byte zero map. This experiment's frame is owned
non-GC memory and contains no managed arguments/results. The module and Type each
have one additional ordinary reference throughout allocation/call/resolution;
there is no arbitrary reference-result transfer or generic caller-root proof.
The resolver must supply the current live eight-byte scalar cell and preserve its
old contents on cancellation/error. Actual VM mapped-cell provenance, managed
arguments and file-call safepoints require a later shared acceptance gate.

Run `utils/tests/native64-fixed-math.py --limbo ... --libdir ...` with the same
cross-compiler/runner environment as the native64 Math registration gate.
`--inject-callee` adds a post-production VM error to a copied owning Math unit.
`--control signature|module|type|frame` must fail an active check when that
contract is removed. Production source is unchanged by these test variants.
