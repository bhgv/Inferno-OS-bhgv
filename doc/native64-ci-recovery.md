# Native64 CI recovery after INFERNO-0206

Base: experiment `a145cff8a09ab7550246212a3cd46537f2a65565`.

The current CI failures include test-source composition and toolchain selection
errors. The new ELF32 acceptance additionally exposed a real Linux/386 executable
memory defect. Compiler lowering, Dis64 wire and calling layouts remain unchanged.

## Reproduced failures and corrections

* The selected Sys table now has 24 exports, including mount. The registration
  fixture still provided 23 callback witnesses. Add the mount witness and require
  the complete published table, callback identities, maps and rollback checks.
* The numeric execution fixture inserted its storage headers by replacing a
  removed include. Anchor the explicit dependency to the current library header
  and fail if the anchor changes again.
* Storage/numeric source slices inadvertently included the newly inserted
  library-instance constructor. Exclude that separately tested constructor from
  lease-only closures, retaining the actual storage bodies unchanged.
* The shared loader now references byte primitive, String allocation and typed
  local-result services. Supply the byte primitive and explicit refusing seams
  in the scalar fixture. Extended gates replace those seams with their actual
  production implementations. No no-op allocator/root/binder can hide accidental
  managed execution in a scalar-only test.
* RV64 CI has no target libubsan, but the mount-tag script forced UBSan. Respect
  selected CFLAGS in target tag/version gates. Keep the deliberate signed-shift
  undefined-behavior control mandatory on the native host with UBSan, clearly
  named `host-ubsan` in its log. Positive tag and version-reader execution, plus
  the version-reader semantic omission control, remain on the selected target.
* After these repairs, RV64 reached the fixed-Sys GC gate and exposed a stale
  test observer: dial/Connection execution retained the previous fixed caller's
  retired RX address range. Bind/reset the observer for every live caller, with
  checked mapping bounds. The existing fast-entry membership predicate remains
  active. Reuse of the same mmap address had hidden this defect on x64.

## Local Linux x64 validation

Host tools were built with `MKBOOTSTRAP_LIMBO=1 sh makemk.sh` into a separate
directory. The relevant commands below use that directory's limbo executable.

```sh
python3 utils/tests/native64-sys-register.py --limbo /path/to/limbo
python3 libinterp/tests/dis64-runtime.py
python3 utils/tests/dis64-numeric-exec.py
python3 utils/tests/dis64-numeric-module.py
python3 utils/tests/dis64-numeric-gc.py
python3 utils/tests/dis64-numeric-gc.py --calls
python3 utils/tests/dis64-numeric-gc.py --managed --limbo /path/to/limbo
python3 utils/tests/dis64-storage-owner.py
python3 utils/tests/dis64-storage-owner.py --control pin
python3 utils/tests/dis64-storage-clone.py
python3 utils/tests/dis64-storage-clone.py --control partial
python3 utils/tests/dis64-sys-string-gc.py --limbo /path/to/limbo
python3 utils/tests/dis64-scalar-client.py --limbo /path/to/limbo --case single
python3 utils/tests/dis64-scalar-client.py --limbo /path/to/limbo \
  --local --case import-local-string-input
python3 utils/tests/configure.py
python3 utils/tests/awm-native64-preflight.py --limbo /path/to/limbo
python3 utils/tests/native64-port.py --arch amd64 --pair-boot-only \
  --build-root /path/to/empty-output --host-tools /path/to/host-tools
```

These checks passed locally, including the complete classic-mk x64 console/pair
boot, 744 scalar runtime images / 1,667,800 predicates, all 24 Sys entries / 53
allocation refusals, real managed collection and active literal-publication and
seed-classification omission controls. Storage ownership also passed with
ASan+UBSan (`detect_leaks=0`); this is not whole-runtime sanitizer/leak evidence.

The four existing native64/Android workflows now also run for pull requests
targeting experiment. Their full remote results remain separate from the local
checks above. ARM64, RV64 and Android/Bionic execution were not run locally.

## Classic 32-bit compatibility

