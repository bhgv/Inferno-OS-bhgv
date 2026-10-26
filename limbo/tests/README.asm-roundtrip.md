C compiler assembly initializer round trips
==========================================

The textual output of limbo/asm.c now preserves three properties already
represented by the binary initializer writer:

* Tcasel emits a word count, skips its header padding, emits two big bounds
  through a long directive, emits the word target, and skips its row padding.
  Offsets follow IBY2WD/IBY2LG. Cell4 uses 24-byte rows; cell8 uses 32-byte rows.
  Empty-table defaults also follow the padded header.
* NaN constants emit a long containing the exact 64-bit pattern obtained from
  dtocanon, with a readable real-NaN comment. The assembler's real NaN grammar
  creates a canonical NaN and cannot represent arbitrary signs/payloads.
  DEFL stores the same resulting eight bytes, although its record tag differs
  from DEFF. Finite values and infinities retain the existing real output.
* Source paths use the same quoted byte escaping as string initializers:
  newline, NUL, double quote and backslash are preserved rather than becoming
  assembler syntax or escape sequences in the source metadata.

The compiler already installs gfltconv for %g, backed by g_fmt. Finite output
therefore already round-trips; libc's default six-digit %g is not its contract.
This series preserves that formatter and tests the real g_fmt implementation.

Run `python3 -O limbo/tests/asm-roundtrip.py`. It extracts the production
asminitializer, asmquoted, asmstring, asmpath, caseend and dtocanon functions,
and production binary Tcasel/Treal serializer arms. A bounded output fixture
uses actual g_fmt/dtoa, normalizing only the Plan 9 integer-format spellings
for host libc output. An independent decoder applies the assembler's word,
long, real and quoted-byte contracts, then compares complete initializer
images, including unused padding. This does not execute the Limbo assembler.

The 3,166-record matrix covers both declared cell widths, empty/multi-row
big tables, wildcards, extreme signed big bounds, finite/subnormal/zero/Inf
bits, all NaN fraction-bit positions in both signs, seeded NaN/random payloads,
all ASCII bytes, embedded NUL, Unicode and source-path punctuation. Replay
against the earlier C source detects each failure family independently:

```
ASM_ROUNDTRIP_SOURCE=/path/to/old-asm.c ASM_ROUNDTRIP_KIND=t python3 limbo/tests/asm-roundtrip.py
ASM_ROUNDTRIP_SOURCE=/path/to/old-asm.c ASM_ROUNDTRIP_KIND=r python3 limbo/tests/asm-roundtrip.py
ASM_ROUNDTRIP_SOURCE=/path/to/old-asm.c ASM_ROUNDTRIP_KIND=q python3 limbo/tests/asm-roundtrip.py
```

CC/CFLAGS select host compilation and instrumentation. Full UBSan and
ASan/UBSan/NDEBUG runs pass, with checks active under python -O. LeakSanitizer
process inspection is unavailable in the host sandbox and is disabled only in
the fixture; address instrumentation remains enabled.

The existing case-max driver collects the new explicit bounds/target directives
and retains 1,184 actual compiler/VM pipeline scenarios. Its C asserts have been
replaced with explicit checks, so NDEBUG does not remove those oracles.
The host checkpoint includes asm-roundtrip.py.

Scope: the native C Limbo compiler. This does not validate a complete assembler
run, the separate self-hosted Limbo compiler, or a full Inferno build/boot.
