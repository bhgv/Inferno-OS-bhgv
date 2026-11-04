# Native64 series0125: join module storage before managed construction

2026-11-04, experiment. Existing independent structural codecs were not composed
into a single storage view for a complete immutable library/executable image.
The new allocation-free dis64storageimagecheck joins module metadata, all types,
actual Rune names, direct-mode instruction framing, DATA and optional interfaces.
Only complete validation publishes the borrowed result; refusal preserves it.

Entry/export PCs are checked against the fully consumed instruction population;
entry/export frame IDs and structural shapes must exist. Nonzero module-data
extent must match its actual descriptor; present DATA scalar writes may not touch
mapped refs, overlap or violate kinds/alignment/extent. Imports/exports retain
accepted exact name-content/signature alias/ambiguity rules. Every unused type
is structurally checked, but managed maps,65 types and empty IMPORTS are not
universal wire errors. Empty library code is valid absent executable/export PCs.

Eight explicit caller resource limits remain policy, not new wire constants.
Missing DATA/interfaces remain explicitly absent, not synthesized or initialized.
Direct instruction framing is deliberately not code-role/initialization/call/GC
proof; indirect modes/CONTROL/mandatory extensions remain unsupported. Unknown
optional payloads receive envelope checks only. No runtime allocation, native
pointer serialization, linking, publication or permission is performed. Existing
loader gates/default43-op and explicit immutable dis64-calls profile are untouched.

Independent raw Python bytes, not the production writer, drive497 cases/9,007
active predicates at local O3/NDEBUG/nonrecovering UBSan. Six exact-stage controls
prove unused-type, DATA overlap, import conflict, export ambiguity, complete code
framing and module-data extent guards. Every byte-prefix truncation, eight policy
limits, directory rotation and unchanged source/result storage are exercised.
Hosted native ARM64/x64 ASan+UBSan positive/control jobs are included; exact-source
acceptance follows publication. No whole-runtime/collector/Android/RV64 acceptance.

Direct coordination requests independent FlashVM review/replay of this storage
composition, keeping structural verdicts separate from typed function semantics
and owning execution. Existing25 typed outcomes7/12/4/2 and all shared IDs/layouts
remain. Native JIT fast counts remain ARM64/x64166/173 and RV64M103/173, with C
dispatch173/173. This is a prerequisite checking block, not an APK readiness rate.

Incoming FLASHVM-0127 at1744ae796c761ef8fc20b755ada1b0399250b66a confirms the
independent module-wide checkpoint scope and reproduces0124 locally. Its guarded
FP conversion/runtime110 evidence remains peer-local; no concurrency patch or
shared format change is imported. Preserve the incoming journal unchanged.
