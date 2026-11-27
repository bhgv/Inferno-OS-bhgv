# Полные getenv и unquoted в native64

Неизменённые тела `getenv` и `unquoted` из `appl/cmd/emuinit.b` теперь
принимаются checked LOAD compiler и исполняются через native module в C/JIT.
Classic Dis и 32-битный ABI сохранены.

Добавлены вычисленный `len` byte array в аргументе импортированного вызова
и преобразование byte array в единственный String аргумент локального вызова.
Для реального getenv проверяется точная последовательность сохранения byte
view, slice, преобразования и очистки view между FRAME и CALL. Ограничение
единственного аргумента исключает последующее выделение памяти после
публикации String в ожидающий frame. Компилятор, независимый source oracle
и native preflight проверяют типы, root maps, адреса и границы frame.

Проверка `utils/tests/native64-emuinit-getenv.py` использует настоящие тела
`Sys_open`, `Sys_fstat`, `Sys_read`, String/List/Array handlers и native
FRAME/MCALL/CALL/RET. Файловый транспорт и выделение FD payload управляются
fixture: это не проверка настоящей файловой системы или kernel FD/fgrp.
Дополнительный экспорт envload нужен только для формирования import table
classic compiler и не исполняется.

Проверены 1080 C/JIT сценариев: короткие чтения, ошибки open/stat/read,
нулевой результат, Unicode, NUL, кавычки, рекурсия unquoted и разные quanta.
Ещё 3072 исполнения проверяют отказы выделения памяти. Проверяются точные
списки результата, закрытие FD и полное освобождение объектов. Все 26/27
точек отказа конструктора C/JIT сохраняют REG и откатывают ресурсы.
Двенадцать повреждений metadata отвергаются до выделения памяти; активное
удаление проверки source root и повреждение публикации prelude обнаруживаются.
Пройдены ASan/UBSan и host static сборка с Android headers; последняя
не означает исполнение под Bionic или на Android устройстве.

Полный emuinit и окно AWM ещё не запущены. В checked frontier принимаются
getenv и unquoted (2 из 4 функций emuinit); init и loadmod остаются на
`multi-call` PC17/PC69, включая variadic fprint/sprint. Регистрация fstat
в обычном boot Sys registry также остаётся отдельным шагом: fixture
регистрирует нужные три импорта явно. File admission не расширялся.
Цель остаётся emuinit → /dis/wm/awm.dis → mod->init(nil, args).

Следующий этап: [реальные variadic-планы и boot Sys.fstat](native64-emuinit-varargs.ru.md). fstat теперь зарегистрирован в обычном выбранном boot-профиле; все три variadic source-плана проверяются и воспроизводятся через typed gateways. Полные init/loadmod пока не исполняются.
