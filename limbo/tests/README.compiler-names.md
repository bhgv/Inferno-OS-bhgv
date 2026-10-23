# Compiler name construction and allocator failures

Run from the repository root:

```
python3 limbo/tests/compiler-names.py
```

The fixture extracts production exception-name, implementation-union-name,
optimizer-warning, optimizer-list, output-suffix and allocation helpers. It uses
actual compiler declarations from limbo.h. The native allocator can fail below
the real allocmem helper; expected fatal diagnostics execute in child processes
and must exit with the diagnostic sentinel instead of crashing or returning.

Independent string oracles cover optional module/function prefixes, long and
empty names, module delimiters, exact suffix replacement, alias filtering,
reused optimizer cells and ownership of every returned/temporary buffer.
Virtual component lengths test signed-size limits without allocating gigabytes.
Both limbo.y and y.tab.c must provide identical output-name helper bodies.

ASan and UBSan are enabled by default. CC and CFLAGS can override the compiler.
NAMES_TYPECHECK, NAMES_TYPES, NAMES_OPTIM, NAMES_LEX and NAMES_DRIVER can select
other source versions. Earlier versions independently fail the regression.
The tests exercise production helpers; they do not build or execute a complete
Limbo compiler binary.
