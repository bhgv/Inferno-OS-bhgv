# Stack and process-startup audit

## Closed defects

- newstack and extend used a literal 16-byte extent header. Their native header
  is four pointer slots, so the literal underallocated small 64-bit extents.
  Stack sizing now uses the actual Stkext frame offset, validates whole sizes
  before narrowing, and checks growth arithmetic before multiplication.
- newstack cleared the source frame header before allocation. A failure could
  therefore change the parent's staging frame. Only the successfully copied
  destination frame is now made a root frame.
- schedmod copied entryt->size bytes from a local Frame object. Frames larger
  than Frame read beyond that C object. Both scheduler copies now allocate a
  complete zeroed source frame, initialize the copied frame's pointer arguments
  to H, restore
  R.s on every protected return and clean up unpublished module/stack resources.
- spawn and mspawn published a child Prog before allocating its stack. A stack
  failure left an incomplete runnable child. The stack is now prepared first;
  an error releases only its borrowed argument storage, while the source frame
  remains the parent owner's responsibility. Successful spawning still calls
  unframe exactly once to transfer that staging frame's arguments.
- newprog published scheduler, PID and group entries before fallible root-group
  or environment creation. Private resources are now completed first. A copied
  user string is cleaned up on error, and parent/group termination is checked
  again after a waiting copy. Environment group references and module ownership
  are acquired only after private preparation succeeds.
- PID exhaustion incremented a signed int before checking its sign. Exhaustion
  is now detected before the increment; terminal exhaustion policy remains.
- frame used an unchecked descriptor index and prospective out-of-range pointer
  arithmetic. mframe/mspawn narrowed WORD link indices and indexed negative
  exports without checking the sentinel. Resolution now checks full indices,
  bounds imports, scans real exports and compares remaining stack bytes before
  advancing the stack pointer.

The active bitmap and frame layout still use native pointer slots. These fixes
remove local size/ownership defects; they do not activate Dis64 or implement a
new JIT backend. Existing native-PC compilers still need ABI-specific layout
integration checks when ported to new architectures.

## Verification and remaining boundaries

`libinterp/tests/stack-startup.py` runs 628 cases across both scheduler copies and
the two host scalar models. Strict UBSan and ASan pass with stack checking on.
Original functions independently fail every targeted family, including ASan's
root-frame stack-buffer-overflow and UBSan's signed stack/PID arithmetic errors.
`libinterp/tests/fp-spawn.py` continues to verify live/stored FPU inheritance.

The fixture uses real source bodies and emulator error frames, with bounded
allocator, scheduler, teardown, MP-linking and OS-environment adapters. The
native scheduler uses emulator declarations and an incref adapter; this is not
a full native-kernel build. Source-template copying is covered independently by
heap-clones.py. Full boot, native compiled frames, real parent-stack unwinding,
concurrent scheduling, GC and OS group teardown remain integration work. See
`libinterp/tests/README.stack-startup.md` for exact boundaries and source overrides.
