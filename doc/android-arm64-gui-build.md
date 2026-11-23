# Android ARM64 full GUI native build

The ordinary Android framebuffer GUI now compiles and links with NDK r27c as
ELF64/AArch64 using WORD32/ref64. This is the complete Draw/Tk/FreeType/audio
NativeActivity graph, rather than the separate builtin-free scalar APK probe.
It is not yet a usable AWM APK: normal startup still rejects legacy builtin
registration on LP64, and general managed Dis64 compiler/loader admission remains
unfinished. No ABI guard has been disabled.

Use core `experiment` and companion `Inferno-OS_Android` branch
`experiment-arm64-gui`, commit `ed8b80ecb64524c86ca1bd2b21e1abbfb494473f`.
Bootstrap host tools with `makemk.sh`, then run:

```sh
python3 utils/tests/android-gui-build.py \
  --ndk /path/to/android-ndk-r27c \
  --host-tools /path/to/host-tools \
  --app-root /path/to/Inferno-OS_Android \
  --build-root /path/to/empty-output
```

The output must be outside both source checkouts and initially empty. The driver
copies current tracked inputs, selects one coherent WORD32 model for every core
object, builds `emu` including GUI devices, builds the remaining application
libraries and links the actual NativeActivity. CMake/Ninja and the modern NDK are
required. Flexible 16KiB page support is selected for the final shared library.
`logs/`, ELF symbol/segment inspection and `result.json` preserve build evidence.
The result explicitly records `gui_native_linked: true` and `awm_started: false`.
It does not package or label this blocked runtime as a working desktop APK.

Fixed real build failures include the audio callback's `int*` declaration,
FreeType inheriting the emulator's header prerequisites, the missing
`attachtowindow` prototype, and duplicate Tk/font/compiler-mode global owners.
The companion frontend uses the supported `ALooper_pollOnce` API and correctly
typed sample-buffer pointers. Android defaults remain stylus input and interpreter
mode, initialized by `amain` before option parsing.

NDK r27c builds actual startup/compatibility objects for ARM32, ARM64 and x64.
Framebuffer lifecycle, startup allocation, pixel layout and three-way CMake ABI
selection tests pass. The native Screen-channel test passes 7245 checks across
188 cases. Native linking does not replace executing AWM on Android or checking
its window, input, storage permissions and JIT policy on a device.

The next startup work is coherent native64 C declarations, module registration,
frame/maps and general managed compiler/runtime support for the AWM dependency
closure. A scalar module's success or a linked GUI library is insufficient proof.

The general compiler now has a type-driven Word32/ref64 storage planner for all
376 FP/MP banks across the sixteen startup sources. Parameters, nested records,
reused locals and compiler-table address roles are checked against actual legacy
Dis descriptors. This is an intermediate compiler foundation: general operand
lowering, result binding, payload descriptors and initializer/table writers are
still needed before managed loader admission and AWM execution. See the
[typed-layout guide](dis64-typed-layouts.md) and its
[Russian version](dis64-typed-layouts.ru.md). Run
`python3 utils/tests/dis64-layout.py --limbo /path/to/limbo` to reproduce the
independent layout and unchanged-bytecode checks.

The private native Sys argument producer now consumes these typed FP/MP source
layouts, including nested initializer fields and projections from direct
aggregate copies. Actual VM-handler replay and the four-gateway call matrix pass
on AMD64 and ARM64/QEMU. See [typed source lowering](dis64-typed-source-lowering.md).
This does not yet admit general managed AWM modules or ordinary variadic startup.

Complete supported void bodies now lower through the typed layouts, including
managed parameters, loops and early returns. Twenty body executions per port/model
combination pass actual VM handlers with GC on AMD64 and ARM64/QEMU. In the actual
startup closure, three of 360 bodies lower completely and 357 still refuse.
[Complete typed bodies](dis64-typed-bodies.md) documents this intermediate step;
value-result/call binding and general module writing remain before AWM execution.

## Native64 Tk registration step

Add `--native64-tk --native64-draw` to compile the whole GUI graph with generated Frame64
Tk declarations and its real `builtinmod64` registration path. Classic mk
regenerates `tk-native64.h` from the actual Sys/Draw/Tk `.m` dependency closure;
only the Tk call table is emitted. The `TkTop` public prefix is checked against
the generated `Tk_Toplevel` field offsets and extent at compile time.

The new initializer stages its descriptor before registration, publishes no
partial top on failure, preserves the registration diagnostic and installs the
formatter/widget table only after success. Repeated initialization is
idempotent. Header-only selection and an implicit WORD model remain refused.
The legacy initializer retains its existing order.

`python3 utils/tests/native64-tk-register.py --limbo /path/to/limbo` compiles
all actual Tk callback bodies and exercises the real module/descriptor
constructor and initializer with controlled allocation/error boundaries. It
checks all 10 generated links, maps, callback identities, the public-prefix
geometry, idempotence, 26 allocation refusals and 378 staging observations.
The GUI callback witnesses abort if called: this registration test deliberately
does not claim GUI operation. Host UBSan and ARM64/Bionic QEMU runs pass;
portable compiler/runner options use `CC`, `NATIVE64_ARCH`, `NATIVE64_CFLAGS`
and `NATIVE64_RUNNER`, as with the other native64 gates.

The full GUI shared library also links with this selection and retains 16KiB
LOAD alignment. `result.json` records `tk_native64_selected: true` while
`awm_started` remains false. Ordinary Android startup still refuses the mixed
legacy builtin set. The full Sys variadic/import contract and general
managed Dis64 compilation remain startup work; this flag does not bypass that
boundary or produce a working AWM APK.

## Coherent Draw, geometry and devgui frames

The Draw owner now registers all 89 fixed exports through generated native64
metadata, retaining staging and rollback for its four native wrappers. Geometry
methods in `geom.c` select the same Draw declarations. The `devgui` C caller
uses one generated Sys/Draw/Tk declaration set for its direct API calls, instead
of silently constructing old frames for new callbacks. The GUI driver requires
Draw and Tk together; either flag alone is refused before creating output.

`native64-draw-register.py` compiles both complete owners and validates every
registered name/hash/frame/map/callback, four wrapper descriptors, idempotence,
187 allocation refusals and 17766 staging observations. It links the actual
`geom.c` and invokes 301 published geometry callbacks, checking modular
arithmetic, zero-divide without result publication, unaligned and aliased Point
results, and the tuple returned by Rect.clip. Other window/device callbacks
are identity witnesses that abort if executed; no window presentation is claimed.

The complete Android NativeActivity links with both new GUI interfaces and
16KiB LOAD alignment. Host UBSan, ARM64, RV64 and ARM64/Bionic registration
checks are separate from running AWM bytecode. `result.json` records both
selected interfaces and still records `awm_started: false`. Ordinary startup
continues to reject the remaining legacy module set. This step changes trusted
C metadata and consumers only; it assigns no new Dis64 wire IDs or import ABI.

## Complete fixed-argument Sys set

Use `--native64-tk --native64-draw --native64-sys-fixed` to build the full GUI
native graph with all 39 fixed Sys exports, beyond the separate 23-entry console
pilot. Both Sys C owners and the service/channel consumer select the same fresh
`sys-native64.h`. The normal Android startup ABI guard remains active.

The Sys initializer allocates all five descriptors before asking the real
`builtinmod64` constructor to publish the complete module. Descriptor globals
and channel maps are published only after registration succeeds, with no later
allocation. Failed registration retires the staged descriptors while preserving
the diagnostic. Repeated initialization verifies the complete module and its
maps; incompatible preexisting modules or partial descriptor sets are refused
without claiming their owners. The original legacy and console pilot paths are
unchanged.

`native64-sys-fixed.py` compiles both complete Sys owners, tests all 39 generated
links/frames/maps and checks the actual descriptor/module initializers. It covers
88 allocation refusals, 32 preexisting-state refusals and 4005 staging observations.
Sixty calls through published links execute the production Unicode handlers,
including tuple/unaligned results, invalid runes and bounds-error nonpublication.
Other callbacks are aborting identity witnesses, so this fixture does not claim
OS I/O or GUI execution. Host UBSan, ARM64, RV64 and ARM64/Bionic checks pass.

