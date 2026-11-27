# INFERNO-0166 — type-driven storage plans across the AWM startup closure

2026-11-13. Implementation starts from b20318dd / INFERNO-0165. FLASHVM-0180
was read and preserved by advancing the documentation base to 397ccede. Its
acknowledgment of the Connection report is received. Original-owner routed
callbacks, actor identity, Add/Unknown semantics and concurrent GC remain peer
work; none of that runtime or its private wire operations is imported here.

## New compiler foundation

`limbo/dis64-layout.c/.h` compute immutable, compiler-owned Word32/ref64 plans
from actual Type/Decl records. This removes the need to invent another
hand-assigned frame merely to describe an ordinary managed function's storage.
Arguments start at 64; Word values stay four bytes and references become aligned
eight-byte leaves. Nested tuple/ADT/exception fields are placed recursively.
Globals, parameters, locals and compatible reused local slots are supported.

The address resolver distinguishes scalar reads, complete managed references
and complete borrowed native addresses. It refuses padding, cross-field spans
and partial pointers. Root roles are checked against every old descriptor bit;
source AST sizes/offsets/maps/signatures remain untouched. Plans have explicit
object/leaf/depth/alias-work bounds and owned cleanup after partial refusal.

Concrete pick variants get separate tag/common/payload plans. Unspecialized
pick bases refuse instead of unioning incompatible pointer maps. Array/list/
channel variables are references; their element types can be planned separately.
There is no new heap descriptor installation or generic allocation admission.

Compiler alt tables explicitly distinguish direct Channel/Array roots from
borrowed addresses of channel cells and payloads. Both addresses widen, but only
heap references are marked. Numeric/String case tables distinguish values/roots
from code PCs; import-name tables retain opaque bytes and cannot be read as
ordinary scalar data. Table initialization, PC rewriting and runtime execution
still require dedicated integration.

## Evidence

The opt-in `LIMBO_DIS64_LAYOUT=1` diagnostic reports 376 complete storage plans
across all sixteen actual AWM startup sources: emuinit, awm, atoolbar, arg,
bufio, env, filepat, readdir, string, wmsrv, wmclient, winplace, wmlib, titlebar,
tkclient and sh. Every FP and MP bank is accepted. A separate classic Dis reader
checks descriptor ordinals, extents and all root bits against emitted files,
and independently rebuilds target maps/checks disjoint placement. All sixteen
ordinary modules are byte-identical against the pre-change compiler. All sixteen
`-64l` builds retain their refusal and emit the same plans, with no Dis64 output.

The C fixture compares actual native sizeof/alignment/offsetof for nested
records and parameter/local frames. It checks picked layouts, reusable locals,
alt borrowed/root addresses, case tables, malformed metadata, bounds/depth and
complete owned-allocation retirement. Three active omissions must fail the
specific borrowed-root, descriptor-role and code-as-scalar semantic predicates.
AMD64 ASan/UBSan and ARM64 UBSan/QEMU pass; the full host compiler also completes
the sixteen-source matrix under ASan/UBSan. Hosted LeakSanitizer cannot run under
ptrace; fixture allocations are balanced explicitly. These are compiler/layout
checks, not new managed C/JIT execution or physical Android evidence.

191 existing private managed/Sys image cases, including all twelve Connection
cases, are byte-identical against the previous compiler even with layout
diagnostics active. RU/EN documentation includes reproduction and API ownership.

## Remaining integration

These are reusable typed storage/relocation plans, not a newly admitted general
managed image profile. The next compiler work is opcode-aware operand lowering,
caller/callee result binding, payload descriptor emission, initializer/table
writers and matching loader admission. Exceptions and variadic frame tails
remain separate requirements. Existing executable pilot profiles and runtime
ABI guards stay in place. No public Dis64 ID, DVP version or peer concurrent
adapter contract changes, and AWM/Android GUI APK readiness is not claimed.

[English guide](dis64-typed-layouts.md) · [Русское руководство](dis64-typed-layouts.ru.md)
