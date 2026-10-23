# Owned native directory metadata

Run from the repository root:

```
python3 lib9/tests/dirstat-owned.py
```

The fixture compiles both production POSIX and NT stat conversion bodies.
Native stat and account/group lookups return controlled records. NT file-type
constants are mapped to their host equivalents for this test; this is not a
Windows build. ASan and UBSan are enabled by default; CC and CFLAGS can override
the compiler.

The path argument is freed immediately after dirstat returns, and account/group
lookup storage is overwritten. The Dir must retain the expected basename and
copied identity strings in one allocation. Tests include forward-slash relative,
absolute, root and trailing-slash paths, UTF-8, long paths, missing user/group
records, descriptor stats, failed allocation and failed stat calls. Directory
mode bits are compared with an unsigned oracle.

DIRSTAT_POSIX_SOURCE and DIRSTAT_NT_SOURCE independently select older sources;
DIRSTAT_OWNED_SOURCE overrides both. These checks isolate native conversion,
not filesystem walking or account-service behavior.
