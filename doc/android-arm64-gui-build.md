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
