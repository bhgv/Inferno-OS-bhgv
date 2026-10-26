# CVTCA byte-array reuse on x64 and ARM64

CVTCA now has a guarded native path on x64 and ARM64. It exports 1 through 256
bytes from a byte-backed String into an existing unique byte Array whose current
logical length is large enough. All byte values are preserved, including NUL
and 0x80..0xff; the path does not reinterpret the bytes as UTF-8. Rune strings,
empty/large inputs, shared/rooted/non-byte or generic old owners retain C.

Every fallible guard precedes payload writes: exact builtin String/Array types,
live owners, source length/capacity, root-H destination, positive descriptor
references, pointer-free one-byte elements, current destination length and full
source/destination address spans. Copying follows memmove order. Only afterward
are the captured WORD-sized length and native owner published. This restores a
result slot overlapping the payload and preserves the complete input context,
reference counters, collector state, FP environment and callee-saved registers.

C now captures source heap and String metadata before reading data. It rejects
wrapped ASCII and complete Rune spans before memcpy, runenlen or encoding.
INT_MIN, capacity and constructor limits are checked before arithmetic or reads.
Empty strings require no source payload pointer. Unique byte Arrays can also
reuse storage for empty, nil and larger byte-backed inputs; their old logical
length bounds reuse, even if a larger physical allocation happens to exist.
Empty exports do not inspect a poisoned destination data pointer.

Generic old owners remain retained through allocation and encoding. Only exact
Arrays are interpreted as Array records. Before releasing a unique old Array,
C validates backing-owner lifetime and borrowed descriptor geometry/reference
count, including Rune/nil-input fallback paths. Invalid old owners are rejected
before allocation or result publication. Valid non-byte Arrays retain their
ordinary allocating/destruction behavior.

Both byte-conversion emitters now share one private layout validator, including
disjoint Array fields and descriptor ref/size/map offsets. This preserves the
previous CVTAC contract and reduces duplicated validation. LeafLayout, builtin
identities, the 128-unit private staging bound, Dis and public runtime ABIs are
unchanged. RV64 and the excluded older backends are unchanged. Checked spans
detect address wrap; they do not certify arbitrary pointers as mapped.

Validation from the repository root:

    PYTHONOPTIMIZE=1 python3 libinterp/tests/string-bytes-jit.py
    python3 libinterp/tests/string-bytes-layouts.py
    python3 libinterp/tests/string-bytes-native.py
    python3 libinterp/tests/string-bytes-spans.py
    STRING_BYTES_NO_ERRORS=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-bytes-native.py
    PYTHONOPTIMIZE=1 python3 libinterp/tests/byte-string-jit.py
    python3 libinterp/tests/byte-string-layouts.py
    python3 libinterp/tests/slice-tokenize-suite.py --core
    python3 libinterp/tests/jit-coverage.py

- Native instruction oracles: 23,792 ARM64 and 23,810 x64 cases, both WORD
  models, 41 metadata/overlap profiles, octets, exact type identities, descriptor
  death, zero/negative/large lengths, all input byte alignments, complete images,
  exact allowed write ranges, FP state, saved registers and every short buffer.
  The ordinary paths occupy 88 ARM64 and 97 x64 four-byte units.
- ASan/UBSan layouts: 1,544,620 cases per backend over 22 independent fields,
  offsets -8 through 2056 and capacities through 128. CVTAC's separate
  1,474,410-case matrix per backend also passes after the common validation change.
- Real C/x64 RX: 77,568 owning, reuse, rooted/shared/generic owner, Rune,
  overlap, source/result slots inside the payload, injected failure and FP cases
  per WORD model. There are 1,856 additional error/publication cases and six
  empty poisoned-data cases per model. Full runs use UBSan and NDEBUG.
- Successful real C/RX paths additionally pass ASan/UBSan: 28,544 cases plus
  six empty cases per model. STRING_BYTES_NO_ERRORS omits longjmp/error injection
  in environments where ASan cannot obtain process stack maps; full UBSan/error
  coverage remains active in the default run.
- Reduced source-address arithmetic: 26,880 cases per WORD model with a 16-bit
  guard domain and unchanged native payload pointers. They cover exact header
  and payload boundaries, empty/ASCII/Rune inputs, invalid metadata, unaligned
  operand slots, complete source/REG images and allocation-free errors. All
  actual Rune reads remain aligned. STRING_SOURCE selects an older source;
  STRING_BYTES_SPAN_HIGH_ONLY isolates upper-address payload checks. The parent
  implementation fails a 128-Rune source whose model span wraps at offset ff00.
- The existing conversion source and UTF round-trip matrices retain allocation
  failure coverage using an explicitly shared old Array. Both runtime adapters
  pass RX publication, fallback, metadata/failure/retry and unmap checks.

Active native availability is now 144/173 on x64 and ARM64; 29 commands remain
interpreter-only. These are guarded partial paths. RV64 stays at 103/70 for M,
90/83 for Zmmul and 87/86 for I. ARM64 execution is emulated; physical target
acceptance remains separate.
