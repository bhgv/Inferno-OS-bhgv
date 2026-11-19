# Native64: временный List root, Client.wins и String head в аргументе

Уточнение предыдущего отчёта: PC544 в handlerequest — nil-проверка
результата tl args, а не Client.wins. Там один compiler-owned root-слот
повторно используется после ref Pointer как list of String. Его сохранённая
декларация отличается от текущего per-use типа из IR. typedloadcompare
теперь сопоставляет роль с текущим типом только для compiler-owned .t
слотов; typedoperand по-прежнему проверяет полный mapped root8. Исходный
classic код и geometry слота не меняются.

Добавлена private role251 для nil-проверки Client.wins. Проверяются полный
канонический Client layout, List field classic24/native48, source owner
root8, readonly nil MP root8, ограничение nil offset65535, локальная цель
ветвления и отсутствие операции внутри подготовки CALL. Сравнение выполняется
IBEQL/IBNEL по всем64 битам; nil Client вызывает штатный unwind до чтения
поля. Тест использует реальный List of ref Window и непрозрачные handles,
не разыменовываемые сравнением. Другие поля Client этим не открываются.

На PC548 теперь проходит c.setimage(hd tl args, nil). Для String-аргумента
разрешена строго проверенная группа role252:

1. От1 до32 ITAIL в один compiler-owned временный List of String root8.
2. IHEADP из этого слота в mapped String-аргумент pending frame.
3. Немедленная очистка того же временного слота readonly nil.

Первый источник списка может быть FP/MP; продолжение цепочки использует
тот же FP-слот. Проверяются declared String argument, import ordinal/hash,
GC maps caller/callee, границы root8, положение внутри ровно одного
внешнего call site, связь соседних инструкций и точная nil-очистка.
Последовательность не разрешает произвольные инструкции подготовки вызова.
Payload строки удерживается pending frame до вызова и освобождается при
ошибке; временный список очищается до dispatch. List/HEADP/TAIL/MOVP и
их unwind используются из production VM.

Настоящий handlerequest проходит PC544 и PC548–595, включая Client.wins
nil-проверку на PC556. Новый отказ — PC596 external-call: вычисление
int hd args / int hd tl args непосредственно в аргументах Point.sub.
awm.init остаётся на PC303 multi-callee, поскольку handlerequest ещё не
принят целиком. Окно AWM и Android UI ещё не запущены.

native64-head-args.py проверяет один/два tail, FP/MP источник, повторные
вызовы и два String-аргумента с Word между ними, Unicode/nil голову,
пустой/короткий список, ошибку callee, очистку временного слота до callback,
String retain и полный retirement. Отдельный reused Pointer/List пример
проверяет именно исправленный nil case. Политика32/33 tail проверяется
компилятором. Callback использует canonical generated frame; настоящий
Wmsrv.Client.setimage этим тестом не исполняется.

Расширенный channel-nil gate проверяет шестое поле wins, eq/ne, FP/MP owner,
nil Client и старшие32 бита. Metadata отказы происходят до первого
выделения; конструкторы проверяются во всех точках отказа. Независимый
classic-byte oracle и активные контроли проверяют данные/адрес/очистку/GC
proof. Оба набора включены в native64-port.py и typed-calls CI.
32-битный код, classic Dis и shared FlashVM wire ABI сохраняются.

Локально amd64 под ASan+UBSan: 306 head-argument сценариев и 1488
channel/Client nil сценариев; 192/768 metadata отказов до выделения,
31/32 и105/106 точек отказа конструкторов C/JIT, три активных контроля
на каждый набор. Регрессия Rect-list/newrect330 и её контроли проходят.
Чистая сборка выполняет 324 процесса реальных неизменённых Arg/emuinit.
Новое выполнение на ARM64/RV64/Android здесь не заявляется.
