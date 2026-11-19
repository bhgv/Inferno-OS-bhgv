# Native64: list of Rect и прямой MP-результат геометрии

Настоящая newrect из appl/wm/awm.b принимается checked-компилятором целиком.
Она проходит CONSM на PC1340, список прямоугольников из Window.r, вызов
Rect.size, Winplace.place с результатом прямо в lastrect и возврат Rect.
handlerequest проходит PC521–543; новый отказ — PC544 load-nil-roles на
c.wins == nil. awm.init пока остаётся на PC303 multi-callee, поскольку
handlerequest ещё не принят целиком. Окно AWM и Android UI ещё не запущены.

| Private role | Операция | Проверенная форма |
| --- | --- | --- |
| 248 | CONSM16 | Rect FP/MP или канонический Window.r; List root8 FP/MP |
| 249 | HEADM16 | List of Rect root8 FP/MP; raw Rect FP/MP |
| 250 | TAIL | List of Rect root8 FP/MP в List root8 FP/MP |

Проверяются nominal Draw.Rect, четыре Word32 leaf и полный layout
Wmsrv.Window для косвенного источника. Поле Window.r имеет native offset8.
Point, произвольные ADT, borrowed result и запись головы прямо в heap field
этим путём не открываются. В payload List узла находятся ровно16 raw байтов;
GC не считает их ссылками. Tail остаётся полноценной native ссылкой.
Используются существующие production cons/consm/headm/tail и освобождение
List. Nil Window/List даёт штатный unwind; ошибка выделения не публикует
неполный узел и сохраняет старый хвост.

До создания модуля проверяются opcode/modes, width16, полные границы
raw диапазонов, owner/List root8 и GC maps. Raw поля не могут пересекаться
с GC slots. Операции не допускаются внутри подготовки CALL. Для CONSM
и HEADM обязательна соответствующая private role. Nominal тип поступает
из доверенного compiler IR; GC map сам по себе его не доказывает.

Импортированный Point/Rect результат теперь может записываться напрямую
в mutable MP storage. Используются существующие contracts96/112 и
roles224/240 с modeAMP. Проверяются весь raw диапазон, выравнивание4 и
границы MP; readonly константы, inherited result и глобальный результат
локального CALL остаются закрытыми. Адрес MP устойчив на время вызова:
caller Module/Modlink удерживается кадром. Runtime binder проверяет Type
активной MP области и каждый её GC slot; значение не принимается за ref.

native64-rect-list.py исполняет неизменённую newrect, извлечённую из awm.b,
с настоящим Rect.size и контролируемыми Wmsrv.top/Winplace.place.
Проверяются обход двух Client с тремя Window, обратный порядок прямоугольников,
канонические аргументы place и точный MP-адрес lastrect. Сам алгоритм
Winplace.place этим тестом не исполняется. Отдельные функции проверяют
FP/MP/Window источники, head/tail, разделяемый хвост, пустые списки, nil
владельцев, ошибку выделения в середине newrect и ошибку place до записи.
Проверяются caller/MP guards и полное освобождение List/Client/Window.

Расширенный native64-geometry-results.py исполняет девять дополнительных
прямых MP-результатов настоящих geom.c методов, включая Rect/Point,
Window geometry и знаковые границы. Проверка отказа metadata использует
изолированный MP producer: другие функции не маскируют пропущенную проверку
его result map. Для List destination root есть аналогичный изолированный
случай. Независимый classic-byte oracle проверяет новые записи; активные
контроли меняют поле Window.r, ширину head copy и GC proof. Оба набора
включены в native64-port.py и typed-calls CI.

Classic Dis, 32-битный код и shared FlashVM wire ABI не меняются.

Локально amd64 под ASan+UBSan проходят 330 List/newrect и 684 geometry
C/JIT сценария (второй набор включает прежние FP и новые MP результаты).
List gate обнаруживает 48 повреждений oracle, 81 metadata отказ до выделения,
39/40 точек отказа конструктора и три активных контроля. Geometry gate
проверяет 243 metadata отказа до выделения и три активных контроля.
Регрессии: local geometry results576 и Window lists/fields408. Чистая сборка
и реальные неизменённые Arg/emuinit проходят 324 C/JIT процесса. Эти локальные
результаты относятся к amd64; новое выполнение на ARM64/RV64/Android здесь
не заявляется.
