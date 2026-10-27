# Native Linux build tools and the remaining VM ABI work

`sh makemk.sh` now builds native `mk`, `iyacc` and `decgen` on Linux, selecting `amd64`
for x86-64 and `arm64` for AArch64. Existing 386/ARM compiler selections remain
available. The compiler identity and pointer width must match the selected host.
The Linux/386 POSIX headers are reused for host-tool declarations only; this does
not create or select a 64-bit emulator target profile.

The decoder follow-up adds exact generator/source equality and strict compilation
of generated `dec.c`: [README.decoded-addresses.md](../../libinterp/tests/README.decoded-addresses.md).

The bootstrap no longer evaluates `mkconfig` as shell, forces `-m32`, deletes
installed tools before compilation, or writes objects into source directories.
It uses private archives, cleans up its own build directory, and installs each
completed executable by rename. Compilation/link failures preserve installed
tools. `CC`, `AR`, `CFLAGS`, `LDFLAGS`, `INFERNO_ROOT`, `MKHOSTOS`, `MKHOSTARCH`
and `MKBOOTSTRAP_BINDIR` can be supplied explicitly. `CC` and `AR` name a single
executable; compiler options belong in `CFLAGS`.

Host compilation exposed and this series fixes:

- `sprint` narrowed native addresses to `uint`; available output extent now uses
  `uintptr` without a wrapping addition.
- `vsnprint` and `vseprint` entered formatting with invalid/wrapping output spans
  or nil format pointers. Rejection now precedes formatter callbacks and writes.
- `create` lacked the `mkdir` declaration needed by strict native compilation.
- `mk` hashed long names using overflowing signed arithmetic, read an entry after
  freeing it, and indexed its diagnostic histogram past 1000 entries. Hashing is
  unsigned, deletion returns after unlink/free, and the last histogram bucket
  explicitly represents chains of 999 or more entries.
- `mk` comparator caching converted pointers directly to `int`; conversions now
  pass through the host's `uintptr`.
- `iyacc` shifted signed `1` into bit 31; terminal bitsets now use unsigned shifts.

## Verification

Run from the repository root:

```sh
python3 utils/tests/native-bootstrap.py
python3 utils/tests/iyacc-bitsets.py
python3 utils/mk/tests/symbols.py
python3 lib9/tests/native-output-spans.py
python3 lib9/tests/format-bounds.py
python3 lib9/tests/create-flags.py
python3 limbo/tests/layout-bounds.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The native integration test builds actual ELF64 executables with UBSan and
strict pointer/prototype diagnostics, rejects a failed compiler and a mismatched
architecture while preserving installed binaries, executes mk recipes/includes/
dependencies/failure handling, generates the real Limbo grammar, and compiles
all 17 compiler translation units. Existing configuration and 386 tools are
fingerprinted for preservation. x64 execution is verified; the ARM64 bootstrap
selection still requires a real ARM64 host run.

The symbol fixture checks 163840 lookup/namespace/deletion outcomes, singleton
freeing and 3000-entry diagnostic chains under ASan/UBSan. The real formatter
fixture checks 918 dispatch/truncation/NUL/full-image cases plus five nil cases;
it reduces address-guard coordinates to 16 bits while retaining native pointers
and the actual formatting engine. This exhaustible model does not map the top
of the real address space. The iyacc fixture checks 2097152 membership outcomes.
All seven independent parent probes fail: hash overflow, deletion use-after-free,
diagnostic overflow, three formatter guards, and the signed bit-31 shift.

## Why the 64-bit VM is not finished

A working native build tool is separate from a compatible target ABI:

| Current source contract | LP64 consequence |
| --- | --- |
| `include/interp.h`: `WORD` is `long`, annotated as 32 bits | The C type becomes eight bytes. |
| `include/isa.h`: `IBY2WD=4`, `NREG=5` | The compiler still reserves 20 bytes before `STemp`. |
| `Frame` stores four native pointers | Its header occupies 32 bytes, overlapping compiler temporaries starting at byte 20. |
| `limbo/types.c`: string/exception/any reference size and alignment use `IBY2WD` | Four-byte reference slots cannot hold native eight-byte pointers. |

The production-header probe `python3 utils/tests/native-vm-abi.py` reports these
widths and offsets directly. Add `--require-compatible` as a release gate: it
currently exits with status 1 on x64 because all three basic contracts fail.
Passing this basic gate will still require full loader/GC/runtime tests.

These contracts require a coordinated compiler/loader/runtime/GC frame and
pointer-map decision before a full native VM build is a meaningful validation.
This series deliberately preserves the existing `.dis` layout. There is still
no complete native Linux AMD64/ARM64 emulator target profile or verified full
Limbo program boot in this series.

JIT availability remains 173/173 complete entries (native fast path plus C),
with 166/173 guarded native fast paths (95.95%) for each priority architecture.
This is opcode emission/dispatch coverage, not 100% of operand domains or proof
of a bootable 64-bit port. Seven complex operations retain C: ALT, NBALT, SPAWN,
MSPAWN, LOAD, EXIT and RAISE. See the per-command
[readiness table](../../libinterp/tests/README.jit-readiness.md).

RV64 remains deferred. Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources
are outside this series.
