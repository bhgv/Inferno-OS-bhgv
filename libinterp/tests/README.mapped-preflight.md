# Mapped record preflight in C, AMD64 and ARM64

This checkpoint checks the complete operands and metadata used by MOVMP/HEADMP
before native copies. C MOVMP, HEADMP and the channel record mover MOVTMP also
validate descriptors, pointer maps and full payload ranges before changing
references or publishing a head address.

## Interpreter behavior

MOVMP/HEADMP require a complete WORD type-index slot; MOVTMP instead requires a
complete native-pointer descriptor slot, independently of WORD32/64. A malformed
slot raises bounds. HEADMP retains listhead's source-slot/nil-owner priority.
The live module resolver retains its existing module/type error identities.

The shared mappedcopytype captures size with memmove, so even unaligned empty
Type headers need only their size field. Nonempty Types must have native
alignment and a complete np field. Map lengths must fit the payload geometry;
the map address must not wrap, and every marked native pointer must fit within
the full record. Invalid nonempty maps raise heap before ownership visitors run.

Nonempty copies reject nil/H data with nil-reference and wrapping byte ranges
with bounds. HEADMP checks its payload base, destination, full backward Heap
origin, alignment and reference-count limits before retention. No rejected
preflight changes payloads, R.s, references or propagation state. The captured
size remains stable across retain/release work. Valid mapped overlap and channel
FIFO/rendezvous behavior preserve the existing ownership order.

Zero-size records return before map, data or heap reads. HEADMP still requires a
nonnil list identity, but does not read that list's header/refcount or publish a
payload address. Nil/H or otherwise inaccessible data operands remain unused.

## Native behavior

AMD64/ARM64 guard the index and optional list-owner slots, Modlink.m prefix,
Module.type/ntype prefix, table identity, scaled table addition, complete pointer
cell and Type.size field. Positive sizes additionally require a complete np
field, an aligned descriptor and np==0. Maps with pointers and records larger
than 256 bytes continue through C.

Payload checks use the carry from (pointer-1)+(size+1), rejecting nil, H and
wrapping exclusive ends without changing the captured size or base pointers.
HEADMP bounds its backward reference cell, uses the full Heap-origin distance
when supplied by the production adapter, requires native owner alignment and
checks payload/destination before reading the refcount. R.s publication follows
all guards. Eight-byte blocks and exact byte tails retain overlap behavior.

The production staging capacity remains 128 u32 units. AMD64 keeps nearby
rejection returns and a rel32 empty-copy skip; ARM64 uses a shared rejection
return. Large private layouts may decline at sizing time without output writes.
These are numeric span checks; they cannot establish whether an arbitrary
nonwrapping address is actually mapped or owns a sufficiently large allocation.

## Evidence

| New driver | Cases | Execution |
| --- | ---: | --- |
| mapped-preflight-native.py | 12,208 | Extracted C and actual production AMD64 RX, WORD32/64, ASan/UBSan, NDEBUG |
| mapped-preflight-jit.py | 241,632 | AMD64/ARM64 Unicorn, WORD32/64, Python -O |
| New total | 253,840 | Complete memory/REG images, error identities and no-store failures |

The native driver includes malformed scalar/pointer slots, full-payload wrap,
invalid maps and marked partial pointers, unaligned nonempty/empty Types,
nil/H descriptors, live module header wrap, low heap origins, overlap and seeded
FP/propagation state. Guest tests independently model metadata offsets and check
nil/H identities, table cell wrap, lazy top-address Type reads, exact valid
upper-address payload endpoints, unaligned operands, aliases and capacity
canaries. Unicorn's x86 physical-page mask is accounted for while all virtual
pointer arithmetic remains 64-bit.

Thirty-one isolated parent b10f81f probes reproduce memory failures: 28 native
AMD64/ARM64 guest probes covering both mapped commands and three C probes
covering MOVMP, HEADMP and MOVTMP. MAPPED_PREFLIGHT_SOURCE and
MAPPED_PREFLIGHT_EMITTER_DIR select read-only baseline snapshots.

Twenty-six focused driver configurations pass: new C/guest preflight; mapped
native/JIT, layout/capacity, page-edge and mapped-moves; lists, list-retains, module-types;
mapped-cons, payload-channel, scalar-channel, heap-bounds; opcode coverage;
leaf64-runtime in both WORD models; raw-copy native/JIT; array-slot native/JIT;
array-view, array-create, type-retains; Limbo layout-bounds and descriptor-maps.
Existing mapped emulation additionally passes 119,240 cases. Layout checks pass
421,056 ASan/UBSan cases. Mapped page-edge checks retain 16,576 cases and now
expect a clean decline for nonempty unaligned synthetic list owners. The mapped
native fixture uses explicit checks throughout, including under NDEBUG.

This is a focused checkpoint, not a complete suite or Inferno/compiler boot.
Physical ARM64/RV64 execution remains unverified. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are unchanged.

## Dis availability

175 enum slots minus invalid INOP and unused IECLR leave 173 active commands.
Counts are unchanged in both WORD models. A native path can be guarded/partial.

| Backend/profile | Native path | Interpreter path |
| --- | ---: | ---: |
| AMD64 | 164 | 9 |
| ARM64 | 164 | 9 |
| RV64 + M | 103 | 70 |
| RV64 + Zmmul | 90 | 83 |
| RV64I | 87 | 86 |

The nine AMD64/ARM64 commands retaining C are ALT, NBALT, SPAWN, LOAD, MSPAWN,
EXIT, SEND, RECV and RAISE.
