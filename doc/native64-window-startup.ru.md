# Window roots и список startinput в checked native64

Добавлены два связанных участка startup AWM: доступ к mapped полям
`Wmclient.Window` и подготовка временного списка строк внутри imported call.
Classic Dis, обычные frame layouts и 32-битный ABI сохранены.

## Window

Compiler проверяет полную nominal раскладку Window: 104 classic байта,
128 native байт, 26 листьев. Переносятся только шесть mapped полей:

| Поле | Classic offset | Native offset |
|---|---:|---:|
| display | 0 | 0 |
| image | 20 | 24 |
| ctxt | 40 | 48 |
| ctl | 52 | 64 |
| titlebar | 56 | 72 |
| screen | 84 | 104 |

Чтение, запись и nil tests поддерживают владельца в FP/MP и прямой второй
операнд в FP/MP. Pointer comparisons используют IBEQL/IBNEL. Аннотация 100
обозначает MOVP с native displacement в m; 101–106 выбирают nil test поля.
Native constructor проверяет spans, обе root maps, branch ownership и запрет
операции внутри pending call setup.
Compiler отдельно сохраняет исходный ref ADT owner в Addr.native64base: тип
поля и первый storage type переиспользованного .t не заменяют тип владельца.
Неизвестный scratch owner закрыт. Проверка использует один и тот же classic
scratch для Window/Other: returned Window допускается, foreign owner отвергается. Semantic owner/field class доказывает
compiler; это immutable C metadata, а не новый формат untrusted file admission.

```sh
python3 utils/tests/native64-window-fields.py --limbo /path/to/host-tools/limbo
```

378 C/JIT вызовов покрывают все шесть полей, nil owner, полностью/частично
пустой Window, FP/MP values, self replacement через FP locals, quanta 1/3/1000
и малый начальный стек. Все 20 scalar/Point/Rect words получают guards и
сохраняются. Проверяются reference counts и полный retirement. Дочерние
payloads непрозрачны: Draw/Tk/channel services не исполняются.

Независимый classic constructor probe подтверждает 104-byte descriptor и
точные шесть roots; сгенерированный `limbo -H -M Wmclient` header проверяет
настоящую natural C раскладку, sizeof/offsetof и native map {0x92,0xc4}.
Readonly unavailable export signatures проверяются отдельным canonical .m
header, как в geometry gate. Oracle обнаруживает 360 повреждений записей,
22 native metadata отказа происходят до allocations, все 23/24 allocation
failures откатываются. Три active controls проверяют displacement и root maps.

Это доступ к существующему managed Window, который fixture создаёт из
сгенерированной native geometry. Limbo allocation Window, произвольные ADT,
scalar/aggregate field operations и nested dereferences здесь не открыты.

## Список аргументов после MFRAME

Для formal `list of string` распознаётся ограниченная линейная схема:
readonly nil → 1..256 CONSP в одном FP root → store в pending argument → clear
временного root. Сохраняются исходные инструкции и PCs. Per-use тип compiler
временного `.t` учитывается, если его storage class был переиспользован.
Существующие tails и int lists остаются закрыты для этого нового lowering.

Аннотация 112 выделяет preparation/clear; materializer нормализует их в обычные
MOVP/CONSP. Constructor проверяет единственный owning imported call, последовательность,
один temp slot, readonly nil, source/temp/target root maps и обязательный clear.
Таким образом receiver уже находится в mapped pending frame во время cons allocations,
а перед входом в сервис единственным владельцем списка остаётся argument cell.

```sh
python3 utils/tests/native64-list-call.py --limbo /path/to/host-tools/limbo
```

48 C/JIT вызовов проверяют точный порядок kbd/ptr, nullable receiver, отказы
первого/второго настоящего cons allocation, ошибку native сервиса, reference
counts и полное освобождение. Используется контролируемый native Sink с canonical
.m hash/prefix; это не исполнение настоящего Window.startinput. Есть 24 oracle
mutations, 21 metadata refusal (включая 257 cons и nil strings table), все
20/21 constructor allocation failures и три active controls (clear и maps).

## Startup evidence

Неизменённый `awm.init` проходит Window.image на PC72 и подготовку startinput
на PC84. Первый checked отказ теперь PC93: local call makescreen; его тело
ещё не допускается. Classic bytes всех 16 startup-модулей совпадают.
Это compiler evidence, а не запуск AWM init. Окно AWM и Android UI ещё не запущены.

Оба gate включены в Linux amd64/arm64/riscv64 и Android ARM64 Bionic CI;
локальные C/JIT проверки относятся к amd64.

Локально оба gate прошли UBSan и ASan/UBSan с шестью active controls. LSan
отключён (`ASAN_OPTIONS=detect_leaks=0`), fixture отдельно учитывает allocations.
Чистая amd64-консоль, paired boot, 324 настоящих emuinit/Arg boot-процесса,
прежние Context/geometry и emuinit owning regressions прошли. Финальный compiler
сохраняет тот же list IR после добавления owner provenance; 16 classic образов
побайтово совпадают с ordinary-компиляцией без diagnostics.
