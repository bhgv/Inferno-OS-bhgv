# Channel allocation audit

## Closed defects

`cnewc` left a Channel heap allocation behind when either queue allocation
failed. A buffer allocation error also bypassed queue/channel cleanup. The
constructor now records the partial object outside the runtime error frame,
initializes all destructor-visible fields before further calls, and destroys
that object on failure. Queue sentinels are explicitly zero-initialized. The
mover Type reference is acquired only after fallible construction succeeds.

The runtime `krethrow` wrapper propagates the existing VM error after cleanup
without replacing its message or exposing Proc error-stack layout to libinterp.
Both emulator and native-kernel runtimes implement the same boundary.

Negative lengths, invalid buffered Types and overflowing sizes are checked
before allocation. `newc` checks the full WORD capacity before converting to
the constructor's int parameter, including the legacy LP64 host model.
`newcm` validates its raw element size and releases its temporary descriptor
on both successful and failed creation. A failed replacement retains the old
channel and its buffer.

## Verification

`libinterp/tests/channels.py` extracts production allocation, descriptor,
channel destructor, array destructor and constructor bodies. It uses real VM
structures and emulator Proc error frames with an injected, bounded allocator.
There are 59 allocation-failure/unwind scenarios plus tests of lengths,
reference counts, old-value retention/replacement and raw/typed/unbuffered
channels. The exact original error is checked after nested cleanup/rethrow.
Every scenario finishes with zero temporary allocations and balanced static
Type references. Emulator/kernel guard and rethrow implementations must match.

```sh
CFLAGS='-O2 -g -fsanitize=undefined' python3 libinterp/tests/channels.py
CFLAGS='-O1 -g -fsanitize=address,undefined --param=asan-stack=0' \
  python3 libinterp/tests/channels.py
python3 libinterp/tests/runtime-protect.py
```

ASan stack instrumentation is disabled for the longjmp harness because of
sandbox /proc limitations; explicit allocation accounting replaces LSan.
The queue stub only frees empty sentinel queues. Real communication, waiting
process cancellation, concurrent GC, allocator monitor callbacks and target
execution remain integration work. No VM ABI or scheduler behavior is changed.
