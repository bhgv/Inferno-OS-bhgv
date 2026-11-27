# Native64: подготовка вызовов оконного клиента

Позднейший результат: [полный Wmclient 21/21 и сквозное создание окна](native64-wmclient-module.ru.md). Ниже описан предыдущий этап.

Одним блоком добавлены три формы compiler IR, на которых останавливался
исходный Wmclient. Classic Dis и 32-битные пути сохраняются.

* `prefix + request[start:]` в аргументе внешнего вызова: ILENC, сохранение
  строки во временный root, ISLICEC, конкатенация в аргумент и nil clear.
* `hd devices` как String tail variadic sprint/aprint/fprint. Канонический
  list of string подтверждается compiler type; формат не определяет тип.
* Фиксированный FD аргумент fprint через `Window.ctxt.connfd`: полный
  nominal Window, полный семипольный Draw.Wmcontext, точные смещения
  classic 40/20 → native 48/40 и очистка промежуточного mapped root.

Частная аннотация 87 обозначает первые три шага String slice preparation;
существующая проверка bounded concat/clear завершает последовательность.
Module builder проверяет исходный String root, сырой length/start, отдельный
String temporary, связи между операндами, единственный external call-site,
карты аргумента и readonly nil clear. Отдельные фрагменты не принимаются.

Аннотация 58 обозначает IHEADP прямо в проверенный String tail слот.
Аннотация 59 — всю тройку Window→Wmcontext, Wmcontext→FD argument, nil clear.
Sys module builder проверяет порядок, operation, fixed/tail позицию, все
карты и границы, единственную variadic site и запрет входящих ветвлений.
Существующий gateway удерживает скопированный FD до синхронного вызова.
Старый channel helper Wmcontext по-прежнему ограничен первыми пятью полями;
расширенная адресация двух ref полей выделена отдельно.

## Исполнение и проверки

Новый `native64-call-prep.py` включён в native64-port и оба задания
native64-typed-calls CI. Fixture содержит шесть checked bodies, два import
модуля и три variadic site. Import frames/signatures получаются из
сгенерированных Sys/Sink/Wmclient headers, classic bytes сверяются.

180 C/JIT прогонов с квантами 1/3/1000 покрывают FP/MP источники String и
list, обычную, пустую и nil строку, nil список/контекст и ошибку границ.
Используются реальные VM head/slice/concat и реальный Sys/lib9 sprint с %q.
Sink и fprint — контролируемые сервисы: проверяются exact payload, FD,
результат 37, refcounts и очистка контекстного temporary до fprint.
Настоящая запись в файловый дескриптор этим тестом не проверяется.

Source oracle обнаруживает 66 изменений новых инструкций и ещё 30
искажений variadic records. Metadata gate отвергает 77 opcode/mode/operand/
annotation/map вариантов до выделения памяти. Конструктор проходит все
48/49 точек отказа C/JIT с атомарным rollback. Активные контроли подменяют
нижнюю границу slice, передают list вместо head и меняют Window.ctxt offset.
Foreign FD owner и namespace отказываются.

На amd64 пройдены UBSan и ASan/UBSan (detect_leaks=0, собственный полный
retirement включён), прежние String preparation, formatter fields,
Wmclient geometry/tuples, Wmsrv и AWM regression gates, чистая console
сборка с pair/emuinit boot. Arm64/riscv64 локально не прогонялись.
Сохраняется прежняя граница Sys gateway: caller и pending callee должны
находиться в одном stack extent; тест заранее резервирует стек.

## Что осталось до окна AWM

Полный Wmclient пока сохраняет 9/21 принятых тел, но три первых отказа
пройдены: startinput дошёл до wmctl (PC 473), wmreq — до recvimage (PC 682),
wmctl — до следующего составного String аргумента size/sendctl (PC 525).
Следующие общие зависимости — несколько String temporaries в аргументе,
image/cursor вызовы и putimage/drawborder. Настоящее окно AWM и Android GUI
ещё не запущены. Принятие всего исходного AWM 20/20 не означает его полного
исполнения с оконными библиотеками.
