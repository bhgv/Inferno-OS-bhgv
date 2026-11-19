# Native64: возврат Draw.Point и Draw.Rect из импортов

Настоящий `awm.reshaped` проходит `w.r.subpt(oldr.min)`, масштабирование
четырёх координат и `nr.addpt(newr.min)`. Следующий отказ — PC783
`external-inherit`: результат `screen.newwindow(...)` направлен прямо в
`w.img`, а такой indirect result binding пока закрыт. `awm.init` остаётся
на PC180 `multi-callee`, пока `reshaped` не принят целиком. Графический AWM
и Android UI этим этапом не запускаются.

## Контракт результата

| Результат | Contract | Binding role | Размер | Выравнивание | GC roots |
| --- | --- | --- | --- | --- | --- |
| Draw.Point | 96 | 224 | 8 | 4 | нет |
| Draw.Rect | 112 | 240 | 16 | 4 | нет |

Компилятор доказывает nominal тип Draw.Point/Rect, точный размер и непрерывные
Tint leaves локального назначения. Legacy ILEA получает native адрес,
публикуемый в ret-слот подготовленного кадра по offset32. Classic Dis
остаётся побайтово прежним, изменения включены только в checked native64.

Builder требует совпадения импортного resultcontract и callsite contract,
точную форму ILEA, полного сырого диапазона FP и отсутствие GC-корней во
всём диапазоне. Orphan роли224/240 отказываются до выделений.
Материализатор знает полную ширину результата и выравнивание4. Runtime binder
повторно проверяет диапазон и карту до публикации адреса. В contract112
есть бит16, но результат остаётся сырым, а не ссылочным: старое трактование
этого бита явно обходится только для двух новых контрактов.

Новые контракты не допускаются для ALT address, local Limbo aggregate
returns, inherited ret pointers, MP/global или indirect destinations.
Существующие контракты48/72/88 (pair/channels/stat) сохраняются.
Поддержка произвольных ADT не добавлена.

## Проверка

```
python3 utils/tests/native64-geometry-results.py --limbo /path/to/limbo
```

Тест генерирует canonical C frames и signatures из draw.m. Native provider
исполняет настоящие тела `Point.add/sub/mul` и `Rect.addpt/subpt/inset` из
`libinterp/geom.c`, извлечённые по полным определениям функций. Добавлен
только счётчик входов. Контролируемого вычислителя вместо этих тел нет.
Ожидания вычисляются отдельно в unsigned Word32 арифметике.

* 300 обычных C/JIT запусков: отдельные функции, Window.r/min/max, цепочка
  subpt/addpt, три quantum, signed extremes и nil-владельцы. Каждый local
  result расположен по адресу4 modulo8; проверяются соседние MP bytes,
  caller guards, сохранность Window/tag и полный retirement.
* 60 runtime fault запусков: карта первого/последнего слота и адрес, выходящий
  за конец кадра. Требуется exBounds до входа в geom.c, без публикации результата.
* 110 подмен metadata/binding полей обнаруживает независимый classic oracle;
  canonical signatures/layouts и неизменность Dis bytes проверяются отдельно.
* 171 повреждение contract/role/mode/span/map/opcode/site отвергается до
  выделений с сохранением REG. Проверка хвоста диапазона включает отдельный
  сырой scratch-участок, не используемый последующими инструкциями.
* Все 46/47 точек отказа конструктора C/JIT дают полный rollback.
* Три активных контроля: Rect ошибочно объявлен ссылкой, проверен только первый
  Word результата, адрес результата сдвинут на4. Требуется смысловой отказ
  проверки, sanitizer crash не засчитывается.

Gate включён в native64-port.py и оба пути CI native64-typed-calls.

Локально прошли UBSan и ASan+UBSan (`detect_leaks=0`; владельцы проверяются
явными счётчиками), включая все три активных контроля. Регрессии:
Window geometry args270, Limbo geometry args480, Draw fields/calls1162,
channel triples400 и emuinit/fstat со своими активными контролями.
Чистая amd64 console-сборка проходит pair boot и настоящий Arg/emuinit boot324.
Arm64/rv64 выполнение и Android UI локально не проверялись.
