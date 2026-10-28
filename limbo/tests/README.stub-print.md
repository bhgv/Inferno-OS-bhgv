# Bounded C stub spelling and layout arithmetic

Run `python3 limbo/tests/stub-print.py`. The driver extracts production `ctprint`,
`dotprint`, `stubalign`, `addsize`, C kind spellings and bounded string helpers.
It links the real lib9 formatter, including the original recursive `%R` callback
path in a frozen shallow reference.

`ctprint` uses explicit continuations for references and anonymous aggregate
fields, and tail iteration for ADT picks. Each aggregate continuation retains its
parent, current field and aligned offset. Field order, explicit padding and layout
validation remain intact even when the output span has filled. Reference/field
printing shares the caller's destination rather than truncating intermediate
Types in a fresh 1024-byte `%R` conversion buffer. Large direct destinations can
therefore receive complete C Types.

`dotprint` gathers borrowed declarations and emits components from their root.
Separators now retain a terminating NUL, including at truncation boundaries and
before a final nameless component. Both printers free their private, checked,
geometrically grown stacks without modifying borrowed compiler data.

`stubalign` validates nonnegative Dis-representable offsets, positive power-of-two
alignments and the final aligned offset before emitting padding. It uses the same
checked `addsize` policy as compiler Type/frame construction. Anonymous C field
accumulation also uses `addsize` before continuing the enclosing layout.

Per target pointer-width model, per build profile:

| Family | Scenarios |
| --- | ---: |
| 1,000 mixed shallow trees, capacities 0..64 and 1024 | 66,000 |
| All 13 literal C spellings and nil, same capacities | 924 |
| 100,000 qualified declarations and nameless final components | 49 |
| 100,000 nested references, picks and anonymous tuples, short/large output | 6 |
| Buffered/streamed alignment, ordinary and boundary/rejected inputs | 280 |
| Invalid field/aggregate layouts with capacities 0..3 | 28 |
| Total | 67,287 |

Both pointer-width models (4 and 8) and three profiles yield **403,722 scenarios**.
Profiles are `-O1`/`-O0` ASan+UBSan and `-O2` UBSan, all with `-DNDEBUG`; optimized
pointer/integer cast diagnostics are errors. Deep families run with a 512 KiB C
stack. They verify canaries, cursors, NUL termination, independent full-output
lengths/content, source preservation and private allocation accounting. Shallow
comparisons derive bounded prefixes from complete reference spellings: the old
truncated `dotprint` output itself may lack a terminator.

Each profile additionally emits **2,000 complete C declarations**, 1,000 per model,
which pass strict C99 syntax checks. The native eight-byte-pointer model also
checks all 1,000 generated object sizes against the compiler layout. Four-byte
pointer output is syntax-checked on this host; it is not a native 32-bit ABI test.
The core suite includes this driver.

`STUB_PRINT_SOURCE=/path/to/stubs.c` selects another source revision;
`STUB_PRINT_CASE=refs|picks|tuples|dots|terminator|alignment` isolates regressions.
On parent revision `7970792`, the first four independently reproduce ASan stack
overflows; the last two fail explicit termination/alignment predicates. Build
failures do not count as runtime reproduction. `layouts` additionally verifies
that clipping output does not suppress compiler layout diagnostics.

Adjacent validation runs the existing native-stub generator/table C99 suite,
Type diagnostics, native tool bootstrap and all 17 Limbo compilation units.
These fixes do not change instruction encodings or JIT coverage, and do not
constitute acceptance of a fully bootable 64-bit Inferno VM.
