# Native64 series0098: refresh streams, output buffers and native queue retirement

The production refresh worker treated each channel read as complete20-byte
records, decoded uninitialized bytes after a short read and retried EOF forever.
It now accumulates a bounded tail, decodes only complete records and stops on
EOF/error. Partial-record EOF records a diagnostic. Oversized responses refuse
before any decoding. The worker releases an acquired display lock and stops
when its output flush fails. Signed coordinates decode through a BGLONG macro
whose high-word shift is explicitly unsigned; wire widths/endianness stay fixed.

Native queuerefresh previously published an uninitialized next pointer. It now
refuses nil callbacks and initializes every field before linking under caller
provided display exclusion. Allocation refusal leaves the prior queue intact.
flushimage detaches an entry, copies its callback values and retires the private
allocation before invoking the callback, including nested enqueue/flush. Native
write failure stops before retiring pending queue entries.

Native output now completes positive short writes instead of dropping their
suffix. It balances release/acquire on each ordinary channel return, preserves
native error diagnostics and reports the exact accepted byte prefix. A failed
write still discards pending buffer bytes as before; bytes already sent cannot
be rolled back. This is not an atomic wire transaction or permission to retry
an opcode after partial effects. Caller must handle the channel failure.

Buffer reservations check bounded numeric offsets before subtraction or append,
including the single reserved visible byte. They do not form an out-of-array
comparison pointer. The existing remote flush-owner check remains; an unowned
flush refuses with a diagnostic instead of aborting the host process. Callback
code, Display and payload lifetime remain caller obligations. No arbitrary
callback destruction, VM longjmp cleanup, moving pin or parallel admission is
certified by this work.

native64-refresh.py executes the complete actual refresh/output/queue bodies
with real native Draw structures. Python independently encodes signed wire
records and expected window callbacks. Bounded allocation/I/O/owner transports
exercise every prefix, fixed chunk sizes and irregular partitions, ignored
window identities, EOF/partial EOF/errors, short writes, partial errors, flush
ownership, buffer boundaries, refused allocation, poisoned queue storage and
callback reentry. Real native allocations are freed exactly once. Four source
controls omit next initialization, discard a short-write suffix, restore the
EOF loop or retire after callback; each must stop at exact semantic10 before
unsafe access. Signals, sanitizer failures and timeouts are not accepted.

Local O3 x64 nonrecovering UBSan with synthetic port aliases passes1549760
predicates per WORD32/WORD32-file and all four strict controls. The existing
complete Draw scalar consumer still passes340506 predicates/model, eleven
actual entries and its model/initializer/result controls. Full legacy draw.c
also compiles. New native ARM64/x64 ASan and complete ARM64/x64/RV64 classic-mk
runs include the refresh gate; target acceptance must be read separately.

Preceding0097 full classic-mk41623878015 has passed native ARM64 and RV64/QEMU;
x64 was still running during this preparation. Its native ARM64/x64 ASan had
already passed. Draw export/frame coverage remains32/89: this series adds native
transport/lifecycle evidence, not another export, full Draw registration or AWM.
JIT coverage and file opcode admission remain unchanged. No Dis64 IDs/layout,
serialized native pointers or file CALL/LEA8 activation changes are proposed.
FLASHVM-0093 remains the newest received entry checked before publication.
