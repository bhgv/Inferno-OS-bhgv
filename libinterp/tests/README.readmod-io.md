# Module file I/O regressions

Run from the repository root:

```
python3 libinterp/tests/readmod-io.py
```

The fixture compiles the production readmod body with the real Module/Dir types.
ASan and UBSan are enabled by default; CC and CFLAGS can override the compiler.
Native file, allocation, parsing and dynamic-loader services are boundary stubs.

Independent expected bytes cover fragmented successful reads, EOF and errors
at every prefix, allocation/open/stat failure, implausible lengths, cached
identity, dynamic loading and built-in modules. Both interpreter-lock modes
and existing/new module ownership are tested. The parser must receive only a
complete image, and every descriptor, temporary image and stat record must be
released exactly once. READMOD_IO_SOURCE can select an earlier implementation.
These tests do not execute a complete bytecode parser or dynamic linker.

Full-width native reply validation and the paired emu/os library bridge matrix
are documented in [README.native-io.md](../../emu/port/tests/README.native-io.md).
The fixture now retains all predicates under NDEBUG and includes 40 additional
native result cases. READMOD_IO_CASE=wide and READMOD_IO_REPLY isolate those cases.
