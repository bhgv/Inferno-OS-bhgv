# Buffered I/O ownership and initialization

Run `python3 libbio/tests/lifecycle.py` from the repository root.

The fixture compiles production `Binits`, `Binit`, `Bopen`, `Bterm`, the exit
registration helpers, `Bgetc`, `Bputc` and `Bflush`. Native descriptor acquisition,
allocation failure, exit registration and byte I/O use deterministic adapters.
The real Biobuf declaration comes from bio.h.

The 96 scenarios cover:

* Negative and undersized capacities, including INT_MIN, and null storage.
  Rejection must preserve the complete buffer and its exit registration.
* Capacities 1 through 32, exact output order through repeated flushes, input
  refills, guard bytes and output-to-input reuse without stale registration.
* Read/write modes with all combinations of the currently accepted flag bits,
  open/create failure, allocation failure and successful Bterm ownership.
  Read opens retain the existing native OREAD argument; no translation of
  Inferno mode flags into host open flags is introduced here.

Initialization validates storage before pointer arithmetic or signed subtraction.
A successful read initialization removes an older output registration. Bopen
closes its descriptor on allocation failure and unwinds both resources if
initialization fails. This does not change descriptor ownership for Binit(s).
Reinitializing still requires the caller to flush pending output first.

The driver defaults to UBSan. ASan can be enabled with:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 libbio/tests/lifecycle.py
```

`BIO_BINIT_SOURCE` selects an alternate production implementation.
`BIO_LIFECYCLE_CASE` selects `storage`, `capacity`, or `ownership` independently.
Each selector fails against the prior binit.c: signed subtraction overflow,
stale exit registration, and an unclosed descriptor respectively.

The tests do not use real host files, process exit, concurrent access or a full
Inferno boot. The failed-initialization branch in Bopen is defensive: its valid
embedded buffer cannot currently trigger Binits failure. LeakSanitizer process
inspection is disabled for the sandbox; ownership is explicitly counted.

Native close failures are also injected for read and write handles. `Bterm`
must return `Beof` while still unregistering, closing and releasing its owned
storage exactly once. The complete lifecycle driver now covers 96 scenarios.