The new classic32 workflow installs multilib, builds the actual Linux/386 console
with ordinary headers and flags, verifies ELF32/x86 identity and executes classic
Dis with the interpreter and JIT selected. Its program exercises Sys registration,
signed32 wrap, Big, Real, arrays and Strings. Host compilation of that ordinary
Limbo source passes. Actual ELF32 build/boot requires the workflow's multilib
environment and is not inferred from a WORD32/ref64 test on a 64-bit host.

The first real CI run built ELF32 successfully and passed the interpreter case,
but its JIT case broke with a segmentation violation. Linux/386 segflush was a
no-op while the classic backend emits its entry stub, type helpers and module
code into malloc storage. On NX-enabled Linux these allocations are not executable.
segflush now checks overflow, rounds only the affected page span and propagates
mprotect failure before the compiler publishes a code pointer. The classic
allocator/literal data remains writable: these legacy shared malloc pages use
RWX. Separate page-owned code allocation/free would be needed for W^X; the newer
64-bit RW-to-RX allocator is unchanged.

`python3 emu/Linux/tests/segflush-386.py` executes the actual Linux function on the
host, checks surrounding guard-page permissions, bytecode execution, writable
literal bytes, overflow and unmapped-page refusal. `--noexec-control` removes only
PROT_EXEC and requires a semantic permission failure before attempting execution.
Both checks pass locally and are also selected with `CC='cc -m32'` in CI. The
full ELF32 interpreter/JIT smoke must pass independently of this focused test.

CI run 43812828469 at commit98e2f69b passed the actual ELF32 build, both focused
page-publication checks and ordinary Dis execution with JIT off/on. The 386 Type
helper test also passes locally after installing its isolated Unicorn dependency:
84 dense/sparse/large-map, executed-initializer, canary and failure checks.

## Remaining AWM frontier

The existing preflight still compiles all 16 startup sources classically and
refuses them in each of the four selected Dis64 profiles. General module data,
import/export shapes and exception tables remain compiler/loader work. These CI
repairs do not establish AWM, Android GUI/APK or physical-device acceptance.
No FlashVM code, vendor pin or coordination journal is changed.

## Second recovery batch: compiler dispatch and active image controls

At a54e7e7, CI confirms the paired console workflow on x64/native ARM64/QEMU
RV64, the Android ARM64 native-link workflow, and actual ELF32 interpreter/JIT
acceptance. The sanitizer matrix passes47/51 jobs. Three remaining failure
families are reproduced and corrected:

* The channels fixture supplies actual String allocators, so it must remove the
  scalar fixture's refusing newstring/newrunes seams before adding those bodies.
  Both WORD layouts and four semantic controls pass locally.
* The compiler's one-export importing-client restriction inadvertently covered
  standalone scalar call files with several exported local functions. Scope it
  to files with import tables. Independent scalar-file proof still applies;
  importing clients retain the one-export restriction. All28 previous source
  programs pass deterministic output and an independent raw oracle,18 damaged
  images refuse, and the full console executes56 C/JIT roots plus28 entryless
  library refusals. Classic output for all28 programs equals the previous
  compiler byte-for-byte. The real AWM16-source frontier stays refused.
* The whole-image omission controls were not being compiled: the shared runtime
  now reaches the production image header through scalar-client/library/chain
  includes before the old local shadow. Include the controlled header first and
  require a test variant marker. Every mutation must match exactly one current
  production predicate. Update the executable-profile mutation for the library-
  aware predicate. All five entry/profile/publication/map/budget controls now hit
  their exact semantic failures, with no malformed guest execution.
* The importing-writer boundary still expected an owned String literal to refuse,
  although literal/length support was already accepted. Retain its original
  source as a positive deterministic/prior-byte compatibility check. The other
  nine unsupported source shapes, including extra importing exports, still
  refuse. No producer admission rule is relaxed by changing this expectation.

The channels and whole-image gates also pass locally with ASan+UBSan and
detect_leaks=0. The independent DAG gate passes94 raw cases and all six active
controls. The scalar writer IR gate passes its positive/proof omission controls.

Reproduce the restored source pipeline using a freshly bootstrapped compiler:

