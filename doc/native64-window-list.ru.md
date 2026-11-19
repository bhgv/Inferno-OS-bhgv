# Native64: списки Wmsrv.Window и поля окна

Настоящий reshaped проходит nil-проверку z.wins, hd списка и очистку w.img
на PC762–764. Следующий отказ — PC765 `image-geometry-owner`: передача w.r
в геометрический вызов. awm.init пока остаётся на PC180 `multi-callee`,
поскольку reshaped ещё не принят целиком. Command остаётся accepted.
Полный AWM event loop, графическое окно и Android UI пока не запущены.

## Выбранные типы и операции

Принимается ровно `list of ref Wmsrv->Window`. Полный nominal Window layout
проверяется перед допуском операций со списком и полями.

| Поле | Classic offset | Native offset | Native size |
| --- | --- | --- | --- |
| tag: string | 0 | 0 | 8 |
| r: Draw.Rect | 4 | 8 | 16 |
| img: ref Draw.Image | 20 | 24 | 8 |
| r.min: Draw.Point | 4 | 8 | 8 |
| r.max: Draw.Point | 12 | 16 | 8 |

Window size: classic 24, native 32, alignment 8, GC map 90 (hex).
Rect/Point остаются наборами Word32 с alignment 4 и без GC roots.
Похожие списки Image/Client и Window с изменёнными tag/img отвергаются.
Classic Dis bytes и код 32-битной сборки сохраняются.

Role 213 обслуживает cons/head/tail через прямые FP/MP root cells.
Nil-сравнение использует существующее полное 64-битное сравнение корня.
Role 214 читает/записывает tag/img через прямого FP/MP owner, включая nil
replacement. Role 215 копирует локальную Rect/Point геометрию между Window
и обычными FP/MP данными. Конвенции List/String и imported Image geometry
не расширяются этим контрактом. Передача Window.r в imported argument
пока остаётся отдельным закрытым участком.

До allocation проверяются opcode, modes, alignment, extent, field/size join,
source и destination GC roots, полный nonroot span геометрии и отсутствие
пересечения с call sites. Для нового list role destination GC bit проверяется
так же, как source. List nodes, pointer payload и освобождение используют
существующий runtime; JIT вызывает тот же общий cons allocator.

## Проверки

`utils/tests/native64-window-list.py --limbo PATH` строит 12 полных Limbo bodies
с canonical .m signatures и независимым classic operand/map oracle.
246 повреждений records обнаруживаются oracle. 492 повреждения metadata
отвергаются до allocation; все 33/34 точки отказа конструктора C/JIT откатываются.

408 C/JIT сценариев включают 396 обычных/ошибочных исполнений и 12 отказов
аллокатора при первом, втором и третьем cons. Проверяются nil list, nil heads,
дубликаты, обратный порядок, alias, общие хвосты, FP/MP head/tail/cons,
циклы обхода и разворота, поля tag/img, очистка ссылок, Rect/Point copies,
signed Word границы и quanta 1/3/1000 со stack growth.

Независимо строится весь достижимый граф владения: C/module roots, list tails,
каждый head Window, last/tag/img, Window fields и descriptor leases. Для каждого
узла проверяется точный refcount; количество Tlist/Tptr leases исключает
недостижимые потерянные nodes. Проверяются полный Window payload и соседние
roots, порядок элементов, caller guards и полное освобождение объектов,
frames, Types, modules и environments. 36 nil-list/nil-owner ошибок и 12
allocator errors освобождают также частично созданные списки. Allocator
использует существующую ошибку exHeap; её класс тестируется точно.

Три active controls подменяют источник cons, смещение Window Rect и снимают
проверку destination list root. Все обязаны дать semantic predicate failure
без sanitizer crash.

Gate включён в native64-port.py, Linux amd64/arm64/riscv64 и Android ARM64
Bionic workflow. Локальное исполнение относится к amd64; запуск других
архитектур здесь не объявляется выполненным. Графическая Draw-служба для
проверки списков и локальных данных не запускалась.

Локально прошли UBSan и ASan/UBSan со всеми тремя active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно считает owners/allocations).
Регрессии: локальная Image geometry 972 и Client fields/channels 4032 C/JIT
сценариев со всеми controls. Старые control matchers уточнены для общей
геометрической ветки и отдельного Client channel encoder. Чистая amd64
console/paired boot сборка и 324 настоящих emuinit C/JIT процесса прошли.