The selected NativeActivity also links with 16KiB LOAD alignment. Its result
records `sys_fixed_native64_selected: true`, alongside Draw/Tk selection and
`awm_started: false`. This fixed set intentionally contains no print/fprint/sprint/
aprint entries. Their typed C gateways are separate from the unimplemented
ordinary variadic frame-tail/compiler contract. No shared Dis64 tags or general
managed file-call admission are added by C registration.

## ARM64/Bionic managed execution gate

The Android workflow also builds and executes the bounded managed String/List
runtime fixture against the NDK's real Bionic headers and static libc. It runs
under `qemu-aarch64`; a GNU ARM64 sysroot is not used for this gate:

```sh
NATIVE64_RUNNER=qemu-aarch64 python3 utils/tests/dis64-numeric-gc.py \
  --managed --limbo /path/to/host-tools/limbo \
  --android-ndk /path/to/android-ndk-r27c
```

The explicit option selects ARM64 NDK clang, Android port headers and a static
Bionic executable. Compiler temporaries remain within the fixture's writable
work directory. The fixture honors Inferno's private Android `free_` name at its
tracked allocation boundary and uses the Android header's inline `getcallerpc`.
The Linux build and allocation hooks retain their existing selection.

This gate executes the real managed loader, collector, bytecode owners and JIT,
including 96 nil/alias/file-reload/stack-growth cases and 324 native completions.
It checks exact active JIT mapping bounds and complete root/module retirement.
The workflow preserves `bionic-managed.log` with the native build artifacts.
QEMU/Bionic execution is separate from AWM's remaining general compiler and
variadic ABI work, and from GUI/input/storage testing on an Android device.

## Typed Sys C gateways in the full GUI library

Add `--native64-formatter` together with `--native64-tk --native64-draw
--native64-sys-fixed` to compile and link the existing four typed C gateways.
The driver checks that `Sys_print64`, `Sys_fprint64`, `Sys_sprint64` and
`Sys_aprint64` are defined dynamic symbols in the resulting ARM64 library,
alongside the NativeActivity entries and 16KiB LOAD alignment. It records
`typed_sys_formatter_linked: true` and still records `awm_started: false`.
The option is refused before output creation if the fixed Sys profile is absent.

The Android workflow executes the complete typed formatter/argument/result/
write-ownership suite with static Bionic under QEMU, preserving
`bionic-sys-format.log`, then builds this full GUI configuration. Its non-callable
variadic prefix schemas come from the actual `sys.m`; all 39 fixed Sys exports
retain their separate registration path. This does not provide a compiler or
bytecode producer for typed variadic tails and does not enable ordinary AWM startup.

## VM-frame consumers in the typed formatter build

The same `--native64-formatter` build now also requires defined dynamic symbols
for `Sys_sprintframe64`, `Sys_aprintframe64`, `Sys_printframe64` and
`Sys_fprintframe64`. The complete GUI library therefore links both the original
four typed-request gateways and four checked active-VM-frame consumers. No
ordinary Sys variadic export is registered by this build option.

The Bionic formatter gate includes 384 actual MFRAME/MCALL cases per port/model,
using trusted C operand and result-address setup, with collection inside gateway allocations,
150-slot frames, return-cell checks, cancellation and cleanup. It remains separate
from serialized Dis64 variadic compilation/admission and from device GUI/AWM
execution. `awm_started` stays false.


The formatter gate also obtains typed tails from real Limbo calls using the
opt-in `-Dq` call-site producer, then runs 768 FRAME/MFRAME/MCALL cases per
port/model. The diagnostic preserves legacy output and is not a Dis64 wire
section. Runtime values and return-address binding in that gate are still
trusted C inputs; source-to-Dis64 variadic execution and AWM remain pending.


The formatter build now also requires the exported `Sys_bindframe64` symbol
(nine typed Sys gateway/binding symbols in total). Its runtime test replaces
manual return-address setup with a checked full-pointer bind before MCALL and
checks 32 refusals for each of the 768 VM cases. This closes the trusted-C
pending-frame binding step; ordinary WORD32 LEA and serialized call admission
retain their existing behavior.


The native formatter link additionally checks four module-owned recipe APIs:
`Sys_setframes64`, `Sys_clearframes64`, `Sys_bindsite64` and `Sys_callsite64`
(13 typed Sys symbols in total). Runtime tests cover source-vector retirement,
partial construction/unwind, caller-module identity and actual `freemod` cleanup.
These native metadata owners do not enable a serialized variadic call or AWM.


The formatter gate now consumes the compiler's canonical argument-store choices
and executes their production handlers before the existing owned-site bind/call
path. Signed extension, reference retention and raw big/real bit preservation
are tested on each target. The source-cell mapping remains a decoded C transport;
this is distinct from compiling and executing an ordinary variadic Dis64 module
or running AWM on an Android device.


The formatter gate now also exercises compiler-selected source-bank addresses
and full maps with real heap/Type/pin/collector ownership. Repeated sources,
format/String aliases and packed numeric reads are checked alongside the
existing call-site lifetime tests. Inputs are still provided by the decoded-C
fixture; a complete caller/MP/data lowering and AWM startup are not implied.

The formatter transport gate additionally uses compiler-derived native FP/MP
source layouts and per-read gathering. Its mutable String-alias case verifies
that a later read sees the changed source while the pending call retains its
old format across GC. Initial values still come from the C fixture. This
progress does not yet supply a serialized variadic caller or enable AWM.

The formatter source gate now validates supported MP initializer records against
the compiler's actual classic data bytes. Its VM transport uses the original
full format for 150 arguments and checks the complete String/array/native-write
result. This replaces hardcoded format values in that transport, while FP
locals and FD setup still come from C. It does not provide a serialized native
caller or an Android AWM startup result.

Direct local FP definition recipes now supply the principal transport's
integer and String values, replacing their hardcoded initialization there.
Source gates verify definition identity, overwrite/branch/call refusals and
input bits; runtime gates retain GC and full 150-argument output checks.
FD creation, unsupported local/control-flow paths and complete serialized
caller execution remain pending, so this does not enable Android AWM startup.

Known FP recipes are now replayed in compiler PC order against managed MP
storage, rather than installing FP cells directly from flattened seed values.
A mutation-before-assignment case checks that the call sees current MP data.
The transport still uses a synthetic preallocated frame; complete caller
execution, allocation-failure ordering, FD setup and AWM startup are pending.


Separately from the variadic transport, the real `limbo -64` backend and Linux
native64 file loader execute Word conditions, conditional fixed
`millisec/sleep/dup` calls, loops with break/continue and early void-entry
returns. Complete console runs on x64, ARM64 and RV64 exercise actual
serialized modules, import LOAD, loop counts and scheduler cleanup. The
compiler preserves call results across branches/iterations in stable Word
cells; the loader checks initialization with a bounded fixed-point analysis
before scheduling. Branches cannot enter argument/result preparation or
re-enter the initial LOAD. This does not admit managed/variadic arguments
or AWM. Nested expressions of the fixed integer calls now preserve results
across later calls, and short-circuit conditions accept reused legacy frame
temporaries. These remain bounded by the same three scalar cells and prepare
one native call at a time. See `build-native64.md` for runnable Limbo examples
and the focused gate.


The separate managed file trial can now start a void entry with independent String
literals read from its serialized Rune32 pool, rather than a console input seed.
Mapped MP ownership, exact values, collection and allocation rollback are
checked independently. See `dis64-managed-literals.md`. General managed
linkage, managed Sys/variadic callers and AWM startup remain pending.

The private managed file profile also executes fixed String-to-Word `Sys->werrstr` through the real builtin LOAD and native frame. See [the tutorial](dis64-sys-string.md) / [русский пример](dis64-sys-string.ru.md). General mixed/variadic calls and AWM remain pending.

The fixed String-to-Word trial now uses returned Words in expressions, conditions and loops, with root-disjoint 112-byte frames and CFG initialization checks. See `dis64-sys-string.md` (and `.ru.md`). Mixed/variadic Sys calls, general managed types and Android AWM remain pending.

Commands and libraries share the module/link model: LOAD does not invoke init; sh calls the exported function explicitly. See [module loading](dis64-module-loading.md) / [загрузка модулей](dis64-module-loading.ru.md). The fixed native String group supports werrstr/chdir/remove through the shared loader and generated Sys exports; general signatures and AWM remain pending.


## Connection I/O and the current source gate

The checked Dis64 console can now consume `Sys->dial` results: status guards,
Connection FD/directory fields, read/write, byte arrays and String conversions.
See [the executable client example](dis64-sys-connection.md). This reuses existing
Inferno, without importing DisVM. AMD64 and ARM64/QEMU console results are not
Android/Bionic GUI acceptance.

