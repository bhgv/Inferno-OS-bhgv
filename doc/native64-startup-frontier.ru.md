# Проверенная граница startup-модулей

Текущий checked LOAD preflight принимает **309 из 385 функций** в 16 исходных
startup-модулях. Для каждой ordinary-компиляции с `-i` classic Dis побайтно
совпадает с результатом компиляции при включённых native64 diagnostics.

| Исходник | Принятые тела |
|---|---:|
| appl/cmd/emuinit.b | 4/4 |
| appl/wm/awm.b | 20/20 |
| appl/wm/atoolbar.b | 26/26 |
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
| appl/lib/tkclient.b | 16/16 |
| appl/cmd/sh/sh.b | 72/134 |

Массивы управляемых ссылок теперь выбирают 8-байтовое размещение по типу
элемента. Это открыло неизменённые `Sh.yytokname` и `Sh.yystatname`: чтение
глобального массива строк, сравнение с `nil` и возврат элемента. Строковое
сравнение сохраняет равенство `nil` и выделенной пустой строки.

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
[Общий startup gate](native64-awm-startup.ru.md) теперь связывает девять
неизменённых native-модулей, исполняет настоящий AWM.init, ввод и файловые
каналы, затем подтверждает ожидание событий в исходном ALT. Это отдельная
исполняемая проверка с управляемыми Sys/Draw/Tk и дочерним приложением.

Android ARM64 интеграция запускает этот граф с реальными Sys, Draw и Tk
в режиме `-nopanel`. Все тела atoolbar и tkclient теперь проходят checked
проверку. Полная работа панели и приложений дополнительно зависит от связки
модулей во время исполнения и зависимостей файлов и оболочки.

В atoolbar приняты `iconify` и `handlerequest`. Первый вычисляет команды Tk
до передачи аргументов, второму нужен `hd tl args` в локальном вызове.
Компилятор и конструктор модуля используют одну проверку последовательности
tail/head/clear для локального и импортированного вызова.

Чтение и запись `Tk->Toplevel.screenr` проходят через проверенный
16-байтовый `Rect` по native64-смещению 32. Это открыло `layout`
и `wmctl` в atoolbar и `readscreenrect` в Tkclient.
Составной результат `Tkclient->toplevel` также проходит текущую проверку тела.

Правка касается внутренней геометрии Frame64. Отдельное сообщение FlashVM
о хвосте Dis с типами для Styx опубликовано в
[issue #4](https://github.com/bhgv/Inferno-OS-bhgv/issues/4).
Актуальная документация FlashVM дополнительно описывает `STYXCS2!`,
который добавляет типы импортов. Эта правка не меняет форматы Dis и Styx.
