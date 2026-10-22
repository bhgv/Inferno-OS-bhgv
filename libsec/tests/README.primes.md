# Prime generation and primality regressions

Run from the repository root:

```sh
python3 libsec/tests/primes.py
python3 libsec/tests/dsa-primes.py
python3 libinterp/tests/ipint-exports.py
```

The native fixture links production portable libmp arithmetic, primality testing,
small-prime trial division, prime generation and safe-prime generation. It uses
the tracked allocator from the libmp text fixture and defaults to UBSan with
immediate failure. Candidate and witness byte streams are deterministic and
separate. A process alarm and a generator-call budget bound regression hangs.
The test streams must never be used for production key generation.

The 2,141 native cases cover every candidate from -32 through 2048 against exact
trial division, the known Mersenne primes at exponents 31, 61 and 127, and a
large composite product. Tests require the input to remain unchanged and all
native temporaries to be freed. Prime-generation cases use zero, all-one and
pseudorandom candidate bytes at every width from 2 through 16, including a reused
negative destination. Exact trial division checks the generated value and its
requested bit length. Safe-prime cases verify both primalities, the requested
width, and that the generator has order 2*q. A scripted stream supplies zero,
one and a quadratic-residue candidate before the valid generator 3 modulo 7.

Production changes handle 3 before sampling the otherwise empty witness interval,
classify negative candidates as nonprime, construct fresh unsigned candidates
through mprand, and wrap an overflowing odd candidate to the first odd value of
the requested width. Safe-prime generation rejects zero/one group elements.
Native generation routines reject impossible sizes through their fatal-error
convention (genprime needs at least 2 bits; gensafeprime needs at least 3).
Ordinary random-buffer failure is reported through that convention too, rather
than dereferencing nil or continuing with an old magnitude.

The VM fixture exercises the real builtin frames and lock-release order. Invalid
references, sizes and repetition counts fail before releasing the interpreter;
nonpositive repetition counts keep the default of 18 without narrowing a large
negative VM word. Generated results retain the old value on VM allocation failure.
Safe-prime pairs publish atomically and reclaim both native/VM values after either
heap failure. Active VM layouts and builtin signatures are unchanged.

These are host functional, bounds and ownership checks. They do not establish
entropy quality, constant-time behavior, cryptographic certification, or a full
Inferno build. Native fatal allocator recovery is outside the fixture. The VM
fixture documents its separate sandbox ASan/longjmp limitations in
libinterp/tests/README.ipint-exports.md.

The DSA regression checks the production seed helper with an actual LP64
unsigned long and eight destination alignments, checking all 64 surrounding
bytes each time. Seed generation requests exactly SHA1dlen bytes through
`genrandom`; it no longer stores five potentially 64-bit host words through
an alignment-dependent cast.

Candidate tests link the production DSA routine and libmp arithmetic, with
Python hashlib SHA1 callbacks and scripted primality decisions. An independent
integer oracle reconstructs q and three successive p candidates from the seed,
including carry across seed bytes and rejection of the first two p candidates.
Each candidate starts W at zero instead of retaining the previous remainder.
Both returned-seed and nil-seed modes check output, guards and tracked native
ownership. These 530 checks validate candidate construction, not actual DSA
primality, SHA1's native implementation, entropy or cryptographic suitability.
The script bounds its scripted primality callbacks to avoid regression hangs.
