# Возврат Module и argv через приватные Limbo-функции

Этот этап соединяет существующие 8-байтные managed results с реальной
файловой загрузкой и подготовкой argv. Checked consumer вызывает приватные
`resolve_forward → resolve` и `prepare_forward → prepare`, затем явно
вызывает файловый Command/Wm.init. Это проверка отдельных Module/List
результатов; общий `(Command, list of string)` пока остаётся закрыт.

## Исправление diagnostic compiler

Раньше ordinary-компиляция приватного `resolve(path): Command` проходила,
но typed-отчёт падал с `signing a narrowed module`: интерфейс загруженного
модуля уже был сужен до используемых методов, а reporter пытался подписать
приватную функцию как внешнюю. Нельзя восстанавливать hash, принудительно
меняя linkall, или выдавать выдуманный hash за внешний контракт.

Readonly walker теперь проверяет доступность подписи перед sign(). Cached
legacy hashes сохраняются. Для несигнируемого суженного типа reporter выдаёт
`"signature": null, "signature_available": false`. Это отсутствие
диагностического hash, а не hash=0. В Native64ModuleSpec приватная функция
не экспортируется и не получает Link; её локальная identity определяется
проверенными PC/descriptor. Внешние подписи сверяются с classic export table.

Walker учитывает storespace, пропускает методы ADT как поля, проверяет
вложенные и tagged Module-поля, recursion, предел 64 уровней и visit budget.
Он не меняет Type/Decl/linkall/rec/eq. Диагностический обход с bounded Decl
циклом также прекращается. Обычный emitter и алгоритм classic sign не менялись.
Classic bytes с отчётом и без него остаются одинаковыми.

## Совместный source proof

Восемь полных тел потребителя включают четыре прежних экспорта и четыре
приватных helper. В графе четыре локальных call site и два импортных:
свежие Module/List results и два inherited forwarding site. Ни один helper
не попадает во внешний export table. Приватные Module-return hashes
явно unavailable, остальные доступные signatures проверяются как раньше.

Общий classic Dis oracle теперь может проверять local и external islands
в одном теле. Он сверяет target PC/descriptor, аргументы, complete result
contract, свежий/унаследованный адрес и непересечение setup islands. Прежний
вызов без rows по-прежнему запрещает local calls. Отдельный nil oracle
сохраняет точную проверку расширения pointer branches до 64 бит.
Четыре corrupt-report controls меняют target, contract, inherited flag и RET.

## Выполнение и воспроизведение

```sh
python3 utils/tests/native64-typed-local-loads.py --limbo /path/to/new/limbo
python3 utils/tests/dis64-typed-signature.py
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-local-loads.py --limbo /path/to/new/limbo
```

3888 C/JIT запусков повторяют две загрузки и две подготовки argv, включая
nil/Rune/NUL, nullable Context, рост стека, кванты 1/3/1000 и девять namespace
политик. Module возвращается из LOAD через local Frame result в MP, List
возвращается через другие helpers. Проверяются ready, точные startup literals,
замена старых владельцев, тождество хвоста и полное освобождение всех refs,
Types, MP и pins. Constructor проверяет 30 C / 31 JIT allocation failures.

C observer перед двумя forwarding CALL проверяет равенство всех восьми байт
borrowed result pointer у caller и pending frame, до его разыменования callee.
В C-случаях проверяется точное число наблюдений. Активный контроль направляет
публикацию в другую reserved ячейку; observer обязан остановить его точным
семантическим отказом до доступа по неверному адресу. Sanitizer/crash не
считаются успехом контроля. Сохраняются прежние hash/map/retain/MP controls.

Отдельный signature unit проверяет readonly snapshots, narrowed/full/nested/
tagged Modules, filtering методов, recursion, depth64/65 и Decl-cycle budget.
Контроли убирают narrow guard и безопасно уменьшают depth limit: каждый
обязан попасть в точный predicate. Публичные hashes не заменяются нулём.

Gate local-loads добавлен семнадцатым для x64/ARM64/RV64/Android ARM64 Bionic
и в full port driver; signature unit выполняется внутри него. Старые argv,
Command, list и module gates остаются отдельными регрессиями. Новые target
receipts следует отличать от локального bounded sanitizer-прогона.

## Следующая граница

Явный source witness `(Command, list of string)` остаётся well-typed в
classic Limbo и получает `aggregate-result` в checked route. Для него нужен
отдельный 16-байтный контракт с доказательством обоих корней и всех путей
возврата. Этот этап его не назначает. File profiles, wire IDs, public managed
ABI и 32-битные пути не расширены. Полный emuinit/sh/AWM пока не исполняется.

Полный новый gate проходит локально x64 ASan+UBSan/NDEBUG с detect_leaks=0,
включая pointer-publication контроль перед CALL. Прежние 3888 argv и 960
module C/RX случаев, оба compiler IR unit с контролями и свежая полная
console/paired boot проходят. Проверка остальных целей — по CI нового SHA.

Следующий этап реализует ограниченный двухкорневой результат:
[Module/List pair results](native64-typed-pairs.ru.md). Описанный выше
external-only отказ сохраняется; новый контракт включён только в LOAD route.
