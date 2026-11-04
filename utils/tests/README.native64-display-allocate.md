# Staged Frame64 Display.allocate

Run `python3 utils/tests/native64-display-allocate.py --limbo /path/to/limbo`.
The driver freshly emits Draw int32/ref64 declarations and runs both WORD32 and
WORD32-file models. Current coverage is39 Frame64 scenarios/16,033 active checks
per model, plus four malformed-result checks. NDEBUG does not disable predicates.

The fixture reuses the actual Heap/Type/refcount/protected-allocation/destructor
closure from native64-draw-owners. It additionally compiles the unchanged cached-
display helper, the new private constructor and actual gc.c ptradd/ptrdel bodies.
Root metadata is real allocated storage with exact registry bookkeeping. Parent
maps and child reachability are independently observed before nested managed
allocation; an observer is not a tracing collector.

Cases cover a result overlapping the device cell, byte-packed result cells,
empty-H device, cache reuse/self-replacement, lock/native-init/DRef/managed-owner/
root-pin/five-child allocation refusal, and optional font/build/cache service
outcomes. Native defaults have deliberately different geometry and channels.
All owners/types/roots/locks return to baseline; cleanup may overwrite diagnostics
but the primary construction error must survive. Successful cyclic display/image
ownership is explicitly broken by test teardown; this does not simulate GC sweep.

Seven controls (`--control input|publication|image|display|unlock|root|edges`) omit
one production safeguard and must reach an exact semantic exit10 before unsafe
continuation. Compiler/resource/sanitizer errors, signals and timeouts never
satisfy controls. Leaked control storage is retired only by the fixture's stop
ledger; that is not production rollback evidence.

Native initdisplay/closedisplay, lock, font/cache, String conversion and device
transport are bounded services. The cached context uses public Prog/TkTop records
without a Tk initializer/event loop or any Tk source change. No forged Heap roots,
native-key substitutes, new wire IDs or execution admission are used.

`CC`, `NATIVE64_ARCH=amd64|arm64|riscv64`, `NATIVE64_CFLAGS`, `NATIVE64_RUNNER` and
`NATIVE64_TMPDIR` select the toolchain/runner. Actual compiler identity is checked.
Local flags default to O3 nonrecovering UBSan. Focused native ARM64/x64 jobs use
ASan+UBSan with ordinary leak detection and a freshly bootstrapped Limbo. The full
native64 pipeline runs both models and all controls too.

Caller frame/input/result owners must remain live under VM admission. The
existing initial ptradd metadata allocation must not yield/collect an unpinned
new owner; destructors must not throw. A real collector epoch, consuming-callback
unwind, native font-cache internals, refresh-worker lifetime, renderer/device and
Android/AWM execution remain separate integration work. Managed Draw registration
stays closed. This fixture is bounded C-consumer evidence, not GUI readiness.
