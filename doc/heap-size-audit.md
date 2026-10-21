# Heap size and slice arithmetic audit

## Closed defects

The old `acheck` multiplied two signed ints before checking a wrapped result
against the element count. This was undefined behavior and was not a sufficient
overflow test. For example, size 16 times count 0x11111111 can wrap to a value
that passes the old heuristic after header bytes are added.

The shared check now divides the available size before multiplying, includes
both Heap and Array headers and checks the full WORD length before narrowing.
The existing 31-bit positive allocation limit remains; this does not enable
large-object or Dis64 support. `newa` and `newaz` retain WORD-sized counts until
the check. `heaparray` and `arraycpy` use the same check.

`nheap`, `heap` and `heapz` reject negative payload sizes and sizes that cannot
fit the complete allocation. `mklinkmod` validates its count before computing
the variable tail, including the zero-link case. List construction validates
its unsigned payload before conversion to int.

`slicea` validates ordered, non-negative endpoints before subtraction and
retains WORD endpoints. `slicela` checks available destination length by
subtraction, avoiding an overflowing offset-plus-length comparison. Errors
leave the destination object and reference counts unchanged. Nil-array empty
slices retain the previous behavior.

## Checks

`libinterp/tests/heap-bounds.py` extracts the production sizing/allocation,
array/copy, module-link, list and VM operation bodies. It uses real VM and Proc
structures, a bounded pool stub and explicit allocation/reference accounting.
There are 23 array boundary checks plus the old overflow counterexample,
header limits, pointer initialization, scalar zeroing, copies, slices (including pointer reference counts), links,
lists and wide-WORD input rejection. Invalid sizes must fail before a pool
call. Large valid boundaries are checked without attempting large allocations.

```sh
CFLAGS='-O2 -g -fsanitize=undefined' python3 libinterp/tests/heap-bounds.py
CFLAGS='-O1 -g -fsanitize=address,undefined --param=asan-stack=0' \
  python3 libinterp/tests/heap-bounds.py
```

ASan stack instrumentation is disabled for the longjmp harness because this
sandbox does not provide the expected /proc stack information. Allocation
accounting substitutes for LeakSanitizer. These checks do not execute the real
pool allocator, scheduler, channel allocation paths or target machine code.
Other VM arithmetic and stack growth still need their own audit. The active
native-width bitmap and scalar layout remain unchanged.
