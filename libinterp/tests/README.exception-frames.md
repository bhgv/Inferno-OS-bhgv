# Exception frame storage and diagnostic ownership

Both emu and native-kernel handler searches now reject unusable handler records
before selecting a target, formatting diagnostics, unwinding frames or writing
the exception value. The exception slot must be pointer-aligned, outside the
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
code plus real VM initmem/freeptrs. It runs 147 cases for each of emu/os and
native/WORD32 layouts (588 total). These include normal and stack-extension
frames, first/last payload slots, pointer ownership, string and typed exceptions,
optional cleanup, failed diagnostic resize/first allocation, 18 invalid metadata
families, later-handler selection and caller-frame unwind/string conversion.
Frame/data canaries, registers, references and diagnostic pointers are checked.
The host pointer size remains native in both WORD layouts, matching current
pointer-map semantics. Heap final destruction, allocation, stack-extension
release, process lookup and diagnostic formatting are controlled boundaries.
This is not a complete native-kernel boot, scheduler or out-of-memory integration
test, and it does not execute target-machine JIT calls.

`EXCEPTION_VARIANT=emu` or `os` selects one source. `EXCEPTION_SOURCE_DIR` points
to an alternate folder containing its `exception.c`. `EXCEPTION_CASES=invalid`
reproduces the pre-fix invalid slot access under UBSan; `EXCEPTION_CASES=valid`
reproduces loss of the old diagnostic pointer after a failed resize. `unwind`,
`later` and `diagnostic` select the other independent fixture families.
