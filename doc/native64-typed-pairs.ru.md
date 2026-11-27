# Два корня в результате Limbo-функции

Checked LOAD route теперь принимает точный результат
`(Command, list of string)` приватной Limbo-функции. Оба классических
4-байтных поля становятся двумя соседними 8-байтными managed roots.
Consumer непосредственно разбирает результат в `(next, words)`, публикует
`words` в MP и явно вызывает файловый `next->init(ctxt, words)`.

Это следующий участок пути emuinit: реальный LOAD уже возвращает Module
через resolve helpers, prepare helpers строят `-l` либо `sh/-c/$*`, а новые
loadmod helpers возвращают Module и argv вместе. Полный emuinit/sh/AWM
этим тестом пока не запускается.

## Проверяемый контракт

Компилятор допускает ровно два поля: Module, затем list of string.
Классические смещения 0/4 должны соответствовать native 0/8, общему размеру
16 и двум pointer leaves. Свежий result slot проверяется по декларациям
caller, размерам и двум битам root map. Forwarding допускается только между
функциями с тем же pair contract. CFG proof пересекает множество определённых
полей на всех путях: каждый достижимый RET требует оба поля. Повторная
запись первого поля не заменяет второе; недостижимый RET не требует записи.

В приватных native metadata contract 48 означает два 8-байтных корня,
а returned 40/56 различает первую и вторую записи IMOVP через borrowed
result pointer в FP32. Native64ModuleFunction.resultcontract связывает
callee, sites и pair stores. Constructor до аллокаций проверяет обе mapped
ячейки свежего результата, границы, контракт и binder step. Runtime LEA
service повторно проверяет обе ячейки реального caller Type и pending frame.
Обычный IMOVP сохраняет retain/release каждого поля; никакого сырого копирования
16 байт между managed объектами не добавлено.

Эти числа относятся к диагностическому checked compiler/builder пути.
Wire IDs, файловые профили, публичный managed ABI и 32-битные форматы не
меняются. External-only profile продолжает возвращать aggregate-result.
Reversed/list-int/three-field/scalar tuples также отклоняются. Общие ADT,
массивы, произвольные tuples и копирование tuple через IMOVMP не открыты.

## Выполнение и отказы

```sh
python3 utils/tests/native64-typed-pairs.py --limbo /path/to/new/limbo
python3 utils/tests/dis64-typed-pairs.py
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-pairs.py --limbo /path/to/new/limbo
```

Три файловых provider варианта проверяют 3888 C/JIT запусков: nil/Rune/NUL,
nullable Context, reload/replacement, рост стека, кванты и namespace faults.
Ещё 432 C consumer запуска проверяют исключение сразу после первого IMOVP
и отсутствие второго root bit в уже созданном caller frame. Для этого bit
убирается после инициализации кадра и восстанавливается до teardown.
Оба пути полностью освобождают managed owners, Types, MP и pins. Provider
для них работает в C и JIT; инъекция между stores ограничена C consumer.

Семь constructor отказов должны пройти без аллокаций и изменения REG.
32 C / 33 JIT allocation failures проверяют rollback построения модуля.
Независимый oracle сопоставляет classic instructions и native offsets,
поля, call islands и точные сигнатуры публичных Command exports.

Активные controls убирают проверку второго корня отдельно в constructor
и runtime binder. Они должны попасть в точный семантический predicate,
прежде чем callee запишет результат. Другой control убирает обязательность
обоих определённых полей в CFG. Сохраняются прежние hash/map/retain,
MP-publication и inherited-pointer controls. Sanitizer crash не является
успешным отрицательным тестом.

Gate pairs добавлен восемнадцатым в CI для x64, ARM64, RV64 и Android ARM64
Bionic, а также в full port driver. Локальные результаты и target CI
следует проверять отдельно по опубликованному SHA.