`awm-native64-preflight.py` now checks both `-64` and `-64l` for all sixteen
startup sources. Ordinary compilation succeeds; both Dis64 profiles still refuse
the broader startup closure. Scalar mode stops at managed LOAD; the private
managed mode additionally exposes the unsupported general provider/caller shapes
and shell exception tables. General argument-bearing entry frames, library bodies
and required variadic imports remain necessary before claiming a working AWM APK.

Further native64 value-return work: [dis64-typed-returns.md](dis64-typed-returns.md).

Next execution gate: [dis64-typed-calls.md](dis64-typed-calls.md).

Source-derived forwarding calls: [dis64-typed-forwarding.md](dis64-typed-forwarding.md).

Local-result calls and continuations: [dis64-typed-local-results.md](dis64-typed-local-results.md).

Multiple calls, prefix computation and void results: [dis64-typed-multiple-calls.md](dis64-typed-multiple-calls.md).

Owned modules, export signatures and cross-module execution: [dis64-typed-modules.md](dis64-typed-modules.md).

INFERNO-0175: complete source-derived Limbo external-call bodies now run against
owned native providers on x64 and ARM64/Linux QEMU (912 cases each). This advances
source MCALL lowering; it does not admit the whole client through a managed file
loader or establish an Android/Bionic/AWM/APK run. See
[RU](dis64-typed-external-calls.ru.md) / [EN](dis64-typed-external-calls.md).

INFERNO-0176: the trusted constructor now owns client import tables and builds
all18 source client functions (local and imported sites). Existing linkmod binds
source provider instances; x64 and ARM64/Linux QEMU each pass960 three-module
cases. General image/load/cache admission and Android GUI startup remain ahead.
See [RU](dis64-owned-imports.ru.md) / [EN](dis64-owned-imports.md).

INFERNO-0177 adds `limbo -64c` for the existing scalar `-X dis64-calls` loader.
Sixteen source programs pass file proof/readmod/C/RX on x64 and ARM64/Linux QEMU;
each rebuilt complete console runs32 source-file OS roots. This is a scalar
console milestone, not managed AWM/Android GUI admission.
[RU](dis64-source-call-files.ru.md) / [EN](dis64-source-call-files.md).

INFERNO-0178 extends this file path to acyclic nested scalar helpers:28 source
programs execute on x64/ARM64 Linux QEMU, each full console runs56 C/RX roots,
and94 raw cases/six semantic controls cover graph/frame/result/refusal boundaries.
This remains the scalar console subset; managed imports, AWM and Android/Bionic
GUI/APK execution remain unverified.
[Report](dis64-inferno-nested-call-file-report.md).

INFERNO-0179 adds existing-profile0 scalar library files (-64b, or -64c without
init) and structurally matched rooted provider instances. No init is invoked on
construction.14 whole libraries pass336 trusted-importer scheduler roots each on
x64/ARM64 Linux QEMU, plus raw/ownership/control gates. General managed importing
client emission, guest namespace/cache LOAD admission and Android/Bionic GUI/APK
remain outside this milestone. [RU](dis64-scalar-libraries.ru.md) /
[EN](dis64-scalar-libraries.md) / [report](dis64-inferno-scalar-library-report.md).

INFERNO-0180 joins compiler -H -R .m interface payloads to an explicit uncached
namespace reader/admission/structural library constructor.14 interfaces/providers
pass672 trusted-importer scheduler roots per x64/ARM64 Linux QEMU target, with
both I/O modes and allocation/I/O/policy controls. Guest Limbo ILOAD and managed
importing-body admission remain next; no Android/Bionic/AWM/APK execution is claimed.
[RU](dis64-scalar-namespace.ru.md) / [EN](dis64-scalar-namespace.md) /
[report](dis64-inferno-namespace-report.md).

2026-11-14, INFERNO-0181: [structural ILOAD](dis64-scalar-iload.md) now joins
explicitly owned `.m` request tables to actual scheduled namespace library loads
and calls. ARM64 evidence is Linux/QEMU with trusted native-authored importing
code, not Android/Bionic or an admitted importing Limbo client. Native Module
layout adds two private fields: fully rebuild target objects/archives. AWM/GUI APK
execution is still unverified; remaining file-client/managed admission is described
in the [report](dis64-inferno-iload-report.md).

2026-11-14, INFERNO-0182: [Limbo file clients](dis64-scalar-clients.md) now compile
with -64c and execute actual LOAD/namespace/library calls in the complete ARM64
Linux/QEMU console:34 C/RX OS roots, provider signature/missing-file/wrong-result
controls and malformed-client refusal. Caller CODE/maps/imports come from the
file rather than a C fixture. The client is still linear with one void no-argument
init and scalar calls; ordinary AWM managed signatures/heap/CFG and a working
Android/Bionic GUI APK remain unfinished. [Report](dis64-inferno-scalar-client-report.md).

2026-11-14, INFERNO-0183: [file-client control flow](dis64-client-control-flow.md)
now supports module nil guards, branches, loops, break/continue and early returns.
Both complete x64/ARM64 Linux/QEMU consoles pass60 additional normal/missing-file
OS roots plus conditional wrong-result controls. This lets a missing library
return normally instead of breaking at MFRAME. AWM context/list init arguments,
general managed heap admission and Android/Bionic GUI APK remain separate work.
[Report](dis64-inferno-client-cfg-report.md).

## Importing-client Real and conversion continuations (INFERNO-0184)

The existing scalar file-client proof now admits Real arithmetic/negation and
Big/Real/Word32 conversions, including arithmetic on values returned by a loaded
file module. Eighteen source pairs execute through C/JIT on x64 and ARM64/Linux
QEMU; both rebuilt complete consoles pass72 new roots, including nil-load exits
and normal retirement after out-of-range conversion errors. RU/EN tutorial:
[EN](dis64-client-numeric.md), [RU](dis64-client-numeric.ru.md).

This extends scalar caller execution using the existing handlers and emitters.
Shell.init still needs ordinary exported managed context/list calls and matching
compiler/loader support. Android GUI/Bionic/device startup and a working AWM APK
are not established by these Linux/QEMU results.

## Mapped String/List file clients and actual GC (INFERNO-0185)

The importing executable profile now admits String/list locals, reference copies,
lengths, pointer cons/head/tail and full-width list nil branches. Its constructor
selects the existing typed result-address binder for mapped callers. Actual GC
runs between quanta in the source-client gate: all62 source pairs/5952 scheduler
roots pass on each x64 and ARM64/Linux QEMU, including12 new managed pairs. Frame-
mark/list-traversal omission controls prove actual graph loss before resuming VM
execution. Both complete consoles pass48 new roots and166 prior roots per target.

RU/EN working tutorial: [EN](dis64-client-managed.md),
[RU](dis64-client-managed.ru.md). Values are currently the owned filename/nil and
flat String lists; scalar provider signatures and the argument-free entry remain.
Ordinary Shell.init(context,argv), context ADTs/general literals and Android GUI/
Bionic/device execution remain open. No working AWM APK is claimed by this gate.

## Ordinary Command.init file calls (INFERNO-0186)

The actual `module/sh.m` Command.init interface now compiles and runs as a
selected entryless Dis64 library plus importing executable, including
`ref Draw->Context` and `list of string` arguments. Normal load/linkmod binds the
export; the caller explicitly invokes init. Bare Context creation, pointer
copies/nil tests and callee list operations are exercised with actual GC and
both C/RX on x64/ARM64 Linux QEMU. New private signature trial `face0186` is not
a public managed ABI assignment. No field/resource access or GUI startup follows
from a non-nil Context. This does not yet establish an Android/Bionic GUI APK.

Working examples: [English](dis64-command-init.md),
[Russian](dis64-command-init.ru.md). Exact evidence and pending GUI boundary:
[INFERNO-0186 report](dis64-inferno-command-init-report.md).

## Context pointer fields (INFERNO-0187)

The selected file-client/Command path now reads, clears and restores the three
actual Draw.Context pointer fields, relocating0/4/8 to0/8/16 after Type/Decl and
raw proof checks. Field aliases remain mapped and are traced by actual GC.
Opaque references do not create Display/Screen/channel resources. Nested fields,
channel operations, fused field comparisons and general initializers still need
work before AWM/Android GUI. Use explicit locals in the updated
[RU](dis64-command-init.ru.md) / [EN](dis64-command-init.md) example.

