# Native 64-bit toolchain profiles

mkfiles now supplies Linux-amd64, Linux-arm64, Linux-riscv64, Android-amd64 and
Android-arm64. amd64 denotes x86-64; arm64 denotes AArch64. RISC-V Linux defaults
to rv64gc/lp64d. CROSS is empty for native builds or a complete tool prefix for
cross compilation. Android uses modern NDK LLVM drivers, API21 by default.
Configure the NDK prebuilt host directory and API in its selected mkfile.

Examples for mkconfig (one assignment per line):

```
SYSHOST=Linux
SYSTARG=Linux
OBJTYPE=amd64
CROSS=
```

For an AArch64 cross build use OBJTYPE=arm64 and CROSS=aarch64-linux-gnu-.
For RISC-V64 use OBJTYPE=riscv64 and CROSS=riscv64-linux-gnu-.
For Android use SYSTARG=Android, OBJTYPE=arm64 or amd64, and ANDROID_NDK_HOME.
Create the selected platform's bin/lib output directories before library builds.

These profiles select compilers, target flags, archive tools, linkers and shared
C declarations. Forced native64-target.h checks reject a mismatched compiler or
non-LP64 data model. Hosted Linux/Android ports now provide machine-specific
assembly, FP contexts, atomics and cache publication. Linux/x64 terminal emu
links as ELF64; Android/ARM64 headless core archives build with NDK r28c.

This is not complete VM startup: installed legacy builtin tables still require
int32/ref32. Startup explicitly refuses them on LP64 before module registration.
The opt-in generated native64 binding is not yet installed for all modules.
Signal-context handling, GUI dependencies and physical-target boot remain
separate acceptance work. See native64-host-port.md for exact tests and limits.

The default scalar model remains legacy. For the opt-in WORD32/ref64 experiment,
set DISMODELFLAGS='-DDIS_WORD32=1' and rebuild every linked runtime object with
matching generated headers. The quotes are required by mk's assignment parser.
A 64-bit C target does not itself enable new Dis64 file/reference admission or
certify the complete generated native module ABI. See dis64-runtime-build.md.

utils/tests/native64-mkfiles.py uses the actual mk parser on all five profiles,
then compiles, links and executes native WORD64/WORD32 probes. The host-port
series also executes real assembly on x64 and ARM64/RV64 under QEMU, and compiles
the actual Android startup/compatibility sources for ARM32, ARM64 and x64 with
an explicitly supplied NDK. These checks do not certify a complete OS boot.

# Android shared runtime tree

The Android runtime root is `/sdcard/Inferno`. Native startup validates that
this existing directory is accessible; it never creates or selects an internal
app directory. INFERNO_ROOT, INFERNO_FILES_DIR and NativeActivity data paths do
not redirect this resolver. Launcher compatibility sets EXTERNAL_STORAGE to
/sdcard only for older bundled amain implementations.

The APK repositories optionally bundle compiled runtime assets and extract
missing files directly into `/sdcard/Inferno`; the option is off by default.
Otherwise populate `/sdcard/Inferno` yourself, including dis/emuinit.dis.
Existing files are preserved and no internal runtime copy is used. If Android denies
shared-storage access, startup fails explicitly. No automatic private fallback
or claim of access on every Android version is made. Android permissions and
storage policy must be handled in the Android launcher repositories.

The previously published private-storage installer and its handoff patch were
withdrawn because they contradicted the required shared-tree location. ABI
selection and GLES fixes remain independent of this correction.

Companion source and build notes:
https://github.com/bhgv/Inferno-OS_Android/blob/master/docs/native-storage.md
https://github.com/bhgv/Inferno-OS_Android-Clutter3d/blob/master/doc/native-gles.md

Host resolver tests check fixed-root selection, denied/missing storage and
output bounds. Startup-option tests check quoting/capacity separately. These
checks do not replace an Android APK/device acceptance test.

## Experimental ARM64 scalar-only emulator startup (INFERNO-0044)

A complete Linux ARM64 console emulator now builds with classic mk and runs
checked scalar Dis64 modules through the real OS, filesystem, pools, scheduler
and ARM64 JIT. This is an explicit builtin-free experiment, not general emuinit
or shell startup. The normal legacy builtin ABI guard remains unchanged.

Prepare a fresh output tree to avoid mixing scalar models:

```
python3 utils/configure.py --host linux --target linux --arch arm64 --console \
  --cross aarch64-linux-gnu- --native64-scalar-boot
mk mkdirs
mk emuinstall
```

Cross builds require host-executable mk, iyacc, decgen and Limbo in the tool
locations used by the existing build. The selected target compiler/archive tools
must supply Linux ARM64 headers and libraries. A native ARM64 build uses an empty
CROSS. The equivalent manual setting is:

```
DISMODELFLAGS='-DDIS_WORD32=1 -DDIS64_PILOT_LOADER=1 -DDIS_NATIVE64_SCALAR_BOOT=1'
```

Rebuild every runtime/library object coherently. The feature rejects missing or
conflicting flags at compilation. It skips all legacy builtin initialization,
rejects `$Sys` and other builtin imports, refuses native plugins and legacy Dis
images, and retains the existing 43-opcode np0 Dis64 admission. No wire IDs,
Frame64/map8 contract, execution_supported flag or SDK default changes.

