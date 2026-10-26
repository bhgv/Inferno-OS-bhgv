# Real formatting checkpoint

ICVTFC now has guarded AMD64 and ARM64 paths for +0 and signed exact powers
2^0 through 2^19. Their six-digit-or-shorter decimal representation agrees with
the existing %g formatter. The emitters decode binary64 bits and use integer
division; they execute no floating-point instructions. Negative zero, non-power
integers, fractions, subnormals, NaNs, infinities and larger values retain C.

Both paths require a valid unique destination with enough byte capacity for the
actual result. ASCII and Rune owners can be reused, including exact capacities.
Rune capacity is checked before multiplying by sizeof(Rune); representation
metadata is published only after validation. Source/owner-slot and payload
aliases capture input before writes. Declines leave memory and FP state intact.

The C handler shares this exact-value domain and reuses valid unique owners.
General %g conversion reuses only owners with at least 32 bytes. Otherwise it
allocates before formatting, preserving FP status on allocation failure.
Formatting goes to a local buffer before publishing payload and metadata.
Shared owners retain replacement semantics; nil, dead and malformed owners are
rejected before allocation or FP work. Cached Rune conversion storage remains
owned by its original string and is reclaimed through the normal destructor.

Validation:
- 626,808 Unicorn differential cases across AMD64/ARM64, both WORD models,
  independent decimal oracles, alignments, aliases and malformed owners.
- 552,960 C/real RX AMD64 checks, including allocation failures, reference
  counts, rounding modes, sticky flags and non-finite values; ASan/UBSan pass.
- 371,700 layout, sizing, publication and canary probes; ASan/UBSan pass.
- Existing integer formatting regressions pass: 627,984 emulated cases and
  48 address-boundary probes per architecture, 352,440 native cases and
  84 Rune round trips per WORD model, plus layout probes.
- 99 core host drivers, 1,171 instruction/ABI checks, and compiler publication
  checks for AMD64, ARM64 and RV64 pass.

Of 173 active Dis commands, AMD64 and ARM64 each have 142 guarded native paths
and 31 remaining interpreter-only commands. RV64 counts remain M 103/70,
Zmmul 90/83 and base I 87/86. A guarded path does not imply native handling of
every operand. ARM64 execution is emulated; physical target acceptance and
complete VM integration remain separate.
