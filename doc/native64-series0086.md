# Native64 series 0086: public compilation mode ownership

Public `Loader.compile` now refuses any nonnil static `cstep` before MP mutation
or compiler entry. This closes the reverse composition path left after 0085:
attaching static translation to an existing JIT was refused, but a guest could
still request JIT compilation of an already statically translated module.
The existing -1 result and a specific diagnostic are preserved on refusal.
Trusted owning C constructors retain their separate compiler entry and mode contract.
No native emitter, managed layout or shared Dis64 wire ID changes.

## Validation and scope

`utils/tests/native64-loader-mode.py` extracts actual loaderword, loadercompile
and Loader_compile production bodies with native headers. Compiler and protected
entry are injected observation boundaries; the test does not emit or execute JIT
code. The known profile symbol is only an identity stub, never its VM body.
Opaque cache records are not RX provenance proofs.

Default optimized nonrecovering UBSan passes on local x64 for WORD32 and WORD64:
1024 state/request/result cases, 96 compiler calls, 896 mode refusals and 32
nil/H owner/result-alignment cases per model. Whole Module/Modlink snapshots,
result byte boundaries and MP restoration are checked. Removing the guard must
produce semantic exit 10, never a compiler error, timeout or sanitizer failure.
Existing loader-runtime tests separately cover actual protected nonlocal recovery.

The new fixture and removal control are integrated into all three classic-mk
port jobs and the focused native ARM64/x64 ASan workflow. Target results for this
series remain pending until its source commit completes CI; 0085 results do not
certify this new source. Local ASan is unavailable because of LSan /proc access.

ARM64/x64 availability stays 166/173 native paths plus C support for all 173.
RV64 I/Zmmul/M stays 87/90/103 native paths plus C support for all 173.
The new-format file admission gate remains 43/173. These are distinct measures;
no Android APK, GUI readiness or general file CALL/LEA8 activation is claimed.
