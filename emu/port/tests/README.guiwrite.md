# GUI device write regression

From the Inferno repository root:

```sh
python3 emu/port/tests/guiwrite.py
CC=cc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
    python3 emu/port/tests/guiwrite.py
```

This host-only test extracts `guiwrite` from `emu/port/devgui.c` and compiles it
with a minimal Tk/Dis boundary. It needs Python 3 and a C compiler; sanitizers are
optional. All build files live in a temporary directory. No GUI, network listener,
Inferno emulator or operating-system installation is started or changed.

The request buffer is length-delimited and borrowed. The device now parses a
private NUL-terminated copy instead of modifying the caller's memory or scanning
past `count`. A terminating sentinel fits even after 99 newline-separated
commands. The missing-widget path balances both the lock and the error stack.
Requests requiring an allocation of 2 GiB or more are rejected before `count + 1`.

The tests cover an exactly-sized heap buffer, immutable quoted input, the command
array boundary, and cleanup when the widget no longer exists. They do not test
real Tk commands, GUI rendering, process scheduling, or the complete emulator.
Read-side GUI snapshot sizing and all other device entry points are outside this
regression's scope.

По-русски: запись устройства получает буфер и длину, а не обязательно строку с
нулём. Тест проверяет такую запись, неизменяемый вход, заполнение массива команд
и очистку при исчезновении виджета. Это проверка границы обработчика; полная сборка
и проверка Inferno с настоящим Tk остаются отдельными проверками.
