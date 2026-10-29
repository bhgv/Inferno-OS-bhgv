# Dis64: согласованный контракт, редакция 2026-10-29

Статус: нормативная **согласованная часть** будущего Dis64. Полная спецификация
файла ещё не завершена: magic/ABI/section/opcode/extension IDs и внутренние
module/type/import/code payloads не назначены. Этот текст не включает target
по умолчанию и не означает готовность нового интерпретатора или JIT.

Основания: INFERNO-0001..0005 в [журнале Inferno](dis64-inferno-journal.md),
FLASHVM-0001..0003 в [журнале FlashVM](dis64-flashvm-journal.md).
Согласие по signed32 Rune: FLASHVM-0003, Inferno подтверждает INFERNO-0006.
При расхождении исторического design с этим документом для перечисленных
согласованных правил применяется этот документ; неописанные поля не угадываются.

## Значения и размещение

| Значение | Байты | Alignment |
| --- | ---: | ---: |
| byte | 1 | 1 |
| int/WORD/UWORD, FIR typed bool | 4 | 4 |
| big, real IEEE binary64 | 8 | 8 |
| ref, string/list/array/chan/module reference | 8 | 8 |

FIR bool имеет значения 0 или 1; обычный Limbo int не ограничивается ими.
Размеры target не выводятся из sizeof(long) или ABI compiler host. Поле
начинается в первом подходящем выровненном byte offset; размер записи
округляется к максимальному alignment полей. Вложенная запись сохраняет
свои размер и alignment; stride массива — полный размер его элемента.
Int-only запись может иметь stride4. Нулевой stride не даёт основания делить
на него или превращать count в объём памяти. Wire bounds и allocation limits
проверяются отдельно от вычисленного layout.

## Frame и результаты

| Byte offset | Поле | Байты |
| ---: | --- | ---: |
| 0 | lr, runtime instruction pointer | 8 |
| 8 | fp, caller frame | 8 |
| 16 | mr, module link | 8 |
| 24 | t, frame descriptor | 8 |
| 32 | caller result address | 8 |
| 40 / 48 / 56 | STemp / RTemp / DTemp | 8 каждое |
| 64 | начало declared args/locals | — |

Это единый публичный VM calling ABI для Limbo, FIR и native module boundaries;
системный C ABI платформы соблюдается переходником отдельно. Поля Frame,
result address и pointer-capable temps не объявляются автоматически heap roots.

Caller создаёт typed GC-visible result slot и инициализирует ref-result nil.
После успешного возврата slot владеет одной сильной ссылкой. Borrowed argument,
возвращаемый как result, получает удержанную ссылку либо явную передачу владения.
Cleanup при исключении освобождает опубликованный result ровно один раз;
незавершённый result caller не читает. Descriptor/safepoint coverage самого
caller slot обязательна. Исполняемое доказательство этого нового ABI ещё нужно.

## Полные GC-карты

ABI64 quantum=8. Старший бит первого байта соответствует offset0, следующий
бит offset8 и так далее. Полная длина — size/64 + (size%64 != 0) байт;
size0 имеет карту длины0. Усечённый нулевой хвост в wire v1 не допускается.
Каждая отмеченная ссылка полностью помещается в объекте; неиспользуемые биты
нулевые. Size4 допускает один нулевой байт карты, но не отмеченную ссылку.
Для int/ref/int offsets0/8/16, size24, map40(hex).

Runtime может сжимать карту внутри, сохраняя те же ссылки и проверенное поведение.
Mark/init/copy/free/type helpers используют явные byte offsets. Никакое правило
нового ABI не переосмысляет карты классических Dis32 или старого NC64.

## Lossless строки и порядок Rune32

Non-nil record: rune_count:u32 BE, затем ровно rune_count значений u32 BE.
Count измеряется в Rune, не UTF-8 bytes и не UTF-16 units. Все 32 бита
сохраняются, включая U+0000, surrogate values и значения выше U+10FFFF.
Пустая строка имеет record 00000000. Nil — отдельная ссылочная инициализация;
её ID ещё не назначен, нулевой count не является nil-tag.

