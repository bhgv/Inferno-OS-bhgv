# Проверенная граница startup-модулей

Текущий checked LOAD preflight принимает **236 из 380 функций** в 16 исходных
startup-модулях. Для каждой ordinary-компиляции с `-i` classic Dis побайтно
совпадает с результатом компиляции при включённых native64 diagnostics.

| Исходник | Принятые тела |
|---|---:|
| appl/cmd/emuinit.b | 4/4 |
| appl/wm/awm.b | 20/20 |
| appl/wm/atoolbar.b | 12/26 |
| appl/lib/arg.b | 8/8 |
| appl/lib/bufio.b | 27/27 |
| appl/lib/env.b | 6/7 |
| appl/lib/filepat.b | 0/6 |
| appl/lib/readdir.b | 1/8 |
| appl/lib/string.b | 21/21 |
| appl/lib/wmsrv.b | 34/34 |
| appl/lib/wmclient.b | 21/21 |
| appl/lib/winplace.b | 21/21 |
| appl/lib/wmlib.b | 25/25 |
| appl/lib/titlebar.b | 7/7 |
| appl/lib/tkclient.b | 7/16 |
| appl/cmd/sh/sh.b | 22/129 |

Команда для воспроизведения:

```
python3 utils/tests/awm-native64-preflight.py --limbo /path/to/limbo --checked-only --report /path/to/report.json
```

Отчёт содержит SHA classic bytes, причины отказов, PC/opcode/операнды,
прочитанные независимо из classic Dis, и контекст подготовки FRAME.
Одинаковые имена методов разных ADT различаются по паре name/first.
Sanitizer/crash не считается обычным отказом функции.

Путь запуска остаётся `emuinit → /dis/wm/awm.dis → mod->init(nil,args)`.
Shell не является обязательной промежуточной стадией запуска AWM.
Проверка исходных тел сама по себе не устанавливает native-модули в этот
LOAD-граф и не исполняет AWM.init. Исполняемые C/JIT-gates проверяют отдельные
полные модули и связанные пути; [Wmlib](native64-wmlib-snarf.ru.md) теперь
проверяет wmctl/cursor/input вместе с каналами и владением объектами.

Для следующего этапа требуется связать реальные native-модули и builtin
службы в исполняемый startup/GUI-граф с Display, Screen, Image, файловыми
каналами и процессами ввода. Для панели и приложений остаются отказы atoolbar,
tkclient и зависимостей файлов/оболочки. Полный sh не является условием
проверки прямого запуска AWM. Android arm64 остаётся отдельным этапом
интеграции и исполнения этого графа.