Incremental mk dependencies now rebuild load64.o when the selected checker or
constructor changes. Complete-console normal-path tests additionally require
successful provider linking, while missing-file tests retain graceful nil return.
Exact evidence and limits: [INFERNO-0187](dis64-inferno-context-fields-report.md).

## Reference conditions and Context construction (INFERNO-0188)

The selected Command/client path now compiles ordinary field nil conditions,
compatible pointer identity tests, short-circuit guards and loops with existing
64-bit comparisons. `ref Draw->Context(nil,nil,nil)` works in local declarations,
assignments, reloads and callees. Context temporary slots keep their type instead
of reusing String/List declarations. Classic output and prior private images
remain byte-identical. Save a constructor into a local before a module call;
allocation inside prepared call setup is still outside this profile.

[RU](dis64-command-init.ru.md) / [EN](dis64-command-init.md) examples now exercise
ordinary construction and a direct field condition. Exact evidence:
[INFERNO-0188](dis64-inferno-reference-branches-report.md). This still does not
create GUI resources or establish AWM/Android/Bionic APK/device execution.

## Owned literals and String content operations (INFERNO-0189)

The selected Command/client path now admits readonly String literals (Unicode,
NUL and empty/nil), character indexing and content comparisons through existing
VM handlers/JIT. Extra owned MP roots reuse the earlier private managed-file
literal convention. Expanded MP gets its own complete map; independent Context
24/E0 descriptor2 prevents allocating a Context with the larger MP layout.
Prior images and all compared classic outputs remain byte-identical.

Updated [RU](dis64-command-init.ru.md) / [EN](dis64-command-init.md) code constructs
Unicode/NUL argv. [INFERNO-0189](dis64-inferno-client-literals-report.md) separates
actual C/RX/GC execution from constructor-only lossless Rune witnesses. This does
not enable general DATA, Draw/Tk/full AWM or an Android/Bionic GUI device run.

## Local String writes and slices (INFERNO-0190)

Selected clients and Command.init libraries now lower concat/append, character
stores and slices with checked FP String ownership and initialized int indexes.
Existing C handlers/JIT preserve shared literals/aliases through copy-on-write;
MP remains readonly. [Report](dis64-inferno-string-write-report.md) records actual
x64/ARM64 Linux-QEMU execution and the source review against FlashVM DisVM, whose
C String implementation is already identical. Updated [EN](dis64-command-init.md)
and [RU](dis64-command-init.ru.md) examples exercise these operations.
This does not admit general DATA/Draw/Tk/full AWM or establish Android device GUI.

## Numeric and byte-array codecs (INFERNO-0191)

Selected clients/Command.init now lower String conversions with int/big/real and
UTF-8 byte arrays through existing C/JIT handlers. Array roots have a separate
proof class; maps, numeric widths/definitions and read/write roles are checked.
Array temporary identity survives fused conversions; classic images stay equal.
[Report](dis64-inferno-codecs-report.md) distinguishes Linux/QEMU full-console
execution and actual GC from constructor/raw proof checks. Updated
[EN](dis64-command-init.md) / [RU](dis64-command-init.ru.md) examples convert a
number through String and byte array before calling init. General arrays/DATA,
Draw/Tk/full AWM and Android device GUI remain outside this result.

## Zeroed byte-array allocation and slices (INFERNO-0192)

Selected clients/Command libraries now construct zeroed byte arrays and owning
slices using existing runtime/JIT. A checked primitive descriptor is separate
from Context and MP and appears only when allocation requires it. Backing arrays
survive source-variable release and GC, including nested slices. Details and
actual x64/ARM64 Linux-QEMU evidence: [report](dis64-inferno-arrays-report.md).
Updated [EN](dis64-command-init.md)/[RU](dis64-command-init.ru.md) examples keep a
view after dropping the original buffer. Element indexing remains refused: the
shared arrayindex helper's WORD-sized address result needs a coherent native
borrowed-pointer port before Word32/ref64 execution. No Android GUI/APK claim.

## Native index-result width (INFERNO-0193)

C and AMD64/ARM64/RV64 array index paths now preserve full native addresses in
Word32/ref64. Index/length scalar widths are unchanged. Independent high-address
oracles, narrowing controls and native x64/ARM64-QEMU differential execution are
recorded in the [report](dis64-inferno-index-width-report.md). Selected Limbo
indexing remains refused until its borrowed temporary slots and owner/consumer
proof are ported. A corrected low-level address result does not enable general
indexed AWM modules or establish Android device GUI/APK execution.

## Selected byte-array indexing (INFERNO-0194)

Selected Limbo clients/libraries now lower indexed byte reads/writes to an adjacent
INDB/consumer pair with a full native pointer in the existing reserved frame tail.
The owner remains a mapped GC root across scheduler pauses; control flow cannot
enter the consumer alone. Native construction also preserves immediate indexes
as numeric operands. See the [report](dis64-inferno-byte-index-report.md).

The rebuilt Linux x64 and ARM64/QEMU consoles each pass684 main ordinary Command
roots;177 source pairs pass real-GC interpreter/JIT execution on each target.
This closes0193's selected byte-index compiler boundary. General element types,
managed AWM dependencies and Android device GUI/APK execution remain outstanding.

## Selected int-array storage (INFERNO-0195)

The selected Limbo file path now admits zeroed int arrays, length, owning slices
and adjacent INDW/MOVW element reads/writes. Int elements remain signed32-bit;
borrowed addresses remain native64. Byte/int arrays have separate proof classes
and canonical checked primitive descriptors. Mixed buffers retain their own
strides and GC owners. See the [report](dis64-inferno-intarrays-report.md).

The source/GC matrix is210 pairs and the complete Linux x64/ARM64-QEMU consoles
each execute816 main roots plus controls. This does not yet compile the general
AWM startup graph: its16 ordinary modules still refuse the bounded Dis64
profiles. Android device startup, GUI/input and APK acceptance remain open.

## Local managed void calls (INFERNO-0196)

Selected Command libraries can now call up to eight internal void helpers with
the same Context/list-of-String arguments. Existing FRAME/CALL/RET and JIT paths
run separate mapped frames; each body and the acyclic graph are independently
checked. Helpers may use the already admitted String/list/byte/int operations,
and live caller values survive nested calls and GC. See the
[report](dis64-inferno-command-local-report.md) and updated RU/EN tutorial.

The Linux x64/ARM64-QEMU source/GC matrix is238 pairs; complete console execution
covers928 main roots per target plus controls. General signatures/managed results,
imports/exports and the remaining AWM startup graph are still outside this fixed
Command-helper profile. This is not Android device GUI/APK acceptance.

## Local managed results (INFERNO-0197)

Selected Command helpers now return managed String/list/ref/byte-array/int-array
values through a full native address in a mapped caller root. Result-bearing
callees are independently checked before caller uses; every normal return must
assign a compatible result. Inherited result addresses support forwarding chains
without dropping the ancestor GC owner. See the
[report](dis64-inferno-managed-result-report.md) and the String-returning helper
in the RU/EN tutorial.

The Linux x64/ARM64-QEMU matrix is268 source pairs and1048 main full-console roots
per target plus controls. Other parameter shapes, local scalar/aggregate results,
imports and remaining AWM dependencies are still open. No Android device GUI/APK
acceptance is established by this selected local-result step.

## Local scalar argument packets (INFERNO-0198)

Selected Command helpers now support parameterless calls and up to eight
byte/int/big/real arguments, optionally following Context/ListString. Complete
call packets determine exact aligned spans, entry definitions and scalar classes;
numeric slots cannot overlap GC roots or the frame tail. Existing managed
results and forwarding compose with both parameter modes. See the
[report](dis64-inferno-local-arguments-report.md) and updated RU/EN tutorial.

The Linux x64/ARM64-QEMU matrix is300 source pairs and1176 main full-console roots
per target plus controls. General standalone managed parameters, local numeric
results, imports and the rest of AWM remain separate work. No Android device
GUI/APK acceptance is implied.

## Local scalar results (INFERNO-0199)

