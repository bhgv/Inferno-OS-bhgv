# Подготовка startup argv в checked Limbo

Checked потребитель теперь сам готовит аргументы перед файловым Command/Wm.init:
при nil-списке создаёт `"-l" :: nil`, иначе — `"sh" :: "-c" :: "$*" :: args`.
Это строки и структура списков из appl/cmd/emuinit.b; сохранён явный вызов
`p->init(ctxt, args)` из Limbo. Тест не запускает весь emuinit или AWM.

Для выбора веток добавлены сравнения с nil для прямых `list of string` и
Module-ячеек FP/MP в режиме `LIMBO_DIS64_TYPED_LOAD=1`. Compiler проверяет
объявление, element type списка, начало ячейки и тип реальной layout leaf.
Второй операнд — compiler-owned readonly Tany MP root. Equality/inequality
понижаются в существующие IBEQL/IBNEL, читающие все восемь байт ссылки.
Поддержаны nil слева и справа и обычные локальные/глобальные ячейки.
Nested lists, arrays, другие ref-классы и aggregate dereferences этим путём
не допускаются. Старый external-only режим сохраняет отказ таких тел.

Fallback typed lowering теперь повторно рассматривает opcode-type refusal
при целых корректных layout в явно выбранном LOAD-режиме, чтобы распознать
эти reference branches. Остальные инструкции снова проходят прежние role/
span checks. File admission profiles и classic Dis bytes не изменяются.
Новых wire IDs, публичного managed ABI и 32-битных изменений нет.

## Литералы, MP и вызовы

Четыре строки берутся из настоящего classic DATA, независимо переводятся
через layout leaves и создаются существующим Native64ModuleString owner.
Стенд теперь использует production c2string и lib9 Rune helpers, как и
настоящие newstring/newrunes. Исходные bytes после конструкции не нужны.

Потребитель имеет четыре полных тела: open, run, ready, clear. Он делает
две свежие LOAD, проверяет ready по nil-модулю, дважды готовит argv и при
каждом run дважды явно вызывает init. Prepared list остаётся в mapped MP.
Второй run заменяет прежний список; сохраняются исходный хвост, Context,
точные String refs и изоляция исходного MP. Очистка освобождает все узлы.

Провайдеры — ограниченные source-файлы Command/Wm, а не настоящие AWM-тела.
Они проверяют полученные длины списков/элементов и потребляют aliases.
Отдельный Wm.init всегда делит на ноль: LOAD не вызывает его автоматически.
Для Wm исходный consumer меняет знак nil-сравнения и ставит nil слева,
проверяя оба направления branch и перестановку операндов.

## Проверка

```sh
python3 utils/tests/native64-typed-argv.py --limbo /path/to/new/limbo
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-argv.py --limbo /path/to/new/limbo
```

3888 C/JIT запусков (3×1296) варьируют оба режима, рост стека, кванты
1/3/1000, ASCII/NUL и Rune исходного argv, nil-список, nullable Context,
две загрузки и девять I/O/admission/pin/dontcompile/no-call режимов.
C observer проверяет точные байты `-l/sh/-c/$*`, порядок трёх prefix nodes,
тождество исходного tail, повторную MP publication, ready=0/1 и охранные
байты вокруг четырёхбайтного результата ready.

Независимый oracle доказывает точное преобразование 32-битного classic
reference branch в IBEQL/IBNEL, перестановку равноправных operands, nil/root
maps и relative target. Затем общий classic oracle проверяет остальные
координаты, кадры и import contracts. Повреждённый narrow report отвергается; четыре обычных well-typed исходника
с nested-list/array/ref/module-pair сравнениями отказывают новому допуску.
Classic bytes с диагностикой и без неё совпадают.

Конструктор проверяет 26 C / 27 JIT allocation failures, полное освобождение
и неизменность REG; отдельный успешный случай портит входные literal buffers
после создания owner и проверяет сохранность String bytes. Прежние 17
installer refusals и три активных Command controls сохранены. Новый контроль
заменяет MP publication безопасным self-move: init продолжает работать,
а observer обязан обнаружить отсутствие подготовленного списка в MP.

Gate включён шестнадцатым в x64/ARM64/RV64/Android ARM64 Bionic и full port
driver. Полный локальный gate проходит ASan+UBSan/NDEBUG с detect_leaks=0;
прежние 3888 Command, 9072 list и 6528 LOAD запусков с их контролями
и свежая console/paired boot также проходят. Эти bounded-прогоны
следует отличать от полного GUI VM и новых target CI receipts. Прежние Command/list gates остаются отдельными
регрессиями. Полный AWM требует дальнейшей поддержки tuple/module результатов,
остальных source-тел и реальных Sys/Draw/GUI связей.

Следующий участок: [приватные Module/List results и вложенные LOAD helpers](native64-typed-local-loads.ru.md).
