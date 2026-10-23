# Buffered Sys.stream transfers

Run `python3 libinterp/tests/sys-stream.py` from the repository root.

The driver extracts both production implementations from emu and os and checks
that their bodies agree. It uses actual interpreter, FD and generated builtin
frame declarations. Each implementation is built with 32-bit and 64-bit VM
scalar words and native host pointers under ASan and strict UBSan.
The four runs check 46,378 cases in total.

The byte oracle covers every buffer size 1..16, input length 0..40, and read/
write fragment size 0..3. Tests fail or return zero at each destination offset,
fail reads after partial buffers, inject allocation failure, and request process
cancellation after a partial read. Exact output bytes, untouched output tails,
returned counts, lock state and buffer ownership are checked independently.

Short writes now continue from the acknowledged suffix. A failed full or final
write contributes only the bytes actually accepted by the destination. The
original implementation lost short full-buffer writes and counted unwritten
final bytes. Nonpositive or out-of-native-int-range buffer sizes now return -1
with Ebadarg before allocation or I/O. Ordinary allocation failure retains the
existing Enomem error path.

The counter uses the VM's unsigned word width, then explicitly interprets the
result as a signed VM word. This retains modular WORD32 count behavior without
C signed overflow and avoids truncating WORD64 counts through a native int.
Virtual acknowledgments model 2, 3 and 4 GiB transfers using four or fewer full
blocks; these count tests neither allocate nor access gigabytes of storage.
Counts exceeding the signed result range still wrap at the VM word width;
this change does not introduce a new large-count API or change Dis layouts.

SYS_STREAM_SOURCE selects a baseline source. SYS_STREAM_CASE selects transfer,
failure, bounds or count. The old source fails the short-write byte assertions
and the multi-gigabyte count case triggers UBSan. LeakSanitizer process inspection
is disabled; owned buffers are counted. Kernel calls, scheduler transitions and
cancellation are deterministic adapters, not a full Inferno boot, actual device
I/O, asynchronous error unwinding or execution through a JIT.
