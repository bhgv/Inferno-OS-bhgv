# Reserve self-load owners before linking

LOAD of `$self` acquires one Module reference and, when module data exists, one
native heap MP reference. iload now rejects nonpositive or maximum signed Module
counts, nil MP, and zero/maximum unsigned MP counts before calling linkmod.
The source MP/header are captured before linking and operand publication.

linkmod consumes the acquired Module reference on either binding failure or a
raised VM error. MP is retained and marked only after a successful link; failure
therefore needs no MP rollback and leaves its counter unchanged. Existing
publication order remains: install the result, then release the old destination.
A data-free self load keeps MP=H and requires no MP heap access. The last legal
Module and MP retains can each reach their maximum representable value.

module-imports.py executes the production iload/moduleimports bodies under both
WORD models. Its native unsigned Heap.ref now matches production ownership
instead of modeling it as int. There are 20491 WORD32 and 20492 WORD64 cases
(40983 total), including 24576 added boundary/error/alias cases. The fixture
covers all eight source/index/destination alignments, a destination aliasing
REG.M, external/self loads, data-free modules, binding failure and protected
link errors. Checks remain active with NDEBUG because the fixture uses explicit
conditions rather than assert. All structure fields/defines are documented.

ASan/UBSan passes the complete matrix. Replaying the parent iload rejects the
new dead-Module regression. Independently removing only the MP maximum check
fails the selected unsigned-overflow regression. SLICEA view reuse and JIT
coverage are documented in README.array-view.md.
