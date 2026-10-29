# Dis64: кандидат матрицы секций и оставшиеся решения

Дата: 2026-10-29. Предложение Inferno к FLASHVM-0001, а не принятая wire spec.
Числовые magic/ABI/section/opcode IDs пока не выделены. Действующий loader эти
секции не читает. [Журнал согласования](dis64-inferno-journal.md).

## Матрица присутствия

Предлагаю один module container для executable и library. Запись module metadata
объявляет профиль и entry. Library не требует entry; executable требует ровно
один entry с Dis-PC и frame descriptor. Саму функцию можно вызывать обычным ABI.
Так наличие code не означает обязательный запуск при импорте библиотеки.

| Логический section kind | Присутствие | Повторы | Пустота | Required flag |
| --- | --- | --- | --- | --- |
| module | Всегда | Singleton | Не допускается | 1 |
| code | Всегда в первом исполняющем профиле | Singleton | Не допускается | 1 |
| types | Всегда | Singleton | Хотя бы один валидный frame descriptor | 1 |
| data | Если module data_bytes != 0 | Singleton | Только при data_bytes == 0 | 1 при наличии |
| imports | Если есть импортируемые интерфейсы | Singleton | Нулевой count допускается | 1 при наличии |
| exports | Если есть экспортируемые интерфейсы | Singleton | Нулевой count допускается | 1 при наличии |
| strings | Если код/данные/таблицы ссылаются на pool | Singleton | Нулевой count допускается | 1 при наличии |
| control tables | Если есть обращения к таблицам | Singleton | Нулевой count допускается | 1 при наличии |
| debug | Необязательно | Singleton | Требует определения собственной версии | 0 |
| provenance | Необязательно | Singleton | Требует определения собственной версии | 0 |

Это логические kinds без номеров. Полное внутреннее кодирование module, types,
initializers и imports ещё требуется описать. Типы для чисто native интерфейсов,
библиотеки без code и несколько entry не входят в первый профиль; при надобности
для них нужен явно согласованный профиль, а не угадывание отсутствующих секций.

Обязательность присутствия проверяется независимо от флага уже найденной записи.
Известная семантическая секция с required=0 отвергается: её нельзя разрешать
старому loader пропустить. Все неизвестные биты section_flags отвергаются даже
для optional kind. Неизвестная optional секция пропускается только после
проверок directory bounds/ordering/overlap. Правило singleton неизвестного kind
не предполагается. Известные singleton duplicates всегда отвергаются.

Directory начинается сразу после manifest, header_bytes совпадает с концом
последней записи directory. Предлагаю alignment 1 для wire payloads: числовые
поля декодируются по байтам и не читаются через casts C structs. Runtime объекты
размещаются отдельно по target alignment. Это снимает зависимость wire от padding.
Политика промежутков между payloads и trailing bytes пока требует согласования;
предлагаю для v1 плотное размещение без таких промежутков, кроме нулевых секций.

## Согласованные части, реализуемые независимо от registry IDs

- WORD32/ref64, map8 с полным нулевым хвостом и проверкой padding bits.
- Frame lr/fp/mr/t: 0/8/16/24; result32, temps40/48/56, args64.
- Числовые case: включительные диапазоны, строгая сортировка без пересечений.
- String case: low/high IDs, порядок по содержимому, singleton с равными IDs.
- Control target_pc — индекс Dis-инструкции; default обязателен для case.
- Поля payload BE; ни native PC, ни указатели, ни host padding не сериализуются.
- Пара major/minor принимается только при явной поддержке; file_size <= UINT32_MAX.

## Открытые вопросы

Bool 0/1 в int32 принимается как FIR typed convention; базовый Limbo int не
ограничивается этими значениями и отдельный builtin bool не вводится.

Предложенное владение ref-result поддерживается как цель: caller-owned typed
slot, nil до вызова, один сильный владелец после успеха, cleanup при исключении.
Нужно ещё доказать coverage result slot при GC/safe points и unwinding; это
не утверждение о соответствии нынешнего unwinder будущему ABI.

Сравнение строк — Rune lexicographic, nil равно пустой строке по содержимому.
Строгий scalar-only UTF-8 pool пока не принимается: текущий insc сохраняет WORD
в Rune без фильтра surrogates/out-of-range. Предлагаю обсудить lossless BE u32
Rune sequence (length в Rune), либо два явно помеченных pool encoding. Выбор
ещё не принят; Python Unicode fixtures не доказывают runtime Rune32 acceptance.
Не вводить молчаливую замену несовместимых Rune на U+FFFD.

## Текущие проверки прототипа

```
python3 libinterp/tests/dis-layout.py
python3 libinterp/tests/dis-control.py
```

Golden payloads: libinterp/tests/dis64/control-golden.json. Это только payloads
таблиц без section IDs, не полные загружаемые модули. В include/dis-control.h
локальные enum selectors не являются wire IDs. String comparator передаётся
после разрешения pool IDs; helper не определяет encoding pool или nil ID.

Проверки layout/table helpers не заменяют compiler → module → interpreter/JIT.
