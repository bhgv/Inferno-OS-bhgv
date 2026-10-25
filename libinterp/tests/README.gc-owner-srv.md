# GC owner and Srv result checkpoint

AMD64 and ARM64 MOVP, SELF, HEADP and TAIL paths now implement the exact
Setmark macro without C calls: after validating a live source and empty or
identical destination, retain when needed, compare the heap color with the
current mutator, and on mismatch store propagator and set the native int nprop
flag to one. Sources already colored propagator still set nprop. Matching
colors and language-nil payloads preserve an existing propagation flag.

All decline guards remain before mutation. Identical owners skip the balanced
retain/release but still mark. List operations publish R.s only on native
success. The adapter supplies both live collector addresses and the propagator
constant. No collector state is frozen at compilation. Both backends retain
the 64-unit private emission limit and use only ABI caller-saved registers.
General owner replacement/destruction continues through C.

This expands operand coverage for four existing opcode IDs. Active opcode
availability remains 125 native paths / 48 interpreter-only for AMD64 and ARM64,
103/70 for RV64+M, 90/83 for RV64+Zmmul and 87/86 for RV64I. The denominator
is 173 active commands, excluding invalid INOP and unused IECLR. WORD32 with
native 64-bit pointers and legacy WORD64 have the same counts.

## Interpreter and library ownership

C consp now rejects zero and maximum source reference counts before calling
cons. An invalid source cannot allocate or publish a list node, mark the old
tail, change type references or wrap its own counter.

Srv_iph2a, Srv_ipa2h and Srv_ipn2p clear output owners with byte copies, publishing
H before releasing the old owner. Resolver wrappers allocate a native vector
before entering their exception frame: vector contents remain defined after
longjmp. Native lookup errors release every populated answer and the vector,
unlock the resolver, and reacquire the VM lock.

A shared list builder prepends answers in reverse order to preserve the native
answer order. Each node is rooted in the actual return slot before allocating
its string. Its pointer map is installed only after that string is stored,
with no intervening fallible operation. On allocation or malformed-answer
errors the builder clears/releases the partial result, frees every native
answer and vector, and rethrows. Failure leaves H in the result slot. It
validates answer counts, missing strings and lengths before narrowing.

## Evidence

- owner-native.py: 385856 cases per WORD model, 771712 total, execute production
  AMD64 RX leaves and actual C handlers against independent ownership/mark/byte
  expectations under ASan/UBSan. The expanded matrix includes all four ordinary
  and propagator colors, three live mutator colors and initial nprop values zero
  and one. Generated machine code itself is not sanitizer-instrumented.
- owner-jit.py: 96948 checks per architecture, 193896 total, execute production
  AMD64 and ARM64 bytes. Full memory images above 4 GiB, guest write hooks,
  native-width count boundaries, extreme color values, FP state, aliases,
  malformed layouts and buffer capacities remain checked.
- srv-results.py: 17600 scenarios per WORD model, 35200 total, pass ASan/UBSan.
  The fixture uses actual cons, newstring and emulator error protection, with
  tracked native resolver and heap adapters. It checks all eight output
  alignments, ordered zero/one/multiple answers, partial native failures, missing
  entries, invalid counts, vector allocation failure, every VM allocation
  failure, type counts, heap guards and lock/error-frame balance. Parent Srv
  replay reproduces unaligned return access and native-answer leaks.
- lists.py with LIST_HEAD_CASE=retain: 1024 cases per WORD model, 2048 total,
  pass ASan/UBSan. Zero/MAX counts, four colors, all alignments and aliased
  source/destination slots must fail without allocation or byte changes.
  Selecting the parent xec.c reproduces invalid-owner acceptance.
- The core suite passes 69 host drivers. Actual AMD64 publication/execution
  passes ASan/UBSan in both WORD models; ARM64 publication passes both models;
  RV64 publication is unchanged. Full Srv source passes a syntax check against
  the generated module declarations, including its exception declarations.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/owner-native.py
PYTHONPATH=../jit-test-deps python3 libinterp/tests/owner-jit.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/srv-results.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' LIST_HEAD_CASE=retain python3 libinterp/tests/lists.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

SRV_SOURCE selects an alternate Srv implementation. SRV_API (0/1/2),
SRV_OFFSET, SRV_COUNT, SRV_FAIL, SRV_NATIVE_ERROR, SRV_MISSING and
SRV_VECTOR_FAIL isolate individual parent failures. LIST_SOURCE selects an
alternate interpreter. Physical ARM64 execution and full VM frame/GC/native-
block ABI acceptance remain separate. Tk, security/crypto and MIPS/SPARC/
PowerPC JIT sources are outside this checkpoint.

## Srv preflight follow-up

All three resolver entry points now reject C-nil and language-nil result slots
before reading an old owner, releasing the VM lock or invoking a resolver. The
shared host-list builder rejects dead, negative and maximum pointer-descriptor
reference counts before allocating a node or incrementing the signed count.
Failure keeps the result H, releases the native answer vector and all populated
answers, and restores the VM/resolver lock and error-frame balance.

`srv-results.py` adds 12 preflight cases per WORD model (24 total) to the existing
35,200-case allocation/unaligned-owner matrix. Both native WORD64 and WORD32 run
the actual production wrappers, cons, newstring and exception protector.
The old implementation fails the nil-slot probe; removing only the new descriptor
guard fails the first invalid-retain probe. These are malformed-state guards,
not a claim that arbitrary invalid pointers are sandboxed.
