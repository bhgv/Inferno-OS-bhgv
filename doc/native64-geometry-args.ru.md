# Point/Rect в аргументах checked native64 вызовов

Checked LOAD lowering теперь передаёт `Draw.Point` и `Draw.Rect` по значению
в imported Limbo calls. Выбираются только эти nominal Draw ADT с полным layout:
2/4 последовательных int32, alignment 4, без mapped roots. Произвольные ADT,
managed aggregates, Draw.Pointer, aggregate results и indirect field sources
этим изменением не допускаются.

Поддержаны две исходные формы: отдельные MOVW для литерала/вычисленных полей
и IMOVM для готового Point/Rect в FP или MP. Число инструкций и classic PCs
сохраняются. Native координаты остаются четырёхбайтными, соседние ссылки
расширяются до восьми байт. Point размером 8 байт сохраняет alignment 4;
следующий Big/Real выравнивается отдельно.

Доверенный Native64TypedStep для копирования имеет IMOVM, AXIMM, длину 8/16
и annotation 64 — запись в pending imported frame. Конструктор проверяет
source/destination spans, alignment, обе root maps и единственный owning call
island до allocations. Materializer повторно проверяет target geometry и
публикует длину в Inst.reg. Root-containing block copies и копирование frame
header запрещены. Обычный Dis, frame layouts и 32-битный ABI не меняются.

## Проверка

```sh
python3 utils/tests/native64-geometry-args.py --limbo /path/to/host-tools/limbo
```

Исполняются два настоящих checked Limbo модуля: Geometry provider и Calls
consumer. Canonical import hashes и frame prefixes проверяются по отдельной
генерации `limbo -H` из .m declarations. Локальные поля provider frame могут
выходить за prefix. Если readonly diagnostics не могут подписать уже narrowed
Module parameter consumer, его classic export hash проверяется по отдельному
canonical Calls header; стандартный oracle без этого явного источника сохраняет
прежний запрет подменять unavailable signature.

480 вызовов включают 336 успешных и 144 ошибочных: nil module и настоящие
provider raise. Проверяются все четыре сочетания C/JIT provider/consumer,
quanta 1/3/1000, предельные int32, литералы, scalar fields, FP/MP copies,
receiver + Point + Rect + Big + byte, fresh/inherited Word result, малый
начальный stack extent, точный порядок координат и полное освобождение
Module/Modlink/Type/heap/exception owners. Доверенный native caller только
готовит входные аргументы; сам переход consumer → provider исполняется из Limbo.
Draw service, Window implementation и Android UI не подменяются этими модулями.

Независимый classic decoder обнаруживает 40 повреждений aggregate records и
stores. Проверяются 22 native metadata refusals до allocations, все 18/19
allocation failures provider и 36/37 consumer, а также три active controls:
удаление copy count, source map guard и target map guard. Прежние emuinit/Arg,
Context и coordinate gates сохраняются. CI включает gate для Linux
amd64/arm64/riscv64 и Android ARM64 Bionic; локальный C/JIT прогон — amd64.

## Изменение барьера AWM

Неизменённый `awm.init` проходит прежний PC61:
`wmclient->win.reshape(((0,0),(100,100)))`.
Первый checked отказ теперь PC72: nil comparison поля `Window.image`
(classic indirect offset 20), пока выбран только Draw.Context field access.
Это compiler evidence, а не выполнение полного AWM init. Classic bytes всех
16 startup-модулей совпадают. Окно AWM и Android UI ещё не запущены.

Локально gate прошёл UBSan и ASan/UBSan вместе с тремя active controls.
В ASan запуске LSan отключён (`ASAN_OPTIONS=detect_leaks=0`); fixture отдельно
учитывает все allocations и проверяет полное освобождение. Чистая amd64-консоль
прошла paired boot и 324 настоящих emuinit/Arg boot-процесса.