Сравнение лексикографическое по **signed32 интерпретации Rune bits**:
80000000 < ffffffff < 00000000 < 7fffffff. Реализация сравнивает sign-flipped
unsigned ключи либо явно декодированные широкие signed значения, без
implementation-defined unsigned-to-int conversion. Одинаковый общий префикс:
короткая строка меньше. Nil равна empty по содержимому, но владение различается.
Byte в mixed byte/Rune comparison — неотрицательное 0..255. Locale, case-folding
и Unicode normalization не применяются.

INDC возвращает signed32 значение Rune, расширенное до runtime WORD при нужде.
INSC сохраняет low32 bits в Rune. Конверсия к внешнему UTF-8 — отдельная операция
и не подменяет lossless wire serialization.

## Typed control payloads

Все числа BE, без padding. Target PC — индекс декодированной Dis-инструкции,
строго меньше instruction_count. Native PC/Inst*/адрес строки в wire не пишется.

| Логический kind | Payload |
| --- | --- |
| case.i32 | count:u32; rows(low:i32, high:i32, target:u32); default:u32 |
| case.big | count:u32; rows(low:i64, high:i64, target:u32); default:u32 |
| case.string | count:u32; rows(low_string_id:u32, high_string_id:u32, target:u32); default:u32 |
| goto | count:u32; target:u32 для каждого элемента |

Границы включительные, low<=high. Строки проверяются по содержимому, не
числовому порядку IDs. Диапазоны отсортированы и не пересекаются; singleton
имеет одинаковые endpoints. Default обязателен даже при count0. Goto сохраняет
порядок индексов; недопустимый индекс даёт определённую ошибку без перехода.
Тип таблицы должен соответствовать operand role инструкции; точная запись
ссылки на table ID ещё требуется в code specification.

Loader проверяет counts/extent/все цели до создания runtime таблицы. Runtime
view неизменяем; JIT native-PC cache принадлежит конкретной генерации кода и
сбрасывается при unload/recompile. Dis-PC остаётся ключом debugger/handlers.

## Контейнер, секции, расширения и импорты

Четыре независимые проверки: format version, ABI, mandatory VM extensions,
структурированные imports. Принимается только явно поддерживаемая пара версии.
Существующий signed30 operand сохраняет предел -2^29..2^29-1; u32 directory
не расширяет этот диапазон. File size <= UINT32_MAX; extent проверяется через
length<=file_size-offset, counts — до умножения. Resource limits могут быть меньше.

Именованные extensions: ASCII namespace-qualified имена из [a-z0-9._-], первая
буква a-z, длина1..255; порядок по имени, duplicates запрещены. Запись: name_bytes,
major, minimum_minor, reserved — по u16 BE, затем name bytes без NUL. Reserved0.
Major совпадает точно; совместимость более нового minor разрешена лишь явным
контрактом extension. Неизвестная обязательная возможность отвергается.
Имена обычных сервисов и библиотек не становятся VM extensions автоматически.

[Матрица секций](dis64-section-matrix.md) задаёт presence/singleton/required flags.
Wire alignment1, payloads плотные: cursor=header_bytes; offset==cursor;
после length cursor обновляется проверенным сложением; конечный cursor=file_size.
Count0 не равен byte_length0: внутренние count/version headers сохраняются.
Unknown required sections и неизвестные flag bits отвергаются; optional kinds
пропускаются только после общих проверок. Известные singleton duplicates запрещены.
Library entry отсутствует и не запускается при импорте; executable имеет один
entry с валидным Frame descriptor. Code-less library пока вне первого профиля.

ABI/version/extension и интерфейсы проверяются по графу зависимостей до
связывания общих объектов. Имя модуля или старый hash без ABI недостаточны.
Разные layouts в одной куче не смешиваются. Старые NC64 модули пересобираются.

## Незавершённая часть и критерии готовности

Новый 8-byte discriminator и 32-byte fixed header с directory обсуждены как
основа, но точные IDs не приняты. [Registry proposal](dis64-registry-proposal.json)
публикуется только для явного ответа FlashVM, не как источник активных констант.
Module/type/import/code payloads, relocation/initializers, подписи интерфейсов,
полные modules/SDK и ref-result execution fixtures ещё предстоят.

Helpers и золотые payloads не являются загружаемой программой нового Dis64.
Доказательство: freshly built compiler → module → interpreter, затем JIT+C на
том же ABI, mixed C calls, GC/calls/channels/exceptions/debug state. Реальное
исполнение ARM64 отличается от harness/emulator, модель WORD32 на x64 не host32.
