# Owned private native64 arguments

`include/native64-arguments.h` adds synchronous construction/retirement of a
private managed argument frame. It does not invoke a formatter, register Sys
variadics, change Dis64 bytes or admit a new file profile.

A trusted source supplies a callback-free fixed schema, its argument body after
the reserved 64-byte header, contiguous eight-byte typed tail slots and canonical
values. Inputs and checker context remain immutable/readable and all input values
stay caller-rooted until construction returns. Descriptor preflight and all
canonical checks precede live reference checks. The fixed-prefix schema checker
has separate canonical/live phases; the tail checker requires exact Type identity.
These are pure, nonthrowing trusted callbacks, not pointer authentication.

The context copies slots, values, prefix and full map before acquiring metadata
owners. It retains one expected Type per reference slot even for H values. A
progress counter makes partial acquisition visible to cleanup. Actual dtype and
heapz allocate the mapped frame; heapz initializes mapped cells to H before pin
publication. Each argument is retained before its mapped cell is published, with
no fallible operation between retain and store. Construction scratch is released
on success; descriptors and full map remain owned independently of source buffers.

Construction runs under the owning port kprotect. Failure detaches state before
cleanup, releases only acquired resources and restores the original diagnostic.
Repeated retirement is harmless. This is a private synchronous owner, not an
escaping/asynchronous call API. Existing heap/pin allocation hooks must not yield
or collect a newly returned unpinned object; destructors must not throw, yield or
reenter construction. Current GC evidence does not establish moving compaction.

## Reproduction

```
python3 utils/tests/native64-arguments.py --limbo /path/to/fresh/limbo
NATIVE64_CFLAGS='-O3 -DNDEBUG -g -fstrict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-arguments.py --limbo /path/to/fresh/limbo
```

The gate uses complete actual port protection/error bodies, heap/Type operations
and collector, with bounded native pool/process services. For each of emu/os and
WORD32/WORD32-file, 264 transactions span three collector phases, eight process
states and eleven construction modes: five C allocation refusals, heap refusal,
pin refusal, partial duplicate value retention, H-only Type ownership, partial
Type retention and success with caller value owners retired. The 1,192-byte frame
has a complete 19-byte map. Source metadata/values are overwritten after success;
collection must preserve the independent frame and its arguments. Active negative
controls remove the frame pin or expected Type owners and must abort on a root
predicate. O1 and O3/NDEBUG strict UBSan pass with 31,226 predicates per combination.

An additional O3 ASan+UBSan run does **not** pass: after the fixture's warning that
__asan_handle_no_return has an unknown stack top, it reports stack-buffer-underflow
while unwinding an injected failure. This is unresolved evidence, not a proven
false positive. Prior optimized UTF/unwind ASan evidence also remains unresolved.

Import the new header and test pair with native64-tail/result/model/entry/bind,
actual heap/gc, existing result/GC test helpers, generated Roots/Math declarations
and both port error source closures. The normal native64-layout gate includes it.
A combined typed formatter and full builtin startup remain subsequent work.
