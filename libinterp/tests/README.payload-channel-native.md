# Raw and mapped channel constructors on AMD64/ARM64

NEWCM and NEWCMP now have guarded native paths on AMD64 and ARM64. Together with
the five preceding scalar constructors, both backends have paths for 162 of
173 active opcode IDs; 11 remain interpreter-only. A native path may be partial
and may use the shared runtime allocator.

Both leaves capture source words and full-WORD capacities before allocation,
validate operand spans, require an empty H destination and preserve exact
R.m == R.d unbuffered semantics. Invalid arguments and existing-owner replacement
return to the interpreter without writes. Native calls retain real VM error
ordering rather than turning a declined operand into a successful no-op.

NEWCM calls cnewcraw for protected descriptor/queue/buffer allocation, then stores
the captured WORD width into Channel.mid.w before publishing the pointer.
NEWCMP supplies the live Modlink, full-WORD index and capacity to cnewcmapped;
that entry resolves the current module Type and uses the common mapped allocator.
No mutable descriptor snapshot is embedded in generated code. Shared C helpers
remain responsible for Type references, pointer-map initialization and allocation
cleanup. Native code supplies checked capture, ABI staging and publication.

AMD64 stages output and source in 24 stack bytes. ARM64 uses 32 bytes, including
saved LR. Helpers may clobber caller-saved registers; neither backend relies on
volatile operand pointers after a call. Raw union stores use exactly sizeof(WORD),
including four-byte WORD/native-pointer configurations.

## Runtime corrections

The C NEWCM path now finishes raw-width initialization before publishing or
releasing its previous owner. Previously a destructor observing the newly
published channel could see a zero width while the intended width was written
only after the old owner had been released. A test observes this exact boundary,
including when the width source belongs to the old channel.

cnewc now validates maps for every descriptor it will retain, including
unbuffered mapped channels. Negative/excessive map lengths and bits that describe
partial or out-of-range pointer slots fail before creating queues or retaining
Types. Previously heaparray validated buffered maps later, but unbuffered mapped
channels bypassed that check. Existing descriptor-reservation and VM error
precedence are retained; map geometry is checked before allocation.

## Evidence and limits

```
python3 libinterp/tests/payload-channel-native.py
python3 libinterp/tests/payload-channel-jit.py
CHANNEL_NO_ERRORS=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/payload-channel-native.py
```

- Actual production C/AMD64 RX: 215,040 cases per WORD model, with poisoned slot
  images, three independent operand alignments, zero-width buffers (including
  INT_MAX element counts), source/destination aliases, old-owned sources,
  descriptor cleanup, allocation cutoffs and FP modes/sticky flags. Every case
  balances live owners and builtin/user descriptor references.
- Live metadata/map checks: 68 cases per WORD model, including unbuffered and
  buffered maps, nil/wrapping modules, invalid indices, dead/saturated Types and
  module-before-capacity error ordering. Twelve additional protected-page cases
  require dead/saturated reference checks before any map-byte read. Rejections
  allocate no resources.
- Machine execution: 49,152 cases per architecture over both WORD widths. High
  helper addresses, hostile volatile-register clobbers, SP/LR and nonvolatile
  restoration, FP cells, full VM images and precise native write spans are checked.
  Error callbacks stop at helper entry; actual AMD64 unwind is checked by C/RX.
- Successful ASan+UBSan subsets: 5,888 cases per WORD model. Explicit accounting
  remains active while unsupported LeakSanitizer process inspection is disabled.
  Full error matrices use UBSan/NDEBUG.
- CHANNEL_RAW_SOURCE can replay the parent's actual raw constructor/build callback;
  the release-boundary oracle rejects its delayed metadata publication.
  CHANNEL_TYPE_SOURCE can replay the parent cnewc; the unbuffered map oracle rejects
  its successful construction of an invalid descriptor.
- Existing scalar-channel, allocation/refcount and raw-width/alignment suites
  remain active. Full xec.c and AMD64/ARM64 translation units pass host syntax
  checks. New drivers are registered in the core/optional-JIT checkpoint.

These tests retain native pointers in the WORD32 model; they are not an ABI32
VM. Physical ARM64, concurrent scheduling/GC and complete Inferno boot remain
separate acceptance work. Dis layouts, scheduler protocols and excluded
MIPS/SPARC/PowerPC JITs are unchanged.
