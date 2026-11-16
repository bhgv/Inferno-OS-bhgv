# Списки внутри checked Limbo-тел

После файловых String/List-вызовов добавлены `::`, `hd`, `tl` внутри самого
потребителя, выбранного через `LIMBO_DIS64_TYPED_LOAD=1`. Они компонуются
с обычными typed инструкциями, LOAD, MP и импортными вызовами. Поддержаны
прямые возвраты `hd` и `tl` через существующий 8-байтный mapped result contract.

Это частный compiler-to-native bridge. Classic Dis остаётся byte-equal при
включении диагностического отчёта. Новые wire IDs, форматы файлов и публичный
managed ABI не вводятся. Прежние file profiles не расширены. Старый external
режим без LOAD opt-in по-прежнему отказывает новым телам со списками.

## Типы и владельцы

В новом режиме разрешён только `list of string` в прямой именованной ячейке
FP/MP. Compiler проверяет объявление списка, его element type и точное начало
ячейки; map bit не подменяет эти проверки. Cons принимает String/nil String,
head возвращает String, tail сохраняет list of String. Nested lists, scalar
cons, поля агрегатов и произвольные косвенные операнды этим допуском не охвачены.
Старый importing-client proof продолжает использовать свои прежние проверки.

Native materializer требует выравнивания 8, полного 8-байтного span, прямых
операндов, отсутствия middle operand и точного результата `24` в FP[32]. Cons
с косвенным result destination отвергается. Module builder до allocation
дополнительно связывает все прямые источники/назначения этих операций с
реальными root-картами FP/MP. Ошибка сохраняет output bytes, REG и пустой owner.

Исполнение использует существующие `consp`, `headp`, `tail`, `movp`, `freelist`
и C/JIT dispatch. Это соединение существующих операций с checked-IR lowering,
а не новая реализация JIT для этих opcodes. Владение tail передаётся через
обычный movp с retain/release; result address остаётся transient native pointer.

## Проверка

```sh
python3 utils/tests/native64-typed-lists.py --limbo /path/to/new/limbo
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-lists.py --limbo /path/to/new/limbo
```

Стенд повторно использует настоящий namespace/admission reader и file proof.
Два независимо скомпилированных провайдера Box/Other меняют имена и порядок
экспорта. Семь полных checked consumer-тел включают локальные cons/head/tail,
очистку aliases, файловые round-trip вызовы и прямые head/tail results.
Classic Dis oracle проверяет координаты, типы, кадры и импортные контракты.
Сохраняются source witnesses Command/Wm и Limbo startup из appl/cmd.

9072 C/JIT запусков (2×4536) варьируют режимы обоих модулей, рост стека,
кванты 1/3/1000, ASCII/NUL, Rune с supplementary code point, nil String,
nil List и непустой tail двухэлементного списка. Последний проверяет alias
между входным списком и возвращённым tail, включая точный refcount=2.
Observer String, входные nodes и результат полностью освобождаются.

Девять режимов I/O/admission/pin/dontcompile/no-call сохраняются. Проверяется
и порядок ошибок: локальный `tl nil` выполняется до импортного вызова;
прямые head/tail могут успешно работать с аргументом после неудачного LOAD,
поскольку они не вызывают provider. Это следует порядку обычного Limbo-кода.

12 новых no-allocation metadata refusals проверяют source/destination roles,
выравнивание, extent, middle, root maps и indirect result contract. Неверное
кодирование сохраняет bytes выходного Inst. Сохраняются прежние 13 installer
отказов и все ошибки allocation/pin файлового builder. Активные контроли
удаляют native root-map join, retain головы при cons и retain непустого tail;
немедленные наблюдения проверяют refcounts до потенциального позднего UAF.
Прежние hash/map/selector controls также выполняются.

Gate добавлен пятнадцатым для x64, ARM64, RV64 и Android ARM64/Bionic, а также
в full port driver. Старый managed file gate сохраняет 4320 прежних запусков.
Полный gate проходит локально ASan+UBSan/NDEBUG с detect_leaks=0 и шестью
активными контролями; проходят прежний managed gate, 960 owned-import
случаев, свежая полная консоль/paired boot и AWM preflight. Результаты
нового x64/ARM64/RV64/Bionic CI подтверждаются отдельно.

## Связь с AWM

emuinit готовит списки `"-l" :: nil` и `"sh" :: "-c" :: "$*" :: args`, затем
вызывает `mod->init(nil, args)` из Limbo. Этот шаг добавляет необходимые для
такой подготовки операции к checked потребителю. Полная функция loadmod
также требует tuple/module результатов, а весь startup — Sys/Draw и других
тел графа. Эти задачи остаются следующими; полный графический AWM не запущен.

Свежий AWM preflight по-прежнему компилирует 16 classic startup-модулей и
фиксирует отказ всех четырёх experimental file profiles на полном графе.
Это подтверждает сохранение границы допуска, а не исполнение AWM.