```sh
python3 utils/tests/dis64-source-calls.py --limbo /path/to/new/limbo \
  --baseline /path/to/previous/limbo
python3 utils/tests/dis64-source-call-boot.py --limbo /path/to/new/limbo \
  --root /path/to/built-tree --emu /path/to/built-tree/emu/Linux/o.emu-g
python3 utils/tests/dis64-call-goldens.py
python3 utils/tests/native64-channels.py --limbo /path/to/new/limbo
```

The quick paired-console matrix now also boots these28 source programs with
interpreter/JIT on every target, including library standalone refusals. The
general layout gate confirms all376 typed banks in the16 AWM startup sources,
with byte-identical classic output against the previous compiler. The ordinary
Command producer passes772 deterministic selected files,62 old clients,11 helper
clients and17 importing triples. These checks do not widen AWM file admission.

## Historical FlashVM report versus the current generator

The independent peer gates correctly pin their external reader/report files,
but compared the current whole Python test driver to a historical producer SHA.
Repairing local C-test controls changes that SHA without changing corpus bytes.
Keep the historical report's generator SHA explicit and keep every peer file pin
unchanged. Regenerate all25 committed images and the exact manifest using the
current producer before replaying the independent reader. Report both generator
identities plus successful regeneration in the local integration artifact.

Both partial-schema and structural-binding gates retain all their identity,
schema/binding, unsupported-form and authority controls. A new generator control
adds an actual byte to every produced container and must hit the exact corpus
drift predicate before peer parsing. Both complete gates and all nine controls
pass against the original0517104 peer checkout. Hashes confirm that the corpus,
peer readers and published reports remain unchanged. No shared wire/ABI ID,
vendor revision, FlashVM source or coordination journal is changed.

## Deeper client gates and reproducible AWM evidence

The restored source path reaches the managed-client collector gate. Its optional
codec detection used the shared Tbyte declaration, which is now also present in
scalar-only fixtures. Select codec hooks by the actual production freearray
function instead. All12 managed source pairs pass with actual collector epochs;
both frame/list omission controls still fail at their owning retention predicate.
The11 local/import source pairs exercise codec ownership and actual collector
quanta too; a Command literal/context case passes96 scheduler roots with real GC.

Move the RX observation reset from optional GC instrumentation to clientempty,
after the assertion that all modules and owners are retired. The ordinary scalar
fixture needs this lifetime boundary as well. It is independent of the RV64
empty-client route-count mismatch described below. Cross-target confirmation
remains a CI requirement, rather than a claimed local RV64 execution.

Cache compiled C fixtures within one scalar-client invocation only when the
complete generated interface bytes and selected export ordinal are identical.
Compiler flags and C source stay fixed within that invocation. Every guest pair
is still compiled, decoded and executed in its own fresh process, with all fault,
mode, stack-growth and quanta loops intact. Local UBSan checks retain17 ordinary,
15 CFG,18 numeric,12 managed/GC and11 local/GC pairs. They require14,1,3,1 and3 C
fixture compilations respectively. This reduces repeated compiler work without
removing guest executions. A separate compatibility replay compares all772
Command producer files under classic and selected flags against the prior
compiler and finds byte-identical output.

The optional AWM preflight JSON records compiler/source hashes, classic artifact
hashes, all64 refusal diagnostics and55 verified static LOAD groups (29 builtin,
23 file,3 self). It explicitly sets execution_tested=false. The quick paired CI
uploads it alongside the existing text log. Its default stdout and refusal
behavior remain unchanged; none of the16 AWM startup modules is admitted yet.
Obsolete PR revisions can now cancel their full/ASan workflows; branch acceptance
runs retain their existing noncancelling behavior.

The2516759 revision passes the paired/source-call boots on all three targets,
classic32 and Android ARM64 GUI native link. The full scalar matrix is still a
separate acceptance requirement. A clean local UBSan reproduction was restarted
with these test corrections; no whole-port or Android AWM runtime pass is claimed.


## Backend-specific empty-client routing

Inspection of the RV64 emitter explains the empty client's final route-count
failure: its source uses complete C tails for module/frame/return operations,
while its ILEA entry aliases the complete dispatch entry. It has no separately
observed raw leaf that could increment nativecalls/nativeaccepted. AMD64 has
additional native frame/module/return leaves, so its identical source satisfies
the old blanket assertion. This is a test expectation mismatch, not evidence
that RV64 failed to execute the guest or needs the JIT disabled.

