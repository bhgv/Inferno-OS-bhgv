# Exact priority JIT cache bindings and code images

The shared decoded-leaf compiler previously checked that cached fast entries and
complete tails were aligned and bounded by their code mapping. Those checks did
not establish which opcode an in-range entry implemented. Swapping IADDW to a
valid ISUBW entry, or another complete tail, could pass cache reuse and execute
the wrong operation. A changed instruction byte could also remain unnoticed.

Cache reuse now validates the canonical compiler publication order: all present
fast entries in opcode order, followed by all present complete tails in opcode
order. Each entry must start exactly at its expected cursor. Its emitted bytes
must equal fresh private emission using the current native layout, opcode,
helper identities and bound fast-entry address. Complete tails cannot silently
lose an available fast entry, and fast entries cannot lose their tails. The
final cursor must exactly exhaust the recorded size. Mapping-release and
entry-synchronization callbacks must retain the compiler's identities.

Validation occurs only on compilation/cache reuse, not on every VM instruction.
It uses a bounded stack staging buffer and does not allocate, rewrite or release
a rejected cache. Existing caller ownership and atomic publication are retained.
The mapping must already be readable, as Linux RX mappings are; these checks
cannot prove that an arbitrary fabricated pointer belongs to a VM allocation or
provide a concurrent mutation protocol. Existing cached opcode supersets remain
allowed when the unchanged program length needs only some of those operations.

## Verification

```sh
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The native AMD64 suite passed 177,152 real C/RX routes, 6,183 compiler/cache cases
and 295 VM integration checks per WORD32/WORD64 model. ARM64 host-side compiler
checks passed 6,215 cases per model. Native pointers remain eight bytes in both
models. These are 4,788 added cache cases over the previous compiler matrices.

The binding family includes all 173 commands, six substitution/removal modes
where applicable, callback identity changes and an enlarged code extent. Each
rejected mutation is restored and the original cache must be reusable without
allocating another descriptor. The byte family independently flips every byte
of a generated ADDW/RAISE image, covering a fast entry and two complete tails.
Test mappings alternate writable and executable permissions; damaged code is
never executed. Restoring each byte must restore valid cache reuse.

Eight independent negative controls run the preceding production compiler with
the new fixture: bindings/bytes for AMD64/ARM64 and WORD32/WORD64. Every control
reaches an active predicate and aborts because the preceding cache incorrectly
accepts the input, rather than failing compilation. Replay with
`BRIDGE_COMPILER_SOURCE`, `BRIDGE_BACKEND`, `BRIDGE_WORD_BITS`,
`BRIDGE_PREFLIGHT_ONLY=1` and `BRIDGE_COMPILER_CASE=bindings|bytes`.

Opcode availability is unchanged: each priority architecture has 173/173
complete entries including C, and 166/173 guarded native fast paths (95.95%).
ALT, NBALT, SPAWN, MSPAWN, LOAD, EXIT and RAISE remain C through complete tails.
The compiler/reference/frame ABI and physical ARM64 acceptance run remain
unfinished; this cache correction does not establish a bootable 64-bit VM.
No RV64-specific emitter work, MIPS/SPARC/PowerPC JIT changes, Tk or
security/crypto changes are included.
