# Native64: Point/Rect в обычных локальных CALL

Компилятор проходит подготовку и вызов `r2s(nr)` на PC791–794 настоящего
`awm.reshaped`. Следующий отказ — PC795 `indirect-or-reserved-operand`, при
подготовке `sys->sprint("!reshape %q -1 %s", w.tag, r2s(nr))`.
Сама r2s accepted. awm.init пока остаётся на PC180 `multi-callee`.
Графический AWM и Android UI пока не запущены.

## Поддержанный ABI

Передаются только nominal Draw.Point/Rect с непрерывными Tint leaves:
8/16 bytes, alignment4, без GC-корней. Для каждого leaf компилятор вычисляет
назначение через раскладку кадра локального callee. Правила упаковки импортов
сюда не переносятся: соседние Word/Big/String и смещения корней могут отличаться.

Допускается один исходный classic IMOVM из FP/MP либо выбранного поля
Image.r/clipr или Wmsrv.Window.r и Point-половин. Тип прямого выражения
должен совпасть с формальным типом callee; каждый исходный Tint leaf и каждый
native destination offset проверяются отдельно. Classic Dis bytes остаются
побайтово прежними. Массивы, общие ADT, scalar-store aggregate literals и
SPAWN arguments этим этапом не добавляются.

Builder сопоставляет инструкцию одному локальному CALL или существующему
импортному вызову, проверяет frame/call opcode и callee GC-map до выделений.
Проверяются полный сырой диапазон источника, корень indirect-владельца и
полный сырой диапазон назначения с учётом alignment4. Подмена IMOVM на другой
opcode с сохранённым AXIMM/argument shape тоже отказывает до выделений.
Материализатор использует обычный pending frame40 и длину IMOVM8/16;
результаты String продолжают идти через существующие fresh/global/inherited
контракты, без нового result ABI. 32-битные layout и инструкции не меняются.

## Проверка

```
python3 utils/tests/native64-local-geometry-args.py --limbo /path/to/limbo
```

Тест исполняет настоящий builder, scheduler, C/JIT, IMOVM и локальные Limbo
функции. Тело r2s берётся непосредственно из appl/wm/awm.b. Конверсия Word в
String и сложение строк берутся из libinterp/string.c, форматирование чисел —
из lib9. Контролируемого formatter/provider вместо этих тел нет; служба rerrstr
для неиспользуемого пути fmt %r ограничена fixture-строкой.

* 240 C/JIT запусков: FP/MP Rect, selected Point, Window.r через FP/MP корень,
  Image.r, цепочка wrap/r2s с inherited String result, смешанные Word/Rect/
  Word/Point/Big/String, три quantum, signed Word32 extremes и nil-владельцы.
  Проверяются точный десятичный String, порядок всех mixed аргументов,
  guard bytes, сохранность Window/Image и полный retirement владельцев.
* 66 подмен полей аргументов обнаруживает независимый classic Dis oracle;
  отдельно сверяются source/callee leaves, GC-карты, canonical signatures и
  неизменность classic bytes.
* 152 повреждения width/span/map/mode/opcode/callsite отказывают до выделений
  с сохранением REG. Проверки карт включают независимые scratch-участки
  caller и callee, не используемые другими инструкциями.
* Все 32/33 точки отказа конструктора C/JIT дают полный rollback.
* Три активных контроля: короткая копия Rect, пропущенная callee raw map и
  пропущенная caller raw map. Требуется смысловой отказ, sanitizer crash
  не засчитывается.

Gate включён в native64-port.py и оба пути CI native64-typed-calls.

Локально прошли UBSan и ASan+UBSan (`detect_leaks=0`; владельцы проверяются
явными счётчиками), включая все три активных контроля. Регрессии:
импортные geometry arguments480, geometry results360 и Window Image results900
со своими активными контролями. Чистая amd64 console-сборка проходит pair boot
и настоящий Arg/emuinit boot324. Arm64/rv64 выполнение и Android UI локально
не проверялись.
