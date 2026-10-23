# Terminal Dis-PC relocation entries

Run `python3 libinterp/tests/legacy-jit-fallthrough.py` from the repository root.
The fixture executes complete production MIPS, PowerPC and SPARC compile
drivers, including both sizing/emission passes, macro emission, entry
relocation and publication. Small encoder boundary functions emit one word
per Dis instruction and deliberately read the successor relocation of the
last instruction, as production call/helper encoders do.

The patch array requires `size+1` entries. Its terminal entry must be set to
the emitted length of all Dis instructions before macro bodies are added.
Each driver checks 32 different terminal positions (96 cases total), with
poisoned uninitialized storage, guard bytes and balanced allocation ownership.
The old drivers fail the terminal lookup regression. The existing MIPS
allocation-failure/publication suite also remains enabled.

This tests driver relocation boundaries rather than native instruction
encoding or execution. No MIPS, PowerPC or SPARC machine code runs on the host.
SPIM uses the same MIPS driver. `LEGACY_JIT_SOURCE`, `CC` and `CFLAGS` support
baseline and sanitizer runs. ASan/UBSan are supported; LeakSanitizer process
inspection is disabled because the sandbox cannot enumerate threads.
