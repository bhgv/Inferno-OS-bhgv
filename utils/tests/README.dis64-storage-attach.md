# Retained Dis64 storage in code-free native Modules

attach64storage(target,source) adds independently owned storage references to an
empty, exclusive, unpublished DONTCOMPILE Module. The caller holds ordinary VM
exclusion and keeps the genuine opaque source lease alive/unchanged throughout
attachment AND until the Module has an independently valid root. The source is
not consumed. Target/source/policy/native spans are disjoint and independently
mapped/live. This does not install prog/entry/frame/export/import/code/cache
identity or relax file/JIT admission. Namespace publication still needs its own
identity/profile/semantic proof; no readmod route calls this API.

Before allocation it checks the empty target (including existing-code/link/cache
state), all descriptor retains, actual MP Heap retain/pin/type membership and
native model. The one fallible native table allocation precedes every retain or
target mutation. Original ordinal identities are retained into that independent
array; MP is retained/marked and its actual ordinal recorded. Failure preserves
all source/target bytes and owners. Success leaves the original source root intact;
RC alone never substitutes for a collector root. Calling free64storage before a
real Module/root owner exists is outside the API contract.

Run `python3 utils/tests/dis64-storage-attach.py`. Independent raw storage fixtures
plus MP ordinal0/64 variants drive63 primary attachments,13 atomic refusals,384
cache/instance-only epochs and954 total real GC epochs/34578 active checks per
WORD32 and WORD32-file model. Additional one-retain INT_MAX/ULONG_MAX boundary
successes, nil/H guards and legacy-model pre-allocation refusal run independently.
Six exact-stage controls: type/data retain, mark, ordinal, atomic refusal, cached
state refusal. Compiler/crash/sanitizer/resource/timeout or unrelated failure is
never a control pass. Fixture-only allocation observers stop missing preflight
before unsafe overflow/publication. Ordinary hosted LSan is left enabled.

Actual prepare/storage/type/heap/newmp/arraycpy, complete collector, linkmod/
freemodlink and real unload/freemod/destroylinks execute. Every managed object is
created by actual heap code, with no fabricated Heap/Type/scheduler root. Native
Module allocation, guarded allocator/12-block arena geometry, diagnostics and the
platform jump are explicit services. Source bytes are poisoned before attachment.
GC before publication uses the retained lease only. An unpublished Module can be
unloaded first without disturbing the source. After native Module-list publication,
lease release removes its explicit pin; actual rootset scans module.origmp instead.
Instances clone original data under actual constructor roots, hold Module refs,
and preserve nested arrays after lease/caller retirement. Final real unload removes
the list entry, retires MP/Type/table/native Module resources and restores baselines.

The Module-list publication here tests the real collector's modules list and real
unload unlink, NOT file-namespace lookup/cache identity, scheduling or executable
entry acceptance. Modules deliberately have no code/entry and are never scheduled.
This is bounded native Module/data/root lifetime proof, not whole production pool,
arbitrary callbacks/destructors, managed instructions/import/frame/result roots,
physical Android/RV64 or concurrent collection. Shared wire/feature/ABI IDs,
Frame64, default43-op and immutable named dis64-calls policy remain unchanged.
