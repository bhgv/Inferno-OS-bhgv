# List and channel constructor operands across C allocation

This checkpoint covers six list constructors (CONSB/CONSW/CONSL/CONSF/CONSP/
CONSM) and seven channel constructors (NEWCB/NEWCW/NEWCL/NEWCF/NEWCP/NEWCM/
NEWCMP) on the priority AMD64 and ARM64 paths.

The four scalar list leaves and CONSP now reject null, H and wrapping
source/result cells before reading the head or calling the C allocator.
CONSM checks its WORD-sized length cell before reading it. C applies the
corresponding source/length checks and reports exHeap; complete JIT entries
retain the ordinary C error path after a native decline. Existing tail,
descriptor lifetime, GC and allocation-failure checks remain in force.
These arithmetic checks do not establish that an arbitrary non-null address
is mapped. The byte constructor still reads one byte but allocates four;
WORD, LONG, real bits and pointer payloads retain their distinct widths.

CONSM captures its source and result addresses before calling cons/nheap.
It copies through the captured source, then publishes through the captured
result cell. Scalar heads were already captured before allocation. The
channel newc helper and raw NEWCM now also retain their original result cell
across queue/buffer allocation. This fixes all seven channel opcodes, including
replacement paths reached through C. Raw channel metadata is completed before
publication and old-owner release.

The real heap monitor can borrow R.s/R.m/R.d during allocation. The new tests
change those registers through that callback and require publication at the
original destination. They preserve callback register effects rather than
restoring the complete REG. Native leaves save their captured arguments in
their existing private aligned frames across C; no new runtime ABI is added.

## Evidence

- cons-slots-native.py: 43,968 successful C, native+C and complete JIT routes,
  plus 636 rejected routes across WORD32/WORD64 with 64-bit native pointers.
  Every invalid operand also checks a pure native decline with no allocation
  or VM mutation. Successful routes cover real nheap/cons allocation, unaligned
  cells, source/result aliases, borrowed registers, head bytes and reference
  counts. ASan+UBSan repeats successful routes; UBSan covers explicit errors.
- channel-capture-native.py: 688,128 successful C, native+C and complete-JIT
  routes with real channel/queue/buffer allocation and cleanup, across both
  WORD layouts. All seven opcodes vary capacity, source/middle/result byte
  alignment, unbuffered middle=result aliases, old-owner replacement and
  borrowing callbacks. The whole matrix passes UBSan and ASan+UBSan.
- cons-slots-jit.py under Python -O: 2,110 AMD64 and 2,140 ARM64 independent
  malformed-cell/capacity checks. Allocator entry, native writes, VM byte
  images, preserved registers, SP/LR and FP state are checked explicitly.
- Existing scalar-cons-native.py: 245,760 head-bit/alias/GC/error/FP cases
  plus 36 tail/address cases. Its address oracle now invokes C on a guarded
  decline, matching the real decoder contract.
- Existing payload-cons-native.py: 135,936 raw cases, 9,728 pointer cases and
  32 raw preflight cases. Existing scalar/payload guest matrices pass for
  both architectures. The pointer guest oracle now expects malformed output
  cells to decline before the allocator instead of requiring a C call.
- Existing scalar-channel-native.py: 161,280 constructor routes plus 100
  preflight checks. Existing payload-channel-native.py: 430,080 constructor
  routes plus 160 descriptor/map checks.
- Thirty independent parent probes against affb4e0 reproduce the prior
  failures: source and result cells for five list leaves and the raw length
  cell on both emitters; five C head-source cells, the C raw length cell,
  raw source/result capture and channel output capture.

The native/RX tests use an AMD64 host. Guest ARM64 executes in Unicorn, not on
physical ARM64 hardware. Full VM boot and performance are not established by
these tests. Explicit error/longjmp fixture paths use UBSan separately from
the successful ASan paths; stack instrumentation is not disabled.

Run `python libinterp/tests/cons-slots-native.py` and
`python libinterp/tests/channel-capture-native.py`. For the list ASan repeat,
set `CONS_SLOT_NO_ERRORS=1` and
`CFLAGS="-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all"`.
The channel fixture has no throwing cases and uses that CFLAGS directly.
`CONS_SLOT_SOURCE`, `CONS_SLOT_INVALID_ONLY`, `CONS_SLOT_OP`, `CONS_SLOT_KIND`
and `CONS_SLOT_CALLBACK_ONLY` select C parent probes. Guest probes accept
`CONS_EMITTER_DIR`, `CONS_ARCHES`, `CONS_SLOT_OP` and `CONS_SLOT_KIND`.
The channel parent probe uses `CHANNEL_XEC_SOURCE` and `CHANNEL_CALLBACK_ONLY`.
Missing emulator packages and unsupported native hosts fail explicitly.

No opcode is newly counted: AMD64 and ARM64 each retain 173/173 complete
JIT+C entries and 166/173 guarded native fast paths (95.95%). Allocation,
queue management, mapped payload handling and exceptional paths stay in C.
RV64 remains deferred; excluded JIT backends and Tk/security/crypto are untouched.
