# Existing native-PC JIT frame audit

The IFRAME fast paths in comp-arm.c, comp-386.c, comp-mips.c, comp-sparc.c and
comp-power.c indexed Module.type and tinit directly with an unchecked immediate.
A negative or excessive index could corrupt helper marks or read outside the
module's descriptor table during compilation. Every backend now checks the
index, table/descriptor presence and minimum Frame size before selecting this
fast path. Other cases use the existing interpreter fallback. The shared VM
frame handler checks the current Module table, including after Loader.tnew
moves it. Negative descriptor sizes are compared against a signed sizeof(Frame)
bound, avoiding accidental unsigned promotion.

The 386, MIPS, SPARC and Power helper-selection tables were allocated without
initialization. Unset entries could therefore request unnecessary Type helper
generation, consuming memory and exposing compilation to failures unrelated to
any selected frame. All four now request cleared allocation, as ARM already did.

The MIPS compile driver also continued after failed allocation of patch, tinit,
preamble and generated-code storage. It now returns failure and releases its
private buffers before dereferencing them or publishing code. If preamble
construction completed before a later failure, the shared preamble cache stays
valid for the next compile. The module's program, entry and export/import PCs
remain unchanged on the tested allocation failures.

Validation uses 540 frame-selection cases and 240 full-driver failure/retry
cases, production switch arms and the complete MIPS compile driver. All pass
strict UBSan and ASan. Old sources reproduce the failures independently. See
libinterp/tests/README.jit-frame-indices.md for injected boundaries and source
substitution controls. Host syntax checks cover all five complete backend files;
these checks do not establish target ABI compatibility or native execution.

This closes the described frame selection and allocation defects. Full native
stack arithmetic, bytecode validation and rollback after errors during helper
emission remain outside this patch. The requested x64, arm64 and riscv64 ports
are still subsequent work; existing RV64 leaf operations are not a complete
native-PC JIT port.
