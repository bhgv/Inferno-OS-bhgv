# Portable memory and byte-search regressions

Run `python3 libkern/tests/portable-memory.py` from the repository root.

The driver compiles complete production portable `memmove`, its `memcpy`
wrapper, `memset`, `strncpy`, `strchr` and `strrchr` bodies. Symbols are renamed
so host libc remains independent. Both generic and SPIM C strchr versions run
with signed and unsigned char: 1,082,642 scenarios in each configuration.

The small cases exhaust overlapping/disjoint copy offsets, fill values and
lengths, string truncation/padding, return pointers and guard bytes. Search
checks use all leading byte values and requested characters -1024 through 1024,
plus integer extremes, matching the low byte and stopping at the first NUL.
Zero-length spans and nonpositive signed strncpy counts access no storage.

Portable memory counts no longer narrow into int. The move direction uses
uintptr addresses, avoids pointer ordering across unrelated objects, and skips
zero-length and self copies. strncpy's index now matches its long argument.
Character searches narrow the requested character once and compare unsigned
bytes. strrchr scans once, including the terminator, instead of repeatedly
calling a target-dependent strchr with an unconverted integer.

Run real large-span operations on a Linux LP64 host:

```sh
PORTABLE_WIDE=1 python3 libkern/tests/portable-memory.py
```

This runs six operations of 2,147,483,681 bytes each: backward and forward moves,
memcpy, memset, strncpy copying and strncpy padding. The entire virtual spans
are valid mappings. A pair of 2 MiB shared-file chunks is mapped repeatedly
inside a private address reservation, keeping backing memory small. Each check
inspects a full destination backing chunk, so skipping the operation or copying
only a narrowed prefix fails. The fixture owns the complete reservation before
using MAP_FIXED. This is a host width test, not target execution or a throughput
benchmark; aliased pages do not model arbitrary large independent payloads.

Default compilation uses UBSan. Small cases also pass ASan:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 libkern/tests/portable-memory.py
```

`PORTABLE_CASE` selects `moves`, `fills`, `searches`, `strings`, or
`wide [move-backward|move-forward|copy|fill|string-copy|string-pad]`.
`PORTABLE_MEMMOVE_SOURCE`, `PORTABLE_MEMSET_SOURCE`, `PORTABLE_STRNCPY_SOURCE`,
`PORTABLE_STRCHR_SOURCE`, `PORTABLE_STRCHR_SPIM_SOURCE` and
`PORTABLE_STRRCHR_SOURCE` select alternate production files. Old move/fill
bodies fail real large-span content checks; old strncpy triggers signed int
increment overflow in both large copying and padding. Both old strchr C bodies
fail search checks. The old strrchr combined with a byte-narrowing strchr runs
past the terminator for a nonzero integer whose low byte is zero.

These portable C routines are not a replacement for the existing target
assembly implementations. The tests execute C on the host; no assembly
behavior, complete OS boot, native JIT ABI or new 64-bit backend is claimed.
