# Native64: списки 32-битных int

Checked file-body профиль допускает ICONSW, IHEADW и ITAIL для `list of int`.
Это следующий участок неизменённого awm.command: `fds := list of {0,1,2}`
перед `Sys.pctl`. Указатели и list roots занимают 8 байт; Limbo int и элемент
списка сохраняют ширину 4 байта. Classic opcode/operand bytes не меняются.

Компилятор проверяет тип исходного списка и назначения, включая типизированные
временные значения, FP/MP operands и immediate Word heads. Возврат hd/tl
использует существующий borrowed-result ABI (4/24). Загрузчик требует root
map для списка и отсутствие root bit для scalar Word. Обычные VM consw,
headw, tail и lenl сохраняют семантику и владение в C и JIT.

`native64-integer-lists.py` проверяет 12 полных Limbo bodies независимым
classic oracle: 60 искажений opcode/mode/operand/contract обнаруживаются.
Отдельный исходник использует настоящий `sys.m`, Sys.NEWFD и Sys.pctl;
сигнатура сверяется с генерируемыми объявлениями Sys; аргументы
исполняемого callback используют такой же 80-байтный frame.
Исполняемый callback управляемый: реальная таблица файловых дескрипторов
процесса этим тестом не изменяется.

1584 C/JIT сценария с quanta 1/3/1000 проверяют literal 0/1/2, prepend,
замену списка, FP/MP и immediate heads, hd/tl/len, nil, обе signed int границы,
borrowed tails/results, untouched соседние Word bytes, ошибки выделения,
native/module errors и полное освобождение узлов, стеков и модулей.
50 повреждений metadata отклоняются до allocation. Все 40/41 точки отказа
конструктора C/JIT откатываются. Два active controls смещают результат hd
и убирают проверку root map; оба должны нарушить смысловой predicate,
без sanitizer crash.

Gate включён в Linux amd64/arm64/riscv64, Android ARM64 Bionic workflow
и общий native64-port.py. Локальное исполнение относится к amd64;
межархитектурный CI здесь не объявляется выполненным. Nested reference
lists и cons для big остаются отрицательными случаями старого list gate.

После изменения проверка настоящего awm.command проходит PC1404 ICONSW
и останавливается на PC1457 IEXIT. Это проверка компилятора; полный AWM,
графический event loop и Android UI ещё не запущены. Следующий участок —
завершение процесса и полный admission command, затем его вызывающий init.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; allocation/ref retirement проверяется fixture).
Регрессии: String prefixes (1800), channel transfer (2592), managed list
file calls (4536 на каждом из двух источников) и полный emuinit/Arg (48).
Чистая amd64 console, paired boot и 324 настоящих C/JIT boot-процесса
с полными Arg/emuinit также прошли.
