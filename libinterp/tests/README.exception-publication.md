# RAISE preflight and exception-owner transfer

RAISE now bounds its decoded source cell before memmove, rejects backward Heap
origin underflow and unaligned native Heap owners, and checks the minimum tuple
payload span before inspecting metadata. It validates the complete read-only
Type.size prefix and copies that field without alignment assumptions. Direct
String exceptions and tuple message owners require a complete String prefix
before string2c can read lengths or modify a terminator/cache.

Existing behavior is retained for nil/H values, nil/H tuple messages, dead or
saturated exception owners, wrong message Types, allocation failure and the last
legal retain from unsigned MAX-1 to MAX. Preparation errors preserve REG, the
previous process exception value, source/message references and existing cache
bytes. These are numeric span and canonical Heap-alignment checks; allocation
membership and arbitrary fabricated pointers are outside their contract. The
packed-Type tests cover the immutable size-field read, without selecting a new
descriptor allocation or GC ABI.

Both emu and os handlers now move the process-owned exception reference into
the already cleared handler slot. They publish the captured value before clearing process.exval, preserving a
root throughout the transfer with no intervening fallible callback.
The old extra retain/release cycle is removed. This preserves owner counts
and permits an already saturated live exception reference without temporarily
wrapping it to zero while the handler value is published. Aliasing between the
previous handler-slot owner and the exception remains balanced.

## Validation

Run from the repository root:

```sh
python3 libinterp/tests/raise-native.py
python3 libinterp/tests/raise-slots.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/exception-frames.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-dispatch-jit.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The new native driver uses the real compiler, executable complete RAISE entry,
cjitexec, RAISE and string2c. The opcode intentionally has no native fast entry;
the generated complete entry tail-calls the ordinary C executor. Message cache
allocations are injected independently of compiler-publication allocations.
Both direct C and real JIT-to-C error routes run with ASan/UBSan and NDEBUG.
Separate processes isolate error-route topology while retaining instrumentation.

| Native RAISE matrix | WORD32 C | WORD32 RX | WORD64 C | WORD64 RX |
| --- | ---: | ---: | ---: | ---: |
| Lifetime/counter/cache/publication baseline | 4,096 | 4,096 | 4,096 | 4,096 |
| Additional span and packed-Type cases | 1,988 | 1,988 | 1,988 | 1,988 |

The total is 24,336 C/RX routes. The malformed-input matrix checks source-cell
nil/H/wrap, low/unaligned/high exception owners, Type-prefix wrap, packed
insufficient descriptors, and low/unaligned/wrapping message owners. All source
byte shifts are exercised. Error cases retain either H or a previously owned
String in process.exval and verify its reference count and complete cache/data
image. Valid packed Type prefixes reach actual message preparation and exception
publication. The existing baseline additionally covers direct/tuple ASCII/Rune
messages, dead/saturated counters, poisoned metadata and allocation failure.

The emu/os receiver fixture passes 1,552 cases per runtime/WORD profile, 6,208
total. Of these, 896 newly added transfer cases vary live reference counts
1, 2, MAX-1 and MAX; String/tuple values; cleanup maps; destination-owner aliases;
and all frame byte alignments. Another 32 cases preserve cleanup of a separately pending exception left by
message construction. Existing handler selection, diagnostic allocation
failure, normal/extended frame unwind and invalid metadata checks also pass.
Inherited fixture predicates now use explicit checks so NDEBUG cannot remove
handler calls or ownership verification. Its real unextend extraction now
includes stacksize/stackextent dependencies required by the current stack code.

Forty isolated parent-40d45b9 probes reproduce the problems: nine RAISE preflight
families under each WORD model through direct C and real RX (36 probes), plus
saturated handler transfer independently in emu/os and both WORD models (four).
RAISE_SLOT_SOURCE, RAISE_NATIVE_WORD_BITS, RAISE_NATIVE_ROUTE=0/1,
RAISE_NATIVE_CASE=spans and RAISE_SPAN_KIND=0..8 select RAISE probes.
EXCEPTION_SOURCE_DIR, EXCEPTION_VARIANT=emu/os, EXCEPTION_MODEL=native/word32 and
EXCEPTION_CASES=transfer select receiver probes. Parent failures are runtime
sanitizer errors or active ownership-check failures, not missing dependencies.

Complete AMD64 dispatch also passes 177,152 routes, 5,002 compiler and 295 VM
integration checks per WORD model. ARM64 compiler preflight passes 5,002 per
model. Both guest complete-tail matrices pass 16,881 cases. Native RAISE execution
here was AMD64; physical ARM64 execution remains outstanding. The new driver
selects ARM64 automatically when run on that host.

Incoming commit 51d44da was integrated before publication. Its production branch
helper dependencies in leaf64-runtime.py are preserved, and that standalone
native diagnostic passes both default WORD64 and LEAF_WORD_BITS=32 profiles.
Its incoming changes do not alter the parent RAISE/handler bodies used by the
40 regression probes.

## Port status

RAISE remains a complex C operation reached from JIT. Both priority architectures
retain 173/173 complete opcode entries including C and 166/173 guarded native
fast paths. These counts describe dispatch availability. The full native
compiler/loader/frame/GC ABI and VM/Limbo-program boot remain unfinished; see
[README.pc-target-boundaries.md](README.pc-target-boundaries.md). RV64 work, Tk,
security/crypto and MIPS/SPARC/PowerPC JIT sources remain outside this series.