Record whether either loaded file actually publishes a separate raw leaf before
installing observation wrappers. Successful cases must always execute both C
handlers and complete JIT tails. When a raw leaf exists, they must also call and
accept it; otherwise both raw-leaf counters must stay zero. Already wrapped
entries do not fabricate raw-leaf availability. Preserve every result, refusal,
rollback, C/JIT combination and quanta check. The single-word guest publishes an
actual MOVW leaf on every backend: new leaf/bridge omission controls remove one
observation counter increment and must fail the exact owning route predicate.
Both controls and the unchanged empty/single source cases pass with host UBSan;
all three targets run the controls in the full console workflow. RV64 execution
of this latest correction remains for CI to confirm.

## Release-mode compiler predicates and RV64 literal execution

The full58e18df matrix confirms the repaired empty-client RV64 route and reaches
new gates. x64/ARM64 stop at the writer-IR omission control because NDEBUG also
removed the fixture's assert expressions, including calls into the writer.
Keep production translation units under the selected release flags, but undefine
NDEBUG inside the shared typed/layout test translation units before including
assert.h. The generated native-layout wrapper does the same. This restores the
actual positive assertions and semantic omission checks in every composed
writer/tail/local/external/multi fixture without changing production builds.

Under -O2 -DNDEBUG with UBSan, the writer and all three controls, four typed-IR
unit families and their controls, the typed-body C gate, and all376 typed AWM
banks now pass locally. The ordinary classic outputs remain unchanged. The
library raw proof separately passes74 cases and all three active controls with
ASan+UBSan (local detect_leaks=0, not leak evidence).

RV64 reaches the literal/scalar-import gate but its Python selector omitted the
existing RV64 compiler option. Select comp-riscv64.c with the same target header
and runner contract as other client gates. The unchanged38 literal/String/byte
and int-array sources execute3648 C/JIT/GC scheduler roots locally on x64. Their
RV64 execution is deliberately required in the quick three-target console
workflow, together with release-mode writer and typed-IR omission controls. The
full matrix still owns broader runtime acceptance; these changes neither admit
AWM modules nor establish an Android GUI runtime. The 32-bit source/build paths
are unchanged.


## Shared library dispatch observations

The same empty-call route requirement occurs in the whole-library, native
namespace and ILOAD fixtures. Determine separate raw-leaf availability once in
the shared observedispatch function, before wrapping entries. Scalar clients and
all three library fixtures consume that observation. Complete JIT tails and C
handlers remain mandatory; a published raw leaf must be called and accepted,
and a no-leaf case must leave both raw counters zero. No architecture is exempt
and no instruction/ownership/result predicate is removed. Discard saved RX
entry observations only after each complete library caller/module retirement.

The quick three-target workflow now runs all14 providers through construction,
native namespace and scheduled ILOAD, with the same exact source/metadata
checks, plus the38 managed literal clients and release-mode compiler gates.
This catches RV64's next library boundary without waiting for every prior full
matrix gate. Target code still uses the configured cross compiler and QEMU
sysroot; sanitizer flags are retained on the native targets. Local shared
scheduler/vmachine execution and its omission controls pass, along with empty
and single scalar clients and both leaf/bridge observation controls. The full
matrix remains the broader acceptance gate; no general AWM admission is added.

The quick matrix also runs both single-word leaf/bridge omission controls on
every target. The leaf control must fail even if a backend accidentally stops
publishing its expected MOVW leaf: an unobserved route would make the control
incorrectly succeed and the driver reject it. Thus the backend-aware empty-call
check does not let wholesale loss of native generation pass the quick matrix.


The library mutation controls also needed a header-order correction: a nested
scalar-client include selected the unmodified production library header before
the temporary variant. Include the marked control header first and require its
variant macro at compile time. Module/signature/entry/atomic controls now reach
their exact semantic failures. The namespace profile control now targets only
the reader's jit-argument predicate, since a second launch guard was added for
import installation. Both guards stay present in production. All11 library,
ILOAD and namespace omission controls pass locally under release flags with
UBSan, and the disabled namespace bridge performs no constructor/namespace work.
All14 whole-library, namespace and ILOAD positive families also pass locally.

