# Limbo scalar Dis64 backend (through series 0064)

`limbo -6` emits the already agreed extension-free Dis64 v1 np0 executable
profile. It is an explicit experimental backend, independent of `-H` C metadata.
Normal Dis, assembly, AOT and declaration generation retain their existing target
layouts. This is not a switch that turns all Limbo programs into native64 programs.

The first accepted shape has one reachable compiled `init()` with no parameters,
no result and pointer-free int/big/real storage. The backend runs after ordinary
parsing, type checking, lowering and optimization. It accepts only the 43 existing
scalar operations validated by dis64-pilot.h; no opcode, feature, section, scalar
kind, signature or calling convention is assigned. Bounded no-argument, no-result
leaf procedures can now be specialized into the entry before scalar lowering.
Scalar helper locals are initialized per call; managed/nested calling remains
closed. See [the helper limits](../../doc/dis64-scalar-helpers.md) and
[the self-checking example](dis64-scalar/procedures.b). General function calls
and a managed ABI remain outside this profile.

The source entry's RET becomes EXIT. Lowering remaps branch targets while
preserving early entry termination.
Imports, linkage/exception tables, other exported functions or exported module
data, managed/byte/aggregate storage and unsupported operand/instruction forms
are refused. Real moves/comparisons are admitted; Real arithmetic/conversions
are outside the current file subset even where the native JIT has fast paths.
Unsupported modes fail the compilation instead of falling through to legacy Dis.

`-6` requires one input. It conflicts with `-S`, `-H`, stub/table/AOT output,
symbol output, signature dumping and compile hints. `-o` is supported and the
default suffix is `.dis64`. Errors use ordinary Limbo output cleanup; this does
not add a transactional replacement service for pre-existing output files.

## Local-frame translation

The compiler still lays out its original scalar IR with a 32-byte reserved frame
prefix. Locals at offsets32 and above are translated by +32, preserving their
relative byte layout and alignment after the Dis64 64-byte reserved prefix.
No Type.size, Decl.offset, legacy descriptor, IR instruction or legacy signature
is mutated by the backend.

The compiler also uses scalar scratch bytes20..31 within its old prefix. Mapping
those bytes by +32 would put them inside Dis64's reserved prefix. They instead
move to a separate aligned 16-byte tail after the translated local extent. The
relative byte positions and overlap of the original scratch span are preserved;
the old return-address word16 remains inadmissible. These are implementation
scratch cells, not new wire fields or compiler-declared native-call witnesses.
The known compiler geometry (WORD4, LONG8, MaxTemp32, STemp20) is checked before
translation. Mapped frames and other geometries are refused.

Every converted instruction passes the existing scalar role validator. Native
middle operands are additionally checked for the existing 16-bit runtime limit;
a WORD middle immediate cannot stand in for a complete LONG. Constant int/big/
real initializers use the agreed scalar kinds and canonical IEEE bits. Full map8
zero suffixes are written for the complete data/frame extents.

All sections use existing module/type/data/string/envelope writers. After
serialization, the complete immutable image must pass dis64pilotcheck before
any image byte is written to the compiler output stream. Existing object/code/
initializer/image bounds are retained. The entry name stays within the current
runtime's ASCII display-name subset. Source paths and debug information are not
silently encoded as a new section.

## Build and run

On a native Linux ARM64 host, first rebuild native tools from current source:

```
MKBOOTSTRAP_LIMBO=1 sh makemk.sh
```

Build the experimental console using the existing clean port driver described
in doc/build-native64.md. For a configured ARM64 tree `/build/arm64`, the supplied
pointer-free source example can then be compiled and run:

```
Linux/arm64/bin/limbo -6 -o /build/arm64/probe.dis64 limbo/tests/dis64-scalar/loop.b
/build/arm64/emu/Linux/o.emu-g -v -r /build/arm64 -c 1 -d /probe.dis64
```

The example self-checks its 4097-iteration result. A normal verbose completion
reports Native64Probe and an empty scheduler after actual program retirement.
It deliberately does not import Sys or use the ordinary shell command ABI.
The default installed legacy runtime is not enabled by this compiler option.

## Executed evidence

```
python3 utils/tests/native64-limbo-boot.py --limbo /host/tools/limbo \
  --root /build/arm64 --emu /build/arm64/emu/Linux/o.emu-g \
  --runner 'qemu-aarch64 -L /sysroot/aarch64-linux-gnu'
```

