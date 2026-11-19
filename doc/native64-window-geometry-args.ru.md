# Native64: аргументы геометрии Wmsrv.Window

Настоящий `awm.reshaped` теперь проходит подготовку аргументов
`w.r.subpt(oldr.min)` на PC765. Отказ на этом PC сменился с
`image-geometry-owner` на `external-result-kind`: следующим остаётся возврат
сырого Draw.Rect. `awm.init` пока останавливается на PC180 `multi-callee`.
Это ещё не запуск графического AWM или Android UI.

## Что допускается

Полный nominal layout `Wmsrv.Window` проверяется существующим проверяющим
кодом: classic24/native32, `tag:string`, `r:Draw.Rect`, `img:ref Draw.Image`.
Выбираются только следующие аргументы подготовленного импортируемого кадра:

| Значение | Classic offset | Native offset | Bytes | Role |
| --- | --- | --- | --- | --- |
| Window.r | 4 | 8 | 16 | 217 |
| Window.r.min | 4 | 8 | 8 | 218 |
| Window.r.max | 12 | 16 | 8 | 219 |

Владелец находится в FP или MP, его слот обязан быть GC-корнем. Назначение
обязано быть полным сырым участком кадра импортируемой функции с выравниванием4.
Все проверки метаданных происходят до выделений. Материализатор выставляет
точный native displacement и длину IMOVM; nil-владелец вызывает штатный unwind.
Чужие типы с похожими полями и чужие пространства имён не допускаются.

Допускается также Point внутри обычного локального Rect (`oldr.min/max`):
проверяется сохранённый тип выбранного выражения, затем каждый Tint leaf и его
непрерывное native размещение. Ранее проверка требовала тип всего объявления
Rect и отвергала выбранную Point. Это не разрешает произвольные aggregate.

Classic Dis и 32-битные смещения не меняются; новые роли действуют в явном
checked native64-профиле. Роли200 и216 результатов каналов/stat не заняты.
Возвраты Point/Rect, общие ADT и глобальные сырые result bindings этим этапом
не добавляются.

## Воспроизведение

```
python3 utils/tests/native64-window-geometry-args.py --limbo /path/to/limbo
```

Проверка использует настоящий typed module/linkmod, scheduler, IMOVM и C/JIT
диспетчеризацию с контролируемым импортом-приёмником. Это проверка ABI передачи,
а не выполнения арифметики libinterp/geom.c.

* 270 C/JIT запусков: FP/MP Window и локальные копии Rect/min/max, три quantum,
  четыре набора Word32 (включая signed extremes), nil-владелец, guards,
  сохранность tag/img и полное освобождение владельцев.
* 36 подмен полей записей обнаруживает независимый classic Dis oracle;
  canonical signatures/layouts и исходные Dis bytes сверяются отдельно.
* 72 повреждения opcode/role/width/span/mode/GC map/callsite отвергаются до
  выделений и сохраняют REG. Подмена opcode раньше доходила до конструктора;
  обход теперь закрыт ранней проверкой роли независимо от opcode.
* Все 36/37 точек отказа конструктора C/JIT дают полный rollback.
* Два активных контроля требуют смыслового отказа теста: неверное смещение
  Window.r и удалённая проверка карты корня. Sanitizer crash не засчитывается.

Gate включён в native64-port.py и оба пути CI native64-typed-calls.

Локально проверены UBSan и ASan+UBSan (`detect_leaks=0`; счётчики владения
проверяются явно). Регрессии: Draw fields/calls1162, Window list408,
локальная Image geometry972 и Limbo-to-Limbo geometry480 с активными
контролями. Чистая amd64 console-сборка проходит pair boot и настоящий
Arg/emuinit boot324. Arm64/rv64 выполнение и Android UI локально не проверены.
