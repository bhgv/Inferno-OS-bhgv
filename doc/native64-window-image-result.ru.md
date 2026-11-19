# Native64: импортный Image прямо в Window.img

Компилятор проходит запись `w.img = screen.newwindow(...)` и следующий
`w.r = nr` в настоящем `awm.reshaped`. Следующий отказ — PC791
`multi-argument`: Rect передаётся локальной `r2s` внутри форматирования
сообщения reshape. `awm.init` остаётся на PC180 `multi-callee`.
Сама r2s уже accepted; остаётся подготовка её by-value аргумента в caller.
Графический AWM и Android UI пока не запущены.

## Точный допуск и владение

Новая роль220 разрешена только для последнего ILEA в подготовке импортного
вызова с contract24. Компилятор доказывает возвращаемый `ref Draw.Image`,
полный nominal `Wmsrv.Window`, classic поле img20 и native поле img24.
Владелец должен находиться в локальном FP GC-корне. Builder повторно проверяет
форму, смещения, GC-карту владельца и единственный callsite до выделений.

Материализатор публикует адрес поля через специальный ILEA с register536
(`512|24`), source `FP indirect`, destination `pending-frame.ret` по offset32.
Runtime binder проверяет корень локального владельца, полный живой heap Type
Window32/map90, nil, размер и границы поля, затем сверяет адрес с vmdecode.
Обычные direct/global bindings и ALT address не получают это разрешение.
Classic Dis bytes и 32-битные смещения остаются прежними.

Локальный владелец остаётся GC-корнем кадра до завершения вызова, даже при
смене stack extent или приостановке. Это удерживает Window и адрес его img.
MP/global владельцы пока закрыты: их общая ссылка может измениться во время
вызова, и для них потребуется отдельное удержание владельца. Также закрыты
чужие ADT/namespace, произвольные поля и indirect local-call результаты.

Сама публикация Image следует существующему контракту производителя: сначала
вписать новое значение, затем освободить прежнее. Binder не меняет refcount
и не очищает img. Ошибка до публикации сохраняет прежнюю ссылку. nil и
возврат того же Image проверяются отдельно. При втором вызове ошибка должна
сохранить результат первого вызова.

## Проверка

```
python3 utils/tests/native64-window-image-result.py --limbo /path/to/limbo
```

Тест использует настоящий builder/linkmod, scheduler, IMOVP/IMOVM,
vmdecode и C/JIT binder. Импорт Screen.newwindow контролируемый: проверяет
canonical Frame64, точный адрес Window.img и аргументы, создаёт реальные
Heap/Type Image, заменяет/освобождает ссылки либо вызывает заданную ошибку.
Это не выполнение графического device.

* 900 C/JIT случаев: прямой/локальный alias/два последовательных вызова,
  три quantum, Word32 extremes, исходный img как Image или nil, fresh/nil/alias
  результаты, ранняя/поздняя ошибка и отказ выделения, nil Screen/Window,
  runtime heap-map/size/map-length повреждения, guards и полный retirement.
* 24 подмены binding-полей обнаруживает независимый classic Dis oracle;
  canonical signatures/layouts и неизменность Dis bytes сверяются отдельно.
* 51 повреждение opcode/role/offset/span/map/mode/callsite отклоняется до
  выделений, включая owner slot в отдельном неиспользуемом scratch-участке.
* Все 22/23 точки отказа конструктора C/JIT дают полный rollback.
* Три активных контроля: неверный field displacement, пропущенный heap img
  root и пропущенный owner slot root. Требуется смысловой отказ проверки;
  sanitizer crash не засчитывается.

Gate включён в native64-port.py и оба пути CI native64-typed-calls.
Контроли pair/triple binder переведены на вычисленный source offset;
их смысл — обязательная проверка второго/третьего корня — сохранён.

Локально прошли UBSan и ASan+UBSan (`detect_leaks=0`, владельцы проверяются
явными счётчиками), включая поздние ошибки второго вызова и три активных
контроля. Регрессии: geometry results360, channel triples400, emuinit/fstat,
полный argv/local/pair file Command gate и Draw fields/calls1162 со своими
активными контролями. Отдельный Draw owner gate выполняет реальные фабрики,
включая Screen.newwindow, с ограниченными native/device services; проходит
72825 predicates, 153 factory scenarios и 556 window/I-O случаев, ledgers zero.
Чистая amd64 console-сборка проходит pair boot и настоящий Arg/emuinit boot324.
Arm64/rv64 выполнение и Android UI локально не проверялись.