Fifteen accepted source programs cover int/big overflow, MIN/-1 division and
remainder, signed division, shifts, mixed locals, real constants/moves, long
loops, real error termination and a deliberately wrong expected answer. The
scratch-alias program keeps Big locals live while the scalar scratch cells are
used by an unrelated WORD expression. Both JIT modes execute each produced
file: **30 full OS processes per architecture**. Ten additional sources first
compile as ordinary Limbo, then must fail cleanly in scalar mode, so syntax
errors cannot masquerade as admission refusals. Independent Python decoding
checks envelope, section, entry, descriptor and map geometry. Default suffix,
six CLI conflicts and byte-for-byte preservation of ordinary Dis products are
also checked. The clean native64-port driver includes this gate. A separately compiled backend
control replaces the scratch-tail mapping with the naive legacy +32 mapping;
the valid bitwise source then fails the scalar role gate instead of publishing
an invalid reserved-prefix access. This is an executed compiler control, not a
C compilation failure used as an oracle.

This passed for native x64 compilation/execution and ARM64/RV64 execution under
QEMU. Actual ARM64 and RV64 ELF64 Limbo binaries also compile the same sources
under QEMU, with their own target-built iyacc used during bootstrap. With an
explicit --reference-limbo, all fifteen scalar products must match the native
x64 compiler byte for byte. The compiler executor is selected separately with
--limbo-runner; it is not inferred from the VM runner. Native host compiler gates
use strict UBSan; cross-target tools use their explicit O2 flags without UBSan.
No physical ARM64/RV64 execution or native Python-host integration-suite run on
those physical platforms is claimed.

The owning native-limbo gate also passes its72 existing Dis/AOT cases, exact
legacy argument/result offsets and failed-link installation preservation. The
fresh ARM64 classic-mk console passes the existing38-process scalar corpus,
744-image/1667800-predicate C/RX runtime, ownership/GC/field gates and controls.
Those closure tests retain their existing bounded services, separately from
whole-OS process execution. Per-command emitter readiness is unchanged.

## Remaining boundary

This supplies a real source-to-bytecode-to-emulator path for the restricted
profile. Ordinary programs with Sys/Draw imports, a full shell, file CALL/MCALL,
managed arguments/results and generic native calls remain outside admission.
The shared execution_supported=false status and all existing default guards
remain unchanged. Complex runtime handlers stay in C where appropriate.

## Small numeric cases (series 0056)

The private compiler lowering also accepts bounded int cases/ranges and discrete
Big labels, including dense cases represented by compiler GOTO IR. It emits only
the same43 scalar wire operations; file CASE/CASEL/GOTO remain closed. Expanded
PCs and diagnostic rows are remapped without mutating legacy compiler IR. Larger
or managed tables refuse. See [scope and reproduction](../../doc/dis64-scalar-cases.md)
and the self-checking [case example](dis64-scalar/case.b).


Series0060 additionally specializes scalar helper branches, loops and early
returns; see doc/dis64-scalar-helpers.md for bounds and continuation semantics.
The wire profile remains43 operations and does not admit FRAME/CALL or imports.


Series0061 admits bounded numeric case tables inside scalar leaf helpers.
Each call owns its table targets; emitted code remains in the existing43-op
profile. No file FRAME/CALL admission is implied. See dis64-scalar-helpers.md.


Series0062 admits finite acyclic helper-to-helper calls without arguments or
results. Separate depth spans preserve ancestor locals; recursive/entry cycles
and expansion beyond65536 instructions are refused. Wire FRAME/CALL and managed
file calls remain outside the existing43-op profile.


Series0064 admits materialized int/big/real arguments in bounded source helper
specialization. Typed argument copies target the child's private cells; local
initialization skips supplied parameters. File calls, scalar results, managed
arguments and computation within a pending setup remain refused. The wire
profile stays43 operations. See doc/dis64-scalar-helpers.md.


Series0066 admits int/big/real results from bounded acyclic source helpers.
Typed direct scalar writes replace the legacy result-pointer setup. Forwarded
results preserve the original destination across nested helpers. Return/setup
pointers are never emitted into Dis64, and the shared43-op profile is unchanged.
The procedures example now returns fold's Word result through a private case arm.
Managed/byte results, runtime file calls and general native pointer ABI remain
closed; see doc/dis64-scalar-helpers.md for evidence and remaining limits.


Series0068 admits complete flat tuple results with2..8 int/big/real fields and
at most64 bytes. Canonical field alignment, padding and forwarded layout/kinds
are validated before each destination is specialized. The procedures example
now returns a mixed Word/Big/Real tuple. Tuple parameters, managed/nested tuples,
global tuple storage and nil-partial unpacking remain outside admission. Runtime
file calls and the existing43-op wire profile are unchanged.
