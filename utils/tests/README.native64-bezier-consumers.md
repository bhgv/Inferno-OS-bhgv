# Four cubic Frame64 consumers

Run `python3 utils/tests/native64-bezier-consumers.py --limbo /path/to/limbo`.
The driver emits fresh Draw int32/ref64 declarations and compiles the actual
Image.bezier/bezierop/fillbezier/fillbezierop consumers and their helpers.
Each WORD32 and WORD32-file profile checks6,144 cases/259,316 active predicates.
Independent fixed offsets and field widths verify the int32/ref64 header, natural
pointer padding and explicit-operator tail, beyond shared generated declarations.

Independent C records hold caller values, signed32 endpoints, all12 operators,
invalid operators/radii, end/winding bit patterns, source/destination identity
and foreign-display decisions. Real generated frame storage has byte guards.
Controlled lookup/lock callbacks overwrite the live frame to detect late argument
reads; these are deterministic boundary observers, not a concurrent-VM claim.
Both acquired exclusion and native local-display no-lock paths are exercised.
Captured destination must survive through checkflush; all acquired locks retire.
Default wrappers ignore the extra operator pattern; explicit wrappers preserve it.

Native curve/lookup/lock/flush are explicit bounded services. The curve services
return both native success1 and refusal0, preserving the VM methods' existing void
interface and absence of a rendering acknowledgment. Native subdivision, point
packing and rasterization are NOT replaced in production and are not executed by
this fixture; their prior separate native tests remain. Borrowed Draw_Image test
identities are never fabricated Heap headers or roots.

Seven exact-stage controls are available with `--control capture|source|flush|
operator|radius|foreign|unlock`. They stop at semantic exit10 before unsafe
continuation. Compiler/resource/sanitizer errors, signals and timeouts never count.
NDEBUG leaves all checks active. `CC`, `NATIVE64_ARCH=amd64|arm64|riscv64`,
`NATIVE64_CFLAGS`, `NATIVE64_RUNNER`, `NATIVE64_TMPDIR` select the toolchain and
runner; actual native compiler identity is checked. Local default is O3
nonrecovering UBSan, hosted ARM64/x64 uses O3 ASan+UBSan with ordinary leak checks.

This closes four bounded C-consumer name entries, not guest CALL admission,
resource ownership, arbitrary pointer provenance, collector integration or GUI
readiness. Callers still provide live frames/images under VM admission. Managed
Draw registration stays closed; no Android/Tk/JIT implementation is changed here.
