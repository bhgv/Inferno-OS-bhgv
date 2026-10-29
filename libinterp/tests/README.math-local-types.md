# Math arrays with module-local primitive types

A checked Limbo call may pass an array whose primitive element descriptor belongs
to the loaded module. readmod constructs Type records from element size and the
pointer map; it does not promise canonical Tbyte/Tword/Treal pointer identity.
Strict pointer comparisons in matharraycount/mathvectorcount rejected these
legitimate arrays with exType. FlashVM's NC64 host had carried a local adapter
for this problem; the fix now belongs to the shared Math implementation.

matharraytype accepts canonical identity or a module-local descriptor with the
expected element size and an empty pointer map. The checked module signature
still supplies the primitive semantic kind; this is not a dynamic cast for
arbitrary ADTs. Wrong known canonical primitive descriptors (including big) remain rejected even
when WORD64 and REAL have the same size. Nil/H descriptors and pointer-bearing
or wrong-sized layouts reject before any element access. Existing nil/type/length
error order, full-width bounds, byte strides and all numerical operations remain
unchanged. There is no frame, Dis wire format or native JIT opcode change.

Run from the source root:

```sh
python3 libinterp/tests/math-local-types.py
python3 libinterp/tests/math-codecs.py
python3 libinterp/tests/math-vectors.py
```

The new fixture extracts the actual production type/count helpers, checks 52
explicit predicates in each WORD32/WORD64 model under ASan/UBSan and NDEBUG,
and uses poisoned payload pointers to prove guards touch metadata only. Both
canonical/local byte and numeric descriptors are accepted; nil/H, pointer maps,
wrong widths, wrong canonical primitives, negative lengths and nil owners retain
exact errors. MATH_LOCAL_SOURCE and MATH_LOCAL_WORD_BITS select a frozen parent
source/model for regression proof: both parent models reject the first valid
module-local input. Existing codec/vector fixtures carry the added np metadata
field and still check 156 and 2408 numerical/bounds predicates per model.

The focused model is not a complete ABI/boot test. FlashVM separately validates
actual native module calls after removing its local Math type rewrite. The test
uses no heap; LeakSanitizer task enumeration is disabled in the restricted host,
while address/undefined-behavior checks remain active.
