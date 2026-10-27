# Exception frame storage and diagnostic ownership

Latest native JIT-to-C RAISE preflight and emu/os exception-owner transfer: [README.exception-publication.md](README.exception-publication.md).

Both emu and native-kernel handler searches now reject unusable handler records
before selecting a target, formatting diagnostics, unwinding frames or writing
the exception value. The exception slot offset must be a multiple of the native pointer width, outside the
Frame header, fully inside the frame and marked as a pointer in its descriptor.
Optional cleanup descriptors must fit the frame and their pointer maps must not
touch the header or extend beyond the cleanup span. Invalid candidates are
skipped so a later usable candidate can still catch the exception. This does
not validate arbitrary corrupted stack links or arbitrary Type allocations.

A failed diagnostic `realloc` keeps the previous allocation and text instead of
losing its pointer. Failure of the first allocation still permits exception
delivery without a diagnostic string. The root-frame walk stops before trying
to reconstruct a nonexistent caller PC from its nil return address.

Run from the repository root:

```sh
python3 libinterp/tests/exception-frames.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/exception-frames.py
```

The fixture extracts real emu/os handler, matcher, frame check and frame cleanup
code plus real VM initmem/freeptrs. It runs 1280 cases for each of emu/os and
native/WORD32 layouts (5120 total), varying all eight ordinary-frame alignments. These include normal and stack-extension
frames, first/last payload slots, pointer ownership, string and typed exceptions,
optional cleanup, failed diagnostic resize/first allocation, 18 invalid metadata
families, later-handler selection, caller-frame unwind/string conversion and
cleanup of an ordinary uncalled frame, and actual extension deallocation
during unwind. Extension roots keep their natural
native alignment.
Frame/data canaries, registers, references and diagnostic pointers are checked.
The host pointer size remains native in both WORD layouts, matching current
pointer-map semantics. Heap final destruction, allocation, process lookup and diagnostic formatting are controlled boundaries.
This is not a complete native-kernel boot, scheduler or out-of-memory integration
test, and it does not execute target-machine JIT calls.

`EXCEPTION_VARIANT=emu` or `os` selects one source. `EXCEPTION_SOURCE_DIR` points
to an alternate folder containing its `exception.c`. `EXCEPTION_CASES=invalid`
reproduces the pre-fix invalid slot access under UBSan; `EXCEPTION_CASES=valid`
reproduces loss of the old diagnostic pointer after a failed resize. `unwind`,
`later` and `diagnostic` select the other independent fixture families.

## Frame alignment and publication checkpoint

The shared C handler implementations now capture frame headers through byte
copies before examining links or releasing a frame. Frame-type lookup and
uncalled-frame cleanup also avoid C structure alignment assumptions. Exception
result slots are accessed as byte addresses: capture the old owner, publish
language nil, release the old owner, then acquire/publish the new value.
Typed-exception flattening captures its String owner with a byte copy too.
No public frame layout, pointer-map stride, handler offset or exception-matching
rule changes.

The fixture's own frame and owner accesses use byte copies, so sanitizer
failures identify production accesses rather than test setup. Tests preserve
header bytes, data guards, native owner pointers, references, process registers
and prior diagnostic storage. The old owner destructor observes nil in its
former result slot before release when no cleanup descriptor is requested.
Selecting the parent source with `EXCEPTION_CASES=valid` reproduces the old
publication order; `EXCEPTION_CASES=unwind` reproduces unaligned Frame reads.
`EXCEPTION_CASES=extension` exercises actual unextend/free with an unaligned
caller. `EXCEPTION_CASES=uncalled` checks cleanup of a not-yet-called ordinary frame.

## Base RV64I fixed multiplication

`IMULX` now uses the existing register-only multiply loop when M/Zmmul is
absent. The helper accepts an explicit multiplier register, preserving the
frame scale in t4 and the captured source in t6. Crucially, multiplication uses
a full 64-bit signed intermediate even with WORD32 operands. Narrowing a
negative multiplier to WORD32 here would give wrong arithmetic right shifts.
Scale bounds, nil/alignment frame guards, aliased operands, signed right shifts
and exact destination widths retain the C contract. Hardware multiplication
paths stay unchanged. DIVX still needs division; complex VM operations retain
their C implementations.

```
LEAF64_CASE=framefixed python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=riscv64 RV_FEATURE_PROFILE=i LEAF64_CASE=framefixed python3 libinterp/tests/leaf64-jit.py
LEAF_RUNTIME_BACKEND=riscv64 RV_FEATURE_PROFILE=i LEAF_WORD_BITS=32 python3 libinterp/tests/leaf64-runtime.py
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

The shared frame arithmetic matrix has 81346 checks; the separate base-I
matrix has 13742. Base-I disassembly excludes MUL/DIV/REM. The latter omits
unsupported DIVX execution while retaining all MULX and frame rejection cases.
Coverage is now 103 native paths / 70 interpreter paths for AMD64, ARM64 and
RV64+M; 90 / 83 for RV64+Zmmul; 87 / 86 for RV64I, out of 173 active commands.
Some paths are guarded/partial. These counts do not require replacing complex
C helpers and do not represent completion of native block execution or the
64-bit pointer ABI. Physical ARM64/RISC-V acceptance remains separate.


## Selected destination checks

Both emu and os validate explicit and default handler PCs before diagnostics,
stack cleanup or publication of exception owners. Interpreted destinations must
name a complete instruction within `nprog`; multiplication, address addition,
`NOPC`, nil/H program bases and the H destination are rejected. Compiled handlers
retain byte offsets and reject address wrap and H without pretending that the
module stores the extent of its generated allocation. Invalid targets do not
hide subsequent valid handlers.

`exception-frames.py` now runs 526,000 cases per runtime/scalar profile, or
2,104,000 for emu/os and WORD32/WORD64 together. This includes 640 new handler
selection cases and 2,097,152 near-limit interpreted/compiled target checks,
besides the 6,208 existing frame and ownership cases. Run with:

```sh
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/exception-frames.py
```

The preceding handler reproduces a selection failure in the new `targets`
family with an active explicit predicate (SIGABRT), rather than a compilation
failure. The pure target helper is supplied for fixture linkage in that negative
control; the preceding handler itself remains unchanged. Boundary code tokens
are synthetic and never dereferenced. These checks do not constitute a native
ARM64 host run or solve the outstanding compiler/reference/frame ABI mismatch.
Opcode coverage remains 173/173 complete entries including C and 166/173 guarded
native paths for each priority architecture.