Local Command helpers now return byte/int/big/real through the existing native
result-address cell, including nested forwarding and computed returns. Numeric
caller spans are unmapped and sized1/4/8/8; addresses remain eight bytes.
The selected reader derives exact classes and definite assignment independently.
The matrix is333 source pairs and1308 main full-console roots per Linux target;
this advances call/frame coverage, while the actual AWM sources still exceed the
bounded profiles. It does not make an Android ARM64 GUI APK available.
See dis64-inferno-scalar-result-report.md for controls and reproduction.

## Managed local parameters (INFERNO-0200)

The existing local call path now accepts inferred String/ListString/byte-array/
int-array/Context arguments alongside numeric values. Callee uses establish the
required class; mapped argument cells preserve ownership across calls and GC.
The matrix is368 source pairs and1448 main full-console roots per Linux target.
The AWM preflight now checks -64/-64l/-64b/-64c rather than only the older pair:
all16 real startup sources still refuse each bounded profile. Current diagnostics
point at general module data/imports/handlers and importing module shapes.
Android/Bionic GUI startup and APK acceptance remain open. Detailed limitations,
malformed-file controls and parameter-only GC omission are in
dis64-inferno-managed-argument-report.md.

## Importing client helpers (INFERNO-0201)

The -64c importing client can now keep up to eight private helpers alongside
root LOAD/MCALL, including managed/numeric arguments/results. Whole-client PC
mapping accounts for inserted scalar import-result copies; helper literals and
primitive/frame descriptors share the existing owned module data. Typed result
binding also applies when the root frame has no GC roots. Extra public exports,
helper imports and mutable MP globals still refuse.

Linux x64/ARM64-QEMU evidence is386 Command pairs plus11 numeric-import pairs and
1564 main full-console roots per architecture. All16 actual AWM startup sources
still refuse all four selected profiles; Android/Bionic GUI/APK is not available
from this step. Details and controls: dis64-inferno-import-helper-report.md.

## Shared typed result storage

`dis64layoutbindresult` in `limbo/dis64-layout.c` now reconstructs caller
result padding from the declared type layout. Local/imported aggregate bindings
and the stat/fstat mixed result use this same operation. The helper checks all
legacy offsets, native widths and root kinds before moving any frame leaf;
refusal leaves the plan unchanged. Later operand translation and GC map
construction consume that single plan. No OS, CPU, module or function name is
part of the storage operation.

The C layout fixture covers Word/root pairs in both orders for String, List,
Array, Ref, Channel and Module, plus a nested aggregate compared with native C
`offsetof`. The fstat fixture retains its active mutation control, now removing
relocation in the shared helper. This is shared Word32/ref64 compiler machinery;
it does not change the classic 32-bit execution path.

Call admission still uses the existing aggregate contracts. In particular,
`Sys.tokenize` is not yet admitted by the native64 external-call path: generic
storage planning alone does not supply result metadata, native-call validation
or a callable Sys gateway. System Limbo sources are unchanged by this change.

## Imported scalar/reference tuple results

Imported native64 calls now use the existing 16-byte mixed result contract for
`(int|big|real, managed-reference)` declarations. Selection checks the declared
type and complete leaf layout; it does not depend on Sys, tokenize or shell
function names. The shared result planner restores padding and the caller GC map.
The VM accepts the import contract only with a matching call-site contract,
aligned caller result and exactly the required reference map. Inherited mixed external results use the original validated caller destination
when source types and both contracts match.

`utils/tests/native64-tokenize.py --limbo /path/to/limbo` checks 18 imported
declarations using String, List, Array, Ref, Channel and Module results, and six
corruptions of the source result binding. It executes the actual Sys_tokenize
body through compiled Limbo FRAME/MCALL instructions in 36 C/JIT cases, including
Unicode delimiters, empty input, repeated delimiters and an empty delimiter set.
The fixture checks count/order and full root/module retirement. Eight malformed
map/contract/binding cases must fail before constructor allocation. The existing
Sys tokenization matrix separately covers allocation failure and old result
ownership in WORD32/WORD64 models.

The original system Sh.getbq now passes checked lowering with both read and
tokenize imports. No system Limbo source was changed. This proves the imported
mixed result path on the tested host; the full shell, arbitrary nested aggregate
calls, Android desktop execution and untested OS/CPU combinations remain separate
work. The typed-calls CI runs the new fixture in its existing host/CPU matrices.

## Local mixed results and forwarding

Local result classification now shares the imported scalar/reference validator.
The second field retains its declared String/List/Array/Ref/Channel/Module kind
instead of being treated as String. Caller storage uses the same shared padding
planner. Forwarding a local mixed result requires matching source types and
matching caller/callee contract 16. Imported mixed result forwarding uses the same original caller destination.

A complete mixed tuple returned with IMOVMP uses a 16-byte descriptor whose only
root is the second eight-byte slot. The constructor reuses that existing mixed
record descriptor rather than the two-root descriptor. It checks source maps,
copy bounds and completion on every return path before allocation. Compiler and
runtime result-flow checks account for both individually written fields and the
complete mapped tuple copy.

`native64-tokenize.py --local` checks 18 local result declarations and executes
`run -> forward -> pair -> Sys.tokenize`. Each of 72 C/JIT cases makes two calls,
with scheduler quanta 1/3/1000 and both initial-stack configurations. It includes
Unicode delimiters, exact list/count checks and complete retirement. Fifteen
malformed map, copy, completion and forwarding cases are refused before
allocation. The local constructor also injects failure at every measured allocation
in both execution modes and requires complete rollback. The direct imported fixture also runs 72 cases. The typed-calls CI
runs both modes on its existing CPU matrices; local validation here is Linux
amd64 with UBSan. System Limbo sources and their classic Dis bytes are unchanged.


## Imported mixed result forwarding

`return provider->pair()` now lowers a mixed tuple result by forwarding the
original borrowed result address at native offset 32. The compiler requires the
complete declared return type to match the caller. The VM checks contract 16 on
both the importing function and the imported declaration, plus the exact
IMOVL result-address binding. This keeps the original caller's GC ownership
through both local and native calls; no extra tuple or reference owner is made.

`native64-tokenize.py --forward-import` executes
`run -> forward -> pair -> Sys.tokenize`, where pair returns the Sys result
directly. It checks 18 imported return declarations across all six supported
managed-reference kinds. The real Sys handler passes 72 C/JIT cases, two calls
per case, quanta 1/3/1000, both initial stack configurations and Unicode delimiters.
Eleven additional malformed caller/import/contract/binding cases are refused
before allocation. Constructor allocation failures roll back completely in both
execution modes. Direct import and local-copy modes remain separate regressions.

These checks use Linux amd64/Word32-ref64 with UBSan; the existing CPU CI matrices
include this mode. They do not establish arbitrary nested aggregate calls, full
shell startup or Android device execution. System Limbo sources are unchanged.


## Local pairs of managed references

Local tuple results now accept any two declared managed-reference kinds:
String, List, Array, Ref, Channel and Module. Their exact field kinds remain in
the compiler proof; forwarding requires equal complete return types. The local
caller binding checks both fields using the shared typed layout planner instead
of requiring a Module followed by a String list. That step covered local calls;
imported two-root calls are covered below.

Both individual pointer results and whole mapped tuple copies use contract 48.
The mapped copy uses the existing two-root descriptor. Runtime result-flow checks
now require both roots on every returning path, including local forwarding.
String slicing into either String field retains the declared destination kind.
No system Limbo source or classic Dis encoding was changed.

`native64-tokenize.py --root-pair` checks 36 type combinations plus nil returns.
Its executing chain creates a real token list, returns the same list through two
aliased owners, copies a List/String tuple, slices the String field and forwards
the pair. Seventy-two C/JIT cases include repeated calls, Unicode, empty strings,
quanta 1/3/1000 and both initial stack configurations. Fifteen malformed metadata
cases are refused before allocation; constructor allocation failures must fully
roll back in both modes. Tests use actual mapped-copy and String-slice handlers.
The existing mixed-result modes remain regressions, and CI runs this additional
mode in its current CPU matrices. Local execution evidence is Linux amd64 with
UBSan, not full shell, Android device or cross-platform acceptance.

The former Tk boundary fixture now distinguishes valid generic local root pairs
from the specialized Tk result contract: changing the channel payload or first
ADT keeps a generic pair with its own signature, never contract 116. This updates
the old refusal expectation without permitting it to impersonate Tk.toplevel.

## Imported pairs of managed references

