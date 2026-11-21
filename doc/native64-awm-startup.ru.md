# Общий native64 запуск emuinit → AWM

`native64-awm-startup.py` впервые исполняет прямую цепочку настоящего
`appl/cmd/emuinit.b` и полного `appl/wm/awm.b` в одном runtime-графе.
Связаны девять исходных модулей: Emuinit, Arg, AWM, String, Wmsrv,
Wmclient, Winplace, Wmlib и Titlebar — 161 функция без изменения их тел.
Все тела создаются; проверка не утверждает, что каждая ветвь всех 161 функций
исполняется при старте.

```
python3 utils/tests/native64-awm-startup.py --limbo /path/to/host/limbo
```

Каждый модуль проходит независимую проверку classic-операндов, таблиц,
экспортов, frame maps и сохранения прежних Dis bytes. Для каждого LOAD
проверяется обнаружение подмены ordinal его таблицы. Генератор различает
одинаковые имена и сигнатуры `init()` по mapped module operand и его
исходному LOAD, а не выбирает первое совпадение.

В C и JIT, с quanta 1/3/1000 проверяются:

* 24 успешных LOAD через настоящий linkmod и проверку сигнатур/карт кадров;
* исходный Arg, nil Context и переход emuinit напрямую в AWM.init;
* исходные Wmclient/Wmlib/Titlebar/Winplace init, создание Context,
  plain Window, двух Screen, window Image и девять вызовов рисования;
* исходные kbdproc, ptrproc, wptrproc и sendreq: UTF-8 клавиша,
  49-байтовый pointer packet и сообщение `haskbdfocus 1`;
* исходный Wmsrv.init и его бесконечный wm-server: два запроса одного fid,
  новый/существующий клиент и точные ответы `-17`;
* исходный ALT AWM: чтение `/chan/wmrect` возвращает `0 0 640 480`,
  смещение за концом даёт пустой массив; запись возвращает `cannot write`;
* исходный command с отказом Sh, повторным LOAD `/dis/wm/atoolbar.dis`
  и передачей дочернему приложению настоящих Context/display/wm/argv;
* завершение четырёх конечных процессов и ожидание трёх бесконечных циклов
  именно в AWM, Wmsrv и Wmlib; их явная отмена через штатный ALT cleanup;
* освобождение всех семи процессов, module links, frames, GC pins,
  FileIO-запросов/ответов, графических объектов и Type references.

Sys/Draw/Tk и дочернее приложение здесь управляемые. Геометрия, formatter,
UTF-8 conversion, клонирование module data, VM handlers, каналы, scheduler,
JIT, native binders и все перечисленные Limbo-модули — код репозитория.
Службы Display/Screen/Image подтверждают ABI, владельцев и параметры;
пиксели в настоящее окно этот gate не выводит. Дочерний atoolbar пока
заменён контролируемым init. Это ещё не GUI boot и не запуск Android.

Полный Wmlib выявил недостающий случай `clients[sc.id]`: после адресации
по Client.id разрешено немедленное чтение, как и запись mapped reference.
Обе инструкции остаются одной защищённой парой с проверкой root maps и
запретом входа в её хвост. 36 повреждений этих пар отклоняются до выделения
памяти; полный Wmlib проверяется на откат каждого выделения C/JIT.
Дополнительно исполняемый Wmlib snarf/control gate использует обе операции
по настоящему `Wmsrv.Client.id`, а не только по скалярному индексу.

Обычный file admission, Sys registry, существующий выбранный emuinit boot
и 32-битные пути не расширены. Для настоящего окна следующий шаг —
перенести связку из проверочного LOAD dispatcher в выбранный boot с
реальными builtin graphics/device services, затем подключить панель и
Android arm64. Gate включён в native64-port и оба CI-набора typed calls.

Локально на amd64 прошли ASan+UBSan (`detect_leaks=0`: LeakSanitizer
недоступен под ptrace этой среды; освобождение дополнительно проверяется
счётчиками allocations, Type refs и GC pins). Новый gate обнаруживает
отключение quote formatter и восстановление прежнего отказа чтению
Client[id]. Полный Wmlib проверен на 195/196 точек отказа выделения C/JIT.
Повторно пройдены Wmlib snarf/control, Wmsrv и Wmclient regression gates.
Arm64/riscv64 здесь добавлены в существующие CI-маршруты; их локальное
исполнение не заявляется.

При регрессии Emuinit обновлена устаревшая raw-IR фикстура variadic test:
она явно предоставляет nil implementation-module и запрещающий неожиданные
nominal-сравнения tequal, а при замене descriptor operand на module operand
сбрасывает старый offset. Полный source compiler продолжает использовать
настоящие impdecl/tequal; production-проверка совпадения module operand
не ослаблена.
