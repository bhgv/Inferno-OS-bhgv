# Bounded, nonrecursive AST diagnostics

Run `python3 limbo/tests/diagnostic-print.py` from the repository root.
`DIAGNOSTIC_CASE` selects shallow, quote, deep or dump. `DIAGNOSTIC_NODES`
selects an alternate nodes.c for isolated before/after execution.

## Changes

Expression printing previously descended recursively through AST edges. A chain
of invisible Oused/Oadr wrappers or left-nested calls could exhaust the native
stack before producing its first output byte. The debug printer also kept
visiting descendants after filling its bounded buffer, so even a short diagnostic
could overflow the native stack on a deep input tree.

Both forms now use a checked private action stack. A small inline stack serves
ordinary expressions; geometric heap growth retains deferred siblings, suffixes,
list separators, parentheses and Type conversions in the original order. Growth
checks byte-size representability before allocation and releases all private
storage after successful or truncated printing. The AST is borrowed, not mutated.
Printing stops when the output buffer is full. Empty spans and nil roots are
no-ops; nonempty output for a nonnil root is NUL-terminated with a live NUL cursor.
The old secpy helper can return the exclusive endpoint on truncation, so the new
driver normalizes that cursor before returning it to its caller.

The quoted `%V` conversion separately reserves its closing quote and NUL. In the
previous code, a long parenthesized identifier could fill the expression span;
secpy returned its exclusive endpoint, and expconv wrote a quote there followed
by a NUL one byte beyond its local array. A final cursor guard prevents this even
if a future expression helper returns the historical exclusive endpoint.

Debug indentation is bounded by available output and saturates before an int
increment can overflow. Type conversion itself retains its existing compiler
implementation; this change removes AST-edge recursion, not every possible
recursion in compiler diagnostics. Cyclic ASTs are not valid compiler input and
are not detected by this printer.

## Evidence

The driver links the actual lib9 formatter engine and extracts production AST
printers and diagnostic string helpers. Only Type/opcode conversion fixtures use
stable test spelling; this isolates AST ordering from unrelated compiler globals.
A test-only recursive reference preserves the old shallow rendering rules.

| Family | Scenarios per profile |
| --- | ---: |
| 500 shallow trees, seven capacities, expression/debug output and list stops | 7,028 |
| Quoted parenthesized identifiers, lengths 4,088 through 8,199 | 4,112 |
| 100,000-node expression chains and empty-span checks | 5 |
| 100,000-node debug chains and maximum indentation | 6 |
| Total | 11,151 |

Shallow checks compare exact output bytes, output canaries, live cursor positions,
private allocation accounting and unchanged AST storage. Trees include casts,
array/channel constructors, calls, tuples, slices, labels, constants, prefix and
postfix operators, explicit parentheses and function-pointer list stop rules.
Deep checks include silent wrappers, left-nested calls with deferred suffixes,
binary trees and ignored descendants. They run with a 512 KiB native stack and
require all action allocations to be released.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass under NDEBUG with sibling-call
optimization disabled: 33,453 scenarios. The optimized profile also treats
pointer/int conversion diagnostics as errors. LSan process inspection is disabled;
address instrumentation and explicit private-owner accounting remain active.

Against parent 467f52d, the quote probe reports ASan stack-buffer-overflow; the
expression and debug probes independently report ASan stack-overflow. These are
executed runtime failures, not compile/link failures. Native bootstrap builds
ELF64 host tools, regenerates the real grammar/decoder and compiles all 17 Limbo
units. The driver is registered in the core interpreter/compiler suite.

No JIT encoder source changes are included. Readiness remains 173/173 complete
JIT+C entries and 166/173 guarded native fast paths on AMD64 and ARM64 in both WORD
models. This does not establish a coordinated 64-bit VM ABI or a full VM boot.
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.
