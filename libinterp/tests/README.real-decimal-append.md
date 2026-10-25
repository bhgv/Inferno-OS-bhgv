# ARM64/AMD64 real decimals, append stores and string comparisons

## Native instruction counts

Counts come from `python3 libinterp/tests/jit-coverage.py`. The denominator is
173 active Dis instructions; INOP and the unused IECLR enum slot are excluded.
WORD32/native-pointer64 and legacy WORD64 currently have the same counts.

| Architecture/profile | With a guarded native path | Interpreter-only remaining |
| --- | ---: | ---: |
| x64 / AMD64 | 132 | 41 |
| ARM64 / AArch64 | 132 | 41 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 base integer | 87 | 86 |

Having a native path does not mean every operand is handled natively. Rejected
operands continue through the existing C interpreter. RISC-V emitter sources
are unchanged in this checkpoint.

## Real decimal conversion

CVTCF now uses the bounded decimal parser on AMD64 and ARM64. It accepts ASCII
integers with an optional sign and up to nine digits, plus empty strings and
language nil. The integer magnitude is exactly representable in binary64 in
every rounding mode. The captured minus sign is retained for negative zero.
The conversion preserves FP controls and exception flags, including when all
exception flags start clear.

Sign-only strings, whitespace, decimal points, exponents, embedded NULs, longer
numbers, Rune strings and malformed storage retain C fallback. In particular,
sign-only real input falls back before any FP instruction. All bytes are parsed
before publishing an eight-byte result, including owner-slot and interior
output aliases. This adds one instruction to each backend's availability count.

## Append without allocation

INSC now handles appending at the logical end of a uniquely owned string when
the existing representation and capacity suffice. The leaf captures operands
before mutation, bounds the full VM-word index by both length and capacity,
stores the character, and extends the signed length only for an append.
Existing-character updates still leave length untouched. Empty ASCII strings
with spare capacity are included; representation promotion, shared strings
and full-capacity growth retain C fallback.

The ARM64 store retains the original owner while calculating the character
address, so the final length store cannot accidentally target the payload.
INT_MAX lengths cannot grow because no representable capacity exceeds them.
No allocations, reference/cache changes, helper calls or FP operations occur.
Both backends retain the 64-unit staging limit and caller-saved register use.

## Comparison capacity errors

The C comparator and all six native string branches reject lengths exceeding
signed logical capacity before payload reads or identical-owner shortcuts.
Negative capacity also rejects an empty non-nil string. Native header checks
compare the absolute length in 64 bits with a sign-extended 32-bit capacity;
this simultaneously rejects INT_MIN without increasing the ARM64 leaf size.
Malformed private capacity offsets reject emission before changing output.

The instruction-emulation fixture also fixes its identical-owner setup: it now
writes each object header once instead of overwriting the first owner's length
with the second operand's metadata. Huge identical-owner tests therefore
exercise the intended lengths rather than silently testing empty strings.

## Validation

```sh
python3 libinterp/tests/slice-tokenize-suite.py --core
STRING_BRANCH_NATIVE=1 python3 libinterp/tests/string-branches.py
python3 libinterp/tests/decimal-native.py
python3 libinterp/tests/string-store-native.py
python3 libinterp/tests/decimal-jit.py
python3 libinterp/tests/string-store-jit.py
LEAF64_ARCH=arm64 LEAF64_CASE=bytebranch python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=amd64 LEAF64_CASE=bytebranch python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/jit-coverage.py
```

The decimal native fixture executes 626,688 actual C/RX-JIT cases per WORD
model, comparing complete object/operand images, error identities and four FP
rounding modes with clear or preexisting flags. Both models pass UBSan and
ASan+UBSan. The native append profile passes ASan+UBSan in both models; these
ASan runs still emit the sandbox's existing nonlocal-jump runtime warning.
They do not establish complete sanitizer acceptance for all VM error paths.

The decimal emulation matrix passes 751,840 combined AMD64/ARM64 checks,
including every byte value in a digit position, metadata rejection, aliases,
all four FP rounding modes and clear/preexisting flags. The expanded string
store matrix passes 336,152 combined checks with exact guest write hooks for
character and length stores. It includes empty strings, full capacity, append,
out-of-bounds indexes and both representations. The string comparison matrix
passes 15,474 instruction-emulation checks per architecture and 833,118
native/C checks per WORD model. Parent C comparison code and both parent comparison emitters fail the added
capacity regression.

All 76 core host drivers pass. These are host fixtures and instruction emulation,
not a complete native
64-bit VM ABI port or physical ARM64/RISC-V acceptance test.