CI at14257fc confirms release-mode writer/tail/local/external/multi assertions,
their active controls and all38 literal/scalar-import programs on each of x64,
native ARM64 and RV64/QEMU. This includes3648 scheduler roots per target with
actual collection. The broader paired workflow and subsequent expanded library
matrix remain separate results, not inferred from these individual gates.

## Source-derived typed module calls

The next full-port boundary composes typed local/multiple/module/external/import
fixtures from the shared scalar runtime. That runtime now supplies explicit
refusing typed-result and root-registration seams. Replace those seams before
adding the actual typed result binder and observed root services; leaving both
bodies caused duplicate definitions before any owning gate could execute.
Scalar-only fixtures retain their refusing services. No production binder,
calling layout, source admission rule or 32-bit build path changes.

All five source-derived native fixtures pass locally with -O2 -DNDEBUG and
UBSan:384 local continuations,480 multiple-call cases,960 whole-module cases,
912 external-body cases and960 three-module owned-import cases, totaling3696
C/RX combinations. Their17 semantic omission controls pass, including exact
classic hash binding, module owner identity, copied import metadata, mapped
slots, constructor rollback and local result ownership. These fixtures consume
source-derived trusted native metadata; this is not general serialized Dis64
module admission or an AWM execution result.

A targeted three-architecture workflow now bootstraps the native compiler and
executes those five complete gates with all their controls. ARM64 runs natively;
RV64 uses the configured cross compiler and QEMU sysroot. Native targets retain
UBSan; RV64 does not require an unavailable target sanitizer runtime. The full
port workflow keeps its existing broader gates.

A separate local replay passes131 raw-proof invocations across nine catalog,
flow, state, CFG, operand, initialization and bound numeric/return/call families,
including all their active omission controls. The result-kind control needed a
more precise marker: conversions introduced a second identical destination-slot
check. Mutate only the result-consumption check immediately before StateRead;
retain the independent conversion check. Every mutation must still match once
and fail its existing semantic predicate. No production proof predicate changes.

The owned-import gate also passes locally with ASan+UBSan, including all960
three-module executions and its three active controls. LeakSanitizer is disabled
for the sandbox's ptrace restriction; this is not leak or whole-VM evidence.

## Android ARM64 typed module-call target

The targeted workflow also builds and executes the complete typed
call gates against static ARM64/Bionic with the pinned NDK r27c toolchain and
Android port headers ahead of Linux fallback headers. A preliminary program
requires __BIONIC__ at compile time and verifies pointer64/int32 under
qemu-aarch64 before the runtime fixtures execute. The existing compiler/runner
interfaces are sufficient; no alternate emitter, binder or production ABI path
is introduced. All normal source generation, constructor rollback and active
omission controls remain selected for this target.

This Android job is new CI work, not a local Android pass: the local environment
has no NDK. Its result must be reviewed independently. Static Bionic/QEMU call
execution is separate from physical-device execution, Android GUI/window/input,
serialized AWM dependency admission and APK acceptance. Existing GUI native-link
checks remain separate. Linux RV64's five typed call gates already pass in the
first targeted workflow run; native x64/ARM64 remote results remain pending at
this checkpoint.

The expanded workflow includes direct typed local bodies and forwarding chains
as well as the five restored gates:4272 C/RX combinations and21 active controls
per target. The additional two gates pass locally under the same release/UBSan
flags. The separate trusted-local gate also passes448 cases in each WORD model.
Android evidence remains pending until the Bionic job actually completes.

Reproduce the targeted Linux gates with a freshly bootstrapped native compiler:

```sh
for gate in calls tail local multi module external imports; do
  python3 utils/tests/native64-typed-$gate.py --limbo /path/to/host-tools/limbo
done
```

For the Android run, set NATIVE64_ARCH=arm64, NATIVE64_RUNNER=qemu-aarch64 and
NATIVE64_CFLAGS='-O2 -g -DNDEBUG -fno-strict-aliasing -static'. CC selects the
NDK's aarch64-linux-android29-clang, followed by -I paths to
Android/arm64/include and Android/arm/include before the scripts' fallback
paths. The workflow's Bionic/width probe must pass first; do not substitute a
Linux cross compiler when interpreting its Android result.


