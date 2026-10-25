# Native numeric case searches and descriptor-group control fixes

## Two new AMD64/ARM64 opcode paths

ICASE and ICASEL now execute a helper-free native binary search for ordinary
Dis-PC tables with 0 through 4095 rows. Larger tables and already relocated
native-PC tables retain the C handlers. Searches preserve signed source/bound
comparisons, exclusive upper bounds, sparse gaps and default destinations.

The emitters validate module/compiled metadata, count/header/span arithmetic,
signed destinations, instruction counts and final native address addition.
Only a fully checked target updates REG.PC. Input/table bytes, other REG fields,
GC ownership and FP state are unchanged; the middle operand is never accessed.
Tables can be byte-unaligned. Integer rows remain 12 bytes and big rows 24
bytes, independently of the active VM WORD width.

AMD64 uses caller-saved registers and compact field displacements where possible,
retaining full displacement encoding for larger private-layout offsets. Two
nearby rejection islands keep conditional transfers short. ARM64 uses one shared
rejection return and a direct signed WORD32 source load. Both retain the existing
64-unit private capacity, without a runtime helper ABI or native stack frame.
Ordinary Dis PCs, frame maps and compiled==0 leaf publication remain unchanged.

Both backends also explicitly reject a classifier kind that they do not handle,
except the intentional LeafNop. Previously a future missing emitter case could
fall through to a successful native no-op. A fault-injection fixture substitutes
an unknown classifier kind, checks zero sizing/no publication, and separately
verifies that ordinary IRUNT is still accepted. It reproduces the old silent
acceptance with preceding emitter sources; it does not assert that an unknown
kind was reachable in the previously complete classifier/backend combination.

Opcode availability is now 121 native-path / 52 interpreter-only on AMD64 and
ARM64 in both WORD models. RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I
87/86. The remaining-name list is regenerated from production emitter output.
These are guarded decoded-operation paths, not a completed native block JIT.

## Sys.pctl descriptor groups in emu and os

FORKFD previously duplicated the descriptor group, called implicit-environment
kclose on the supplied descriptors, and only then installed the duplicate.
That closed descriptors in the old group, potentially shared with other
programs, while leaving the requested descriptors open in the new group.
It now uses kfgrpclose on the unpublished duplicate explicitly, then publishes
that group and drops the old group reference. Native close-error handling and
the release/acquire ordering remain in their C implementations.

NEWFD and FORKFD now read list integers at VM WORD width with memmove rather
than loading only a typed native int. Values outside native signed-int range
cannot become a different descriptor by losing high bits. NEWFD ignores such
out-of-range indices as other non-preserved descriptors; FORKFD records Ebadarg
and skips that close, preserving its existing nonfatal per-close behavior.
Representable native arguments, including negative error/sentinel values,
retain native handling. Success and handled-error statuses are copied into
possibly unaligned return slots at full VM WORD width.

No namespace, environment-group or device-permission behavior is changed.

## Validation

- Actual production C versus executable AMD64 case leaves: 614,428 cases per
  WORD model, 1,228,856 total, ASan/UBSan. An independent linear oracle covers
  integer/big bounds, gaps, defaults, source/table byte alignments, target and
  module errors, and relocated-table rejection/C behavior. Every REG/data byte
  is compared. Additional limit tests cover 4095-row native execution and
  4096-row C fallback: 64 cases per model, 128 total, ASan/UBSan.
- Existing C/native IGOTO matrix: 88,733 cases per model, 177,466 total,
  ASan/UBSan, now compiled alongside both new case leaves.
- Unicorn/Capstone: 13,516 numeric-case checks, including ARM64 signed loads,
  extreme operands, aliases, all relevant metadata guards and table-read bounds.
  Rejected negative/oversized counts may not read table payloads. IGOTO
  regression passes 73,768 checks with the expanded opcode set.
- Unknown-kind sizing/publication fault injection passes on both emitters,
  WORD32/WORD64, with ordinary nop preservation and unchanged rejected output.
- Pctl fixture: 1,752 cases per emu/os and WORD32/WORD64 configuration, 7,008
  total, ASan/UBSan. It executes complete production wrappers with real nested
  protection frames and controlled native groups/channels. Tests check old-group
  sharing, descriptor contents, exact channel/group refs, repeated/invalid/high
  descriptor values, allocation failures, all result offsets, full byte guards
  and scheduler transitions. The os adapter maps Plan 9 anonymous Lock/Ref
  promotions onto explicit host structs; unrelated namespace branches abort if
  accidentally entered, rather than being silently treated as covered.
- Separate preceding-source pctl replays reproduce wrong-group closure,
  WORD64 descriptor truncation and UBSan unaligned result stores. PCTL_SOURCE,
  PCTL_MODE, PCTL_PROFILE and PCTL_OFFSET isolate each scenario. The shared
  fd-word helper in a previous-source fixture serves only its direct word tests;
  it does not replace the preceding wrapper's descriptor reads or close calls.
- All 67 core drivers pass. Complete native AMD64 runtime matrices pass
  ASan/UBSan in both WORD models. Publication now includes 69 explicit
  instructions; failure/retry/freecode checks pass on AMD64, ARM64 and RV64.

No sanitizer instrumentation is relaxed beyond the established hook for
unavailable leak-process inspection. No Tk, crypto/security or excluded legacy
JIT sources are modified. Physical ARM64 execution, full OS/kernel acceptance,
native block/helper transitions and a coordinated native 64-bit
compiler/loader/frame/GC ABI remain separate work.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/goto-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=case python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/sys-pctl.py
python3 libinterp/tests/jit-unknown-kind.py
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```
