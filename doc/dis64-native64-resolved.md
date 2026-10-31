# Protected results with late destination resolution

native64resultresolved extends the existing private result guard with a trusted
resolver callback. The original native64resultcall keeps its stable-address
contract. Neither helper enables variadic Sys, a wire signature or file admission.

The new path calls the producer and then resolves the current caller-owned cell
under the same actual kprotect error owner. Mutable result/pin/destination state
lives outside that setjmp frame. No destination address is cached before production.
The producer and resolver may allocate, collect or transfer a logical owner to a
new mapped cell while preserving the old logical result. Nil from the resolver
is a cooperative decline/cancellation; its diagnostic is retained. H, wrapping
cell extents and nil old-owner values are refused before publication.

The caller serializes access to the destination. After resolution succeeds,
there is no allocating callback between reading the
old owner and publishing the new one. The guard detaches its temporary state,
removes only its own successful pin and releases the old owner after publication.
On producer/resolver failure it cleans only acquired result ownership and restores
the original error after destructors. Callee arguments remain borrowed; the caller
still owns their frame/Type retirement. Destructors must not throw or relocate
the destination during commit/cleanup.

This is a trusted C interface: the resolver is responsible for returning the
current logical destination with valid readable/writable mapped-owner provenance.
Bounds/nil checks cannot authenticate arbitrary pointers or determine which of
two otherwise valid cells represents the caller's current logical result.

## Actual combined argument/result tests

```
python3 utils/tests/native64-resolved.py --limbo /path/to/fresh/limbo
```

The driver reuses actual collector/heap bodies and both emu/os kprotect/krethrow
and error-read bodies from native64-result.py. Only native jump/process/allocator
services remain bounded. Matched fresh Roots/Math declarations and owning heap/
Math objects are compiled so the entire reused source closure remains linkable
under sanitizer instrumentation. The new cases do not call Math numerically.

Each port and each WORD32 model executes720 transactions with131,477 active
predicates. Fifteen behaviors span all process states, collector phases and
both error-return/rethrow routes: relocation during production/resolution,
resolver decline/cancellation, errors before/after relocation, H/wrapping/nil
cell refusal, alias/H results, producer decline/error, actual ptradd allocation
failure and nested protected result transactions. Resolver failures cannot reach commit; producer failures cannot reach
the resolver. Cleanup destructors inspect detached result/pin state and the
correct logical old/new destination, and deliberately alter the error text to
verify its restoration.

Nested transactions retain separate pins and mapped destinations while the outer
result and argument context stay rooted. The same transactions hold a1,184-byte typed argument frame with a19-byte dynamic
map. Actual initmem initializes mapped slots before publication; actual incmem
acquires an argument from an existing caller root. The caller's original argument
owner is retired, so the dynamic argument root must survive producer/resolver GC
and nested allocations. Callbacks read the actual typed argument storage and Type
owner. After success, cancellation or error, ordinary freeptrs and Type retirement
release the caller-owned argument context. This combines valid argument-frame
ownership with result transitions; it is not a general fallible argument-context
constructor or an escaping/asynchronous call implementation.

Two independent negative controls must fail their active predicates: returning
a stale physical destination after logical relocation, and truncating the dynamic
argument map. The stale case tests the resolver's logical-identity obligation;
it does not claim the helper can identify a malicious resolver automatically.
Relocation is an explicit mapped-owner transfer between distinct live cells,
not a claim of whole-heap moving compaction.

O1/strict UBSan and O3/NDEBUG/strict UBSan pass. O3 address+undefined execution
also passes this gate with the existing fixture's process leak-scan limitation;
ASan executable-name/longjmp warnings remain. This does not resolve the earlier
optimized UTF unwind report. The complete native64-layout suite includes these
cases and passes O3/NDEBUG/strict UBSan with full Math integration and prior
constructor/GC/Sys gates. The complete helper compiles with NDK r28c for ARM64/x64;
cross compilation does not prove target execution.

## Remaining integration

The generic context still needs owned immutable descriptor/vector storage,
fallible partial argument acquisition/retirement, Type-owner lifetime through a
protected call and a real typed formatter adapter. Cooperative nil/decline is
not asynchronous VM cancellation. Structural callable admission, full fixed Sys
registration and remaining builtin initializers are separate work. Keep the four
Sys variadic refusals, legacy full-startup guard,43-op np0 file profile and all
NC64/SDK defaults unchanged. Fast opcode counts are unaffected.
