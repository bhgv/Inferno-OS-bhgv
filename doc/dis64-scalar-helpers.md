# Bounded scalar source procedures: series 0057

`limbo -6` can specialize direct FRAME/CALL pairs into its sole exported
`init()` before the existing numeric-case and scalar lowering passes. This is
source inlining, not a Dis64 calling ABI. The agreed np0 executable profile still
admits exactly 43 of 173 Dis operations. No opcode, section, feature bit,
signature, coordinate schema or wire identifier is assigned.

The current bounds are 16 compiled functions including the entry, 32 scalar
instructions per helper excluding its final RET, and 128 bytes of legacy helper
frame extent. Each helper has no arguments or result, no recursion or nested
calls, no branches or cases, no managed frame map, and no user-declared locals.
Only scalar compiler-generated temporaries are accepted. The entry can retain
its own loops, branches and bounded numeric cases around the calls.

The compiler validates exact adjacent FRAME/CALL pairs, callee start and frame
descriptor identity, and matching caller frame-pointer temporary. Branches and
case arms cannot enter a CALL while skipping its FRAME. Flattening uses private
instruction and case-table copies; it leaves ordinary Dis output and original
compiler metadata untouched. Empty procedures become a jump to the next
instruction. Each helper's terminal RET disappears; the entry RET becomes EXIT.

Helper scratch and compiler temporaries occupy an appended region separate
from caller locals and the caller's scalar scratch tail. Sequential helper
invocations reuse that region; they do not implement fresh-frame initialization
for declared locals. Unsupported shapes fail compilation rather than emitting
file CALL/FRAME or falling back to an incompatible ABI.

`utils/tests/dis64-inline.py` covers 18 accepted sources, each executed once
through the interpreter and once through JIT. It checks helpers before and after
the entry, empty bodies, repeated calls, loops, three numeric case forms, Big
wraparound, Real moves, live caller Big/Real locals, scratch isolation, both
accepted size boundaries, and faults traced back to helper source lines.
Nine valid ordinary-Limbo programs independently test refusal of unsupported
calling/storage shapes. Ordinary assembly must still contain FRAME/CALL;
ordinary Dis output must remain unchanged. A deliberately misplaced helper
scratch address tests the independent storage-window witness.

Local strict-UBSan host bootstrap, clean classic-mk ARM64 port gates, x64
execution and RV64/QEMU execution pass. ARM64 whole-process replay uses QEMU;
subsequent GitHub native ARM64 CI is separate hardware-backed evidence. The
external FlashVM reader accepts the images without a reader change. See CI
logs for complete runtime/GC/lease gates; this does not establish full shell,
imports, managed calls, Android startup or native RV64 producer execution.

The downloadable console bundles include `procedures.b`, `procedures.dis64`
and its private diagnostic map. Run `./emu -v -r "$PWD" -c 1 -d /procedures.dis64`
and repeat with `-c 0`. It checks 4097 calls, helper scratch, caller locals and a
numeric case; expected module is Native64Procedure.

JIT coverage remains distinct from file admission: ARM64/x64 have 166/173
native fast paths (95.95%); RV64I has 87 (50.29%), Zmmul 90 (52.02%), and M
103 (59.54%). All three retain 173/173 support through JIT plus C handlers.
Complex operations continue to use C where appropriate.

## Published validation and downloads

Source commit: [ab03187](https://github.com/bhgv/Inferno-OS-bhgv/commit/ab03187119dc96800ac05c1a41afce48969354d3).
[CI 41343429742](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742)
passes all three clean classic-mk ports: native Linux ARM64 and x64, RV64/QEMU.
Each archived helper execution log records 18 sources / 36 whole C/JIT
processes; the separate external-reader log records 18 images / zero execution
processes, with all nine refusal controls active. The existing 67-case gate
still runs 134 whole processes per architecture.

| Target | Console, three source/image/map examples and complete logs |
| --- | --- |
| ARM64, native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742/artifacts/13098273734) |
| x64, native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742/artifacts/13098770059) |
| RV64, QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742/artifacts/13097759848) |

Artifacts require a GitHub login and expire according to repository retention.
All three archives contain byte-identical procedures.dis64, SHA256
`a07604d05c0d11efee54d2ae8fe874c5c9c5c7c467ae2bf4f6f7591c99bb2763`.
The downloaded ARM64 console additionally executes all three bundled examples
successfully with JIT0/1 under Linux/QEMU and an explicit ARM64 libc sysroot.
This replay is separate from the native ARM64 CI evidence above.

The owning native Limbo integration gate also passes 72 ordinary Dis/AOT source
cases and exact ABI32/late-link checks. The previous compiler produces identical
ordinary Dis for the scalar compatibility corpus. No APK or full shell claim
follows from these restricted console results.
