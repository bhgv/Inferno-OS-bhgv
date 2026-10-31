# Transactional Sys descriptor initialization

Both emu/port/inferno.c and os/port/inferno.c previously assigned TFD, TFileIO,
FioTread, FioTwrite and Tdir as dtype returned. A failed allocation could leave
nil/partial globals, lose already acquired descriptor owners on retry and
continue initialization without reporting the failure. Repeated success also
overwrote the five owning pointers without releasing the original descriptors.

sysinit now builds all five descriptors privately, checks every result and
publishes only after construction succeeds. dtype and its ordinary mallocz
boundary return nil for allocation failure; those returned failures are cleaned
with actual freetype before reporting exNomem. No global pointer or request map
changes during an allocating callback. A complete registry makes repeated
initialization allocation-free; a partial registry is refused without touching
its existing owners. Module registration failure can therefore retry initialization
without discarding the canonical types.

This is a transaction under the existing serialized startup/VM call contract,
not a lock-free or concurrently readable atomic multi-pointer update. No new
thread-safety guarantee is made. Descriptor storage is reference-counted Type
storage, not a replacement managed-heap collector or wire type registry.

## Full native request maps

Request maps are constructed in local arrays from actual C reference-field
positions. Alignment and map capacity are checked before allocation. Bit selection
uses both the byte index and slot-within-byte; the previous first-byte-only shift
could not represent a reference beyond slot7. Zeroing the whole local map also
removes stale bits before setting the known request roots. dtype copies the maps;
published global map bytes are updated only after every descriptor is owned.

## Actual failure and ownership checks

```
python3 utils/tests/native64-sys-init.py --limbo /path/to/fresh/limbo
```

Fresh actual Limbo emits Sys declarations. The test executes owning emu and os
sysinit bodies, actual dtype/freetype and kprotect, with bounded allocator/process
and payload-finalizer boundaries. Payload finalizers abort if invoked: these are
descriptor-owner tests, not live FD/FileIO heap destruction. Each allocation
entry independently observes that globals and maps remain unchanged.

Both WORD32 and WORD32+file exercise normal and test-only wider request shapes.
Each of these eight combinations checks all five allocation failures, successful
retries, five allocation-free repeat calls preserving outstanding Type references, and
all30 partial-registry masks.
All acquired descriptor references return to the original live allocation count.
Every map bit is checked against actual managed field positions. Synthetic wide
payloads put roots at slots8 and10, demonstrating second-byte map handling.
Four additional combinations reject insufficient map capacity before allocation.
These shapes are private test declarations, not a new channel payload ABI.

The optional --emu-source accepts a parent source for an active failure control.
The unpatched parent fails the allocation/publication predicate; it is not treated
as a passing gate or silently adapted to the new expectation.

The gate passes O3/NDEBUG/strict UBSan and O3 address+undefined instrumentation
with explicit NATIVE64_NO_LSAN for unavailable process leak inspection. Owner
counts stay active; host executable-name/longjmp warnings remain. Full fresh
native64-layout with this gate and complete Math integration passes
O3/NDEBUG/strict UBSan. Dependent directory and stat tests retain
successful-only static descriptor fixtures and explicitly abort if failure cleanup
is entered; new allocation-failure coverage uses actual dynamic descriptors.
File2chan keeps its owning dtype/freetype closure. See the outgoing journal for
completed regression and sanitizer evidence. No result here resolves the previous
optimized UTF error-unwind ASan report or certifies full native64 startup.
