# String read capacity checkpoint

Five production string paths now reject a logical character count exceeding
String.max: indc, slicer, stringdup, addstring and cvtca. Negative capacity also
rejects empty strings. INT_MIN is rejected before negation. Checks precede
character reads, allocation, source mutation and output publication. Concatenation
validates both inputs before its language-nil shortcuts; slices validate before
the empty-result shortcut. Valid nil, ASCII and Rune behavior is retained.

The ARM64 and AMD64 INDC leaves perform the same signed capacity check before
payload access. They retain complete VM-word index comparisons, signed Rune
results, alignment-safe operand access and write-free C fallback. Private max
offsets are checked before emission. Neither the 64-unit staging limit nor
the VM/Dis layouts change. Both backends retain 131 guarded native paths and
42 interpreter-only active instructions. Other backend sources are unchanged.

The compiler trap fixture now models the String.max field used by the real
slicer. Its bounded source and allocated result strings each have capacity 16.
This is a fixture correction, not a change to compiler folding semantics.

Validation from the repository root:

```sh
python3 libinterp/tests/slice-tokenize-suite.py --core
python3 libinterp/tests/string-codecs.py
python3 libinterp/tests/string-store-native.py
LEAF64_ARCH=arm64 LEAF64_CASE=character python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=amd64 LEAF64_CASE=character python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/jit-coverage.py
```

All 76 core host drivers pass. The codec matrix passes 40,188 checks per WORD
model under UBSan. The native AMD64 fixture adds 5,120 malformed-capacity
alignment scenarios per model, checking actual RX leaves and C error identities
with unchanged heap/operand images and allocation counts. Its capacity profile
also passes ASan+UBSan in both models, with the runtime warnings described below.
The existing native store matrix still runs alongside these added checks.

The instruction-emulation fixture passes 2,664 checks per architecture across
WORD32 and WORD64. It includes negative and very large capacities/lengths,
output aliases, rejected metadata and an unmapped payload page that must never
be read on capacity fallback. Both parent emitters fail the extended fixture.
Independently removing each of the five C capacity checks also fails the
extended codec fixture. These are regression checks of production bodies,
not substitutes for a complete VM build or physical ARM64 execution.

The extended codec error matrix does not pass ASan in this environment. After
the runtime warning that __asan_handle_no_return cannot handle the sandbox
stack, ASan reports a stack overflow inside its reporting/error machinery when
the unchanged string2c Rune-capacity guard delivers exHeap via longjmp. The
failure reproduces with each capacity mode selected individually. Stack
instrumentation remains enabled; this is an unresolved sanitizer limitation,
not a claimed passing full ASan run. UBSan, native AMD64 checks and ARM64/AMD64
instruction emulation pass. LeakSanitizer process inspection remains disabled
by the existing fixture, which separately counts heap owners and checks guards.

## Native dispatch fixture follow-up

The standalone `leaf64-runtime.py` driver builds its own manually initialized
String values. Its valid character fixture now supplies `String.max = 8`, so the
new production capacity guard tests valid strings rather than rejecting its old
zero-capacity headers. Without this fixture correction the e6c056c snapshot
fails in `characters()` before finishing the native publication suite.

Both forced C and real AMD64 JIT dispatch additionally reject capacities -1, 0
and 7 for eight-character ASCII/Rune strings. The destination sentinel, complete
String image and PC remain unchanged. `python3 libinterp/tests/leaf64-runtime.py`
passes after the correction. This is fixture/acceptance work; production guards
are unchanged.