The module construct-only and external/import materialize-only modes also pass
locally with release/UBSan flags and their applicable omission controls. Native
ARM64 and RV64 pass the first complete targeted five-gate matrix; x64 remote
acceptance is still running. Subsequent typed workflow revisions retain that
active run and only the latest pending batch, avoiding cancellation of lengthy
control compilations for a documentation or additional-target update.

## Real AWM startup data images

At c4544dc the seven typed-call gates and all21 controls pass on x64, native
ARM64, RV64/QEMU and Android ARM64/Bionic. This is now target execution evidence,
not merely Android GUI linking.

The next stage consumes actual startup source MP layouts and independently
decoded classic initializers. Thirteen flat startup data images now relocate
all scalar/opaque records and construct actual owned Strings at widened root
slots. AWM itself grows from736 to1368 bytes with all142 initializers accounted
for. The local gate checks every value, default H/zero storage, two actual newmp
clones, String retain counts, retirement and all877 construction allocation
refusals across JIT publication off/on. The whole13-image gate passes ASan+UBSan
with detect_leaks=0; it is not collector, instruction-execution or leak evidence.
Scalar corruption, a removed root-map bit and omitted newmp retain each hit their
specific semantic failure. The common module test unit is extracted without any
change to its generated C bytes, so this gate reuses the existing owning code.

The atoolbar/titlebar/sh nested initializers remain explicitly unsupported. Old code-coordinate values in
opaque tables remain unrelocated. No AWM instruction, general Dis64 module
admission or Android GUI/APK result is claimed. The four-target workflow now
requires these data gates too. See doc/awm-native64-data.ru.md for exact scope,
reproduction and the remaining route to AWM.

## Limbo launches a differently named Command implementation

The startup route is already explicit in appl/cmd: emuinit loads
/dis/wm/awm.dis as Command and calls mod->init(nil,args); sh.b/sh.y perform the
same separate LOAD and init call with their draw context. The host starts the
first Limbo module through disinit/schedmod. No C-side AWM init hook is needed.

The fixed Command file producer and raw image checker incorrectly also required
the implementation label to be Command. Preserve the actual implementation name
in the existing Module/Strings record and admit any bounded valid label under
the same private fixed-shape policy. Requests still name Command, exports still
name init, all signature bytes must match and all body/map/entry/ownership checks
remain. Enforce the single-public-export boundary before selecting the local
Command writer, preventing unrecorded public exports. No wire ID or classic ABI
changes; the full Wm/sh/emuinit sources remain outside general file admission.

The new alias gate checks the real appl/cmd witnesses and real Wm.init hash
0x4244b354 against the caller's module/sh.m import. Finite Command, Wm and
Application implementations pass243 raw image/match/refusal cases and54 full
OS interpreter/JIT roots, including an explicit Limbo Emuinit caller, local
helpers, nested imports, no-auto-init, explicit init failure, wrong argument
type/missing file and default/entryless startup refusals. A long255-byte label
passes;256 bytes emits no selected payload. Extra public exports refuse. The
Russian launcher tutorial is compiled and booted by the same gate.

All772 existing Command source files retain exact classic and selected bytes
against the previous compiler. The producer boundary,41 ordinary raw cases,
240 importing-library raw cases and all six omission controls pass. Wm-named
providers also pass six direct and three nested managed file-call families
(864 scheduler roots); nested ownership passes ASan+UBSan with detect_leaks=0.
The243-case raw alias gate passes ASan+UBSan too. A clean classic-mk full x64
console/pair boot was built with the updated compiler/loader for these tests.
AWM preflight still refuses every startup source in all four selected profiles.

The paired-console matrix now runs the full Limbo caller/alias gate on all
three architectures. The typed/Bionic matrix runs the raw gate and actual
managed Wm-named file-call families. Target results on this new revision remain
pending. See doc/dis64-command-alias.ru.md for architecture and reproduction.

## Complete startup MP data, including static arrays

