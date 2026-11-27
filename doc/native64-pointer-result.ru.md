# Native64: millisec в поле Pointer и передача события

Настоящий handlerequest проходит PC438–440: Sys.millisec записывает результат
в p.msec. Следующая отправка нового Pointer в c.ptr на PC441 тоже проходит
checked-компиляцию. Новый отказ — PC506 multi-callee на fitrect; сама fitrect
останавливается на aggregate-result, как и newrect. Это следующий участок
с возвращаемым Draw.Rect. reshape, setfocus, controlevent и reshaped приняты
целиком. awm.init пока остаётся на PC303 multi-callee, поскольку handlerequest
ещё не принят целиком. Окно AWM и Android UI ещё не запущены.

Новая private role245 разрешает результат импортированной fn():int прямо
в msec канонического Draw.Pointer. Ссылка владельца должна находиться
в локальном mapped FP root8; поле msec имеет offset12 и ширину Word32.
Nominal layout Pointer/Point и все поля проверяются по compiler IR.
Глобальный владелец и произвольные ADT не открываются. Локальная ссылка
сохраняется в приостановленном caller frame на всё время вызова; глобальную
ссылку другой процесс мог бы заменить.

Материализация использует существующий private LEA binder с contract4
и field marker512. Runtime отдельно проверяет восемь байтов owner slot
и четыре байта результата. Проверяются caller/callee Types и GC maps,
геометрия pending frame, положительные ссылки heap-владельца, payload16,
отсутствие GC-полей и точный offset12. Допускаются обе эквивалентные формы
raw Type: пустая map и один нулевой байт map, как у generated native ABI.
Nil владелец даёт штатный exNilref до вызова clock handler; неверная
геометрия даёт exBounds. Для существующего Window.img сохраняются его
проверки payload32/map0x90 и восьмибайтный результат.

Module preflight проверяет роль245, source root8, принадлежность ровно
одному внешнему call site, contract4 и отсутствие inherited result.
Дополнительно indirect LEA в подготовке результата обязан иметь одну из
двух разрешённых field roles. Подмена роли на обычный scalar result
отвергается до первого выделения, а не только при материализации модуля.
Произвольный native файл и shared FlashVM wire ABI этим не открываются.

native64-pointer-result.py исполняет реальный Sys_millisec из inferno.c
с контролируемым osmillisec, production channel allocation/send/receive
и production Pointer allocation. Проверяются прямой/локальный/повторный
вызовы, новый объект, буферизованная передача существующего и нового
Pointer, int32 границы, отказ clock до записи, nil владелец, повреждённые
heap size/map, caller и heap guards, сохранение остальных полей и полное
освобождение объектов/канала. Проверки проходят в C и JIT с разными квантами
и переходом на новое расширение стека. Исполнение настоящего blocking
пути AWM целиком здесь не заявляется.

Независимый oracle проверяет classic инструкции, descriptor/map/hash и
каждый компонент новых result bindings. Metadata повреждается до создания
модуля; все точки отказа конструктора проверяются с полным rollback.
Активные контроли меняют смещение поля, проверку heap map, GC root и ширину
записи настоящего Sys_millisec. Тест включён в native64-port.py и обе
секции typed-calls CI. Classic Dis и 32-битный код не меняются.

Локально amd64: 864 C/JIT сценария под ASan+UBSan, 96 metadata refusals
до выделения, 42 повреждения независимого oracle, 29/30 точек отказа
конструктора C/JIT модуля и четыре активных контроля. Регрессии проходят:
Window Image results900, geometry results360, Pointer allocation/store384.
Сборка с чистого дерева native64-port --emuinit-boot --pair-boot-only
успешна; реальный неизменённый Arg/emuinit проходит 324 C/JIT процесса.
Эти локальные результаты относятся к amd64; новое выполнение на ARM64,
RV64 и Android здесь не заявляется.