`-d /dis/pilot.dis64` selects the initial module (`-i` and positional arguments
are not module selectors). `-c 0` selects interpretation; `-c 1` enables JIT.
`-v` reports scalar module termination and whether a JIT was installed.
Linux cleanexit kills its process group; isolate test runs with a new session.
The automated test does this and requires a termination marker, rather than
interpreting SIGKILL alone as success:

```
python3 utils/tests/native64-scalar-boot.py --emu /build/emu/Linux/o.emu-g \
  --root /build --runner qemu-aarch64
```

Verified with a static ELF64 AArch64 executable, Clang/LLD from NDK r28c targeting
Linux GNU, Debian ARM64 glibc 2.36 and QEMU user emulation. Both pilot42 and
pilot-init42 complete with JIT off/on; builtin and legacy image refusals pass.
A complete ordinary Inferno userspace still requires native64 builtin/module ABI
integration. This does not resolve the previously recorded UTF/error-unwind ASan
findings, nor prove execution on physical ARM64 or an Android APK.

Separate target coverage uses actual VM source closures with bounded OS/GC
fixtures, not the complete emulator:

```
python3 utils/tests/arm64-runtime.py --system Linux \
  --cc aarch64-linux-gnu-gcc --runner 'qemu-aarch64 -L /arm64-sysroot'
```

Linux/glibc and Android/Bionic ARM64 both pass 744 complete image inputs and
1,667,800 scalar runtime predicates, plus scheduler, file loader, vmachine and
exception propagation/selection controls. The Android run uses a static target
C executable, not Java or an APK.

Pool geometry now reserves enough bytes for Bhdr's five free-list/tree pointers
and Btail: a 32-byte quantum on ILP32, 64 bytes on LP64. A 32-byte LP64 block could
corrupt its successor when freed; this was reproduced during real emulator boot.
Both hosted and OS pool initializers use the shared mask. The owning hosted pool
gate checks 10,240 allocations, 5,120 reallocations, payload preservation and
bidirectional coalescing on AMD64/UBSan and ARM64/QEMU:

```
python3 utils/tests/pool-native64.py
NATIVE64_ARCH=arm64 CC=aarch64-linux-gnu-gcc \
  NATIVE64_CFLAGS='-O2 -DNDEBUG -static -fno-strict-aliasing' \
  NATIVE64_RUNNER=qemu-aarch64 python3 utils/tests/pool-native64.py
```

Two additional actual build blockers were fixed: plain mk selected an early
assembly-object rule instead of the full Linux/Android target, and a dead pthread
stack probe required an undeclared GNU extension. Private fdlibm core names now
have an Inferno prefix to avoid static glibc libm strong-alias collisions (notably
__ieee754_pow). Rebuild libmath and its consumers together; the 68-export actual
Math registration/invocation gate passes against the rebuilt numeric libraries.

## Trusted Math during scalar startup (INFERNO-0045)

The next explicit startup stage registers the complete fixed-frame Math module
using native64 metadata generated by host Limbo. It is independently selectable:

```
python3 utils/configure.py --host linux --target linux --arch arm64 --console \
  --cross aarch64-linux-gnu- --native64-scalar-boot --native64-math-boot
```

The selected DISMODELFLAGS add `-DDIS_NATIVE64_BOOT_MATH=1` and
`NATIVE64_BOOT_HEADERS=math-native64.h` makes the Math object depend on classic
mk's `limbo -H ../module/math.m` rule. The header is published by a temporary-file
rename and removed by nuke. No generated native64 header is checked in or used
by the default build. Rebuild coherently when switching profiles.

The full Linux ARM64 static emulator and Linux x64 hosted emulator both register
Math and complete pilot42/pilot-init42 with JIT off/on. Test the selected stage:

```
python3 utils/tests/native64-scalar-boot.py --math --emu /build/emu/Linux/o.emu-g \
  --root /build --runner qemu-aarch64
```

For native x64 omit --runner. The test requires a real Math registration marker
and verifies rejection of `$Math`, `$Sys` and legacy images. Math is registered
for trusted C integration only: builtin file imports remain disabled and the
43-opcode np0 file admission does not change. Full shell startup is still pending.
Conflicting explicit Math headers/registration flags and incomplete scalar model
flags fail compilation rather than silently selecting another calling ABI.

The existing builtin, Math registration and Sys drivers now accept
`NATIVE64_ARCH` (default amd64), `NATIVE64_RUNNER` and `NATIVE64_TMPDIR`, while CC
and NATIVE64_CFLAGS continue to select the actual compiler/linker. Host Limbo is
still a separate host executable. For example, with a static Linux cross toolchain:

```
export CC=aarch64-linux-gnu-gcc NATIVE64_ARCH=arm64
export NATIVE64_CFLAGS='-O2 -DNDEBUG -static -fno-strict-aliasing'
export NATIVE64_RUNNER=qemu-aarch64
export NATIVE64_LIMBO=/host/tools/limbo
python3 utils/tests/native64-builtins.py --sets
python3 utils/tests/native64-math-register.py --limbo "$NATIVE64_LIMBO" \
  --libdir /build/Linux/arm64/lib
python3 utils/tests/native64-sys.py --limbo "$NATIVE64_LIMBO"
```

These three ARM64 gates pass: complete Math registration/invocation of all68
exports,141 allocation refusals, atomic module sets and publication/retirement
controls, plus actual Sys I/O/UTF handlers (1,115,061 UTF cases per model and40
injected I/O boundary cases). Their OS transports/allocator fixtures are bounded;
they do not establish file-level Math/Sys imports or complete Sys registration.
The same Math/module gates pass O3 strict UBSan on x64.