The Command alias Bionic gate exposed two fixture integration errors: missing
private lib9 strtoll/strtoull owners (03287fc), then global free wrapping also
intercepting static Bionic fclose's private buffers (a58617b). Link the real
converters and detect Bionic from target headers: Inferno runtime frees still
pass through the strict tracked free_ boundary, while libc keeps its own free.
The complete six-direct/three-nested families pass locally both with private
numeric names and with Android allocator headers in a static host executable.
This host header check is not an Android execution claim; target CI is separate.

Extend the opt-in compiler diagnostics with AST-derived primitive/String array
element kinds and old/native strides. Ordinary Dis bytes remain unchanged.
The independent classic reader can now explicitly decode DEFA/DIND/DAPOP with
the runtime's four-level address stack, bounds, overlap and truncation checks;
its default flat-only policy remains. Four positive graphs and 24 malformed
records actively check that oracle.

All 16 real startup MP images now construct, including the previously refused
atoolbar/titlebar/sh. They contain 608 MP Strings, 251 ordinary scalar records,
365 opaque/table records and 13 arrays (22 contained Strings, 546 numeric
elements). Real dtype/heaparray/initmem/String factories build the arrays;
real newmp/arraycpy make independent headers and backing data in two MP clones.
Shared contained Strings traverse refcounts 1 -> 3 -> 2 -> 1. Exact values,
element maps/strides, H/zero defaults, external descriptor ownership and complete
retirement are checked. All 1680 construction and 94 clone allocation-failure
positions roll back, including partially copied array trees and temporary pins.

The full x64 data gate passes ASan+UBSan with detect_leaks=0. Three existing
controls plus three array controls (element map, aliased clone, numeric value)
must produce their specific semantic refusals. The independent typed-layout
gate still proves 376 storage plans across all 16 sources and compares exact
classic outputs with the preceding compiler. The four-target typed workflow
and the complete port driver require the expanded data and refusal gates.

This is data construction and MP cloning, not AWM code execution or general
Dis64 source/file admission. Opaque tables still retain old code coordinates;
compound array elements/multidimensional initializers remain refused here.
No classic ABI, wire ID or shared FlashVM schema is changed. New target results
remain pending until their jobs finish.

## Execute numeric tables from checked Limbo bodies

The 1be2c4e paired-console matrix and classic32 job passed; its typed ARM64,
RV64 and Android ARM64/Bionic jobs also passed. The Android GUI native-link
job passed as a link check, not an AWM execution claim.

Lower compiler-owned ICASE/ICASEL/IGOTO with typed source storage and local
row/default ownership. Include table edges in definite-result analysis, reset
table metadata on multi-call replay and reject entries into call preparation.
Optional diagnostics retain their existing body schema plus table metadata;
ordinary bytes are unchanged and an independent classic reader checks bounds,
targets and maps.

Add private table metadata to the trusted module constructor. Validate complete
MP spans/maps, count/row budgets, matching instructions, call-island exclusion
and initializer/table overlap before allocation. Relocate local targets using
the new function offsets and retain Dis ordinals for the existing leaf JIT.
No native-PC narrowing, new wire ID or general file admission is introduced.

Four actual Limbo functions execute 2280 interpreter/JIT module-call roots,
including sparse ranges, dense computed goto, signed minima/maxima, stack
growth and three quanta. A private prefix shifts the functions relative to the
classic file, so unchanged PCs cannot accidentally pass. The x64 gate sees 696
C table operations and zero C table fallbacks in JIT mode. All 20 metadata
refusals occur before allocation, and all 39 constructor allocation positions
roll back. Active row-owner and target-relocation omissions must fail their
semantic assertions. The complete new gate passes ASan+UBSan, detect_leaks=0.

Existing typed body/multi/external IR controls and module-construction controls
pass. All 1024 distinct accepted source/flag pairs in the Command producer
retain exact outputs against the preceding compiler; its 772 deterministic
files and existing source refusals still pass. The startup typed-body report
finds 15 accepted bodies and 345 whole-body refusals. This is not general AWM
admission. The expanded eight-gate typed matrix and full port driver now require
the table gate. See doc/native64-typed-tables.ru.md for reproduction and limits.
