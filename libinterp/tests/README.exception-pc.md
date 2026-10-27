# Exception predecessor coordinates

The emu/os exception walkers now calculate predecessor positions through numeric
native addresses, never subtraction of unrelated pointers. A builtin module has
no bytecode image and must not select an image handler before walking to its
caller. Interpreted positions must be instruction-aligned, strictly after the
image base and no later than one past its declared final instruction. Compiled
handlers retain byte-offset coordinates and reject unrepresentable offsets.

The exception-frames matrix includes misaligned, past-end, nil-image and nil-PC
cases. The parent selects a handler for the first misaligned-PC probe; the fixed
emu/os implementations reject it before diagnostics or exception-slot writes.
The complete frame matrix passes UBSan in native and WORD32 models: 1312 cases
per implementation/model. Run `python3 libinterp/tests/exception-frames.py`.
Existing handler maps, cleanup, typed payloads and caller-frame traversal remain
covered by that same matrix. This does not claim a full Inferno boot.

## Русский

Обработчики исключений emu/os вычисляют позицию предыдущей инструкции через
числовые native-адреса. Builtin без bytecode не имеет собственной позиции для
выбора обработчика; поиск продолжается в вызывающем кадре. Для интерпретируемого
кода проверяются выравнивание, начало и конец образа. Для скомпилированных
обработчиков сохраняется система байтовых смещений.

Родительский код ошибочно выбирает обработчик для PC посреди инструкции. Новый
тест воспроизводит это, а исправленная версия проходит все 1312 случаев для
каждого emu/os и native/WORD32 под UBSan. Полная загрузка Inferno не проверялась.


The absent-handler sentinel now has native `ulong` width, matching the loader's
`(ulong)-1`. A new no-pattern-match probe rejects the former LP64 jump before
the image. The matrix contains 1320 cases per implementation/model.
Значение отсутствующего обработчика имеет native-ширину `ulong`, как в loader.
Новый тест несовпавшего шаблона воспроизводит ошибочный переход перед образом
на LP64; обновлённая матрица содержит 1320 случаев на вариант/модель.
