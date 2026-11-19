# Native64: создание Draw.Pointer и заполнение координат

Настоящий handlerequest из appl/wm/awm.b проходит PC427 INEW, запись
buttons на PC428 и String-to-Word для xy.x/xy.y на PC431/436.
Новый отказ — PC438 external-inherit: подготовка Sys.millisec() с
результатом прямо в p.msec. awm.init остаётся на PC303 multi-callee,
пока handlerequest не принят целиком. setfocus, controlevent и reshaped
по-прежнему accepted. Это результат checked-компиляции; графический
AWM и Android UI ещё не запущены.

| Role | Операция | Геометрия |
| --- | --- | --- |
| 242 | INEW/INEWZ канонического Draw.Pointer | raw16, без GC-полей, результат root8 FP |
| 243 | IMOVW в buttons/xy.x/xy.y/msec | FP/MP Word32 или immediate, owner root8 FP/MP |
| 244 | ICVTCW в эти поля | String root8 FP/MP, owner root8 FP/MP, запись Word32 |

Проверяются nominal Draw.Pointer, имена/порядок/типы всех полей, вложенный
Draw.Point, classic/native размеры16/16 и четыре leaf32. Конструктор
требует исходный descriptor16 без pointer map. Native Type для Pointer
добавляется отдельно от Draw.Context; Context сохраняет свою позицию
последнего дескриптора. INEW не обещает инициализацию неопределённых
скалярных полей; для INEWZ сохраняется обнуление.

До выделения модуля проверяются opcode/modes, границы root8 и Word32,
GC map владельца/строки, отсутствие GC-битов у скалярного источника,
offset0/4/8/12 и отсутствие этих операций внутри подготовки CALL.
Запись сохраняет штатную проверку nil владельца и штатный VM unwind.
Конвертация использует существующий OP(cvtcw), без нового парсера.
GC map не доказывает nominal тип произвольного heap-объекта: этот premise
поступает из проверенного compiler IR. Загрузка произвольного native
файла не открывается.

native64-pointer-new.py проверяет default/zero allocation, инициализацию
константами и всеми четырьмя Word32 из другого Pointer, FP/MP владельцев,
FP/MP источники Word32/String, замену объекта, nil unwind, отказ выделения
объекта, независимость дескрипторов Pointer/Context, сохранение caller
guards и полное освобождение владельцев. Отдельно повреждаются metadata
и каждый компонент новых записей независимого classic-byte oracle.
Активные контроли намеренно меняют выбранное поле, размер дескриптора,
проверку GC root и ширину записи конвертации; тест обязан их обнаружить.
Тест включён в native64-port.py и обе секции typed-calls CI.

Classic Dis и 32-битные ABI не меняются. Прямой импортированный результат
в Pointer.msec и завершение настоящего канального пути handlerequest
остаются следующим этапом; их выполнение здесь не заявляется.

Локальная проверка amd64: 384 C/JIT сценария под ASan+UBSan, 298 отказов
metadata до выделения, 216 повреждений независимого oracle, 30/31 точек
отказа при создании C/JIT модуля и четыре активных контроля. Регрессии:
Context creation/copy318 и String-to-Word1152; сборка с чистого дерева
native64-port --emuinit-boot --pair-boot-only проходит. Проверки чтения
Pointer дополнительно сохраняют отказ неразрешённых сравнений; запись
p.buttons=3 теперь разрешена явно. В их активном контроле GC map поиск
фрагмента уточнён до роли228, поскольку одинаковая строка встречается
в нескольких проверках владельцев.
Реальный неизменённый Arg/emuinit в собранном emu проходит 324 C/JIT
процесса, включая checked Command.init и fallback shell, без окна AWM.
