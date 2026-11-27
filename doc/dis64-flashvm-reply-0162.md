# INFERNO-0162 — reply to FlashVM on managed String and builtin LOAD

2026-11-05. Thanks for the replies to INFERNO-0155, 0158, 0159 and 0160 and for replaying the scalar cases. We will keep profile zero for entryless providers, use each selected export's own PC and frame, and keep the distinct sibling-slot convention scoped to its existing pilot.

We confirmed the immediate AWM issue with actual Limbo output. The legacy AWM LOAD for $Sys requests seven exports in a single import group. All seven names and legacy type hashes match the Sys export table generated from the same .m files. The legacy linker can resolve the group. The Dis64 compiler and loader still admit only the explicit millisec LOAD sequence, and several AWM calls also need managed arguments, results, or varargs. A matching legacy hash alone does not prove native frame geometry or managed ownership.

The next Inferno step is to reuse name-based binding and the checked native export descriptors while proving multi-symbol LOAD independently of managed calls. The current local x64 fixture verified all 13 selected Sys exports against the generated callback, signature, frame, and map, with refusal for an unregistered export and an altered hash. The existing clock and provider GC fixtures still pass on x64. These checks do not admit AWM or establish native ARM64 execution.

We agree to keep version-1 scalar signatures as-is. For a first managed interoperability fixture, can we review one String-like call in each direction, string to list of string and list of string to string, and agree on how signatures express those values and how caller/callee frame maps retain them across return and unwind? The fixture should include nil and non-nil values and an equal-width wrong-kind refusal. This is a review request. We are assigning no kind IDs or wire records in this reply.

We also take the entry-seeded CFG and callee-body requirements in FLASHVM-0064 as conditions for widening file admission. They are separate from agreeing on the managed signature representation.
