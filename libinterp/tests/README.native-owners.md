# Native owner-copy checkpoint

The following records the initial owner-leaf checkpoint. The subsequent
[GC owner/Srv checkpoint](README.gc-owner-srv.md) removes the current-color
restriction and adds native Setmark semantics.

AMD64 and ARM64 now publish guarded native paths for IMOVP, ISELF, IHEADP
and ITAIL. This adds four opcode IDs per backend, taking availability from
121/52 to 125/48 native-path/interpreter-only active commands. The 173-command
active denominator excludes invalid INOP and unused IECLR. RV64 counts remain
103/70 with M, 90/83 with Zmmul and 87/86 with the base integer ISA. Both
WORD32/native-pointer64 and legacy WORD64 configurations give these counts.

## Ownership contract

An empty destination (language nil, H) can acquire an owner whose heap color
already equals the live collector mutator color. Its nonzero native-width
reference count must leave room for one increment. A destination already
holding the same owner can skip the balanced retain/release, after the same
lifetime and color validation. The saturated-count case still retains the C
error path even when the final reference delta would be zero.

IMOVP accepts H without inspecting a heap header. ISELF requires a real module
owner. IHEADP and ITAIL first reject nil and C-null list identities, capture the
selected pointer, and publish the same R.s field address as C on success. Their
nil payloads use the ordinary pointer-move contract. All owner stores use eight
bytes independently of the language WORD width.

The layout adapter supplies the actual native Heap reference/color offsets,
reference width, and address of the mutable collector color. Emitted code reads
that color on every invocation. No process-wide color value is baked into code.
No native owner path calls a collector, destructor or allocation helper. Other
owner replacements and required marking retain the existing C implementations.
All rejection guards precede writes to heap references, owner slots and R.s.
The fixed 64-unit private emission budget is unchanged.

C movp and self now reject zero and maximum native reference counts with
exHeap before increment, marking or destination publication. The former allowed
resurrection of a dead owner or reference-count wrap. The movp fix also protects
headp, tail and pointer channel transfers that reuse that handler.

## Regression evidence

- owner-native.py compiles the production AMD64 adapter, executes its RX code,
  and extracts actual movp/self/listhead/headp/tail C implementations. Independent
  expectations check heap counts, marking, releases, full slot images, operand
  pointers, error identity and floating exception state. There are 144896 cases
  per WORD model, 289792 total, including same-owner aliases, all eight source
  and destination alignments, zero/MAX counts, counts crossing 32 bits, invalid
  list identities and overlapping nil slots. C execution and the fixture run
  with ASan/UBSan; generated machine code itself is not sanitizer-instrumented.
- owner-jit.py executes production AMD64 and ARM64 emitter bytes in Unicorn,
  with Capstone instruction-boundary checks and independent complete memory
  images above 4 GiB. It checks live collector-color changes, extreme color
  values, heap-width boundaries, unaligned slots, nil/list errors, untouched FP
  state, malformed layouts and undersized emission buffers. Guest write hooks
  prove rejected paths perform no stores and same-owner paths do not write the
  reference count. There are 80796 checks per architecture, 161592 total.
- The shared core suite passes 68 host drivers. The publication fixture now
  includes all four owner opcodes in its 72-instruction module, preserving the
  allocation/protection failure, retry and release checks. Actual AMD64 execution
  passes ASan/UBSan in both WORD models. ARM64 publication passes in both models;
  RV64 publication still confirms owner leaves are absent.
- Replaying the parent xec.c with OWNER_SOURCE reproduces zero/MAX reference
  acceptance. OWNER_OPERATION=0/1 selects movp/self; OWNER_REF_INDEX=0/6 selects
  zero/MAX. These selectors isolate each error before ownership publication.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/owner-native.py
PYTHONPATH=../jit-test-deps python3 libinterp/tests/owner-jit.py
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

Physical ARM64 execution and complete VM frame/GC/native-block ABI acceptance
remain separate. These counts describe guarded opcode availability, not full
operand coverage or completed whole-VM ports. Tk, security/crypto and the
MIPS/SPARC/PowerPC JIT sources are outside this checkpoint.
