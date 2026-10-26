# Integer formatting into reusable Rune allocations

C CVTWC/CVTLC and AMD64/ARM64 leaves now reuse a unique Rune allocation when
its byte capacity fits the formatted signed integer. Rune max is multiplied
by sizeof(Rune) using bounded arithmetic. The resulting byte capacity must fit
the signed String.max field. Invalid oversized metadata fails before allocation,
cache preparation or header/payload writes. Shared, H and insufficient-capacity
destinations retain C construction paths.

Successful reuse preserves owner identity, original allocation, heap/Type
counters and the old UTF-8 cache. max becomes byte-oriented and len becomes
positive ASCII length; later String destruction still owns and frees the cache
once. ASCII capacity stays unchanged. Inputs are captured before output writes,
including values inside the old Rune payload and source/result slot aliases.

Both backends first count sign/digits and validate capacity and native payload
start/end sums. The end includes the terminating NUL address. Only after these
guards may a Rune max change. Digits are emitted backwards directly into their
final positions, avoiding the former reversal pass. Length is published last.
The private staging bound remains 64 units; ARM64 emits 55 units, or 56 for
WORD32 CVTWC, despite the extra Rune and address guards. No layout fields or
VM ABI changes are introduced.

Validation after restoring the interrupted execution environment:

- integer-format-jit.py: 627984 independent complete-image/write/FP cases per
  backend (1255968 total), both WORD models, both opcodes, every operand byte
  alignment, embedded-source/shared-slot aliases, decimal boundaries through
  10^18, small/exact capacities and Rune metadata/count limits.
- An additional 48 top-address-space probes per backend (96 total) check
  payload-start and end-pointer overflow, accepted exact-NUL boundaries and
  write-free declines before converted max publication. The x86 probe mirrors
  metadata through Unicorn's default 52-bit physical mask while keeping full
  64-bit owner values in guest registers.
- integer-format-native.py: 352440 actual C/RX cases per WORD model (704880
  total), real Rune allocations with retained caches, saturated/dead descriptor
  counts, shared-owner preservation, exact header images and allocator failure.
  Both models pass ASan/UBSan.
- Another 84 C/RX sequences per WORD model format, append ASCII, append a Rune,
  slice, export UTF-8 and decode it again using production helpers. This checks
  converted capacity, cache ownership and full Type/allocation restoration.
- Existing 743400 layout/sizing/canary probes pass ASan/UBSan; 1171 cross-target
  instruction/ABI checks and all three compiler publication/fallback/metadata/
  failure/retry/unmap fixtures pass. The full 98-driver core suite passes.
- Parent string.c replay fails on the formerly allocating Rune path and on
  unchecked nonrepresentable byte-capacity metadata. FORMAT_MODE/FORMAT_C_ONLY
  select these probes; FORMAT_BOUNDARY_ONLY selects the address probes.

Guarded availability remains AMD64/ARM64 141 commands / 32 remaining, RV64 M
103/70, Zmmul 90/83 and I 87/86 out of 173 active commands. This broadens two
existing opcodes. Native semantics remain partial; ARM64 instruction execution
is emulated and physical target acceptance is separate.


Current pointer-temporary handling adds an exact Tstring guard and raises the
private AMD64/ARM64 staging bound to 80 words. This extends the earlier layout
contract described above; the Dis instruction/container ABI is unchanged. See
[generic pointer temporary regression](README.format-pointer-temporaries.md).
