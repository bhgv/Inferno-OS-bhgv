# Experimental local-call catalog preflight: checkpoint 0073

Status: private proposal checker, NOT agreed file-call admission. The three
questions in dis64-local-call-proposal.md remain for FlashVM review. This header
is included only by a test, not by the loader, compiler or shared pilot checker.
No wire section, opcode, signature version, profile or feature ID is allocated.

The checker reuses the accepted raw string-pool, Type, signature and exports
codecs. It revalidates those raw extents rather than trusting mutable borrowed
view counts. It then checks the proposed subset: a nonempty bounded function
catalog, unique entry PCs, module entry at0 with matching frame and void/no-arg
signature, np0 aligned frames, canonical scalar argument layout from byte64 and
a non-overlapping final eight-byte result cell. Unordered exports are sorted
into proposed disjoint PC intervals. Shared frame Types are legal; duplicate
entry PCs are deliberately refused by this draft policy.

Bounds are explicit implementation policy:64 functions/parameters,65536
instructions and1MiB per metadata payload. Object size has a separate caller
limit. All arithmetic is bounded before alignment/addition. No heap allocation
is performed. The complete result is assembled in bounded private scratch and
published only after every record and interval check succeeds. Failed checks
leave output records, guards and the written count unchanged. Inputs are
accessible immutable spans; caller output/count storage must be disjoint.

The independent Python encoder/oracle supplies628 cases: reordered catalogs,
all three-parameter scalar-kind combinations,0..64 parameters, alignment/tail
boundaries, reused Types, exact limits, Unicode names, every truncation of a
representative payload, trailing bytes, bad signatures/IDs, duplicate names/PCs,
mapped frames and wrong module-entry records. The native C driver compares
every successful field/parameter and all unused output/guard bytes. Two active
removal controls disable duplicate-PC rejection or the private result-cell
reservation. They must hit a semantic predicate with status10, not a compiler
error, signal or sanitizer report. The same portable driver runs on native
ARM64/x64 and RV64/QEMU through native64-port.py.

This does NOT inspect function instructions, prove CFG ownership/argument setup,
check dynamic bool values, definite assignment, result lifetimes or call graphs,
bind a native result pointer, execute a call or make a callable Module. It does
not prove that existing exports are sufficient for the final shared verifier.
It provides executable geometry/atomic-failure evidence for reviewing the
proposal; those unresolved choices can still change. Existing43-op np0 file
admission, ordinary-Dis LEA and source inlining remain unchanged.
