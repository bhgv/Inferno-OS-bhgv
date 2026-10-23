# Constant string semantics

Run from the repository root:

```sh
python3 limbo/tests/fold-strings.py
python3 limbo/tests/fold-string-length.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 limbo/tests/fold-strings.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 limbo/tests/fold-string-length.py
```

String casts follow the language's string conversion rules rather than its
numeric literal grammar. Leading ASCII whitespace and an optional sign are
accepted; conversion stops at the first nondecimal character and returns zero
when no decimal digits follow. The strict source-literal lexer is unchanged.
Big conversion uses the same lib9 strtoll as the interpreter, including its
existing 64-bit modular accumulation. Int and byte conversions fold only when
the parsed prefix fits signed 32 bits. Larger prefixes remain unchanged runtime
ASTs because strtol overflow depends on the target's native long width. Such
expressions can be rejected in con contexts that require a compile-time value;
the compiler no longer guesses a value using its own host width or literal parser.

fold-strings.py runs 14,819 cases in each WORD32/WORD64 model. Python arbitrary-
precision decimal parsing supplies independent expected values. The fixture
executes production cast folding, actual lexer parsing for old-source regression
substitution, lib9 integer parsing, and VM cvtcw/cvtcl bodies. It checks source
AST preservation on deferred folds and destination/source-slot aliasing.
The native strtol width is modeled with host-libc syntax and signed saturation;
VM string storage is ASCII with controlled UTF conversion, not a complete heap.
Inputs include suffixes, missing digits, all ASCII whitespace, embedded NULs,
integer boundaries and 2,048 deterministic prefixes up to 199 decimal digits.

Constant len counts Unicode characters across the complete interned byte extent,
including leading, interior and trailing NULs. fold-string-length.py runs 4,128
cases per word model. Python character counts are compared with production
recursive folding, actual constant concatenation, lib9 UTF decoding, interpreter
c2string and lenc. Literals, named constants and concatenations include one- to
four-byte Unicode characters, combining marks and NULs. Valid UTF is generated
at character boundaries; malformed UTF handling has separate codec tests.

Both drivers accept NODES_SOURCE for old-source substitution. The previous
implementation fails both semantic regressions. The cast driver also accepts
FOLD_STRING_CASE=prefix or range to isolate decimal syntax or target-width
deferral, respectively. Minimal AST/allocator adapters reuse the cast/identity
fixtures; the decoder's allocation-limit checks and real
heap layout are outside these length tests. Existing codec tests cover real VM
declarations separately. UBSan is enabled by default; supplementary ASan leaves
stack instrumentation enabled and disables only LeakSanitizer process inspection.
These are host fixtures, not full compiler/VM runs or native target ABI tests.
No VM layouts, instruction encodings or JIT backends change.
