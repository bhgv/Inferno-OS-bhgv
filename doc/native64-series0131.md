# Native64 series0131: real numeric Module/Jit rollback through nested VM errors

2026-11-04, experiment. Harden prepare64numeric's output-address geometry before
its first typed read: nil/H, unaligned and wrapping output pointers refuse before
allocation or output writes. Occupied output and invalid Jit choices also preserve
the caller's slot. This does not promise mapping/provenance discovery for arbitrary
native pointers; the existing live/disjoint/immutable native caller contract stays.

A new native fixture constructs all eight independent accepted raw numeric images
with jit=0 and jit=1. Every actual native allocation is faulted both by nil return
and by the actual VM error transfer. Existing production nested kprotect, storage
construction, attach, numeric lowering, compiler and unload bodies remain real.
The Jit table allocation is included; no fake successful compiler is supplied.
Actual mmap/munmap/mprotect are linker-wrapped to observe exact mapping identities,
extents and RW-to-RX publication. Both mapping allocation and RX publication refuse
once by ordinary OS failure and once by VM error. The real compiler must retire
its partial table/mapping before the outer constructor retires Module and lease.
Native allocation guards, Type/Heap owners, external-root metadata/cache, actual
modules list, error-stack depth and mapping counts all return to baseline.

Local O3/NDEBUG nonrecovering UBSan:8 raw images,304 allocation-return refusals,
304 VM-error refusals,32 mapping/publication refusals,24 mappings retired,8 RX
publications and516788 active checks. Fifty-six argument-refusal cases run before
allocation. Two exact semantic controls omit partial lease retirement or real
mapping release; only exit10 at root-baseline/mapping-baseline is accepted, never
compiler/sanitizer/crash/timeout. The fixture constructs but does not execute guests
or run the collector. It complements, not replaces, series0130's separate C/RX and
complete-collector fixtures. No GC-during-execution or managed scheduler/frame-root
acceptance follows. Ordinary hosted leak detection remains enabled.

Classic mk rebuilt actual load64.o successfully. The native ARM64/x64 numeric job
now includes this positive and both controls. Hosted exact-source receipt follows
publication. No new readmod/file admission, cache identity, wire/feature/ABI IDs,
Frame64/default43 or immutable explicit calls choice. Fast opcode counts remain
ARM64/x64166/173, RV64M103/173, C dispatch173/173; no Android/AWM/RV64 hardware or
complete workflow-matrix claim. No changes to Java/Tk/security/crypto or old
MIPS/SPARC/PPC backends.

Series0130's previously queued source9cd6c85cf325657d52b96f4a8ac6451307ed8b35 now
has actual decoded successful native logs: workflow41863971496, ARM64 numeric
job126693204362 and x64 numeric job126693205036. Both show1020/19293 guard matrix,
five map controls;8/8/148/96/125339 constructor/GC counts per WORD32/file, legacy
refusal and four constructor controls; actual C/RX15 images/6 fast completions/
1 decline/12303 checks plus strict fallback control. O3/NDEBUG ASan+UBSan with
ordinary leak detection. This is acceptance of those bounded separate fixtures,
not Android GUI, managed execution/imports/frame-result roots or the full matrix.

## Exact-source native ARM64/x64 acceptance

Source4671aa6f5f32d27e97bd14dad3dc8e22f4456199, workflow41870797516:
ARM64 numeric job126716982376 and x64 numeric job126716982736 both completed
successfully; actual decoded logs read, checkout SHA and O3/NDEBUG ASan+UBSan flags
verified. Ordinary leak detection stays enabled. Each shows the new8-image fault
fixture with304 allocation-return refusals/304 VM-error refusals/32 OS refusals/
24 mapping retirements/8 RX publications/516788 active checks, plus both exact
lease/mapping controls. Existing1020-image numeric-map matrix/five controls, both
WORD32/file constructor/collector passes/four controls/legacy refusal, actual
C/RX15 images/6 completed fast returns/1 decline/12303 predicates and strict
fallback control also pass. No full workflow-matrix, managed-import/frame-root,
GC-during-execution, Android/AWM or physical RV64 claim follows.

The deterministic jit-readiness.py --check report also passed locally:173 complete
rows, fast/C modes, x64/ARM64/RV64 WORD32/64 and capacity checks. Published opcode
counts remain unchanged. Native evidence:
https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41870797516
