# Native file flags and positional I/O

Run `python3 lib9/tests/create-flags.py` and
`python3 lib9/tests/positional-io.py` from the repository root.

Creation tests use real host files to verify exclusive creation preserves existing
contents, new files and directories, and close-on-exec flags. A second build hides
native inheritance flags to exercise fcntl, including get/set failures, descriptor
cleanup and preservation of errno. ASan and UBSan are enabled by default.

Positional I/O tests compile the NT pread body with controlled read/seek boundaries:
short reads, EOF, read errors, all three seek failures, invalid arguments and offsets
above 2 GiB. Both seek platform branches preserve signed 64-bit boundary values.
This fixture defaults to UBSan; CC/CFLAGS can override instrumentation.

The NT seek/read fallback restores position but still requires external
synchronization for concurrent access to a shared descriptor. These are host
regressions, not a Windows build or an Inferno boot. Source overrides are
CREATE_FLAGS_SOURCE, POSITIONAL_PREAD_SOURCE and POSITIONAL_SEEK_SOURCE.
