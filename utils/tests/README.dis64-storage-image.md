# Joined Dis64 structural storage inspection

`python3 utils/tests/dis64-storage-image.py` compiles the real allocation-free
`include/dis64-storage-image.h` reader at O3/NDEBUG/nonrecovering UBSan locally.
Hosted native ARM64/x64 jobs use ASan+UBSan with ordinary leak detection enabled.
Six controls (`types`, `data`, `imports`, `exports`, `code`, `module`) each require
exit10 at their named semantic witness; compiler, sanitizer, crash, timeout and
unrelated failures are rejected.

The independent Python serializer writes the raw envelope, metadata, descriptors,
Rune pool, scalar signature/initializer and interface records using `struct`.
It never calls production writers.497 cases/9,007 active C predicates cover:
managed storage and managed entry-frame structure without a GC grant;65 types
including unused managed descriptors; library with zero instructions/types;
nonempty library exports; optional missing versus present-empty DATA/interfaces;
scalar bit patterns including binary64 NaN/signed-zero bits;
unsorted non-overlapping writes; aliases with distinct equal-content pool IDs;
conflicting imports and duplicate exports; every byte-prefix truncation; invalid
module/type/name/entry/DATA/interface joins; all eight independent caller policy
fields; directory rotations and an opaque unknown optional section. Source bytes
are compared after every call. Refusals preserve every byte of poisoned result
storage, including padding.

`Dis64StorageLimits` contains explicit implementation resource policy, not shared
wire maxima. Types, pools, interfaces and DATA are joined to one borrowed immutable
container. `present` is a private local section-presence bitset, not a feature ID.
Absent DATA is recorded as absent; no default initialization or allocation is
performed or authorized. Present DATA is checked against the actual declared
module-data descriptor. The reader does not reject unused managed descriptors or
empty IMPORTS merely because the scalar runtime currently does.

The code view is deliberately restricted to **direct-mode instruction framing**
and existing opcode IDs. It consumes the complete instruction population before
entry/export PCs can rely on that count. It does not prove operand roles, numeric
kinds, definite initialization, control flow, return/result slots or call safety.
Indirect modes, CONTROL and mandatory extensions are unsupported. Optional unknown
payloads receive envelope validation only. Export frames are structural size>=64/
alignment8 checks; no parameter placement, managed header-root or lifetime proof.
Imports are local names/signatures/alias validation, never dependency resolution.

Success is neither a Type/Module/Heap/root/cache/RX owner nor an execution
capability. This helper is not wired into the loader's admission decision; default
43-op policy and the immutable explicit dis64-calls profile stay unchanged. The
new composition is available for independent peer review and later loader staging,
not a claim of whole-file semantic or Android readiness.

Native caller contract: source bytes, policy and result occupy independently live,
mapped, disjoint storage. All borrowed source spans remain immutable and mapped
through the complete view lifetime. Aggregate assignment does not handle aliasing
or acquire source lifetime. This is an API precondition, not a new wire rejection.
