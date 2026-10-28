# Sys.pipe element-layout preflight / Проверка типа элементов Sys.pipe

## English

`python3 libinterp/tests/sys-pipe-types.py` extracts the complete production
Sys.pipe and ownership helpers from both emu/port/inferno.c and os/port/inferno.c.
It runs seven invalid element layouts in each native/WORD32 profile: 28 cases.
The real Type and Array definitions are used; native pipe operations are observers.
ASan/UBSan are enabled by default; unsupported LeakSanitizer process discovery
is disabled by the shared fixture. No real filesystem pipe is opened.

Previously Sys.pipe checked the array length/data address but treated the first
two elements as managed Sys_FD pointers regardless of element type. A byte or
unmapped scalar array cannot safely provide two pointer-sized owner slots.
The new preflight copies the element descriptor and requires pointer-sized
storage, one pointer-map byte and its leading bit 0x80. It does not require a
canonical descriptor identity, so a separate equivalent pointer descriptor works.
Nil/H, implausibly low or wrapping descriptor addresses are rejected before
reading Type, array data, previous owners or making a native call. Rejection
preserves the existing array and returns -1 through the original captured result.
This validates layout, not arbitrary native-address provenance or all VM typing.

The fixture covers nil/H element types, byte-sized elements, missing pointer maps,
wrong pointer bits, and low/wrapping type pointers. Poison data proves those cases
are rejected before inspecting the first element. Existing owner matrices now
provide an explicit valid pointer-element descriptor; their normal admission,
reference counts, rollback and packed-frame scenarios remain applicable.
The test fails against the parent production source and passes after the fix.

## Русский

Команда `python3 libinterp/tests/sys-pipe-types.py` извлекает полные функции
Sys.pipe и helpers владения из emu и os. Семь неверных описаний элементов
проверяются в каждом профиле native/WORD32: всего 28 случаев. Используются настоящие
структуры Type/Array; создание нативной трубы заменено наблюдаемой границей.
По умолчанию включены ASan/UBSan; недоступная в sandbox проверка процессов LSan
отключена общим fixture. Настоящие файловые трубы не открываются.

Прежде проверялись длина массива и адрес данных, но первые два элемента всегда
читались и заменялись как управляемые указатели Sys_FD. Массив байтов или скаляров
без карты указателей не предоставляет два таких слота. Теперь до обращения к
данным копируется описание элемента: нужны размер указателя, один байт карты и
старший бит 0x80. Проверяется структура, а не совпадение адреса с каноническим Type;
отдельный эквивалентный дескриптор остаётся допустимым.

Nil/H, слишком низкий или переполняющийся адрес Type отвергаются до чтения Type,
старых владельцев и вызова нативной операции. Возвращается -1 в исходную ячейку,
массив остаётся прежним. Это проверка разметки, а не доказательство происхождения
любого адреса C или полноценной типизации всей VM.

Проверяются nil/H, байтовый размер, отсутствие карты, неверный бит, низкий и
переполняющийся указатели Type. Заведомо непригодный адрес данных подтверждает,
что обращение к элементам не начинается. Прежние матрицы владения теперь задают
явное корректное описание указательного элемента и сохраняют проверки счётчиков,
отката и невыровненных кадров. Новый тест падает на родительском коде и проходит
с исправлением.