The same two-root layout is now supported across imported calls. The compiler
uses the shared result planner to pack destructured caller storage and accepts
`return provider->fn(...)` only when the complete result types are equal.
Diagnostics retain both declared field kinds. Contract 48 describes two native
root cells; it does not identify a particular module, function or root kind.
The classic Dis bytes and system Limbo sources remain unchanged.

The module constructor and call materializer require matching import/site result
contracts, both root-map bits for fresh storage and the caller's pair result
contract for inherited storage. Metadata failures occur before allocation;
materializer failures leave the output instruction buffer unchanged.

Run `python3 utils/tests/native64-root-pair-import.py --limbo /path/to/limbo`.
It independently checks all 36 combinations of String, List, Array, Ref, Channel
and Module result declarations in destructured and inherited forms. Executing
String/String cases cross a real module link to a controlled C provider or a
separate interpreted/JIT Limbo provider. The 504 cases cover interpreted/JIT
callers, quanta 1/3/1000, two calls per successful case, stack growth, field and
mapped copies, local forwarding, nil/aliased roots, missing modules and provider
exceptions. Both returned pointers, existing result owners, guard bytes and
complete caller/provider retirement are checked. Published Limbo exception values
are retired through the actual handler before fixture teardown.

The test rejects 24 malformed import/site/result/map/binding cases before
allocation, detects 24 independent source-proof mutations (including field
kinds), and injects all 30/31 constructor allocation failures in C/JIT modes.
Two active controls disable the callee/caller result proofs separately and must
fail the corresponding materializer predicate. CI includes this test in the
existing native64 CPU jobs.

Local execution evidence is Linux amd64 with UBSan. This closes the imported
pair-result gap; the unchanged Sh diagnostic frontier remains 70/134 bodies.
It does not establish full shell startup, an Android APK/device launch, or
validation of the Windows/macOS/32-bit and other CPU targets.

The existing local/imported call regressions keep their active refusal controls.
Two stale mutation targets now follow the shared module-operand validator and the
current source-root map predicate, retaining alignment and ownership checks.

## Arrays of managed references

Native64 array allocation, ordinary element reads/writes, nil comparisons,
element results, returned slices and slice copies now use the element's declared
managed type. String, List, Array, Ref, Channel and Module elements share one
mapped 8-byte cell, retaining their complete types for bindings and copies.
The ordinary path also covers nominal Draw.Image and Wmsrv.Client references;
the existing longer window/registry protocols keep their specific owner checks.
The previous shell-variable-only classifier and duplicate Image mover are gone.

An indexed element address stays in the existing reserved FP48 cell until the
adjacent mover, comparison or return. Branches cannot enter the consuming step.
Global arrays and direct element results use the same proof. String nil tests
retain String comparisons, preserving equality between nil and an allocated
empty String. Dedicated array checks validate their borrowed owner and readonly
nil operand. The constructor rejects slice-copy instructions without a known
raw, mapped, library or byte proof before allocating anything.

`python3 utils/tests/native64-root-arrays.py --limbo /path/to/limbo` executes
6912 C/JIT cases using real objects of all six managed kinds. It checks mapped
initialization, FP/MP indexing, replacement, nil/aliased ownership, array copies,
backing-owner slices, direct returned elements, signed bounds, allocation errors,
source exceptions and exact retirement. A separately allocated empty String
checks the language's nil comparison semantics. The independent classic oracle
detects 936 mutations, while 1248 metadata mutations fail before allocation.
All 35/36 constructor allocation failures roll back in C/JIT modes. Active
controls remove the mapped element bit, replace String comparison with pointer
comparison, and remove the unproved-copy precheck; each must fail its predicate.

Image regressions retain 3168 scenarios and their three active controls. Their
checked IR is identical after routing ordinary accesses through the common path.
The old nominal allocation boundary now accepts arbitrary reference elements;
Word and supported mixed-record arrays are covered below; big/real array
allocation remains outside these protocols.
The Image stride control and Wmsrv trailing-root control target the current
shared code after earlier refactors. CI runs the managed-array test in its
existing CPU jobs and triggers for changes to its fixtures.

The unchanged Sh.yytokname and Sh.yystatname now pass checked lowering. Sh moves
from 70/134 to 72/134 bodies; the 16-module startup frontier is 309/385. All
ordinary/classic bytes remain identical, and no system Limbo source was edited.
Local runtime evidence is Linux amd64 with UBSan; full shell and Android device
startup, and the other OS/32-bit/CPU combinations, are not established here.

## Global Word32 arrays and raw owner identity

The existing raw Word/Winplace.Delta/Draw.Rect array protocols now support MP
owners as well as FP owners: allocation, indexing, slice copies and slices.
An integer element or record field can return directly through the caller's
Word32 result cell. Native addresses remain 64-bit; Limbo `int` values and raw
Word array elements remain 32-bit.

The owned module spec has trailing private C metadata for Word-array initializers.
The constructor validates sorted, mapped, nonoverlapping MP root slots, at most
256 arrays and 65536 total elements, and collisions with String/array/table and
ordinary initializers before allocating. It uses its owned unmapped 4-byte Type
and publishes each allocated array into the rooted MP before copying values.
Dense, sparse and zero-length arrays use the same path; sparse omitted cells are
zero. Existing C source initializers can omit the new trailing fields.
No serialized Dis or Styx layout changed.

Raw address proofs retain the owner's FP/MP identity, so equal offsets in the
two spaces cannot be confused. Replacing a proved owner invalidates its address;
CALL/MCALL/SPAWN conservatively invalidate MP borrows. The runtime resolves the
proved MP root, checks its bounds and map, and checks the actual array element
Type before opcode dispatch. Local owner proofs retain their previous call
behavior. The existing recursive `arraycpy`/`newmp` path provides independent
raw array payloads in module instances, including rollback of partial clones.

`python3 utils/tests/native64-word-arrays.py --limbo /path/to/limbo` checks 13
source bodies with canonical signatures and identical classic bytes, detecting
204 independent instruction-proof mutations. Its 4920 C/JIT cases cover dense,
sparse and empty data, signed extremes/bounds, FP/MP operations, returned Words,
MP rebinding and independent payloads, and global 12/16-byte record fields.
Wrong width or mapped element Types fail before the index opcode runs. Fourteen
malformed initializer cases fail before allocation. All 43/44 constructor and
five MP-clone allocation failures roll back. Three active controls damage
initializer bits, owner-space identity and global-call invalidation separately.

The source-data regression additionally constructs all 11 unchanged Sh arrays
and verifies their 546 numeric elements, two independent clones and retirement;
it does not execute Sh. Its classic-reader negative test explicitly selects
`with_arrays=False`, preserving that option while the decoder default supports
arrays. Managed-array and full Winplace regressions retain 6912 scenarios and
60 processes with active controls. CI includes the new fixture in both existing
CPU jobs. The former managed-array boundary now admits global Word allocation;
big/real and unsupported mixed layouts remain outside these protocols.

Local evidence is Linux amd64 with UBSan. The checked startup frontier remains
309/385, with Sh at 72/134: parser-stack allocation is covered below, while
other shell dependencies are still pending. System Limbo files are untouched. This is not a full shell,
Android device/APK, or other OS/32-bit/CPU acceptance result.

## Mixed record arrays and the yacc stack

The owned FP/MP array protocol now classifies ADTs and tuples by their complete
layout, without checking module, record or field names. It admits a prefix of
one to three managed roots followed by one or two `int` fields, including
padding introduced by a nested ADT. All six root kinds use the common layout
path; declared referent and nested field types remain part of the compiler
proof. Interleaved roots/Words, big/real fields, excess fields and general
mixed-ADT call/result ABIs remain outside this bounded protocol.

Compiler and runtime share a private layout catalog. Classic descriptors still
use four-byte reference slots; native element Types use eight-byte references
and exact GC maps. Allocation, indexing, Word/root field moves, mapped copies,
slice copies and returned fields retain the existing owner-lifetime checks.
Each mapped copy gets its own pooled Type. Copy validation compares active
Word locations as well as GC roots: padding must not overwrite another field.
Malformed class and mode high bits are rejected before module allocation.
Neither the serialized Dis format nor system Limbo files changed.

For the unchanged Sh declarations, `YYSTYPE` grows from 16 to 32 bytes and
`YYS` from 20 to 40 bytes. Its state Word moves to offset 32. Copying `yyv`
uses the 32-byte descriptor, preserving the separate state; copying the whole
array uses the 40-byte element Type. The test extracts the original declarations
and compiles the yacc-style growth expression without editing `sh.b` or `sh.y`.

