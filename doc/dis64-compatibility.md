# Dis64 и совместимость с FlashVM / DisVM

Обязательное требование пользователя от 2026-10-29: при разработке нового
64-битного Dis сохранить захват, загрузку и исполнение существующих `.dis`
в DisVM. Расширения DisVM должны быть надмножеством поддержанных возможностей,
а не основанием заменить старый формат несовместимым под тем же обозначением.
Успешный разбор файла сам по себе не доказывает его исполнимость.

Статус: согласованный набор требований и проверенная карта исходников,
**не реализованный универсальный загрузчик и не объявление нового magic**.
Текущий native-cell host отвергает classic Dis. Эта граница остаётся открытой;
смена названия NC64 на Dis64 её не устраняет.

## Проверенная исходная база

FlashVM: `master`, commit
[`6eb042846b9872b55e659abfd146d0bacc9f3a9a`](https://github.com/bhgv/flashVM/tree/6eb042846b9872b55e659abfd146d0bacc9f3a9a).
Его DisVM snapshot закреплён на Inferno `experiment` commit `a67032e`;
это не утверждение о переносе всех последующих upstream-коммитов.

| Назначение | Источник в FlashVM |
| --- | --- |
| Прямая генерация `.dis` из FIR | [`fir/v1/compiler/dis.c`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/fir/v1/compiler/dis.c), функция `fir_dis` |
| Генерация hosted table/module вызовов | [`fir/v1/compiler/dis_table.c`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/fir/v1/compiler/dis_table.c), функция `fir_dis_table` |
| Описание прямого backend и наблюдаемых результатов | [`fir/v1/doc/DIS_BACKEND.md`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/fir/v1/doc/DIS_BACKEND.md) |
| Выбор target-cell ABI bootstrap-компилятора | [`tools/limbo-bootstrap/CMakeLists.txt`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/tools/limbo-bootstrap/CMakeLists.txt) |
| Сборка исполняющего NC64 host и native ABI headers | [`disVM/runtime/CMakeLists.txt`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/disVM/runtime/CMakeLists.txt), [`disVM/runtime/prepare.py`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/disVM/runtime/prepare.py) |
| Исполнение, ограничения и отдельный NC64 ABI | [`disVM/runtime/README.md`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/disVM/runtime/README.md) |
| Numeric case/goto ABI и политика нативного подхвата | [`disVM/runtime/native_tables.c`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/disVM/runtime/native_tables.c), [`disVM/runtime/native_leaf.c`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/disVM/runtime/native_leaf.c) |
| Загрузчик, который используется в адаптированном source view | [`disVM/upstream/libinterp/load.c`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/disVM/upstream/libinterp/load.c) |
| Другая ветвь backend: SWO1/SWA1 с AVM1 | [`linker/FORMAT.md`](https://github.com/bhgv/flashVM/blob/6eb042846b9872b55e659abfd146d0bacc9f3a9a/linker/FORMAT.md) |

`linker/FORMAT.md` описывает SWO1/SWA1, little-endian поля и AVM1, а не Dis.
Не переносить его заголовок, порядок байтов или инструкции в `.dis` по сходству
назначения. FIR может иметь несколько независимых backend.

## Три разных профиля, а не два названия одного ABI

| Свойство | Classic Dis / ABI32 | FlashVM NC64 | Предлагаемый canonical ABI64 |
| --- | ---: | ---: | ---: |
| Unsigned magic | 819248 | **819264 — уже занят** | Пока не назначен |
| Signed magic | 923426 | **923442 — уже занят** | Пока не назначен |
| `WORD` / ячейка int | 4 | 8 | 4 |
| Pointer/ref cell | 4 | 8 | 8 |
| `big`, `real` | 8 | 8 | 8 |
| Quantum pointer bitmap | 4 | 8 | 4 |
| Четыре link/type поля обычного Frame | 16 | 32 | 32 |
| Offset result-pointer slot обычного вызова | 16 | 32 | 32 |
| Область обычного frame с ret и temps | 32 | 64 | 64 |

Правая колонка пока является проектом из [dis64-design.md](dis64-design.md),
а не действующим compiler/VM ABI. NC64 нельзя выдавать за этот ABI64: у него
шире не только указатели, но и int-ячейки, карты, numeric tables и builtin frames.
Прямой scalar FIR backend использует собственные entry/capture slots после
32-байтного Frame; нельзя выводить его offsets из layout обычного Limbo-вызова.

У всех проверенных профилей остаются существующие числовые opcode IDs,
адресные режимы и signed-30-bit кодирование operands. Счётчики/offsets файла
не превращаются в нативные указатели. В NC64 `DEFW` по-прежнему содержит
четыре big-endian байта на элемент, которые загрузчик знаково расширяет в
восьмибайтовый `WORD`; полный 64-битный payload передаётся `DEFL`. `DEFF`
содержит binary64 в существующем canonical порядке. Размер ячейки назначения
и число байтов в записи файла — разные свойства.

FIR i32 в NC64 явно нормализуется до signed32 после арифметики. Это не делает
весь NC64 WORD равным int32: низкоуровневые WORD-операции и private layouts
остаются восьмибайтовыми. Не удалять эту нормализацию и не сужать все NC64
слоты при добавлении нового профиля.

## Обязательный контракт нового загрузчика и исполнения

1. Распознавать профиль явно по формату/версии, до публикации heap, frame,
   imports или JIT. Не угадывать его по CPU, `sizeof(long)`, размерам descriptor
   или присутствию какого-либо opcode. Не переиспользовать четыре занятых magic
   из таблицы для несовместимого layout.
2. Новый 64-битный Inferno/DisVM должен принимать существующие файлы,
   порождённые FlashVM FIR backend и `limbo-native`, через совместимый NC64
   контекст, а не только файлы собственного нового compiler. В FlashVM DisVM
   сохранять работающий путь NC64 и добавить совместимый путь classic Dis.
   Наличие будущего ABI64 не должно требовать перекомпиляции существующего
   `.dis`, молчаливой замены его bytes или ручного поиска другого интерпретатора.
   Пока такой путь не выполняет старые файлы, поддержку формата нельзя объявлять
   готовой. Проверка заголовка, disassembly и импорт в module store недостаточны.
3. Не делать механическую конверсию NC64 → ABI64 заменой magic и offsets.
   Карта указателей не описывает все scalar типы; из неё нельзя восстановить
   различия int/big/real и смысл всех private tables. Для сохранения старого
   bytecode допустим отдельный профиль исполнения; для преобразования нужен
   отдельно проверенный перевод с достаточной типовой информацией.
4. В одном исполняющем контексте использовать согласованный ABI. Между профилями
   не передавать native frame/heap pointers и не выполнять прямой MCALL/RET.
   Возможное взаимодействие — через явный типизированный value/channel/module
   bridge с определённым владением. Согласованный путь для classic pointer32
   в 64-битном процессе должен сохранять guest addresses/handles либо доказывать
   корректность перевода; расширения typedef недостаточно.
5. Выбирать зависимости, builtin stubs, GC-visitors и numeric-table helpers по
   тому же профилю, что и модуль. Module/JIT caches обязаны различать профили;
   одинаковые имя, opcode или структурная сигнатура не разрешают смешение ABI.
   Сохранить существующие classic и NC64 сигнатуры внутри их старых профилей.
6. Сохранить bytecode PC как индекс/проверенный Inst pointer внутри runtime,
   а native PC — как private JIT metadata. Не сериализовать `Inst`, C function
   pointers, адреса heap или машинный код как обычные поля старого Dis.
7. Расширения должны явно обозначать необходимый профиль/версию/модули.
   Сохранять значения старых opcode, flag и data-record IDs. Не объявлять
   поддержку unknown instructions через молчаливый NOP или успешную заглушку.
   Hosted FIR-вызовы требуют также соответствующих native modules (например,
   `$Bigtable` там, где он импортирован), их сигнатур и владения, а не только
   распознавания базового opcode. Runtime compiler service проверяется отдельно
   для тех модулей, которые действительно его используют.
8. Прямой и полный JIT+C входы обязаны применять одну профильную политику.
   В частности, NC64 CASE/CASEL/GOTO нельзя передавать classic leaves, ожидающим
   четырёхбайтовые таблицы. Существующий NC64 C-подхват сохраняется, пока нет
   проверенного нативного пути именно для этих таблиц.

Номера/структуру нового заголовка согласовать одновременно в Inferno и
FlashVM: compiler emitters, DisVM reader/executor, FIR backend, SDK metadata
и инструменты просмотра. Сначала документированный общий контракт и тесты,
затем изменение формата; не вводить две независимые версии «Dis64».

## Условия приёмки совместимости

Корпус фиксируется настоящими bytes и provenance исходного compiler/revision.
Проверки должны включать старые файлы без перекомпиляции, а также свежую
генерацию обоими compiler paths. Новый emitter не может быть единственным
источником ожидаемых результатов для собственного reader.

| Корпус | Обязательные проверки |
| --- | --- |
| Classic Limbo `.dis` | Загрузка и фактическое исполнение int32/big/real, обычных calls/returns, heap/GC, массивов, строк, каналов и native modules в совместимом контексте |
| NC64 bootstrap `.dis` | Существующие runnable примеры и библиотеки DisVM, native builtin frames, imports и callback/owner lifetime; прежние результаты |
| FIR → NC64 scalar | Оригинальные восемь байт capture values/flags, i32 normalization, i64, binary64, CFG и optimized/unoptimized результаты |
| FIR → NC64 hosted | Таблицы/строки/refs, module signatures, typed calls, владельцы, loops и error cleanup |
| Новый ABI64 | int32 + pointer64, смешанные GC maps, адреса выше 4 ГБ, regenerated stubs, вызовы C, scheduler и interpreter/JIT эквивалентность |
| Несовместимость/ошибки | Unknown ABI, truncated/повреждённый файл, неправильная dependency ABI, одинаковое имя модуля разных профилей и отказ прямого cross-profile pointer/call |

В каждом поддержанном профиле сравнивать interpreter и JIT+C по наблюдаемым
результатам и исключениям. Приоритет платформ — x86-64 и ARM64; исполнение на
эмуляторе отделять от физического host acceptance. Распознавание signed magic
сохраняет существующую политику проверки подписей; менять security/crypto в этой
работе не требуется и готовность подписанных файлов одной таблицей не доказана.

Сейчас эта матрица является **release gate будущего Dis64**, а не списком уже
пройденных тестов. Текущие 173/173 JIT+C и 166/173 guarded native commands
измеряют другое: доступность opcode входов при проверенных runtime layouts.
