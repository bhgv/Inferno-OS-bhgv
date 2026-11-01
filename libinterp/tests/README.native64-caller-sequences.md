# Independent caller-lease sequence oracle

`native64-caller-sequences.py` compiles the actual private
`include/native64-caller.h` against a small independent scalar-state oracle.
It does not copy the implementation's expected byte stream or use the production
readiness function to decide expected results.

Each compatible WORD32 / WORD32-file compile enumerates all32768 length-five
words over eight operations in both published-register and live-R modes:
65536 traces per compile,131072 across the two profiles. Operations acquire,
invalidate, resolve, copy, checked-rebind, clear, approach epoch exhaustion and
desynchronize live/saved FP. The model tracks generation, captured generation,
hold ownership, saved/live frame choice and descriptor references. Every trace
finishes with two clears and must leave exactly the initial descriptor owner.

Two aligned80-byte zero-map frames, one current Pready process and one descriptor
are bounded C fixtures. The scheduler-availability marker is stubbed as coherent;
descriptor retirement counts references without invoking GC. Consequently this
is **header transition evidence**, not real scheduler lifetime, collector, JIT,
file-call admission or detached-Prog ownership evidence. Actual port, Math,
collector and clean emulator gates must remain alongside it.

Four independently compiled removals (epoch, self identity, hold, live FP rebind)
must exit through an active named predicate; a sanitizer crash alone is insufficient.
Owning O1 and O3/NDEBUG strict UBSan runs pass both profiles and all controls.
Signed-count and Plan9 pragma warnings in existing owning headers are suppressed;
new handwritten oracle code otherwise compiles with Wall/Wextra/Werror by default.

```
TMPDIR=/writable/scratch python3 libinterp/tests/native64-caller-sequences.py
```

CC/CFLAGS are explicit compiler inputs. NATIVE64_ARCH may select the matching
Linux64 header profile; tested here is native Linux x64. No target runner is
used and no RV64/ARM64 target-execution claim follows from this test.
