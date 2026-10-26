# Mapped list construction: ownership rollback and native pointer-free payloads

CONSMP now has guarded AMD64 and ARM64 paths for pointer-free module descriptors
with payloads up to 256 bytes. Both backends have paths for 163 of 173 active
opcode IDs; ten remain interpreter-only. This counts partial native paths, not
universal operand coverage or complete platform acceptance.

## Shared constructor and failure ownership

cconsmappedtype resolves the live module table and validates descriptor reference
capacity, allocation geometry and complete pointer-map slots. It performs no
allocation or mutation. Dead/saturated descriptors are rejected before map bytes
are inspected; a Tlist payload reserves both the heap and element-Type retains.
CONSMP also validates index/output cells and nonempty source spans before allocating.

The interpreter constructs its node inside a protected ConsMapped operation.
The old output still owns the tail, so the unpublished node temporarily uses H
as its tail. Raw bytes are copied before output publication and mapped cells are
initialized to H. Each mapped owner is checked, retained/marked and only then
stored in the node. Duplicate owners are checked against their current reference
count on every acquisition, without a quadratic duplicate scan.

On a later error, destroying the partial node releases only acquired heads and
its descriptor. The old slot/tail remains unchanged. On success, the original
tail reference is transferred into the node and the completed owner is published.
Conservative GC marking already performed for acquired heads may survive a failed
construction; reference counts and logical owners are rolled back. The collector
barrier is not reversed while undoing ownership.

Previously incmem incremented unchecked mapped references after node allocation.
A saturated or duplicate-near-limit owner could wrap to zero, while nil/invalid
head fields could fault without releasing the partial node. The new transaction
preserves existing valid shallow-copy semantics, including heads that alias the
tail and source records owned by the old output.

## Native path

Generated code captures source/output/index cells, then calls the pure live-Type
validator. Mapped or larger descriptors decline with their native frame restored.
Accepted pointer-free records call the common cons allocator through a staged tail,
copy bytes natively, increment the live Type reference, set List.t and publish.
No Type reference value is frozen before allocation. Helpers may clobber ordinary
caller-saved registers. AMD64 uses 56 private stack bytes; ARM64 uses 64 including
saved LR. A dedicated AMD64 guard trampoline restores the frame on post-lookup
rejection. VM operand metadata, Dis PC and FP state remain untouched by the
native integer/copy instructions.

The source guard conservatively requires a valid 256-byte arithmetic window;
valid unusual/empty source addresses can retain C. Span checks establish arithmetic
contracts, not proof that arbitrary host addresses are mapped. Pointer-bearing
records retain the corrected interpreter transaction. Allocation and tail marking
remain in the existing runtime allocator.

## Reproduce

```
python3 libinterp/tests/mapped-cons-native.py
python3 libinterp/tests/mapped-cons-jit.py
MAPPED_CONS_NO_ERRORS=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/mapped-cons-native.py
```

- Per WORD model: 15,360 raw C/actual AMD64 RX copy/alias/Type/failure cases and
  6,912 mapped retain/rollback cases. They cover aliased old-owned sources,
  duplicate and saturated owners, partial acquisition, nil/low heap prefixes,
  head/tail identity, allocator failure and independent operand alignments.
- Twenty-six slot/map preflight cases per model reject before allocation. All
  owners, builtin/element Type references and allocation canaries balance.
- The native driver records 3,072 successful generated executions per model,
  independently of interpreter fallback. Raw bits, Type identity and output
  snapshots are checked. Raw constructor cases also preserve four FP modes and
  a seeded sticky divide-by-zero flag.
- AMD64/ARM64 instruction emulation: 8,448 cases each, covering both callbacks,
  high callback/heap addresses, hostile volatile registers, live post-allocation
  Type references, complete VM images, native write spans, FP cells and exact
  SP/LR/nonvolatile restoration on fallback.
- Successful ASan+UBSan subsets: 8,832 cases per model. Full errors use UBSan and
  NDEBUG. LeakSanitizer process inspection is disabled, not resource accounting.
- MAPPED_CONS_PARENT can substitute the actual parent opcode. With
  MAPPED_CONS_OVERFLOW_ONLY=1 it reproduces successful construction after an
  unsigned reference wrap; the independent oracle rejects that result.
- Existing list/Type-table/reference tests remain active. The core checkpoint
  passes 119 host drivers; complete xec.c and both backend units pass host syntax.

WORD32 tests retain native pointers and are not a complete ABI32 VM. ARM64 machine
execution is emulated. Physical targets, concurrent GC and complete Inferno boot
remain separate. VM layouts, opcode IDs and excluded MIPS/SPARC/PowerPC JIT sources
are unchanged.
