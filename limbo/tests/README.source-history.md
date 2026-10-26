# Source history capacity and search arithmetic

The compiler now grows its source-segment pointer table geometrically rather than
copying it after every 32 entries. Capacity is bounded by both the signed int
index domain and the allocator's ulong byte-size domain. A failed allocation does
not publish a new capacity. Exhaustion reports `too many source files` before
incrementing an exhausted index or calculating an overflowing allocation size.

Both source-coordinate lookup and case-label lookup calculate their midpoint as
`l + (r-l)/2`, avoiding signed overflow for large valid tables. Bytecode and VM
layouts are unchanged.

Run `python3 limbo/tests/source-history.py`. The production-function harness uses
UBSan by default and checks 100,000 appends, 299,999 reverse source lookups,
100,000 exact case-label lookups, allocation failure and INT_MAX exhaustion.
The harness runs with native and 32-bit ulong allocator counts while retaining
native pointers, including saturated allocation-size checks. It is not a full
32-bit compiler execution. Growth must take at most 14 reallocations. Substituting the parent lex.c through
SOURCE_HISTORY_SOURCE fails this growth oracle. The test is in the core suite.

The tests execute extracted production functions with bounded host adapters;
they do not allocate an INT_MAX-entry table or prove a complete compiler build.
An attempted ASan run encountered unsupported LeakSanitizer process inspection
in this environment and is not counted as passing.
