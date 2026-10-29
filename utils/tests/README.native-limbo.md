# Runnable native Limbo and the remaining 64-bit VM boundary

The native bootstrap can now link and install the full Limbo compiler, rather
than stopping after compiling its C units. On Linux x64 or native Linux ARM64:

```
MKBOOTSTRAP_LIMBO=1 sh makemk.sh
Linux/amd64/bin/limbo -I module -I utils/dis2c/examples -o sum.dis utils/dis2c/examples/sum.b
```

Use Linux/arm64/bin/limbo on an ARM64 host. MKBOOTSTRAP_BINDIR overrides the
installation directory. The default bootstrap still installs only mk, iyacc
and decgen. Invalid option values and compilation/link failures occur before
installation begins. A final Limbo link failure preserves all installed tools.
Target mkconfig, committed grammar inputs and old 386 tools are not rewritten.
The fresh parser generator uses the selected source root even if an inherited
ROOT variable names another tree.

The optional build links existing MD5 sources unchanged for type signatures,
uses the production decimal parser/formatter and builds a fresh Limbo grammar.
It supplies native floating-point control through Linux/glibc fenv, rather than
linking the existing 386 assembly into a 64-bit tool. Control/status map to the
active thread's real hardware environment. Masked updates preserve unselected
bits; status publication does not raise an enabled trap; rounding is updated
only when selected. This file is selected by the native host-tool bootstrap,
not by the existing emulator platform makefiles.

## Parser errors exposed by executing the compiler

UBSan stopped even an ordinary sum.b compile at the initial &yys[-1]. The yacc
skeleton formed a pointer before its stack array. Error recovery could also pop
past that array and print a nonexistent predecessor state. Empty reductions
read a nonexistent first RHS value, including beyond capacity at a full stack.

Both the source and installed skeleton now reserve an in-array sentinel. Pushes
check capacity before incrementing. Recovery stops at the sentinel and describes
an empty stack without reading it. Empty reductions receive a defined zero value.
The number of live slots still equals YYMAXDEPTH. limbo/y.tab.c is regenerated;
its grammar/actions/tables and y.tab.h remain unchanged. Valid grammar actions
retain their meanings; relying on an unspecified empty-rule default is unsupported.

## Executed evidence

- native-limbo.py builds four native ELF64 tools, runs 72 real-source Dis cases,
  repeats output for determinism, checks source/optimized variants of sum.b,
  driver.b and echo.b, validates Dis with dis2c and compiles their generated AOT C.
  It also checks CLI diagnostics, current ABI32 offsets and a deliberately failed
  final link with exact preservation of installed binaries. UBSan remains active
  while the real compiler runs. The pre-fix generated parser fails at startup.
- iyacc-stack.py executes 572 generated-parser cases under ASan/UBSan/NDEBUG,
  with capacities 1..32, nested valid inputs, empty rules, syntax errors, debug
  levels 0/4, recovery/no-recovery grammars and restoration of parser globals.
  Parent-template replay reproduces the startup out-of-array index.
- native-fp-control.py checks 557,056 masked control/status transitions against
  native fenv state, all four rounding modes, thread isolation and a real SIGFPE.
- Existing native-bootstrap checks, complete AMD64 dispatch, ARM64 compiler
  preflight and per-opcode readiness still pass. The three new drivers are wired
  into the core checkpoint runner. ASan's unavailable process leak scan is
  disabled in the parser fixture; address/stack/undefined checks stay enabled.

```
python3 utils/tests/native-limbo.py
python3 utils/tests/iyacc-stack.py
python3 libmath/tests/native-fp-control.py
python3 utils/tests/native-bootstrap.py
```

All native execution above was checked on Linux x64. The ARM64 host-tool build
path and fenv implementation need a native ARM64 run; this is not hardware ARM64
acceptance or a complete 64-bit emulator boot.

## What the JIT percentages do not measure

| Layer | Current evidence | Remaining boundary |
| --- | --- | --- |
| x64 / ARM64 opcode dispatch | 173/173 JIT+C; 166/173 guarded native paths | Seven complex commands intentionally use C |
| Native Limbo host executable | Linked and run on x64 | Native ARM64 acceptance |
| Compiler output layout | Existing ABI32 Dis | Explicit target ABI64, separate from host word width |
| Native VM frame | Four native pointer fields, 32 bytes on LP64 | Compiler ABI32 reserves 16 bytes for these fields |
| Function result slot | Compiler ABI32 offset 16 | Overlaps the native frame's mr field on LP64 |
| int and pointers | Compiler int/ref slots are 4 bytes | Native WORD is long; WORD32 fixtures still have 8-byte pointers |
| Complete emulator | Not established by these tests | ABI/file discrimination, maps, builtins and platform startup together |

The compiler-to-Dis/AOT tests stop before executing those legacy frames in an
LP64 VM. Merely changing OBJTYPE, widening WORD or reporting 100% opcode dispatch
cannot resolve these layout conflicts. See ../../doc/dis64-design.md for the
explicit ABI migration contract. No VM ABI or wire encoding is changed here.