`python3 utils/tests/native64-mixed-arrays.py --limbo /path/to/limbo` executes
6624 C/JIT scenarios across all nine admitted layouts. Generated natural C
`sizeof`/`offsetof` witnesses independently check the native geometry. The
fixture uses real allocation, index, mapped-copy, slice and recursive
`arraycpy`/`newmp` code; it checks nil roots, signed bounds, three allocation
faults, source exceptions, result guards, independent MP/array clones, exact
reference counts and complete retirement. Its classic oracle detects 2664
instruction-proof mutations. Another 2220 malformed instruction records fail
before allocation; all 56/57 constructor allocation faults roll back in C/JIT
modes. Three active controls damage the element GC map, padding check and
nested-copy Type separately; each must fail its named predicate.

The original `yyparse` passes its allocation at PC 5528 and reaches PC 5552,
where the unsupported `yylex1` call is the next barrier. Sh remains 72/134
checked bodies and the complete startup frontier remains 309/385. Word arrays
retain 4920 cases, managed arrays 6912 cases and full Winplace 60 processes,
with their active controls. CI runs the mixed-array regression in both existing
CPU jobs. Local execution evidence is Linux amd64 with UBSan; full shell and
Android APK/device startup, other OSes and other CPU/32-bit combinations remain
unverified.

## Generic nested ADT fields and lexer state

Checked native64 lowering now derives Word32 and managed-root field accesses
from the complete declared ADT layout. It handles inline nested ADTs/tuples,
mixed ordering and padding without choosing a layout by module or field name.
Existing closed library/Draw protocols retain their encodings. Other ADTs,
including a structurally identical Context in another namespace, use the new
generic protocol with runtime heap checks. Declared root types still govern
copies and returned fields; this does not introduce arbitrary pointer casts.

The private annotation covers field reads/writes, integer arithmetic/branches,
root copies/results, pointer and String nil tests, lengths and String character
reads. All six managed root kinds retain their ordinary ownership semantics.
String tests use String comparison, preserving equality of nil and an allocated
empty String. Selected leaves must be `int` or managed roots; byte/big/real
operations, field offsets beyond 4096 bytes, owner slots beyond the 16-bit
operand range, allocation of arbitrary records, direct field-to-call-argument
preparation and aggregate result bindings remain outside this protocol.

An owned, immutable per-PC capability extends the existing predecode array
hook. It records the exact instruction and frame Type. Before `vmdecode` forms
an indirect field address, the hook reads the proved FP/MP owner and validates
the actual heap Type, complete selected span and every byte's GC role. Nil
owners, short payloads and scalar/root map mismatches raise through normal VM
errors before opcode dispatch. Module construction validates frame/data root
maps, packed operands, branch targets and exclusion from pending call regions
before allocation. Capability allocation and teardown use the existing owned
module lifecycle; no serialized Dis or Module ABI fields were added.

Diagnostic IR additionally carries recursive type trees and per-instruction
owner witnesses. The classic oracle independently places their fields with
native alignment rules and checks the original operands and frame/data maps.
Generated natural C `sizeof`/`offsetof` checks and execution assertions provide
separate geometry witnesses. A pre-existing Context oracle ambiguity between
an MP owner and a direct MP nil value is corrected by comparing their offsets,
while retaining all canonical Context instruction encodings.

`python3 utils/tests/native64-record-fields.py --limbo /path/to/limbo` executes
2268 generic C/JIT cases with nested FP/MP fields, all six root results,
arithmetic, Unicode, nil/alias/empty-String cases, real heap-map/size failures,
four-byte field/result padding guards, exact references and complete retirement.
The operand/type-tree oracle detects 1716 instruction mutations and nine
record-extent mutations. Another 1716 malformed records fail before allocation;
all 28/29 constructor allocation failures roll back in C/JIT modes. Three active
controls independently damage the heap extent check, GC-role check and field
displacement; each must fail its named predicate.

The same fixture executes another 864 C/JIT cases using the unchanged bodies
of Sh's `YYLEX.error` and `YYLEX.ungetc`, connected by a local-call wrapper.
Their original data declarations are extracted; declarations of unrelated
methods are omitted solely to isolate this fixture. Generated C checks verify
the full 128-byte YYLEX geometry and its nine GC roots. These runs cover signed
Word32 decrement/increment overflow, nil/empty/nonempty buffers, file-state
branches, first-error preservation, String nil semantics, caller guards and
exact ownership. They perform no Bufio I/O.

The unchanged full Sh compiler frontier advances from 72/134 to 74/134 bodies
(`error` and `ungetc`); the 16-module startup frontier is now 311/385. `getc`
passes its initial state fields and reaches PC 5315, an `IINDW` through the
buffer-array field. `yyparse` still stops at the unsupported lexer call at PC
5552. Mixed arrays retain 6624 cases, root arrays 6912 cases and Draw.Context
252 calls, with active controls. CI includes the new regression in both existing
CPU jobs. System `.b`/`.y` files and ordinary Dis bytes remain unchanged.
Local execution evidence is Linux amd64 with UBSan; full shell and Android
APK/device startup, other OSes and other CPU/32-bit combinations are unverified.

## Array fields and lexer execution

Native64 lowering uses declared ADT field layouts for Word, managed-root and
supported mixed-record array allocation, indexing, slicing and copy. An index
through an FP or MP record owner requires a single adjacent consumer. The
module checks its scratch slot, element span and GC roles, and excludes control
flow into the consumer. The VM checks the heap owner, array element Type and
both owned instruction snapshots before executing the pair in one quantum.
Word and managed fields can also supply arguments to fixed calls and supported
Sys formatting calls. Variadic Word arguments use signed 64-bit slots.

`python3 utils/tests/native64-field-arrays.py --limbo /path/to/limbo` runs
162 interpreter/JIT cases using original Sh lexer bodies and small generic ADT
probes. The fixture checks Unicode input, ring wrap, EOF, quoted words,
redirections, parser token mapping, the Bufio and Sys prompt call boundaries,
and result and reference ownership. It exercises real VM calls with controlled
file input and output. Linux amd64 execution uses UBSan. The source oracle
also checks exact classic Dis bytes across compiler modes. The metadata suite
rejects 34 malformed operands, entry targets and pending-frame slot bindings.
The scheduler test detects a skipped adjacent consumer.

The checked native64 frontier reaches 79/134 Sh bodies and 316/385 bodies
across the 16 startup modules. `getc`, `lex`, `readnum`, `readfdassign`
and `yylex1` contribute the five additional bodies. `yyparse` advances to
PC 5577, where it copies `YYLEX.lval` to the parser stack.

## Original Sh parser execution

The compiler lowers a mapped ADT copy between an inline heap field and an
array element using the declared subtype and its relocated field layout.
The VM verifies the actual heap span and all GC roles before decoding the
copy. Managed scalar moves use the same field check. For nested indexing the
module proves that the earlier array element supplies a Word and that its
owner remains live until the next index takes the value. The next index can
reuse the same address slot after reading the Word.

Local calls can take a root from an indexed array element, including after
an addition or subtraction in the caller's index expression. The checked
call path also permits construction of a record with a supported descriptor
in a mapped local before passing it to the pending frame. Its validation
checks the preceding arguments, initializer operands, and root slot in
the callee.

`native64-field-arrays.py` compiles the original `yyparse`, helper bodies,
and parser tables into a test module with controlled services. Its 54
interpreter/JIT parser cases cover command trees, pipe and redirection
descriptors, error recovery, a 225-level input that grows the original
200-element stack, and refusal of a mismatched aggregate GC map. The suite
also checks 162 lexer and generic ADT array cases and 71 malformed metadata
cases. The deep-tree test checks each node's ownership before releasing the
chain in bounded steps.

The source check covers all 485 instructions of `yyparse` and also accepts
`pipe2cmd`. Sh reaches 81/134 checked bodies and the 16 startup modules reach
318/385. Both compiler modes emit identical classic Dis bytes. These parser
execution results were verified on Linux amd64 with UBSan.

## Original Sh lexer construction

Checked native64 can allocate and copy ADTs up to 192 bytes using a
descriptor derived from the declared layout and validated against the
classic GC map. Mapped copies check the natural span and root slots of
each heap operand at execution. Local and imported calls can write a
managed result directly into a record field when the caller retains its
owner in a mapped stack slot across the call.

