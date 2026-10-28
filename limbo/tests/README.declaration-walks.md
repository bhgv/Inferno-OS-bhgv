# Declaration traversal, full-width group metadata and tuple blocks

Run `python3 limbo/tests/declaration-walks.py`. It extracts the actual declaration
walkers, tuple-block predicate, declaration constructors, scope installation,
redeclaration logic, frame offsets and checked size/alignment helpers. Persistent
Decl allocations are owned independently of temporary traversal stacks. Compiler
warning/error callbacks record their ordered source identities; formatting itself
is outside this fixture.

Production changes:

- Tuple declaration and marking use an explicit continuation stack. Names are
  installed eagerly in the same left-to-right order, including names following an
  invalid tuple member. A nil name still disables local block grouping.
- Nested global `:=` expressions preserve right-before-left declaration order;
  other expressions preserve left-before-right traversal. AST links are borrowed.
- Tuple block recognition preserves short-circuit behavior, partial `last` output
  on failure and contiguous Decl requirements, without recursive C frames.
- `Decl.nid` is an `int` instead of an `uchar`. A group of 256 local names can no
  longer turn its head marker into zero, nor can a 257-name group appear to be an
  independent single local. Producers and consumers already operate on int values.
  This is compiler-internal metadata; Dis encodings and runtime VM structs do not
  change. Count growth rejects INT_MAX explicitly before signed addition.

Per WORD model and profile:

| Family | Scenarios |
| --- | ---: |
| 1,000 reproducible mixed trees × four operations × global/local scopes | 8,000 |
| Independent invalid-member, nil, terminal-name and empty-tuple checks | 4 |
| Deep tuple declaration, marking, global assignment and block recognition | 4 |
| Groups of 2, 255, 256, 257, 511, 512, 513 and 1,000 names | 8 |
| Exhausted int declaration counter | 1 |
| Total | 8,017 |

Both WORD32/WORD64 configurations and three profiles yield **48,102 scenarios**.
Profiles are `-O1`/`-O0` ASan+UBSan and `-O2` UBSan, all with `-DNDEBUG`; optimized
pointer/integer casts are errors. The 100,000-level deep inputs run with a 512 KiB
C stack and exercise private stack growth with deferred siblings. Block rejection
also checks private cleanup after deep traversal. Boundary groups run the actual
`idoffsets` with mixed 4/8-byte alignment and verify the group starts at its largest
member's alignment. Native pointers remain native in both test configurations.

The frozen recursive oracle shares production constructors and scope installation
with the new walkers. Shallow comparisons cover action-order fingerprints, result
Decl metadata, old/next links, scope heads/tails, symbol bindings, borrowed AST
links and the final tuple-block cursor. Deep tests independently check group heads,
continuation marks, declaration order and ownership. The core suite includes this
driver. `Decl` storage is released by the fixture after each attempt, not by the
production traversal.

Environment overrides:

- `DECL_WALK_SOURCE`: another `limbo/decls.c`.
- `DECL_WALK_ECOM`: another `limbo/ecom.c`.
- `DECL_WALK_HEADER`: another `limbo/limbo.h`, needed to reproduce byte-sized nid.
- `DECL_WALK_CASE`: `shallow`, `edges`, `declare`, `mark`, `global`, `block`,
  `groups` or `overflow`.

On parent revision `6649950`, four independent deep probes reproduce ASan stack
overflows. The old header fails the group metadata/frame regression, and the old
counter increment triggers UBSan signed overflow. These are runtime failures,
not failed compilations.

Adjacent checks rebuild native tools and all 17 Limbo compilation units, run Type
parent/generalization and map/zero walkers, and validate existing native-stub table
output as strict C99. JIT readiness is unchanged; this is not full VM boot or
physical ARM64 acceptance.
