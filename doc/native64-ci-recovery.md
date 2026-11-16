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

## Remaining AWM frontier

The existing preflight still compiles all 16 startup sources classically and
refuses them in each of the four selected Dis64 profiles. General module data,
import/export shapes and exception tables remain compiler/loader work. These CI
repairs do not establish AWM, Android GUI/APK or physical-device acceptance.
No FlashVM code, vendor pin or coordination journal is changed.
