# Series0105: explicitly selected Dis64 scalar-call file launch

FLASHVM-0102/0103 agreed to a distinct Inferno experimental launch path, with
ordinary/default/shared admission unchanged. This series implements that path;
it does not change wire bytes, feature IDs, calling ABI or ordinary WORD32 LEA.
FlashVM still needs its own validator/executor and may explicitly refuse these
files. This is policy agreement and Inferno implementation, not reciprocal runtime
compatibility evidence.

Build the coherent experimental WORD32/native-pointer64 scalar console using
classic mk as described in build-native64.md. Select the profile explicitly:

```
emu -r /path/to/inferno -c 1 -X dis64-calls -d /path/in/tree/answer42.dis64
```

`-X` is startup-only and requires an explicit `-d` initial module, in either option
order. `-I` alone is insufficient. Unknown names, missing arguments, an incoherent
native model, selection after namespace use, or an already published module cache
refuse. The selected mode freezes after EMU/argv processing, before VM initialization,
and also at the first standalone reader use. File bytes cannot select this policy.
`-c 0` executes C; `-c 1` requests the existing native leaf JIT with C fallback.

Actual readmod joins bounded short reads, preserves filesystem identity, retains
the existing mustbesigned admission predicate, and selects prepare64calls only in
this mode. Full raw-image inference/proof precedes VM owners, the constructor owns
its names/types/instructions/JIT pages, and only then does parsemod publish the
module. No manifest locals, source pointers or workspace authority survive.
Default loading still selects the prior scalar pilot. The selected call profile
explicitly refuses that pilot, legacy images, dynamic libraries and builtin imports
rather than silently falling back. No Bool, managed object, import, nested-helper
or arbitrary old Dis64 call support is added.

A cache alias is checked against the immutable process profile before I/O/release
and before a matching device/type/mtime/full Qid can return it. An incompatible
owned alias is consumed once and refused, never reinterpreted/recompiled under a
new mode. A compatible hit retains ordinary cache ownership. The cstep comparison
is only a compatibility discriminator for already constructed modules; it is not
validation, proof authority or a retention mechanism.

## Owning evidence

`utils/tests/dis64-file-calls.py` compiles actual load64/readmod/parsemod, scheduler,
stack/call handlers and C/JIT bodies. Forty immutable raw inputs cover10 accepted
and30 refused images; no trusted local-kind declarations cross the file boundary.
Fresh processes test frozen profile selection; both directions of opposite-profile
cache refusal use matching full-width filesystem identity. Executable mode bits
force the selected-profile dynamic-loader refusal boundary. C/RX accepted modules
exercise every actual reader/parser/constructor allocation failure, baseline file
admission refusal and compatible cache hits. Six typed/frame-order positive roots
check exact Int32/Big/Real result bits after source bytes are poisoned. A leaf divide
error separately retires its real process/frame/module/code owners through progexit.
The OS/GC boundary in this diagnostic remains bounded single-worker fixture code.
Three source controls remove cache discrimination, startup sealing or call-proof
routing; each must stop at semantic status10, with ledger retirement. Crashes,
compiler failures, sanitizer reports and timeouts are not control successes.

The complete Linux/x64 emulator was separately rebuilt from clean tracked sources
with classic mk, trusted Math/Sys/Freetype and caller guards. `dis64-call-boot.py`
uses its actual OS worker/startup/vmachine:11 successful C/RX call roots, two
leaf-error roots, CLI argument/order checks, default call refusal and explicit
cross-profile/builtin refusal. Exact bits remain the separate scheduled-root oracle;
boot success alone is not an arithmetic-result proof. The previous default boot
corpus also passes38 processes/1096 in-program checks plus pilots/refusals.
Existing default fileloader and eight direct/file live-vmachine UBSan cases pass.

Local evidence is x64 O3 nonrecovering UBSan for the owning diagnostic and the
normal optimized classic-mk build for the full emulator. Native ARM64/x64 ASan
jobs now run file admission and all three controls after constructor checks.
The full classic-mk ARM64/x64/RV64 matrix runs complete call boot and the same
owning gates. New target results remain pending until actual logs are read.

Native fast availability remains ARM64/x64166/173 (95.95%), RV64M103/173 (59.54%);
all three provide173 complete JIT+C entries. Default public scalar admission stays
43/173. The explicit reviewed call subset is a distinct policy, not a new blanket
opcode count or full VM readiness. Managed graphics/resource ownership, full
Limbo/AWM startup and physical Android ARM64 APK acceptance remain open milestones.
