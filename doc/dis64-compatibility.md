# Dis64 и совместимость с FlashVM / DisVM

Решение уточнено пользователем 2026-10-29: DisVM активно разрабатывается,
сохранение уже собранных NC64-программ **не требуется**. Предыдущая обязательная
матрица исполнения старых NC64-файлов без перекомпиляции отменена.

Цель — **один общий новый Dis64 ABI для Inferno и DisVM**: int32, pointer64,
big64 и binary64. DisVM, FIR backend, bootstrap compiler и native stubs
переходят на него согласованно. Модули, библиотеки и SDK пересобираются.
Отдельный интерпретатор NC64, автоматическая конверсия старых файлов и
cross-profile marshalling не нужны для этого перехода.

Статус: решение о направлении миграции и проверенная карта исходников;
**новый wire format и общий ABI64 runtime ещё не включены**. Текущий runnable
host использует NC64 до согласованной замены компилятора и исполнения.

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

## Исходные layouts и один целевой ABI64

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

FIR i32 в текущем NC64 явно нормализуется до signed32 после арифметики.
При переходе на настоящий int32 новый backend сможет убрать избыточную
нормализацию только вместе с изменением типов инструкций/слотов и проверками
семантики. Для big, real, refs и private tables размеры описываются отдельно;
механическое сужение всех восьмибайтовых ячеек неверно.

## Общий контракт перехода

1. Выбрать и опубликовать один ABI/version для обеих VM и обоих compiler paths.
   Новый несовместимый layout должен отличаться от NC64. Старые experimental
   файлы можно отклонять с понятным требованием пересборки; нельзя молча
   интерпретировать их старые offsets/карты как новые. Не требуется сохранять
   старую NC64 машину исполнения ради уже собранных программ.
2. Int и адреса разделить: WORD/UWORD = 32 бита, native pointer/ref = 64 бита.
   Не делать общий Dis64 простым `IBY2WD=8` и не выводить target layout из
   `sizeof(long)` хоста. Общий layout-модуль должен задавать scalar/ref размеры,
   alignment, frame/ret/temp offsets, maps, cases, imports и native C-кадры.
3. В compiler/VM/FIR одновременно поменять target layout и выбор файла.
   Последовательно регенерировать builtin stubs, библиотеки, `.dis`/debug,
   module metadata и SDK. Зависимости неправильного ABI отклонять до выполнения;
   путь/имя и старая структурная сигнатура не делают два ABI совместимыми.
4. Сохранить обычную семантику Limbo int32, big64 и binary64. Восемь байт
   указателя не должны расширять int-арифметику. Фиксировать signedness,
   переполнение, shifts, MIN/-1, NaN и signed zero независимыми оракулами.
5. Pointer-map quantum в предлагаемом ABI64 — четыре байта. Отмечается начало
   полного восьмибайтового, выровненного ref; вторая половина не отмечается.
   GC/RC, init/copy/free, списки, массивы, каналы и исключения обязаны использовать
   байтовый offset карты, а не шаг `sizeof(void*)` как в текущем NC64.
6. Обычный frame содержит четыре восьмибайтовых runtime поля, затем result
   pointer и три pointer-capable temporary slots: всего 64 байта до аргументов.
   Компилятор, xec, JIT и generated C stubs должны брать один и тот же layout.
   Адресные инструкции обязаны писать pointer64 независимо от ширины int.
7. Старые opcode IDs и адресные режимы сохранять там, где семантика не меняется.
   Несовместимое расширение явно версионировать. Bytecode PC хранить как индекс
   или проверенный runtime Inst pointer; native JIT PC и C function pointers
   не сериализовать как обычные поля Dis. Формат SWO1/SWA1 остаётся отдельным.
8. C и нативный быстрый путь должны видеть один layout. Numeric/string case
   tables и GOTO описать явно. Нельзя просто снять NC64 C-fallback guards и
   отправить старые восьмибайтовые таблицы в classic leaves. Менять emitter,
   reader и runtime helper вместе, сравнивая интерпретатор с JIT+C.

Поддержка классического Inferno ABI32 остаётся отдельным существующим режимом.
Добавление его исполнения в новый LP64 host, если потребуется, — отдельная
задача; она не блокирует переход активно разрабатываемого DisVM на общий ABI64.
Новый upstream Inferno не обязан вводить NC64-only engine ради старого FIR output.

## Порядок согласованной реализации

1. Общие target-ABI константы и layout-проверки compiler/VM/FIR; затем явный
   file discriminator, одинаковый в Inferno и FlashVM. Номер нового magic
   назначается вместе с emitter/reader, не отдельно в двух репозиториях.
2. Compiler output, bytecode data records, C stubs и библиотеки для общего ABI64.
   Неготовый runtime должен отвергать такой файл, а не пытаться его исполнить.
3. VM pointer slots, frame, maps, builtin registration и loader. Сначала реальное
   выполнение в интерпретаторе с общей целевой семантикой.
4. x86-64 и ARM64 JIT с отдельными ширинами int/pointer и точными layout inputs.
   RV64 использует тот же ABI после двух приоритетных платформ.
5. Обновить FIR direct/backend и hosted вызовы, пересобрать DisVM приложения/SDK.
   Прогнать общие source-level сценарии на interpreter и JIT+C обеих VM.

## Условия приёмки нового общего ABI

| Область | Обязательная проверка |
| --- | --- |
| Scalars | int32 wrap, big64, binary64, shifts/division, NaN/signed zero |
| Pointer64 | Адреса выше 4 ГБ, LEA/IND, array/ref/string/module slots без усечения |
| Frame/C | CALL/MCALL/RET, рост стека, result/temp slots и regenerated builtin stubs |
| Heap/GC | Смешанные int32/ref64 записи, карты, массивы/списки, retain/free/copy |
| Runtime | Каналы, scheduler, исключения, callbacks и владение результатами C |
| Compiler/FIR | Одинаковый новый discriminator, target layout и module signatures; свежие modules/SDK |
| JIT+C | Те же результаты и ошибки, что у интерпретатора; одинаковый ABI в прямом и полном входах |
| Неверный ABI | Старые NC64 и неизвестные layouts отвергаются до исполнения с требованием пересборки |

Старые бинарники NC64 не входят в release gate и не требуют адаптера.
Проверки выполняются на заново собранных программах. Корпус source-level
сценариев может сохраняться для проверки семантики, но не закрепляет старый ABI.
Crypto/Tk и исключённые JIT backends в этот переход не включаются.

Текущие 173/173 JIT+C и 166/173 guarded native commands измеряют доступность
opcode входов при проверенных layouts, а не готовность нового общего ABI64.
Реальный ARM64 host acceptance отличается от исполнения в эмуляторе.
