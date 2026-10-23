# Module import indices and self-loading

Run `python3 libinterp/tests/module-imports.py` from the repository root.
The test extracts production `moduleimports` and `iload` bodies from `xec.c`
and runs them with 32-bit and 64-bit VM words under UBSan.

It rejects missing module owners, absent import tables, negative indices,
the table sentinel, INT_MAX, and a 2^32 index. Invalid imports must leave the
old destination untouched and perform no string conversion or module read.
Valid `$self` and external loads exercise failed links, MP reference counts,
and destination aliasing with the current module register. A data-free
module must keep VM nil MP without computing a heap header from it; native
NULL MP must be rejected before acquiring references.

Filesystem, string conversion and linker ownership are explicit boundaries
in this fixture. `module-links.py` separately runs production linking and
checks nil import rejection consumes the transferred module reference once.
No native JIT instructions execute here; existing native JIT load paths punt
to this interpreter operation. No Module or Modlink layout changes are needed.

`XEC_SOURCE` selects an earlier production file for failure reproduction.
`CC` and `CFLAGS` override the compiler and default sanitizer flags.
