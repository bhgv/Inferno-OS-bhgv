# Native descriptor ownership in Sys results

Run `python3 libinterp/tests/sys-fd-owners.py` from the repository root.
`CC` and `CFLAGS` select the compiler and flags. Defaults enable ASan, UBSan
and NDEBUG; fixture predicates stay active under NDEBUG. `FD_OWNER_SOURCE`
selects an alternate port source, `FD_OWNER_PORTS` selects emu/os and
`FD_OWNER_MODELS` selects 32/64-bit WORD profiles. `FD_OWNER_CASE=single`,
`files`, `pipe` or `connection` selects the corresponding subset.

The default complete legacy matrix runs **4,102 scenarios per profile**,
**16,408 across both ports and WORD models**. It extracts the actual native FD
declaration, constructor/finalizer, open/create/fildes/pipe/dial/announce/listen,
ownership helpers, runtime protection, string constructors/conversion and
generated Sys frames. Native I/O and scheduler transitions are bounded
observers. No real files, pipes or network connections are opened. WORD32
changes VM scalars while retaining native pointers.

Checks include every successive owner-allocation failure and one position
beyond the last allocation, native failures, descriptor zero, shared previous
handles, nil/short/absent/wrapping pipe arrays, untouched extra array slots,
UTF-8 directory conversion and all return byte alignments. Listen borrows its
previous result's directory. Every case checks exact descriptor roles and
close-once ownership, file-group/heap/type references, allocation guards,
scheduler ownership and balanced real VM error frames. Close observers
deliberately overwrite errstr; cleanup must retain the original allocation or
native error. Explicit counters track every heap block and native descriptor.

Mkfd consumes a descriptor on success and failure. Multi-owner helpers mark
transfers before calling it, keep partial state volatile across error jumps,
and release raw descriptors and managed partial results on failure. Pipe slots
and connection fields are published together only after all new owners have
been constructed. Allocation errors retain the previous multi-owner result;
native connection errors clear the previous connection. Single-owner file
calls keep their established nil-on-error replacement convention.

The complete matrix, including nested pipe/connection allocation failures,
passes ASan/UBSan on a joined pthread. The worker provides sanitizer-tracked
stack bounds even when process-stack discovery is unavailable. Stack
instrumentation is not disabled or manually unpoisoned. Unsupported
LeakSanitizer process inspection is disabled; explicit resource and guard
checks remain active. This supersedes the earlier main-stack ASan limitation.

For open/create/fildes frame capture, modes, old owners and complete result
preflight, see [native file-query and creation capture](README.sys-query-capture.md).
For expanded pipe/connection callbacks, packed array headers, captured previous
connections and aggregate reference validation, see
[captured pipe and connection ownership](README.sys-multi-capture.md).

These checks do not boot an emulator/kernel, perform real filesystem/network
operations or prove the full compiler/loader/GC 64-bit ABI. Neighboring MCALL
and x64/ARM64 compiler-bridge checks provide separate JIT integration evidence.
