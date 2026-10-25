# Native/Python emitter layout agreement

`leaf64-jit.py` passes a ctypes record directly to native `leafemit`. FRAME and
MFRAME extended `LeafLayout` before its final `stringmax` field. The old Python
record kept the old size/offset, so native code read `stringmax` beyond that
record: byte-branch fixtures declined to emit and character fixtures could read
an unmapped emulated address. This was a test ABI mismatch, not evidence that
the production string instructions had changed semantics.

The Python record now includes the import-table and frame fields in C order.
Each compiled fixture also exports independent C `sizeof`/`offsetof` probes.
Before emitting any machine code, Python compares its size and every field
offset. A future C layout change therefore produces an explicit diagnostic
before passing a potentially undersized record across ctypes. The probes cover
both LeafLayout and the separately defined RvLayout.

Reviewed at experiment `49c4de7b81c209133884e1ac49c18238e493514b`:

- `LEAF64_CASE=bytebranch LEAF64_ARCH=amd64`: 15474 checks.
- `LEAF64_CASE=bytebranch LEAF64_ARCH=arm64`: 15474 checks.
- `LEAF64_CASE=character`: 6814 checks across AMD64/ARM64/RISC-V64.

These cases compile native emitters and inspect/emulate emitted instructions.
Foreign instruction emulation is not physical ARM64/RISC-V runtime acceptance.
The complete unfiltered leaf matrix was not rerun for this fixture-only repair.

## Русский

После добавления FRAME/MFRAME Python-структура перестала совпадать с C
LeafLayout: сместился `stringmax`, а запись ctypes осталась короче. Исправлен
порядок полей; C-пробы `sizeof` и `offsetof` теперь проверяют весь контракт до
передачи структуры в emitter. Ошибка будущего изменения layout выявляется до
чтения памяти за записью ctypes. Пройдены указанные выше выборки сравнений и
индексации строк; эмуляция чужой архитектуры не заменяет проверку на устройстве.
