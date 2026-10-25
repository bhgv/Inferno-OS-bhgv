# Loader construction and scalar result slots

The remaining Loader result APIs now use the same explicit output discipline as
ifetch/link/tdesc. newmod/dnew validate the result slot, copy and publish H before
destroying the former owner, then byte-copy the completed new pointer. tnew/ext/
compile validate the slot before module mutations and publish a full WORD with
memmove, without assuming native alignment. This preserves existing status codes,
construction cleanup, compiler retry and failure ownership. No new ABI is added.

loader-runtime.py extracts the real Loader functions and shared helpers, alongside
actual dtype/freetype, mklinkmod, branch relocation and emulator kprotect. Native
allocation/compiler boundaries remain injected, as documented in the driver.
Its existing allocation-failure/retry suite now runs in both WORD64 and WORD32.
The added loader-results.c fixture uses explicit checks, including in NDEBUG.

For each WORD model, 40 ordinary cases cover five methods at all eight output
byte alignments. Scalar rejection fills exactly one WORD with -1. dnew/newmod
construct real owned objects/modules and replace them again: the injected
destructor observes H in the actual result slot before releasing the old owner.
Outer canaries and total allocation ownership must be restored. Ten separate
nil/H probes cover all five methods before any construction or module mutation.

Both WORD models pass combined ASan/UBSan: 100 added cases total plus the existing
suite twice. Each nil probe uses a fresh process, keeping one native error unwind
per process; this avoids stale sanitizer stack-shadow state when this sandbox
cannot discover the stack top. No sanitizer or individual bounds check is disabled.
Replaying the parent Loader reproduces a UBSan misaligned WORD store.

Run:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/loader-runtime.py
```

These checks cover return-slot geometry/publication and existing construction
paths, not arbitrary invalid frame pointers or a complete untrusted-bytecode
sandbox. Snapshot-specific regressions remain in README.loader-snapshots.md.

## По-русски

Исправлены оставшиеся ответы Loader: newmod/dnew публикуют H до освобождения
старого владельца и копируют указатели побайтно; tnew/ext/compile записывают
полный WORD без требования выравнивания. Нулевые слоты отклоняются заранее.
Новые 100 проверок и прежние сценарии конструктора проходят ASan/UBSan в двух
моделях WORD. Старый Loader воспроизводит невыровненную запись под UBSan.
