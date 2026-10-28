# Sys native I/O capture and span regressions

Run `python3 libinterp/tests/sys-io.py` from the repository root.
The driver extracts production `Sys_read`, `Sys_readn`, `Sys_write`,
`Sys_pread`, `Sys_pwrite` and their private validators from both
`emu/port/inferno.c` and `os/port/inferno.c`. It uses real generated frames
and heap/array declarations. The default CORE checkpoint includes this test.

Each port and scalar model runs **230,400 ordinary scenarios**:
5 operations × 30 operand modes × 8 array shifts × 8 frame/FD shifts ×
8 result shifts × 3 callback phases. Four profiles total **921,600**.
A separate process for each invalid operand checks 40 preflight cases per
profile, **160** total. Every predicate remains active under `NDEBUG`.

The ordinary matrix checks full-width clamping, negative-count and H-buffer
no-op conventions, invalid array kinds and lengths, nil/H/wrapping payloads,
low/wrapping array headers and nil heap origins, signed native FD boundaries, positive and negative
FD numbers that cannot fit native int, H descriptors, empty arrays, short
reads, initial native errors and errors after progress. Positional operations
must preserve offsets above 32 bits. WORD32 skips the two impossible wide-FD
values but still runs their ordinary valid-FD counterparts.

Callbacks at release or inside the first native call overwrite the original
frame and replace array-data and FD-number fields. The original data pointer,
count, offset, descriptor and result cell must still be used. The full modified
frame must remain modified: no state restoration is allowed. Result-cell
canaries detect partial or oversized stores. Readn must keep its captured data
pointer across every short read and return progress after a later error.

Preflight processes check nil/H/wrapping frames, nil/H/wrapping result cells,
and nil/wrapping FD headers before any release, native call or Sys argument
error. Their original result sentinel remains unchanged; fixture heap storage
is freed before the successful error-path process exits.

Defaults enable ASan, UBSan and `NDEBUG`. The matrix runs on a joined pthread
so ASan can track its stack when process-stack discovery is unavailable.
LeakSanitizer process inspection is disabled; address and undefined-behavior
checks remain enabled.

`SYS_IO_SOURCE` selects an alternate source. `SYS_IO_PORTS` and
`SYS_IO_MODELS` select profiles (defaults `emu os` and `native word32`).
`SYS_IO_OP`, `SYS_IO_MODE`, `SYS_IO_PHASE`, `SYS_IO_ARRAY_SHIFT`,
`SYS_IO_FRAME_SHIFT` and `SYS_IO_RESULT_SHIFT` isolate ordinary scenarios.
For example, phase 1 overwrites operands at release; phase 2 does so during
native I/O; mode 22 rejects a positive FD above 32 bits in WORD64.

Against parent `3fdd90d`, 52 separately compiled controls fail at runtime:
for each port and operation, packed frames, packed arrays, release callbacks,
wide FDs and wrapping payloads; additionally, each port's readn fails when a
callback changes its operands during the first short read. These controls
must produce a sanitizer diagnostic or an active fixture predicate failure,
not merely a compilation error.

Native I/O and scheduler boundaries are controlled stubs. This does not boot
an emulator or kernel, perform filesystem I/O, or prove a complete 64-bit
compiler/loader/GC ABI. WORD32 retains host pointers. Packed array and FD
headers are borrowed byte views; these tests do not transfer their ownership.
