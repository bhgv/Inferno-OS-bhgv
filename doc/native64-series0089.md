# Native64 series0089: allocator-alias coherence in Bionic module gates

Android run41557654440 compiled the core and passed binding/Loader matrices,
then failed builtin construction at a real port-header mismatch in the fixture.
Android fns.h defines object-like allocator aliases. Including interp.h under
`free -> free_` renamed Type.free; replacing that alias with a function-like
fixture macro later left extracted dtype accessing a differently named field.
The same aliases could redirect isolated libc allocation transports to missing
Inferno allocator symbols in Sys I/O and UTF tests.

A shared portfixture transformation now removes allocator aliases immediately
after fns.h, before interp.h and isolated transport functions. Only isolated
test units use this boundary. Production Android objects, headers and allocator
policy remain unchanged. Sys templates reuse the same transformation. No error
is silenced or counted as a semantic removal control.

`--port-aliases` reproduces the object-like aliases on a host before removal.
The classic-mk target driver runs module sets under this regression mode, with
a matching host-built Limbo. All allocation-failure/publication/retirement
checks remain active.

Local x64 O3 nonrecovering UBSan passes builtin sets with alias injection:
per WORD32 and WORD32-file,67 balanced builtin transactions,60 module-set
transactions,654 visibility checkpoints and72 request/retirement permutations;
publication/retirement controls pass. Legacy and packed models refuse.
Sys init (emu/os, ordinary/wide/small-map), profile registration (8 entries,21
allocation refusals and descriptor retry) and actual I/O/UTF handlers pass with
alias injection; UTF checks1115061 scalar/bounds cases per active model.

Prior owning source81385bb2 now has complete successful classic-mk matrix
41554977837 on native ARM64/x64 and QEMU RV64. Its focused ARM64/x64
ASan/UBSan also passed. New source target results remain pending.

FLASHVM0078 agrees with0086 independently. Read MANAGED-OWNERS.md at
3eec62d1d7c6de55904b4a847f27f99e708b59e3. Rooted-ticket capture/borrow admission,
separate source ownership, nonfinal-drop no-access, producer lifetime pins and
stopped-world destruction preserve the required distinction between native
counts and managed roots. This is protocol review, not implementation replay
or import. Retained destroy/high-gchalt and whole C/JIT exclusion remain required.
No native ticket is a wire capability or a moving-GC pin.

No runtime/JIT implementation, shared Dis64 ID or file-admission gate changes.
ARM64/x64 native paths166/173, complete native+C173/173; RV64I/Zmmul/M87/90/103
respectively with complete173/173. Production file gate stays43/173.
Full AWM/managed imports and file-call ownership remain separate acceptance work.
