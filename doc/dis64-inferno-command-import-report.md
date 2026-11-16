# INFERNO-0202: entryless Command libraries with outgoing imports

2026-11-16; base45072fb4, experiment. ARM64 remains the first runtime target.

## Result and representation

The existing Limbo `-64b` path now compiles an ordinary Command.init that loads
one file dependency and calls its scalar interface or fixed Command.init.
The executable client uses `-64c`. Actual three-file paths run through namespace
LOAD, normal linkmod, MCALL/RET, scheduler and collector in the Inferno runtime.
Loading a library does not execute init; only the caller does that.

The importing library is entryless (profile0, native entry=nil) and carries six
existing sections: Module, Code, Types, Imports, Exports and Strings. Imports are
outgoing requests; the independent one-record Exports table supplies incoming
Command.init, including the hash calculated by Limbo from the real .m interface.
No new wire kind, opcode, managed ID, signature annotation or hash algorithm is
introduced. The export-name slot precedes literal strings. Existing five-section
pure libraries and executable clients retain their bytes.

The shared producer preserves the library root RET and executable root IEXIT.
Private helper graphs, frame descriptors, inferred parameter/result contracts,
PC mapping across import-result copies, strings and primitive array descriptors
compose with the importing root. Only root init may LOAD/MCALL; helpers keep pure
library permissions. The reader separately proves both tables, metadata names,
frame maps, load facts, setup boundaries, control flow and the local call graph
before construction. Matching incoming requests reads Exports, never Imports.

Construction copies outgoing request/name tables into the module's existing
native import owner, together with its C/JIT choice. Instructions, descriptors,
literals and public init links remain native-owned. The executable publication
path and entryless library registry path keep their prior ownership rules.

## Validation

* Seventeen actual compiler triples on Linux x64 and ARM64/QEMU, C/JIT, with
  actual GC and96 scheduler roots per triple. Includes scalar and Command
  dependencies, loops/reload, private helpers before/after imports, fresh managed
  results/lists, Unicode, int arrays, exception unwinding and load-only behavior.
* Complete consoles:68 main roots per architecture for the new triples, plus
  missing dependency, wrong signature/result, wrong incoming hash, no-auto-init,
  explicit policy and entryless-launch controls. RU/EN four-file tutorial compiles
  and executes in C/JIT on both architectures.
* 240 raw valid/malformed images per architecture; host ASan/UBSan included.
  Missing/wrong tables, signatures, symbols, frame maps, module slots, paths,
  setup entries, load facts and root terminators are covered. Active omissions
  disable the load fact or wrongly match Imports instead of Exports and produce
  semantic proof mismatches without executing malformed images.
* Namespace source images are overwritten before free; the retained outer file
  buffer is overwritten before the nested import. The positive chains still
  pass. Borrowing the importing library's request/name pointers from that
  retained buffer instead of copying them fails the strict required-load guard,
  without a sanitizer error. Removing the callee frame marker detects the fresh
  list's lost sole owner. These controls run on both architectures.
* Host ASan/UBSan covers construction/rollback of all17 new triples and386
  existing Command pairs. The test dispatch observer now holds32 bounded nested
  observations, preserves the underlying target when re-observed and clears
  retired entries between process roots. This changes no runtime GC algorithm.
* Existing386 Command pairs,11 scalar helper clients,62 prior scalar clients,
  full-console1520 Command and44 scalar-helper main roots per architecture,
  existing raw proofs/omissions and managed/string/array/Context gates remain
  exercised. Prior772 selected files,11 scalar helper callers,62 older clients
  and all classic bytes equal0201. All17 triples are deterministic;0201 refuses
  the importing library, while the leaf/client bytes stay equal.
* Sixteen prior library/four client refusals remain. Four new well-typed importer
  refusals cover multiple paths, helper imports, mutable MP and extra exports.

## Reproduce and remaining work

Use `dis64-command.py --chain`, optionally `--chain-owner-control --case
chain-required`, or `--gc-control callee-frame --case chain-owned`.
`dis64-command-chain-proof.py` accepts `--control load` and `--control export`.
`dis64-command-chain-boot.py` takes --limbo, --emu, --root and optional --runner.
The producer's --command-import-baseline points to0201's compiler. Existing
CC/NATIVE64_ARCH/NATIVE64_RUNNER settings select ARM64/QEMU. Gates are added to
native64-port.py. Worked examples: [EN](dis64-command-import.md),
[RU](dis64-command-import.ru.md).

This removes the blanket restriction on imports in Command libraries. The
selected compiler still requires one import path/group, one public init, scalar
or fixed Command dependency signatures and pure private helpers. General module
data/ADT shapes, arbitrary public exports, multiple paths and exception handlers
remain to be serialized and linked for real AWM. Actual16-source AWM preflight
passes classic compilation and refuses all four Dis64 trial profiles. Android
Bionic GUI/device/APK startup, physical ARM64/RV64, concurrent GC and whole-VM
sanitizer acceptance are not established by Linux/QEMU results.

FlashVM master rechecked a3788cc04f8705a7a45eb263538003883665f7fb, four commits
after4a40ada1. It adds native Inferno lib9/libstyx packaging and Styx integration;
its README states that source fixes belong in Inferno before vendor-sync. No
journal or public Dis64 wire change appears in this comparison. Incoming0202/
0203 and our previous journal prefix remain verbatim. No DisVM/runtime/concurrent
ownership transfer, managed-ID agreement or vendor-pin7f224145 change is made.

Publication integrates upstream experiment4029a622 (libstyx stream/session,
backend refusal, walk and directory/transport lifecycle fixes). These16 files
are disjoint from this change and preserved unchanged; the feature was tested
from45072fb4. libstyx is outside these VM/compiler test binaries.
