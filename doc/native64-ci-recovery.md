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
