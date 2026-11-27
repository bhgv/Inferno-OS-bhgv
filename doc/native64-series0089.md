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

## Downloaded previous-source acceptance artifacts

Verified the0086 ARM64 ZIP digest against GitHub and its ELF64/AArch64 machine
identity. It contains190 gate logs. Download:
https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41554977837/artifacts/13193968360
ZIP SHA2568313d749180dcea96a33cff94b99ed3833fd8d028faedbfc5c15eadadcad8026.

Also verified the x64 ZIP digest e78bd7afb9d4b927c19a5a5d581ef7713726812539e9c2378e0350c091321d68
and replayed its packaged loop/case/procedures in isolated processes, JIT0/1.
All six retire with the expected module, empty exit text and empty scheduler.
The procedures image hash remains6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7.
These archives are Linux scalar consoles, not Android or full GUI clients.

New-source Linux run41558647434 remains in progress. Corrected Android
run41558762745 completed successfully at source3184e90b9f87afc60da0ae29c8b302ef87619171,
pinning core619d58bc. Actual Bionic module-set/initializer/registration and
I/O/UTF suites pass, followed by all existing whole C/JIT processes, exact
packaged images and real worker retirement. No new Linux result is inferred.

Downloaded Android artifact13196446425, checked ZIP SHA256
43f1b13006897fba3daba80f63f4e9d648db7362c85e45ecbe5c2da00795f6d8,
both native-only APKs, ELF64/AArch64 and16KiB LOAD alignment. The unchanged
probe image hash is9138ae33fb041adb8b0b689ad1e5c1420fdb5d86a33f0c4dad6c6e96b05c18e0.
APK identities/file names remain series0087 because this series changes test
fixture portability, not the scalar application. Physical phone/GUI acceptance
is not supplied by QEMU or archive checks.

| Mode | APK SHA256 |
|---|---|
| C | 0472e217ba4dcead865563a27b8032bcf4399e005bcd045f4139a28346ea467a |
| JIT | 208620b0675cf08d144780114cbd54d44441ba7f78048c05d1f1945c0eeefddc |
