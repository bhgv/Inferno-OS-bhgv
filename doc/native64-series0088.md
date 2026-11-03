# Native64 series0088: Android managed ABI validation

ARM64 compilation/startup is already achieved for the bounded scalar profile.
Android run41556102823 passes at launcher dc8073e9 and core81385bb2. Checked job
logs confirm38 boot processes/1096 in-program checks,67 case sources/134 whole
C/JIT processes,241 helper/result sources/482 whole C/JIT processes and the exact
packaged-image pair through real worker retirement. Native-only ELF64/AArch64
and16KiB LOAD checks pass. This is not physical-phone or AWM acceptance.

Current Linux run41554977837 has completed native ARM64 and QEMU RV64 successfully;
x64 remains in progress at this report. Focused ARM64/x64 ASan/UBSan is successful.

## Next acceptance boundary

A complete AWM client requires coherent native metadata and selected builtin
module registration, managed argument/result ownership and module call admission,
then end-to-end GUI/storage/lifecycle tests. Fast opcode counts alone cannot
measure this. No delivery date is inferred from isolated tests.

Android commit4bfe55d136e20d31dbb3a41de80c5f82d965ad55 extends the actual NDK
pipeline with existing owning-source builtin-set, Sys-init, Sys-register and
Sys-handler suites under static Bionic/QEMU and matching host-generated metadata.
Publication/retirement controls, long maps, descriptor failures and real fixed-frame
I/O/UTF bodies are covered by the suites. Registration callbacks remain observed
stubs in the registration fixture; handler execution is a separate fixture.
Bounded OS transports do not prove full device I/O. No new imports are admitted.

Local host launcher tests and shell syntax pass. New target build41557654440 is
pending; no result is borrowed from0087. Core/JIT implementation and Dis64 IDs
are unchanged in this pipeline series. The user authorized Android master
publication; both Android commits are there. Inferno work stays on experiment.