Additional owning allocator checks now refuse oversized requests before tracing
padding can wrap or narrow. Rejected realloc retains its original block and
contents. First-allocation realloc writes its malloc tag to the returned owner;
getrealloctag reads the slot when it exists rather than returning the disabled
sentinel. The latter two corrections are mirrored in the OS allocator. Hosted
AMD64/UBSan and ARM64/QEMU tests cover128 oversized API refusals and both tags,
with independent size-guard, new-owner-tag and inverted-read mutation controls.
The existing minimum-block/coalescing checks remain active.

## Trusted Sys I/O startup (INFERNO-0046)

The explicit scalar runtime can additionally initialize native64 Sys descriptors
and register thirteen fixed-frame Sys exports. The set includes byte2char, char2byte,
utfbytes, read, readn, write, pread, pwrite, pctl, file2chan, millisec, open and fildes. These entries
use the trusted C registration path.

```
python3 utils/configure.py --host linux --target linux --arch arm64 --console \
  --cross aarch64-linux-gnu- --native64-scalar-boot --native64-math-boot \
  --native64-sys-io-boot
```

DIS_NATIVE64_BOOT_SYS_IO=1 requires the guarded scalar profile. Classic mk
creates libinterp/sys-native64.h with host Limbo -H before compiling both runt.c
and emu/port/inferno.c. Both owning translation units use the same generated Sys
ADTs, fixed frames and complete maps. NATIVE64_BOOT_SYS_HEADERS stores an absolute
path: mk expands assignments immediately, so a generated assignment before ROOT
must not contain an unresolved $ROOT reference. nuke removes the generated header.
Default and externally selected native64 Sys declarations retain the existing
registration refusal. Explicit conflicting header selectors fail compilation.

Real Linux ARM64 static/QEMU and x64 hosted emulators now initialize Sys, register
Math and the thirteen-entry Sys profile, and finish both pilot modules with JIT off
and on. Require the selected startup markers and continued closed admission:

```
python3 utils/tests/native64-scalar-boot.py --math --sys-io \
  --emu /build/emu/Linux/o.emu-g --root /build --runner qemu-aarch64
```

Ordinary file admission still rejects builtin modules and legacy images.
The explicit clock pilot accepts one checked scalar import and bounded
call sequences. An optional initial LOAD binds that import at execution
time from a managed module-name cell to a mapped module-result cell. Run a pilot image with `emu -c 1 -d /clock.dis64 -P '$Sys'`
and repeat with `-c 0` for the interpreter. The paired boot test with
`--sys-clock` generates the image and checks both modes. The pilot
reserves `$Sys` as an explicit provider selector and shares the
existing indirect LEA encoding with the local result binder.

Additional gates separate registration ownership from actual callback behavior:

```
python3 utils/tests/native64-sys-register.py --limbo /host/tools/limbo
python3 utils/tests/native64-sys-init.py --limbo /host/tools/limbo
python3 utils/tests/native64-sys.py --limbo /host/tools/limbo
```

All support the same NATIVE64_ARCH/CC/NATIVE64_CFLAGS/NATIVE64_RUNNER environment.
The registration gate compiles both complete owning production units, executes
the actual registration and constructor bodies, validates the thirteen published
names, types and maps, fails all 31 constructor allocations individually, rejects
descriptor initialization before allocation, and retries. It dispatches the
production millisec handler through the registered export using a deterministic
host clock and rejects invalid result cells before reading the clock. The other
callbacks are bounded identity probes. Separate Sys handler and initializer
gates cover their implementation behavior. Registration
failure does not retire already published canonical Sys descriptors; sysinit's
idempotent retry contract owns those process-lifetime descriptors.

ARM64 execution additionally passes actual emu/OS sysinit descriptor construction
in normal/wide/insufficient-map modes with allocation refusals, retries and partial
state rejection. The earlier actual I/O/UTF source gates remain active; x64 strict
UBSan also verifies unchanged default native64 registration/variadic refusals.
This is not closure of previously recorded optimized UTF/error-unwind ASan work.

## First structural fixed-call experiment (INFERNO-0047)

Following FLASHVM-0044, an explicit private `Math_bits64real64` C gateway now
matches the existing Rune pool/import codecs and exact v1 Real(Big) signature.
The shared proposal bytes are in libinterp/tests/dis64/fixed-math. No new IDs,
loader profile, file CALL/MCALL or signature hashes are introduced.

Compile owning Math with DIS_NATIVE64_FIXED_MATH_CALL=1, DIS_WORD32=1 and the
explicit native64 Math registration header/options. The startup Math profile can
supply those generated declarations. The fixed-call experiment is off by default;
without it the C gateway declines before reading even its request. Incorrect
feature values, missing native64 registration metadata and wrong scalar models
fail compilation. The flag is not added by configure.py or ordinary builds.

The synchronous request supplies complete readable immutable strings/imports,
an ordinal, input bits and a trusted logical result resolver. Preflight checks
all canonical imports, names and exact signature before allocating. Request and
input are snapshotted; serialized metadata is not reread after allocation starts.
Runtime callback identity, hash consistency and generated complete frame maps are
checked separately. The legacy hash never substitutes for structural matching.

