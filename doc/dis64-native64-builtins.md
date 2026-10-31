# Explicit native64 builtin construction

builtinmod64 is a new, explicit trusted-C registration path. It consumes the
same generated specifications and implementation records as native64bind,
constructs real Module/Type/Link owners and publishes the finished module once.
It never converts a full map into the legacy Runtab map[16]. Existing builtinmod,
installed module initializers and default startup/admission are unchanged.

The new native64-entry.h owns the named LimboNative64Entry record definition.
Fresh limbo -H output includes it when records/tables are requested. Import the
header with the generator, native64-bind.h and consumers; regenerate old private
headers rather than mixing anonymous old typedefs with the new forward-declared
runtime interface. No field order, bytecode, signature hash or wire ID changes.
The C record itself is not a portable serialized representation.

## Preconditions and ownership

The caller serializes module-registry access as for builtinmod and supplies a
valid current VM error owner. Names, specifications, maps and implementation
records must be readable and immutable throughout construction. C callbacks are
trusted identities, not addresses accepted from files. Their bodies and caller
frames must obey the generated argument/result and ownership contract.

The constructor rejects an incompatible scalar/pointer/Frame model, unexpected
natural alignment or non-IEEE double geometry before allocation. WORD32 alone
is insufficient: packed structures can preserve Frame offsets while breaking
argument alignment. The packed negative control caught that missing check during
this series; independent scalar/reference/big/real alignment is now required.

native64bindcheck validates all specifications, including unselected ones, and
all requested implementations before construction. Names, hashes, frame extents
and every map byte must agree. Duplicate or absent names, missing callbacks,
truncated maps, marked Frame64 links/result/temp slots and nonzero padding bits
are refused. Limits remain private4096 entries/255 name bytes/1MiB frame bounds.
This does not establish structural callable signatures from a legacy hash.

The selected implementation order becomes export order. dtype copies each full
map and runtime copies each name. The completed module owns those copies; it
does not retain specification/record/name/map storage. Callback code must remain
loaded until module retirement. Each descriptor is attached to the partial
module before the next name allocation can fail. kprotect owns VM errors;
failed construction releases links, descriptors, arrays and module names without
publishing, removing or modifying an existing module. Empty selections have an
owned terminated export array and no descriptor array. Ordinary unload retires
successful modules through the existing destructor path.

## Actual tests and limits

```
NATIVE64_LIMBO=/path/to/fresh/limbo python3 utils/tests/native64-builtins.py
```

Without an explicit compiler, the driver builds fresh owning Limbo. It emits a
real two-function probe and complete Math specifications. The complete owning
runt.c is compiled in every model; runtime checks execute the actual constructor,
kprotect, dtype/freetype, newmod, runtime/newlink, destroylinks, unload and freemod
bodies. Only OS allocation/diagnostic/heap-callback boundaries use the existing
bounded module-constructor adapter. The test does not substitute constructor or
cleanup logic. Active checks remain enabled under NDEBUG.

Both WORD32 and WORD32+file execute67 balanced construction/refusal/unload
transactions each. Allocation cutoffs cover both returned allocation failures
and real VM-error unwinds during names. Tests verify20-byte maps, export order,
metadata deep copies, preservation of an already published module, all-table
preflight, unchanged error depth and exact live allocation balance. Legacy
WORD64 and packed structures are explicitly refused without allocation.

Actual Math is compiled with the fresh native64 header. Four real bit-conversion
implementations are selected against the complete Math specification table,
registered into a real module and invoked256 times per compatible model through
Module.ext callbacks. Synthetic generated add/many callbacks are also invoked.
This is explicit registration and C invocation, not a Dis MCALL/JIT/full-boot
claim. The fixture's freeheap/markheap callbacks do not run a collector; earlier
owning GC/root-transfer gates remain separate evidence.

The new gate is included in native64-layout.py. Full fresh layout/Math/GC/nested/
result/module-root checks pass O1/strict UBSan. The constructor gate also passes
O3/NDEBUG/strict UBSan and O3 address+undefined sanitizers when host process leak
inspection is explicitly disabled with -DNATIVE64_NO_LSAN. Its allocation-owner
oracle remains active; this is not a LeakSanitizer pass. Host ASan still warns
about executable-name inspection and longjmp stack bookkeeping. Earlier optimized
recursive-unwind ASan limitations are not declared resolved.

The owning runt.c also compiles with modern NDK for ARM32, ARM64 and x64 in the
experimental scalar model. That is compile evidence, not target execution.

## Remaining integration

Ordinary installed initializers are unchanged. Math can now explicitly select
its complete native64 registration with DIS_NATIVE64_MATH_REGISTER=1 and a fresh
generated header; see dis64-native64-math-register.md. Header-only consumer mode
without that additional flag still refuses legacy registration.
Sys variadic tails, structural reference/call signatures, async/native result
ownership and registration of the whole builtin closure remain separate work.
The startup legacy-ABI refusal is retained. Existing43-op np0 Dis64 admission,
NC64/SDK defaults and JIT fast-path counts are unchanged. Full native64 boot and
physical ARM64/RV64 execution are not claimed.
