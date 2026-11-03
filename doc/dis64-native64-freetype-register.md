# Explicit native64 Freetype registration

The optional `DIS_NATIVE64_FREETYPE_HEADER` selects fresh int32/ref64 declarations.
`DIS_NATIVE64_FREETYPE_REGISTER=1` additionally registers all six existing exports
through builtinmod64. Other values, registration without a header and non-WORD32
consumers are compile-time errors. Header-only mode refuses before allocation;
packed geometry refuses before registration. No installed legacy table is used
as a fallback. Ordinary legacy builds retain their existing declarations.

Generate the header with the actual compiler and module dependencies:

```sh
limbo -H -M Freetype -I module module/freetype-native64.m > freetype-native64.h
```

The selector emits Freetype's declarations and dependent Draw_Point, without
Sys/Draw callable records. Generated qualified method symbols are mapped to
the existing owning bodies inside the explicit consumer. Matrix/Vector/Face/
Glyph maps are still derived from native public fields and exclude private FT
handles. Four Type creator owners remain staged until complete registration;
all constructor failures retire them and preserve diagnostics. Complete init
is idempotent; partial states refuse unchanged under initialization exclusion.

All six frames have a result-address field at32 and arguments from64. Four
frames have80 bytes; setcharsize/settransform have88. Full two-byte argument
maps are00:80 except settransform00:e0. Existing module-relative names and Limbo
hashes are preserved. These hashes are trusted consistency checks, not new
structural Dis64 signatures. newmemface retains its explicit not-implemented
result; registration does not implement that API.

## Classic Linux startup

Add `--native64-freetype-boot` to a Linux64 console `--native64-scalar-boot`
configuration, optionally alongside the existing Math/Sys/caller-guard flags.
Classic mk builds the selected header with host Limbo, compiles the actual font
archive and links it after libinterp and before lib9. No graphics device/Tk
closure is added. `disinit` calls the coherent initializer before scheduling the
initial scalar program. Changing model flags requires a clean coherent rebuild.
Android's scalar APK remains builtin-free; its separate Bionic harness exercises
this module and the real font engine without activating file imports.

Configuration clears optional font libraries/dependencies on deselection. Leaving
a previously generated scalar profile also clears its stale generated native
model flags. Invalid selections preserve an existing output.

## Actual constructor/call evidence

```sh
python3 utils/tests/native64-freetype-register.py --limbo /host/tools/limbo
```

The driver compiles complete owning freetype.c, real newmod/dtype/runtime/unload,
actual initmem/freeptrs/freeheap and published freeface. The retained FreeType
engine and lib9 sorter are compiled with incompatible-pointer diagnostics as
errors. Its standalone libc file adapter is explicit; this is not the Inferno
namespace adapter or a physical-phone test.

Per WORD32 and WORD32-file,754 active predicates cover all21 constructor/staged
Type allocation refusals, unchanged registry/globals, successful publication,
idempotence and final ownership. Every export is called through Module.ext with
fresh generated frames. Vera/Noto face creation, metrics, character queries,
transforms, glyph data and real native-handle cleanup execute. Actual public
record maps and callbacks retire names and glyph arrays. Heap/ASCII string/error/
exclusion transports are bounded fixture services, not production scheduler,
moving collector, Unicode conversion or asynchronous-call proofs.

Three strict controls remove creator retirement or corrupt generated frame size/
reference maps. They must fail exact semantic status10 before any unsafe call.
Compiler/signal/sanitizer failures never count. Header-only/packed refusals, five
invalid build selections, deterministic real-source generation and seven CLI/
selection/dependency cases are separate checks. No LSan opt-out is added.

Local x64 O3 nonrecovering UBSan passes the complete gate with synthetic allocator
aliases. A fresh classic-mk Linux/x64 emulator links and boots with all six font
entries registered, through JIT0/1,38 corpus processes/1096 in-program checks and
four explicit legacy/builtin refusals. Native ARM64/x64 sanitizer and Android
source-specific results follow separately.

The shared43-op scalar file profile and closed file CALL/MCALL/managed imports
remain. Full Draw/Tk/AWM, live-object module unload, scheduler/collector call
boundaries and physical targets still require independent admission/evidence.
Complex font work stays in C. No wire/layout/extension/opcode IDs are assigned.