The gateway retains Module/Type, allocates a raw owned72-byte scalar Frame64,
points its result at private aligned storage and calls the actual registered
implementation. The zero-map frame and scalar result are not GC heap objects;
there are no managed argument/result owners in this first profile. The resolver
runs after the producer and may relocate, cancel or raise. It is responsible for
current live eight-byte caller-cell provenance and caller rooting. Publication
uses a byte-safe copy with no intervening callback. Frame, Type and module retire
once, preserving diagnostics and the old logical result on refusal/unwind.
This contract does not validate arbitrary VM caller layouts or reference graphs.

```
python3 utils/tests/native64-fixed-math.py --limbo /host/tools/limbo \
  --libdir /build/Linux/arm64/lib
```

Use the previously documented NATIVE64_ARCH/CC/NATIVE64_CFLAGS/NATIVE64_RUNNER
variables for ARM64. Actual ARM64/QEMU and x64 strict-UBSan gates each pass12,672
calls per WORD32/WORD32-file model, after the full68-export Math registration/
invocation and141 construction-refusal gate. Coverage includes all eight result
alignments, finite/infinite/NaN bit patterns, late/moved cells, cancellation,
resolver errors, module retirement, owned request snapshot, ordinal aliases,
wrong canonical signatures and runtime metadata, all fixture byte mutations and
truncations, and temporary-frame allocation refusal.

`--inject-callee` compiles a copied actual Math producer with an error after its
private scalar store, and checks pre-publication cleanup. Four separate
`--control signature|module|type|frame` gates remove contract steps and must fail
the active oracle. Incoherent-selection and disabled pre-read refusal gates also
pass on both targets. The complete ARM64 emulator links this optional gateway
with actual runtime ownership and still passes both pilot programs with JIT
off/on and closed builtin/legacy admission; that boot test does not invoke the
new C gateway from a file. Prior optimized UTF/unwind ASan findings stay open.

## Typed current-frame result adapter (INFERNO-0048)

The private fixed Math call now has `Math_bits64realframe64`, taking a rooted
Prog, its exact retained generated frame Type and an aligned eight-byte scalar
field offset. The caller must publish its live REG into Prog before entry and
serialize stack access throughout the synchronous call. This is an explicit C
contract; ordinary xec global-R publication is not changed by this series.

The shared native64framecell helper bounds the current stack extent by EX/SP/TS,
walks every complete frame boundary, handles the real Stkext.TR fallback, validates
full map8 and the exact target Type, and refuses reserved header/temp bytes,
reference slots, interior addresses, wrong Types and incomplete extents. Only the
active extent is accepted; suspended/older segments remain outside the profile.
A GC map proves a non-reference eight-byte span, not the semantic difference
between Big, Real and padding. Exact scalar kind and declared field offset still
come from trusted compiler metadata. No arbitrary native pointer provenance is
inferred, and no file-native call profile is enabled.

The adapter validates once before dispatch and resolves again after the actual
producer. It retains no physical result pointer across producer allocation or
collection. The existing structural signature, module/Type ownership and
protected cleanup in Math_bits64real64 remain in force. Both APIs still decline
before reading requests when their explicit fixed-call experiment is disabled.

Two complementary owning gates now run on x64 strict UBSan and ARM64/QEMU:

```
python3 utils/tests/native64-frame-cell.py --limbo /host/tools/limbo
python3 utils/tests/native64-frame-cell.py --gateway --limbo /host/tools/limbo
```

Use the existing cross compiler/runner environment for ARM64. Each compatible
WORD32/WORD32-file model runs48 standalone GC transactions and96 combined fixed
call transactions across all process states/colors. Generated caller frames have
a Real result at72 and an independent managed array at80. Actual heap/collector
bodies keep those neighboring owners alive through repeated epochs and release
them through real cleanup exactly once. Standalone cases include the extent's
fallback Type, live-frame relocation and scalar/reference/Type/bounds refusals.

The combined gate executes actual structural import matching, typed frame adapter,
Math producer, emu kprotect/error bodies and unload reference decrement. Its
registry bootstrap and OS pool/error-label services are explicitly bounded;
registration itself is covered separately by the68-export gate. A test-only hook
in a copied owning Math body forces real collection after its private result
store and changes the logical current frame before resolution. Allocation/error
cases preserve both old caller cells and balance module/Type/raw-frame ownership.
This is a synchronous callback/safepoint simulation, not file-bytecode execution
or a claim about every normal scheduler transition.

```
python3 utils/tests/native64-frame-cell.py --control scalar --limbo /host/tools/limbo
python3 utils/tests/native64-frame-cell.py --control identity --limbo /host/tools/limbo
python3 utils/tests/native64-frame-cell.py --control boundary --limbo /host/tools/limbo
python3 utils/tests/native64-frame-cell.py --control map --limbo /host/tools/limbo
python3 utils/tests/native64-frame-cell.py --gateway --control map --limbo /host/tools/limbo
```

All removals must fail active predicates: reference-cell checks, Type identity,
frame-boundary membership, or the sole independent managed roots. The existing
fixed import gate also exercises the new adapter with stable/moved frames and
post-production invalid boundaries. The full static ARM64 emulator is rebuilt
through mk with the new adapter and retains its pilot JIT-off/on and builtin/
legacy refusal tests. Shared43-op np0 execution, defaults and the recorded
optimized UTF/error-unwind ASan limitations remain unchanged.

