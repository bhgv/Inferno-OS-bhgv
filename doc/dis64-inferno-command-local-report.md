# INFERNO-0196 — local managed void calls in Command libraries

2026-11-15. Base d2df6854 /0195. The selected ordinary Command file path now
connects existing typed local-call lowering to actual FRAME/CALL/RET execution.
A library can contain init and up to eight local helpers, with separate native
frames, nested calls, loops, branches and existing String/list/array operations.
Helpers have exactly the current Command parameter geometry: an opaque Context
reference at FP64 and list-of-String at FP72, with void return. This is a bounded
managed-call integration step, not general module or arbitrary ABI admission.

## Producer and independent reader

The existing multi-body compiler already resolves local call sites, typed
arguments and legacy descriptor identities. The new writer preserves that
complete lowering for every emitted body rather than rejecting nf !=1. It emits
ordinary FRAME, two indirect MOVP argument stores, CALL and RET. Root body stays
at PC0/frame0 and remains the sole exported Command.init. Internal helpers add no
public signature kinds, export hashes, sections or opcodes. There are no imports
in this selected provider, and executable clients retain their existing profile.

The MP/Context/optional primitive descriptors keep their old positions. Helper
frame descriptors follow them in source entry order, with eight-byte alignment,
exact maps, mapped parameters and the existing reserved tail cells. Up to13 type
records cover the largest combination of literals, byte/int primitives and eight
helpers. Images without local helpers retain their prior representation exactly.
Incremental build dependencies cover the new writer and proof fragments.

The reader discovers each helper entry from matching FRAME descriptor/CALL PC
pairs. Every appended frame must have one entry; entries are ordered, unique and
inside CODE. Each function is checked independently with its own descriptor,
frame extent, root classes and definite scalar initialization. Branch targets
are rebased only for this body proof and must stay within it. Full output/native
construction keeps the original global instruction indexes.

Local call setup is one CFG node: exact prepared-frame cell FP40, mapped Context
and argv destinations64/72, matching descriptor/target and no result binder.
Branches cannot enter argument setup or CALL alone. Transitive closure checks
cycles and root reachability before native allocation. Recursive, unmatched,
unreachable-frame, cross-body and malformed argument/descriptor cases refuse.
The compiler independently refuses recursive graphs, more than eight helpers,
other helper signatures/results and imports from helpers, with no partial file.

A per-body descriptor selector is essential: applying init's map to a helper
would incorrectly reject its private String/list/array roots or validate the
wrong geometry. The new proof carries this internal selector; no wire field is
added. Existing stack construction, call/return handlers and JIT emitters are
used directly. There is no substitute call executor or DisVM runtime import.

## Execution and ownership evidence

* 238 ordinary source pairs, each through96 scheduler roots, on native x64 and
 ARM64/Linux QEMU with real collector epochs, caller/provider interpreter/JIT,
 stack growth and normal/error retirement.28 new pairs exercise local calls:
 repeated and nested calls, a diamond/shared child, maximum eight helpers and
 thirteen types, nil/non-nil arguments, callee-private lists and strings, byte/int
 arrays, live caller arrays, slices and exceptions from nested helpers.
* 928 main roots per rebuilt complete x64/ARM64 console, including ordinary and
 missing-file paths; policy/hash/entryless controls also pass. The revised RU/EN
 tutorial is separately compiled and executed in C/RX on both consoles.
* 148 independent valid/malformed raw images on each architecture; host proof uses
 ASan/UBSan. Active argument-offset and cycle-check omissions produce the intended
 semantic proof failure without executing malformed code.
* An independent real-GC observer records active helper frames. Positive local
 cases require observations; removing helper-frame root marking loses a private
 list and is detected before resuming on x64 and ARM64. Index observations now
 select the active frame's descriptor, including extension-frame ownership,
 rather than assuming every provider activation uses type0.
* 476 deterministic selected images. All prior210 Command pairs/420 selected
 images,62 prior scalar clients and all classic outputs compare byte-for-byte
 with0195's compiler. Thirteen well-typed complete sources test refusal boundaries.
 Previous descriptor, String/codec/array/index, reference, Context, constructor
 rollback and scalar-return gates remain exercised.

Reproduce with dis64-command.py, dis64-command-boot.py and
`dis64-command-local-proof.py --limbo PATH`; the latter also accepts
`--control arguments` and `--control cycle`. The targeted GC omission is
`dis64-command.py --limbo PATH --case local-callee-list --gc-control local-frame`.
The producer gate accepts `--local-baseline` for0195 image/classic comparisons.
The existing ARM64 cross-compiler/runner environment applies.

## Remaining boundary and peer receipt

General helper arguments and managed results, recursion, exception tables,
multiple exports/imports and general ADT/DATA construction remain outside this
selected integration. The actual16-module AWM startup preflight still compiles
classically and refuses the bounded scalar/managed profiles. Linux/QEMU console
execution does not establish Android/Bionic GUI/APK or physical ARM64/RV64
acceptance. The priority remains the existing Inferno Android ARM64 client.

FlashVM c81c123fb68840ad64017fff1cf33e6d3d5dfb90 is two commits above af92efe2:
whole-record system-event delivery and retained provider subscriptions. Neither
commit updates the peer journals or managed-call wire contracts. Existing local
incoming0202/0203 records remain intact; this own journal relays0196's frame/map,
call graph and GC evidence. No peer code or concurrent ownership model is imported.