This covers `YYLEX.initstring`, `YYLEX.initfile`, `doparse` and `Sh.parse`
in the source preflight. The constructors and `doparse` also run in 30
interpreter/JIT tests on Linux amd64 with UBSan and controlled file
services. The tests cover successful string and file parses, diagnostics,
a nil `Bufio.fopen` result, and a record copy with a live String and a
signed Word. Checked coverage rises to 85/134 Sh bodies and 322/385 bodies
across the 16 startup modules. The preflight compares classic Dis output
byte for byte.

## Sh context setup and builtin tables

Checked calls can load nested ADT references through a mapped temporary, pass
the final field to the callee, and clear the temporary. The VM verifies the
heap span and root slot of each field before dereferencing it. Local calls may
also build their first list argument in a mapped temporary, and fixed calls
can take the length of an array stored in a record field.

The same array operations used for mixed records now support elements made of
two or three managed fields. Allocation, element copies, slicing and bulk
copies use the element's verified size and GC map. The two-root layout is
used by Sh's builtin tables.

The source preflight accepts `Context.push`, `concatwords`, `copybuiltins`,
`addbuiltin`, `Context.addbuiltin`, `Context.addsbuiltin` and `initbuiltin`.
Sh reaches 92/134 checked functions and the 16 startup modules reach 329/385.
`runfile` advances to its first call to `Context.setlocal`, which constructs
an argument after staging a managed reference in the pending frame. Classic
Dis output is identical in the preflight comparison.

The field-array fixture runs 42 additional interpreter/JIT cases covering
nested call arguments and results, a two-root array copy, array lengths, a
locally constructed list, nil references and a mismatched heap Type.

## Sh environment updates during call setup

Checked local calls can allocate and initialize a record after copying Word
or managed arguments into the callee frame. Validation checks the caller and
callee root slots and keeps the argument writes disjoint. A call can also
cons the new record into a one-element list before handing it to the callee.
The record and list have separate mapped temporaries until the handoff.

The original `Context.set`, `Context.setlocal` and `setstatus` pass the source
preflight. Checked coverage reaches 95/134 Sh functions and 332/385 functions
across the startup modules, with identical classic Dis output. `runfile`
advances to the call to `doparse`, where evaluating a Boolean argument uses
branches inside the pending call setup.

The fixture runs 30 interpreter/JIT cases for environment updates and status
lists on Linux amd64. Allocation failures exercise both record and list
cleanup. The collector test retains an object reachable only from a pending
frame in a separate stack extent, then reclaims it after the extent leaves
the process roots.

## Sh Boolean arguments and list head fields

Checked `FRAME` and `MFRAME` sites can materialize a Boolean from a signed
Word comparison. Both paths store a value in the same unmapped callee slot
and rejoin at the next instruction. Validation checks the two targets and
both caller operands. Entry into the call still goes through its frame setup.

Calls can pass a managed field of a list head. The list head stays in a mapped
temporary until its field is handed off, then the temporary is cleared.
The compiler accepts `Context.pop`, `glob` and `sbuiltin_unquote`, reaching
98/134 checked Sh functions and 335/385 checked startup functions. Classic
Dis output matches byte for byte. `runfile` advances through `doparse` to
the call to `walk`.

The Linux amd64 interpreter/JIT fixture covers `hd tl prompt` and
`!interactive` together, all six signed Word comparisons at the caller,
an imported `Bufio.fopen` mode and a list head field with nil and ownership
checks.

## Checked recursive calls and record selectors

The checked compiler uses the frame and result layout of an active callee
when validating recursion. Every reachable body must complete validation.
The calls keep ordinary Dis frames and result bindings, including recursive
results written to a caller's local slot.

Integer `case` and computed `goto` accept a Word selector from a mapped
record. The VM checks the field before dispatch, and table targets continue
to use Dis instruction ordinals. Call setup can copy a list head, list tail
or reference array element straight into a mapped argument slot. An array
argument read checks the element geometry and map before indexing.

The unchanged Sh source now has 99 of 134 checked bodies, including
`sbuiltin_quote`. `walk` reaches `walkpipeline` past the recursive call and
record selector. `walkpipeline` passes `fds[1]` to itself and reaches `glom`.
The allocation of a `Redirlist` while preparing a call in `glomoperation`
remains a compiler barrier. Classic Dis is byte-identical for all 16 modules.

## Generic records during local call setup

After `FRAME`, a call can allocate a reference to a declared ADT in a mapped
caller slot, initialize its Word and managed fields, and pass the reference
or a singleton list of the reference. The checked layout and the pending
callee map prove the temporary and argument slots. Earlier arguments stay
rooted through allocation. Validation checks each initializer against the
allocation and requires control to enter through the frame setup.

The original `glomoperation` passes the `ref Redirlist(nil)` setup and
reaches the call to `pipecmd`. Its large redirection mask still uses a
reserved legacy temporary. The original `concat` is checked through both
list tails in its pair assignment. Sh reaches 100 of 134 checked bodies,
and the startup set reaches 337 of 385. Classic Dis remains byte-identical
for all 16 modules.

## Record masks and Word copies

A checked bitwise AND can load a Word from a record and then apply a large
literal in place. The literal uses the ordinary Dis source operand. The
second instruction requires the preceding field read, and control-flow
validation protects that entry. Word-only declared record spans use ordinary
`MOVM` with exact source and destination layouts. The VM verifies the heap
span before decoding its address.

`pipecmd` passes the `OAPPEND` mask, copies `Redir`, and proceeds through
`sys->pipe` to the mapped channel allocation for `(int, ref Expropagate)`.

## Word and managed reference channels

Checked Limbo shares one descriptor for a two-field channel payload with a
32-bit Word at offset 0 and a managed reference at offset 8. The compiler
checks the declared types and the legacy descriptor, and uses the same
native layout for allocation, tuple copies, send and receive. The interpreter
and JIT tests cover buffered and unbuffered channels, nil, retained reference
identity and receiving into a discarded temporary.

The unchanged `pipecmd`, `runsync` and `builtin_subsh` now pass the channel
allocation and reach the `runasync` spawn setup. Inside `runasync`, the
checked compiler still stops while resolving `Context.copy`.

## Shell environment copying and spawn arguments

The checked compiler reads a single-root array element through a declared
record field, including the variable buckets in `Localenv`. It keeps the
index and its consumer in the same scheduling quantum. A local call can
receive a direct array length, and a spawn can receive a list tail in its
mapped argument slot.

A local call can also allocate a fresh record from a list head. The
compiler checks the record layout and the call frame. The list head stays
rooted until the mapped copy completes. The copy retains managed fields
and gives the callee independent Word fields.

The original `copylocalenv` and `Context.copy` now pass checked compilation.
`runasync` reaches `doredirs`, which passes the redirection list nil check
and stops at the first `Redirword` list-head copy. Classic Dis remains
byte-identical for all 16 startup modules.

## Managed aggregate lists and redirections

The checked compiler uses the declared element layout for `ICONSMP` and
`IHEADMP`, including nested Word fields and managed roots. It reserves
native padding before lowering frame accesses. Direct list slots and list
fields in records share the same descriptor and the VM checks a node's
payload layout before reading it.

A variadic Word argument can come from a record reached through an
intermediate reference. The reference stays in a mapped cell through the
Word read and is cleared before the formatter call. An integer list can
also prepend a Word directly from a mapped record field.

The unchanged Sh `dup` and `doredirs` pass checked compilation. In
`runasync`, the next barrier is `Context.run`. The compound redirection
argument during `pipecmd` spawn setup remains open. Sh reaches 106 of 134
checked bodies, and all 16 classic Dis outputs remain byte-identical.

## Shell dispatch and call arguments

A checked String case can read its selector from a mapped record field.
The table bridge validates the owner and field before dispatching. An
imported call can write the reference field of a mixed result directly.
The return analysis tracks that field separately from the scalar field.

The compiler retains a list head while building a singleton in a pending
local call, then releases both caller temporaries after publication. An
untyped `nil` inside a list element contributes its mapped reference slot
to the element descriptor.

The original `trybuiltin` and `runsbuiltin` raise the checked Sh count to
108 of 134. `runblock` passes the singleton argument and reaches `walk`.
`pipecmd` passes layout planning for the redirection list and reaches the
compound argument setup of `spawn`. Classic Dis remains byte-identical
for all 16 startup modules.