FLASHVM-0047's next proposal additionally requires process/frame generation and
an owned caller-lifetime capability. The current exact-address/Type/boundary
resolver cannot distinguish reuse at the same address with the same Type and
does not acquire a Prog lifetime token. Its caller-rooting contract is therefore
still trusted; these missing stale-reuse/lifetime protections must be addressed
before file-native call admission. No such admission is implied by this series.

## Synchronous current-caller generation lease (INFERNO-0049)

The explicit private profile DIS_NATIVE64_CALLER_GUARD=1 requires WORD32 and the
fixed Math experiment. It adds a noncopyable Native64Caller lease and the
Math_bits64realguard64 adapter. This is not a generic Prog reference count:
acquisition is restricted to the current scheduler head in Pready, with no kill
pending and exactly one lease. Existing scheduler ownership remains in place;
the lease blocks its normal destructive paths and VM release until clear.
Blocked, Prelease, exiting, suspended and noncurrent processes are refused.
This avoids deferring a blocked-process kill without a proven wake/replay path.

Prog now has private native64epoch/native64holds fields. Rebuild all linked
runtime objects coherently even when the guard is off; neither field enters Dis
or Frame64 wire data. New processes explicitly initialize them. With the profile
selected, interpreter entry/steps and normal stack extent mutations advance a
saturating64-bit invalidation epoch. delprog/progexit refuse held teardown before
retirement, and release refuses to yield VM ownership. These attempted actions
also invalidate the pending lease. killprog records cancellation and invalidates
without destroying the held current process; normal VM resumption consumes the
pending kill after the synchronous adapter releases its hold.

Acquire before allocation/callbacks, retain the exact caller Type and record
process PID/epoch/frame/extent identity. Resolve checks current scheduler identity,
Pready/cancellation, generation and frame identity before reading stack storage.
Copied lease records cannot resolve or release the original. Counter saturation
refuses future acquisition; generations never wrap. A live lease can explicitly
transfer its result to another validated frame in the same retained active
extent. Only FP changes; module, PC, SP/TS/EX and unrelated REG fields are not
replaced. An invalid/stale lease cannot rebind itself into validity. Clear detaches
its identity first, drops the process hold and releases exactly one Type owner;
repeated clear is harmless. State storage must remain live, noncopyable and
disjoint from Prog/Type until retirement. Access is serialized by the VM owner.

Ordinary global-R/Prog publication is still a trusted caller obligation. This
profile does not acquire an independent GC root for a detached Prog, support
VM release/blocking or prove arbitrary scheduler-list detachment/replay. It
protects normal runtime paths for a synchronous current caller; native code
bypassing those paths is outside the contract. File CALL/MCALL, older extents,
managed argument/result graphs and execution_supported remain closed.

```
python3 utils/tests/native64-fixed-math.py --caller-guard \
  --limbo /host/tools/limbo --libdir /build/Linux/arm64/lib
python3 utils/tests/native64-fixed-math.py --caller-guard --caller-port os \
  --limbo /host/tools/limbo --libdir /build/Linux/arm64/lib
python3 utils/tests/native64-frame-cell.py --gateway --lease --limbo /host/tools/limbo
```

Use the existing cross environment for ARM64. Owning x64 strict-UBSan and
ARM64/QEMU tests retain the12672-case/68-export/141-refusal Math gates and add
lease identity/copy/Type balance, explicit transfer, same-address/same-Type stale
reuse, poisoned retired extent, PID/current/state/cancellation mismatches,
saturation, nested refusal and protected producer errors. The emu/OS lifecycle
checks execute their exact early guard blocks with bounded scheduler/error
services; they are not complete process-teardown reproductions. The full ordinary
scheduler gate separately retains actual newprog/schedmod/progexit/delprog paths.

The acquired-lease GC gate has96 transactions per model:12 current-Pready protected
call cases and84 noncurrent-state preflight refusals. Real heap/Math/collector and
port protection execute, with collection after the callee's private scalar store,
explicit checked FP transfer or same-address epoch invalidation, unchanged old
results on cancellation/error and balanced Module/Type/raw-frame/lease ownership.
The scheduler identity/bootstrap and OS pools remain bounded. This does not prove
an independent last-owner detached-Prog lifetime. Five compiled epoch/hold/Type/
retirement/self controls and the GC map-removal control must reach active failures.
`--inject-callee` additionally checks error after private result production.

Full classic-mk ARM64 and x64 emulators build with the coherent opt-in flags and
retain their two pilot JIT-off/on cases and closed builtin/legacy admission.
No native-fast opcode counts or shared file defaults change. Earlier optimized
UTF/injected-unwind ASan findings are not closed by these UBSan/target tests.

Guard-profile coherence is also checked at link time. The scheduler marker calls
owning VM/stack markers, and acquisition requires that runtime marker. Mixing
Guard-enabled consumers with an unguarded scheduler, xec or stack unit fails
linkage. This does not replace coherent Prog-layout rebuilds or a VM lock.
The actual ARM64 classic-mk link was reproduced successfully, then repeated with
each real owning unit recompiled without the guard: all three partial variants
fail their intended missing symbol. Original objects/archives are preserved.

```
python3 utils/tests/native64-guard-link.py --root /build \
  --log /logs/coherent-mk-build.log --ar aarch64-linux-gnu-ar
```

