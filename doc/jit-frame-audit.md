# Native frame allocation and module-call dispatch

## Stack addresses are unsigned and must not wrap

IA-32's frame macro added Type.size to R.SP and selected its fast path using
a signed less-than test against R.TS. Addresses on opposite sides of bit 31
could select either unnecessary extension or allocation beyond the stack
limit. The comparison now uses unsigned below. Both IA-32 and ARM also check
the carry from the addition before comparing the new address to the limit;
a wrapped address must enter extend rather than become the new R.SP.
Equality with the limit continues to use extend, matching the interpreter's
existing convention. Frame initialization and the slow-path interface remain
unchanged. The tests assume valid nonnegative frame Type sizes.

## Nil module calls need a defined entry sentinel

commcall skips the entry lookup when its Modlink operand is H. Its entry
register previously retained an unrelated value, while the module-call macro
tested that register against H before deciding whether to use rmcall or read
Modlink.prog. Consequently nil could dereference an unmapped Modlink before
reaching the helper, or pass an arbitrary callback address to it.

Both encoders initialize the entry register to H before the nil test.
A non-nil entry lookup overwrites it. Before the subsequent guard work, the
nil path therefore supplied dt == H
to rmcall, whose existing first check raises exModule. The newer module-link
guard now directs nil to the validating VM helper before Frame writes. This
change does not
add native exception-PC reconstruction or execute the error handler in tests.

## Signed export references and VM exits

A negative module link operand selects Module.ext[-index-1] in the interpreter;
it is not an index in Modlink.links. The immediate JIT paths treated the raw
16-bit field as a links offset. Both architectures now retain native immediate
mcall/mframe dispatch only for nonnegative operands. Negative references, like
variable references, use the existing VM helper and its export semantics.

The mcall helper path also needs TCHECK before NEWPC. mcall returns t == 0 for
a completed runtime-module call, where the saved next native PC is usable;
a Dis call returns t != 0 to hand execution back to the VM. Jumping directly
to R.PC in the latter case could execute an uncompiled Dis entry as machine
code. Both backend selectors now request TCHECK as well as NEWPC. This can
add VM transitions for helper-based calls; no performance measurements were
made. Existing nonnegative immediate calls retain their native path.

## Regression checks

```
python3 libinterp/tests/jit-frames.py
python3 libinterp/tests/jit-module-dispatch.py
python3 libinterp/tests/jit-handoff.py
```

- jit-frames executes production IA-32/ARM frame macros, operand/call emitters,
  commcall and module-call macros: 296 cases cover unsigned stack boundaries,
  exact/exceeded limits, address wrap, prior flags, initialization, extend
  arguments, slow-path FP/MP reload and returned frame pointers. Entire stack
  pages are compared with expected canaries. Nil calls cover frame/module/
  indirect operands and several poisoned entry-register values. Non-nil calls
  check compiled/interpreted/runtime dispatch, entry replacement, frame links
  and the native Modlink reference increment. Type, Frame,
  Modlink, Heap and REG are projected target layouts. extend and rmcall C
  bodies are recording boundaries; the initializer is a native return stub.
- jit-module-dispatch compiles the actual mcall/mframe selector blocks with
  recording native/helper boundaries. All 65,536 immediate bit patterns,
  both operations, both backends and four addressing selectors produce
  1,048,576 eligibility/mask checks. This does not execute export lookup,
  module ownership, imported link bounds or the target C runtime.
- jit-handoff now executes 144 production bridge cases, adding combined
  NEWPC/TCHECK outcomes. A yielding callback writes an unmapped PC: the
  native bridge must leave through the VM return rather than fetch it.
  A zero-t callback must continue through NEWPC. Existing operand, synthetic
  macro, error-PC, clobber and register/frame restoration checks remain.

Separate negative controls restored each old frame macro and removed each nil
sentinel independently; the corresponding native tests failed on unsigned
stack selection, wrapped allocation or unmapped Modlink reads.

All 34 libinterp Python suites pass; the expanded frame suite was rerun after
its final coverage additions. The frame suite also passes with __ARM_ARCH=4.
Full IA-32/ARM source syntax checks pass with Linux headers.

The tests need a host C compiler and Unicorn, with UBSan enabled by default.
They do not boot Inferno, exercise the target C allocator/ABI or actual native
exception/GC integration. Arbitrary corrupt objects and complete module-call/return lifecycle
integration remain separate audit areas. No 64-bit ABI or new backend is enabled here.

Checked module link resolution, native guards and export cleanup are covered
in [module-link-audit.md](module-link-audit.md).

Permanent RET continuations, frame/module release order and checked mspawn
are covered in [jit-return-audit.md](jit-return-audit.md).
