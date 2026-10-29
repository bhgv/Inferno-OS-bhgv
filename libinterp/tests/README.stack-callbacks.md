# Stack cleanup and return state across REG-borrowing callbacks

`destroystack` captured each frame link before cleanup but then reread REG.FP and
REG.M after calling destructors. A destructor borrowing the active decoded REG
could redirect the remaining traversal, lose a module owner or leave REG.EX
attached to an unrelated value. The global-R route is used by emu/os fault
teardown; saved process REG images are also passed to the same routine.

Frame, module and extent cursors now remain private locals throughout teardown.
`unwindstate` republishes the surviving FP/M roots and detached EX around cleanup,
extent release and final module destruction. A saved caller module is published
before releasing the departed module. Existing frame maps, captured extent parent
links and root Type fallback retain their ownership semantics. Each frame and
module owner is released exactly as before on an ordinary successful teardown.

This fixes cleanup traversal and its terminal FP/M/EX publication. It does not
restore every unrelated field of a destroyed process's REG, validate arbitrary
stack corruption, or promise rollback/completion if a destructor raises rather
than returning. No new stack/REG/Dis/native ABI or JIT opcode is introduced.

## RET and extension removal

`RET` had the same dependency on global REG during mapped frame cleanup and final
module destruction. It now captures the departed module and full resumed REG
image. Ordinary mapped cleanup cannot overwrite that resumed image; caller
module/MP and mode-switch IC/t are published before the departed module is
released, and restored after its destructor returns.

`unextend` captures the completed SP/TS/EX restoration together with the other
REG fields before actual mapped cleanup and extent release. It republishes the
complete resumed image after both callbacks. RET uses this completed result for
extension roots, preserving legitimate extent restoration rather than restoring
an obsolete pre-unextend SP/TS/EX. Checks still precede register/ownership changes.
Callback exceptions retain the existing partial-operation semantics.

## Executable matrices

| Suite | New scenarios, both WORD models | Full result |
| --- | ---: | --- |
| stack-unwind | 655,384 | 851,968 frame/type/module cases plus 24 module-only cases |
| return-native | 3,072 | 3,072 callback cases plus 3,840 existing C/RX return cases |
| stack-lifecycle | 36 | 6,126 copy/pop/extent/callback/invalid cases |
| New total | 658,492 | All predicates remain active with NDEBUG |

The unwind matrix adds separate FP, M, EX, combined and full-byte REG borrowing,
for both global R and saved-process REG targets. It crosses both frame
alignments, packed Type alignments, all module-switch masks and extent-root Type
fallbacks. Existing extension-parent/root mutation controls remain. Exact owner
counts, reference changes, extension release order and unchanged frame bytes
are checked. Module-only images exercise the final destructor without extents.

The return matrix uses actual published AMD64 RET leaves, declines to the real C
handler where cleanup is required, and compares direct C execution. It covers
unaligned frames, root/ordinary/mapped frames, module switches, legacy compiled
mode transitions and last-module-owner callbacks. Callback adapters intentionally
borrow REG; they model owner release while retaining fixture storage for checks.
The lifecycle family executes actual extend/unextend, pointer-map cleanup and a
live mapped owner's last release, as well as a REG-borrowing extent-free adapter.

Full unwind, return and new extent-callback checks also complete ASan/UBSan.
The sandbox emits executable-name warnings, and existing error-bearing RET cases
emit the known no-return/stack warning; this is not a claim that those warnings
were eliminated. UBSan/NDEBUG is the principal full-matrix run.

Additional passing checks include emu/os exception frames (526,000 per
runtime/WORD profile, 2,104,000 total), MCALL/RET round trips (6,144 per WORD
model), module-transition tests, complete AMD64 dispatch, ARM64 compiler bridge,
readiness and dis2c differential tests. ARM64 is compiler-only here; native
machine-code execution is AMD64. WORD32/64 profiles retain native host pointers
and do not replace target ABI acceptance.

Parent `95df086` compiles but fails runtime controls for each of FP/M/EX borrowing,
module-only teardown, mapped RET cleanup, final module RET cleanup and extent
cleanup. Overrides are UNWIND_XEC_SOURCE, RETURN_SLOT_SOURCE and LIFECYCLE_SOURCE.

```
CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/stack-unwind.py
python3 libinterp/tests/return-native.py
python3 libinterp/tests/stack-lifecycle.py
```

UNWIND_CASE=frames|empty, UNWIND_CALLBACK_MODE=0..7 and
UNWIND_GLOBAL_TARGET=0|1 select unwind controls. RETURN_BORROW_MODE=0..3 narrows
the added return matrix; LIFECYCLE_CASE=borrow selects extension callbacks.
Coverage remains 173/173 JIT+C and 166/173 guarded native fast commands, with
seven complex C-only commands, for AMD64 and ARM64.