The build log must include full owning xec.c/stack.c/dis.c compile commands and
the final o.emu-g link command. The driver uses those exact toolchain options;
CC/archive tools must be available on PATH. Old unleased C APIs retain their
previous trusted contracts; selecting this profile does not turn every native
call into an acquired lease automatically.

## Clean native64 build and execution reproduction

`utils/configure.py --native64-caller-guard` now selects the fixed Math experiment
and caller guard together, for every owning unit. It requires explicit scalar
and Math startup selections and the Linux 64-bit console profile. Sys I/O remains
optional. Invalid combinations fail before replacing an existing configuration.
The model flags are composed once, so selecting Sys cannot discard the guard.
Changing profiles still requires a complete runtime rebuild.

```
python3 utils/configure.py --host linux --arch arm64 --console \
  --native64-scalar-boot --native64-math-boot --native64-sys-io-boot \
  --native64-caller-guard
```

For repeatable target testing, the new driver copies tracked source inputs,
including their current working edits, into an empty external build directory.
It does not copy untracked build products or alter the source mkconfig. Prepare
native host mk/limbo/iyacc/decgen using makemk.sh first. Then run:

```
python3 utils/tests/native64-port.py \
  --build-root /work/inferno-arm64-check \
  --host-tools /work/host-tools --arch arm64 \
  --cross aarch64-linux-gnu- \
  --runner 'qemu-aarch64 -L /usr/aarch64-linux-gnu'
```

The driver prepares mkconfig, invokes classic mk in emu/Linux, and preserves each
stage log under port-logs. It runs both scalar pilots JIT-off/on, closed admission,
actual partial-guard relinks, incremental dependency checks, both emu/OS lease
fixtures, post-callee unwind, actual GC/lease publication and its active map
removal control. ARM64 additionally runs all744 independent images and actual
scheduler/file/vmachine source closures plus exception controls. The OS lifecycle
lease checks remain bounded guard fixtures, not full kernel boot.

Use `--arch amd64` for native host reproduction; `--arch riscv64` selects that
build profile but this series does not claim a new RV64 execution run. The target
C gate default is O3/NDEBUG without sanitizers. For native x64 add:

```
--test-cflags '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'
```

`--test-cc` overrides the C fixture compiler command independently of the classic
mk CROSS prefix. This matters when the emulator is linked statically: GNU static
glibc's internal free calls can enter the fixture's --wrap=free interposer and
correctly trip its unknown-allocation oracle before bytecode execution. Use a
dynamic glibc compiler command and matching QEMU sysroot for these fixtures;
do not weaken the tracked-allocation predicate or count that failure as a VM
instruction regression. Android keeps its existing free_ boundary. Production
libc/allocator code is unchanged.

The actual stack/xec/dis objects now depend explicitly on the complete inline
caller/frame-cell/model/layout contract. Previously a frame-cell-only edit could
leave stale interpreter/stack/scheduler guards while Math was rebuilt. The
new dry-run gate first requires no pending owner compilations, advances each
header timestamp, requires all three owners to compile, then restores timestamps
without changing header bytes. Removing the frame-cell dependency reaches the
active failure. The OS scheduler receives the same explicit source dependencies;
no OS kernel build is claimed by the hosted Linux gate.

This reproduction is scalar boot plus private C/GC evidence. It does not connect
the private gateway to file CALL/MCALL or admit a complete Limbo shell and managed
native call graphs. Live-R publication is limited to the explicit private entry
described below; other C entry points retain their own contracts. The shared43-op
np0 file profile and execution_supported=false remain unchanged.

## Live interpreter register publication (private fixed scalar call)

The caller-guard profile now binds Prog.native64live to global R only after
vmstart restores the current process. xec refuses recursive entry before touching
R. Its ordinary return publishes saved registers and retires the live binding;
both port vmachine error paths retire it before handler/propex/progexit. Retirement
also publishes R when that process still owns it. The field is private, not part
of Dis64, and requires a coherent rebuild of every Prog consumer.

Math_bits64realvm64 is a new explicit C entry for the existing Big->Real private
experiment. Unlike the older published-frame adapter it requires current Pready
scheduler identity and an active &R binding. It validates a candidate live frame,
publishes its REG without allocation, then acquires the existing generation/Type/
scheduler hold before producer allocation. Rejected acquisition restores the old
saved REG. No arbitrary REG pointer is accepted as a live interpreter owner.

The lease captures whether it was acquired from a live binding. Late resolution
checks that binding still matches, together with current/saved FP, EX, SP, TS,
M and MP, before reading stack cells. Generation, PID, cancellation, Type and
map checks remain active. Checked within-extent rebind updates both saved FP and
live R.FP. Ordinary transfer through other VM stack operations still invalidates
the old generation; this does not authorize general frame relocation.

The existing Math_bits64realguard64 retains its explicitly published-frame
contract outside live VM execution. Selecting the profile does not automatically
convert all native operations into the new entry. A disabled vm64 entry refuses
before reading a request, just like the other private adapters.

```
python3 utils/tests/native64-fixed-math.py --caller-guard \
  --limbo /host/tools/limbo --libdir /build/Linux/arm64/lib
python3 utils/tests/native64-frame-cell.py --gateway --lease --live \
  --limbo /host/tools/limbo
python3 utils/tests/native64-live-vm.py
```

