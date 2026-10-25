# Native module returns on AMD64 and ARM64

RET now restores a saved module owner directly when the ordinary frame Type has
np==0, the saved/current module identities are valid, their compiled flags are
identical, and the current module's heap reference count is greater than one.
The decrement therefore takes exactly destroy's non-destructive fast path: no
module finalizer, GC lock, allocator or heap monitor is involved. Maximum native
unsigned counts can be decremented; zero and one decline to C. A saved owner
identical to the current module is supported, as is a different caller module.

Frame/type/caller/PC checks and all module checks precede the first VM store.
Saved FP, return PC, current owner, decremented count, caller identity and MP are
captured before publication. Native writes follow C's effective order: FP, SP,
PC, current owner's count, M, MP. IC and the VM temporary register are preserved
when the flags match. An ordinary return without a saved module does not inspect
the current module or its reference header.

Mapped cleanup, stack extensions, mode changes and last-reference destruction
retain their C handlers. Declines make no VM stores, including same-value stores.
C RET now also rejects a zero current-module count and a self-module return
with only one counted owner before changing REG or freeing frame fields; previously destroy could decrement it only after VM publication.
The fixture uses actual Heap/Modlink adjacency rather than stack-only module
payloads and checks owner counts on successful and failed returns.

AMD64 shares bounded rejection stubs and short field loads with MCALL. Its field
load helpers now encode the REX.R bit and low ModRM register bits explicitly,
allowing r8-r11 destinations with either short or full displacements. ARM64 uses
caller-saved registers and a shared decline return. Both keep the private limit
of 64 four-byte staging units. No private layout fields or ABI offsets are added.

Validation:

- `return-native.py`: 1920 C/RX cases per WORD model (3840 total), including
  unaligned frames, saved same/different modules, zero/last references, mode
  changes, mapped/extension fallback and exact REG/frame/owner images.
- `call-return-native.py`: 6144 mixed C/RX round trips per WORD model (12288
  total), all combinations of C/native call and return, four equal compiled
  flag patterns, self/cross-module calls, last-legal MCALL counts and all eight
  operand/frame alignments. Every completed round trip restores the original
  reference count, caller, PC, FP, MP, operand bytes and expected SP/temp state.
- `return-jit.py`: 5504 cases per backend (11008 total), both WORD models and
  64 frame byte offsets, exact image/store traces and FP-state preservation.
  Includes maximum and zero counts, ordinary returns with unavailable current
  modules, saved-module aliases, nil/H MP and malformed layouts/capacities.
- `return-layouts.py`: 29120 layout/capacity probes per backend (58240 total),
  including native offsets above disp8 reach and staging/output canaries.
- Parent C RET replay and an independent removal of the alias-only count guard
  fail the selected dead-owner/self-module regressions as expected.
- `amd64-field-registers.py`: 21060 independent executed field-load checks,
  covering low/extended registers, owner aliases, both displacement encodings
  and signed 32-bit/native 64-bit loads above 4 GiB.
- Native RET, round-trip and layout matrices pass ASan/UBSan. Existing MCALL
  instruction matrices pass 9216 cases per backend after the shared helper
  changes. The full core suite includes 92 drivers. Runtime RX publication,
  fallback tables, allocation/protection failure, retry and unmap are checked.

Coverage stays at 138 guarded native paths / 35 interpreter-only commands on
AMD64 and ARM64, out of 173 active Dis commands. RV64 M, Zmmul and I remain
103/70, 90/83 and 87/86. This expands RET's operand coverage without adding an
opcode ID. WORD32 retains native 64-bit pointers; legacy WORD64 is also tested.
ARM64 execution is emulated; physical target execution remains outstanding.
