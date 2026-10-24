# Rounded fixed coefficients and native return/mover slots

The AMD64, arm64 and RV64 backends now publish MULX1, DIVX1 and CVTXX1 leaf
entries for the mode with both rounding flags clear. They capture the encoded
scale, reject nonzero low flag bits before publication, perform an arithmetic
right shift by two, then reuse the guarded coefficient path. Nonzero rounding
flags, exceptional divisors and out-of-range scales decline to C with unchanged
result/frame storage. C retains its complete rounding and zero shortcuts. RV64
requires division (and multiplication for MULX1/DIVX1), so Zmmul-only builds do
not publish these entries. No public frame layout or Dis wire format changes.

The shared runtime repairs are independent of the new native paths:

* `movtmp` captures its native Type pointer without alignment assumptions and
  rejects nil/H or negative-sized descriptors before touching payloads or refs.
  Zero-sized mapped records still accept empty backing identities.
* `retstr` and `retnstr` capture/publish potentially unaligned String owner slots
  with `memmove`, preparing the new string before replacing/releasing its owner.
* `retstr` validates the complete native `strlen` result before narrowing to the
  integer constructor length. Strings above INT_MAX fail with `exHeap`; a large
  native length cannot wrap into a short successful conversion.

## Checks

```
PYTHONPATH=../jit-test-deps LEAF64_CASE=roundedcoefficient python libinterp/tests/leaf64-jit.py
COEFFICIENT_NATIVE=1 python libinterp/tests/fixed-coeff.py
MAPPED_MOVE_CASE=overlap python libinterp/tests/mapped-moves.py
MAPPED_MOVE_CASE=metadata python libinterp/tests/mapped-moves.py
STRING_SLOT_CASE=vm python libinterp/tests/string-slots.py
STRING_SLOT_CASE=returns-valid python libinterp/tests/string-slots.py
STRING_SLOT_CASE=returns-length python libinterp/tests/string-slots.py
python libinterp/tests/string-slots.py
python libinterp/tests/slice-tokenize-suite.py --core
```

The six-operation C/native coefficient matrix contains 4,390,928 checks per
word model (8,781,856 total) and passes ASan/UBSan. It covers complete WORD32 and
WORD64 arithmetic, independently aligned operands/frames and overlapping result,
coefficient and scale slots, plus C fallback. Three-backend rounded-coefficient
encoding and Unicorn execution passes 111,142 checks; all four rounding modes
are presented to the leaf, with nonzero modes required to reject without writing
even an inaccessible result. Capstone decodes every emitted instruction.

Mapped overlap checks cover 5,168 cases per model, including every descriptor
slot alignment, plus 32 metadata rejection/empty cases per model; these pass
ASan/UBSan. Existing typed FIFO and rendezvous tests remain in the complete core
run. The Sys dirread and FD-owner drivers also exercise the changed return helper.

The complete string slot matrix contains 79,391 checks per model and passes
UBSan. Its isolated VM/split group (`STRING_SLOT_CASE=vm`, 79,271 per model)
passes ASan/UBSan. The return-only group contains 120 checks per model; its length group
uses a synthetic strlen result to test INT_MAX and 64-bit narrowing without a
multi-gigabyte allocation. Native return publication/alias/empty paths pass
ASan/UBSan in fresh `returns-valid` processes (64 checks per model). Error-bearing
return runs report stack redzones after longjmp/__asan_handle_no_return in this
sandbox and are not counted as passing ASan. Stack instrumentation remains on.
The existing leak hook only avoids unsupported process inspection.

`MAPPED_MOVE_SOURCE` and `RUNT_RET_SOURCE` parent overrides reproduce unaligned
metadata/return accesses; `STRING_SLOT_CASE=returns-length` isolates the old
silent narrowing without an earlier alignment failure.

Native AMD64 compiler/runtime checks pass ASan/UBSan in both word models. ARM64
and RV64 publication/teardown tests run on the host and execution is emulated;
no hardware boot or complete native-rounding JIT is claimed.