Use CC/NATIVE64_ARCH/NATIVE64_RUNNER for target fixtures as in the clean driver.
The new vmachine gate instruments actual xec/scheduler/exception bodies and
checks entry, ordinary quantum return and exception retirement in direct and
file-loader variants, with C and RX execution. It retains the existing bounded
OS/GC services. Its --control enter/unwind variants must fail active ownership
predicates. The owning Math gate adds stale saved-register publication, changed
live frame/extent/bounds/module cancellation, forged owner refusal, generation
exhaustion rollback, recursive-entry error and synchronized FP transfer. Compiled
--caller-control live/publish/rebind variants must fail their intended predicates.

The combined --live GC gate performs96 transactions per model:12 current-ready
acquired cases and84 nonrunning-state refusals. Saved FP/EX are deliberately
poisoned before acquisition in admitted cases; real collector/heap/Math run only
after the live snapshot has been validated and published. Its map-removal control
must still fail. Forced collections before allocation and after the private
callee result store retain adjacent managed roots and balance them on retirement.

This closes one publication step, not the entire calling ABI. The binding is
owned by trusted serialized interpreter execution, and synchronous release stays
forbidden while held. Caller/extent descriptors are still trusted VM storage.
Independent detached-Prog lifetime, full managed call graphs and file CALL/MCALL
remain open. The default43-op np0 Dis64 file profile and wire IDs are unchanged.
Earlier optimized UTF/error-unwind ASan findings remain open.

## Compiler field witnesses

The next opt-in C adapter is Math_bits64realfield64. It binds a compiler-declared
Real field by source ordinal to the exact current caller Module/Type/callable,
retains that Module and acquires the existing caller lease before protected
production. Big, padding and a matching legacy hash do not substitute for a Real
field. Limbo -H with LIMBO_NATIVE64_FIELDS supplies the private metadata; the
existing entry layout, wire profile and other adapter contracts are unchanged.
See [field witness scope and reproduction](dis64-native64-field-witness.md).
The clean port driver includes the standalone field gate and its four controls.

## Full scalar console regression corpus (series 0053)

The clean driver now runs self-checking arithmetic, moves, branches, long loops
and terminal errors in full Linux OS processes on ARM64, x64 and RV64. The scalar
console retires erroneous programs instead of retaining an inaccessible debugger
session; ordinary Inferno keeps its existing broken-program default. The final
verbose marker is emitted after real program retirement. See
[native64-series0053.md](native64-series0053.md) for precise scope, target evidence,
reproduction and unchanged JIT/file-admission statistics.

## Compile scalar Limbo source to Dis64 (series 0054)

Rebuild native tools from current sources. `limbo -64` emits the existing43-op np0
profile for one pointer-free void init() with int/big/real storage. The classic
limbo mkfile includes the new backend and its codec dependencies; makemk.sh also
builds it through its ordinary source list. The clean port driver now checks real
source compilation and30 whole-OS interpreter/JIT processes in addition to the
hand-encoded corpus. See [compiler scope and example](../limbo/tests/README.dis64-scalar.md).

The native bootstrap now recognizes Linux riscv64 and checks that the selected
compiler actually targets RV64. An optional MKBOOTSTRAP_RUNNER names one executable
that runs freshly built host tools, principally iyacc. With no runner, execution
is unchanged. For an explicit cross-host bootstrap, select MKHOSTARCH, CC and a
matching runner; this never disables the compiler architecture guard. A QEMU
executable can take its sysroot through QEMU_LD_PREFIX, or the selected executable
can be a wrapper supplying its arguments. MKBOOTSTRAP_RUNNER is one executable
path/name, not a shell expression or a whitespace-separated command string.

For example, to build tools for an ARM64 Linux host on an x64 build machine:

```
MKHOSTARCH=arm64 CC=/tools/aarch64-linux-gnu-gcc \
  MKBOOTSTRAP_RUNNER=/tools/qemu-aarch64 \
  QEMU_LD_PREFIX=/sysroot/aarch64-linux-gnu \
  MKBOOTSTRAP_LIMBO=1 MKBOOTSTRAP_BINDIR=/build/tools-arm64 sh makemk.sh
```

Use riscv64, the matching GNU compiler/runner and lp64d sysroot for the tested
RV64GC toolchain. Native ARM64/RV64 hosts need no QEMU runner. Target bootstrap
and compiler execution were verified under QEMU, including identical scalar
file bytes across x64, ARM64 and RV64 compilers. This is not physical-hardware
acceptance or automatic binfmt setup for arbitrary child programs.

## Source-level fault diagnosis and optimized UTF gates (series 0055)

The scalar console reports a checked decoded Dis-PC for a terminal error when
verbose mode is selected. `utils/dis64-locations.py` binds the successful scalar
compiler's optional trace to the exact image and resolves that PC offline. This
is private diagnostic tooling, not a shared debug section, new executable profile,
source-variable witness or expanded shell/import ABI. See
[the source-location proposal](dis64-source-locations.md).

The clean port driver includes ten complete C/JIT executions checking word/big
arithmetic failures, invalid shifts and clean retirement, with exact source-line
resolution. It also executes the UTF C oracle with always-active predicates and
wrong-count semantic controls. Older Sys UTF `assert` comparisons were disabled
under NDEBUG; the new gate corrects that evidence limitation. The complete ASan
error-unwind report remains open.

