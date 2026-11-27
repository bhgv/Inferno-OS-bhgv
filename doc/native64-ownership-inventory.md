# Native64 ownership conversion inventory: initial owning-source audit

Status: inventory and protocol constraints, not an atomic/parallel-heap
conversion. This covers selected owning paths in Inferno experiment after0084;
it is not an exhaustive claim about all C modules or every historical backend.
Existing exclusive VM/heap admission remains required.

| Surface | Current owning source | Conversion requirement |
|---|---|---|
| Heap count | `include/interp.h`: native `ulong Heap.ref` | Use actual native field width/alignment, not language WORD or an assumed universal uint64. |
| Type count | `include/interp.h`: signed `int Type.ref` | Checked nonzero/positive retain/drop, explicit overflow refusal and unique final ownership. |
| Heap destruction | `libinterp/heap.c`: `destroy`, `freeptrs`, heap/type finalizers | Preserve retirement/callback ordering, including the existing high-gchalt C-stack protection. LAST cannot blindly mean immediate free. |
| Type destruction | `libinterp/heap.c`: `freetype`, `dtype`, array/channel finalizers | Account for builtin and retained descriptor owners, initialization/destruction callbacks and failure rollback. |
| Copies and construction | `libinterp/heap.c` `incmem`; `libinterp/xec.c` ownership helpers; link/load/stack/string paths | Pin the target while its source slot is protected; retain a separate destination owner before publication; release replaced owners outside locks. |
| External roots | `libinterp/gc.c`: `ptradd`, `ptrdel`, `ptrmark` | Root registration is separate from a strong count and marking. These functions do not retain/drop Heap.ref. |
| Marking/collector | `Setmark` in `include/interp.h`; `libinterp/gc.c` root traversal and sweep | Plain color/nprop and portioned traversal are not concurrent-safe merely because each portion stops mutators. |
| ARM64 direct code | `libinterp/arm64-emit.h` heapref/heapcolor/nprop and typeref paths | Replace/audit every relevant emitted access and fallback together; changing C helpers or offsets alone leaves plain native accesses. |
| x64 direct code | `libinterp/amd64-emit.h` heapref/heapcolor/nprop and typeref paths | Same count/publication/marking protocol as C; account for flags, native call ABI, aliases and refusal before mutation. |
| RV64 delegation | `libinterp/comp-riscv.c`, `riscv-emit.h`, complete C dispatch | Current owner-changing commands such as MOVP/MCALL/RET use C paths. This emitter/layout has no heapref/heapcolor/nprop fields; newly added direct paths must join the same protocol. |
| Register/context owner | global `REG R` in `libinterp/xec.c`, saved `Prog.R`, scheduler/native entries | Per-context register/root publication and checked native transitions precede simultaneous mutators. Heap locks do not protect global R. |
| Mode selection | `disc_register` / `disc_tryattach`, JIT compile/cache, xec/instrumented/direct entries | Binding must be explicit, compatible and quiescent; byte identity or a callback pointer alone is not a container proof or context lease. |

## Native layout and instruction selection

`comp-leaf64.h` derives heap-relative offsets from sizeof/offsetof and sets
`abi.refbytes = sizeof(Heap.ref)` independently of `abi.wordbytes`. Current
native owner paths contain plain loads/stores and marking writes; their existing
native-width guards are not an atomicity protocol. Heap/Type layout stays unchanged.

For ARM64, LSE is an optional ISA capability. A future direct CAS path needs an
explicit capability decision or baseline exclusive-instruction/helper path;
an ARM64 architecture label alone does not authorize LSE instructions. For RV64,
I/Zmmul/M selection is not evidence for the atomic A extension. A future atomic
path needs its own capability/fallback contract. Native C primitive availability
is separate from the ISA emitted by the JIT. No new instruction selection is
implemented in this audit.

Adding object guard words changes native H2D/D2H and generated offsets. Rebuild
all affected native code/adapters and fence caches/entry ABI before use. Static
AOT byte identity does not prove compatibility with a different native header
layout. Such changes do not by themselves require Dis64 wire IDs. Unsupported
or unconverted backends retain exclusive fallback; MIPS/SPARC/PowerPC JIT code
is outside this series and remains untouched.

## Lifetime and transition constraints

`mklinkmod` constructs a Modlink from an already-owned Module reference; it does
not itself increment Module.ref. Its caller may transfer that reference into the
instance. A count of one therefore does not prove that no instance/frame can use
the module. This audit introduces no reference-count-based quiescence detector.
Binding still requires the owning caller's VM-lock/prepublication protocol.

The checked count proposal's LIVE/LAST result must be consumed without rereading
counter storage after a successful release. A nonfinal releaser can overlap a
different final owner freeing storage in a future parallel implementation.
Source-slot lifetime acquisition, external root publication, mutable payload
locks and GC marking remain separate obligations.

Object guards require caller-held, address-stable storage. Strong ownership is
not automatically a pin against future moving GC. AVAILABLE after wait grants no
ownership: reenter the correct VM state and retry. Held-guard accounting fences
transitions; it is not automatic cleanup through VM longjmp/error/callback paths.
Release before safepoints, native I/O, callbacks or VM error transfer. Registry
fallback must not recursively acquire its own mutex.

## Evidence boundary

FLASHVM0076's local/shared proposal was read at
`b1a3099d6ffa0b78954f8e7dc964b3673da010cf`. Local frame slots do not make their
referenced payloads local. Analysis is per Modlink/MP instance and alias graph,
not Module/code name, Module.ref, cstep identity or absence of spawn syntax.
Unknown imports/native calls use the conservative shared path. Before a second
executor is admitted, every old local alias and reachable mutable child must
switch to synchronized operations through proved specialization/deoptimization
or conservative classification from allocation. A shared bit alone is insufficient.

An independent native strong-owner ledger is not a mapped-root list or automatic
Dis error/retirement cleanup. Native callbacks that must return normally cannot
simply be substituted for existing VM error/longjmp paths. No native scope API,
escape analysis or local/shared opcode is imported or assigned a wire ID here.

Read FlashVM0075 and its immutable `OBJECT-GUARDS.md` at
`66c5e094e0ccb9f31796eb9fd093e463efb8e652`. Its native object tests and RC
foundation are peer evidence, not Inferno ARM/RV execution or managed-heap
conversion. No peer primitive is imported here. Existing real target JIT/heap
tests do not certify concurrent plain/atomic mixing, full managed collection
under object locks, cancellation/unwind cleanup or arbitrary C-module safety.

The next conversion must inventory remaining callers, decide native context and
field APIs, preserve C-stack/collector retirement semantics, then audit C and
emitted paths with target tests and active failure controls. Exclusive admission
cannot be removed by replacing only one retain/drop function.
