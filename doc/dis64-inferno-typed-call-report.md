# INFERNO-0170 — source-derived bodies in active FRAME/CALL/RET

2026-11-14. Base4520c641. Adds native64-typed-code.h, a trusted checked-IR to
native Inst bridge. It retains private schema v2 and shared ISA IDs, resolves
local branch targets at the actual embedded body address and uses full-width
indirect result addressing at native FP32. All structural checks finish before
output writes. This consumes compiler premises; it is not arbitrary image/type
validation or a new file profile.

New native64-typed-calls.py/.c consumes actual host compiler output and descriptor
maps for six return bodies. Production FRAME/CALL/RET, decoder, stack and owning
JIT backend execute real active frames. The caller and intermediary wrapper are
trusted C constructions; source call lowering remains closed. Caller argument
and result fields64/72 are disjoint, eight-byte address scratch48 and pending
frame40 follow the existing trusted native call constructor convention.

Each x64 and ARM64/QEMU run passes288 cases:6 result kinds × C/RX × extent growth
× direct/nested ×3 quanta ×2 inputs. Coverage includes byte/int/big/real extrema,
reference nil/old/alias replacement, two-level opaque ownership graphs, exact
refcounts after returning through both frames, scalar neighboring canaries and
zero tracked allocations after teardown. The managed identities here transport
opaque owners, not actual String/list-specific operations. Existing separate
Unicode/list/GC and staged-return gates retain that coverage. Runtime is UBSan
with bounded OS/allocator/collector support, not concurrent GC or scheduling.

13 encoding/geometry refusals preserve output bytes. Active removal of branch
ownership upper bound and signed16 middle bound reaches only the expected
semantic exit10. No crash, timeout or sanitizer failure counts as success.
Existing896 local-call cases and four pending-ownership controls remain green
on x64. Existing20 body/24 return/768 Sys frame cases per emu/os and WORD32/
WORD32-file combination remain green after sharing the v2 step declaration.
The common native64-port.py now includes this new source-call execution gate.

AWM complete-body analysis stays14 accepted/346 refused. General source calls,
heap operations and managed module writer/loader admission remain ahead. No
Android/Bionic, device, AWM or GUI APK execution is claimed. No FlashVM runtime
or protocol is imported. RU/EN instructions: dis64-typed-calls.ru.md/.md.