The native Linux ARM64 CI job now passes, alongside native x64 and QEMU RV64:
[run41334049753](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41334049753).
Downloadable experimental consoles, example images/maps, reproduction and exact
scope are listed in [series0055](native64-series0055.md#native-linux-arm64-ci-acceptance).
The unpacked example runs with `./emu -v -r "$PWD" -c 1 -d /loop.dis64`; use
`-c 0` for interpreter execution. These are Linux console executables, not APKs.
The full O3/NDEBUG UTF ASan+UBSan oracle additionally passes on native Linux
ARM64/x64 in run41334449491; the older local sandbox report remains separately
reproducible rather than being relabeled a proven false positive.

## Small numeric source cases (series 0056)

The existing scalar backend accepts bounded numeric case IR and lowers it to
ordinary scalar comparisons/jumps. This includes int ranges and discrete Big
labels, dense int dispatch, nested cases and loops. CASE/CASEL/GOTO themselves
remain outside file admission. No shared wire IDs or managed-call contract change.
The clean port driver runs67 sources through134 additional complete interpreter/
JIT processes and checks remapped targets/source rows; CI also consumes all67
products through the unchanged FlashVM reader. See
[scope, limits and reproduction](dis64-scalar-cases.md). Console archives now
also contain the self-checking Native64Case source, image and diagnostic map.

## Private local-call preflights (series0073–0075)

The clean port driver now also runs three separate private gates after classic
mk build. These do not enable file calls:

| Gate | Checked boundary | Independent cases | Removal controls |
|---|---|---:|---:|
| `dis64-local-catalog.py` | Raw metadata and caller/callee geometry |628|2|
| `dis64-local-flow.py` | Abstract function ownership and bounded call graph |1540|5|
| `dis64-local-state.py` | Abstract preparation, arguments, typed result and joins |98032|13|

For a focused host run from the repository root:

```
python3 utils/tests/dis64-local-catalog.py
python3 utils/tests/dis64-local-flow.py
python3 utils/tests/dis64-local-state.py
```

The state gate defaults to optimized nonrecovering UBSan. As with the other
portable C gates, `CC`, `NATIVE64_CFLAGS`, `NATIVE64_RUNNER` and
`NATIVE64_TMPDIR` select compiler, flags, target executor and scratch directory.
The full native64-port matrix runs all semantic removal controls as well as the
normal oracles. Look for `local-state.log` and `local-state-*.log` in the console
artifact's complete gate logs.

The state model consumes trusted semantic facts, not wire instructions. The
catalog/control/state gates still need decoded-operand/CFG composition, numeric
local definite assignment, setup-interior ownership and callee result guarantees,
followed by whole-file C/RX positive/refusal goldens. FLASHVM-0063 accepts the
initial subset design but opens no loader path or registry IDs. See
[state scope](dis64-local-state.md) and [agreed subset](dis64-local-call-proposal.md).

## Entry-seeded semantic CFG (series0076)

`python3 utils/tests/dis64-local-cfg.py` checks4005 semantic graphs against an
independent DFS oracle. Native64-port additionally runs eight semantic removal
controls and preserves `local-cfg.log` plus `local-cfg-*.log`. Cross-target compiler,
runner, flags and temporary-directory variables are the same as for the other
portable preflights. The Python fixture imports `dis64-local-state.py`; copy both
when replaying selected sources.

The checker resets scratch, seeds zero entry state and requires exact joins;
unknown/unreachable malformed events and setup-interior targets refuse. Its
work budget counts processed nodes and edge comparisons. Numeric source/span
proofs and actual decoded file operands remain separate. See
[precise CFG scope](dis64-local-cfg.md) and [series0076](native64-series0076.md).

## Actual typed scalar operands (series0077)

`python3 utils/tests/dis64-local-operands.py` decodes actual scalar code bytes
against an independent Python parser/type-role model:17544 payload/layout cases
plus direct invalid-width/wide-offset probes. The complete port driver also
runs12 semantic removal controls, including an extra-record publication probe. Logs are `local-operands.log` and
`local-operands-*.log`; compiler/runner/flags/scratch variables match other C gates.

This remains a private adapter under trusted local declarations. It derives
read/write roles but not numeric initialization or whole callable admission.
The shared slot-helper width/narrowing repair is exercised in the same fixture.
See [scope](dis64-local-operands.md) and [series0077](native64-series0077.md).

## Numeric definite initialization (series0078)

`python3 utils/tests/dis64-local-initialized.py` compares actual scalar CFGs and
incoming signatures against a synchronous fixed-point set oracle:2029 cases,
with13 semantic removal controls in native64-port. It imports the existing raw
operand oracle. Compiler/runner/flags/scratch variables match other C gates.
Logs are `local-initialized.log` and `local-initialized-*.log`.

Canonical incoming arguments seed exact prefix slots; locals do not initialize
by declaration. Numeric joins intersect, losses requeue nodes, and all reads
precede writes. Deterministic information-budget and hard visit/queue bounds
refuse transactionally. Context/catalog binding and callee return guarantees
remain separate. See [scope](dis64-local-initialized.md) and
[series0078](native64-series0078.md).

## Full Android ARM64 GUI native build

The ordinary framebuffer Draw/Tk graph can now be compiled and linked with a
modern NDK. See [the full GUI build reproduction](android-arm64-gui-build.md).
This is separate from scalar APK probes; AWM startup remains blocked by native
builtin ABI and general managed compiler/runtime admission.
