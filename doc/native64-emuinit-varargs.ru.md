# Реальные variadic-планы emuinit и boot Sys.fstat

В обычный выбранный native64 boot-профиль `$Sys` добавлен `fstat`.
Производственный `sysmodinit` и `native64sysexport` проверены настоящим
конструктором builtin-модуля: 25 экспортов, 55 отказов выделения памяти,
откат и повторная регистрация. Для fstat отдельно проверяются 72-байтный
frame, FD root на offset64, восьмибайтный result address на offset32 и
88-байтный смешанный результат. Повреждённые hash/map отвергаются. Проверка
регистрации наблюдает callback; само тело Sys_fstat проверяется отдельными
fstat/getenv runtime gates. Variadic sprint/fprint по-прежнему отсутствуют
в обычном callable registry.

Все три variadic-вызова неизменённого `appl/cmd/emuinit.b` теперь имеют
полный private C-план аргументов: fprint в init (88 байт), sprint в loadmod
(72 байта, пустой tail), fprint в loadmod (96 байт). У двух fprint optimizer
вставляет очистку временного FD после передачи в ожидающий frame. Разбор
проверяет именно MOVP nil → mapped FP owner, предыдущую передачу ref и
отсутствие последующего чтения очищенного владельца. Очистка сохраняется
в отдельном списке точных classic PCs; она не считается записью аргумента.

Независимый classic decoder проверяет FRAME/LEA/MCALL, callers init/loadmod,
все записи аргументов и очистки, полные maps и типизированные source banks.
Callers подтверждаются entry или реальными локальными FRAME/CALL парами.
Пять повреждений отчёта должны провалить проверку. Компиляция с диагностикой
и без неё даёт одинаковые classic bytes; diagnostic JSON не является
файловым ABI и не разрешает исполнение целого тела.

`native64-arguments.py --formatter` теперь воспроизводит эти три реальных
source-плана через настоящие MOVP, managed heap/collector и typed
Sys_sprint64/Sys_fprint64 gateways. Девять исполнений проверяют точные
сообщения, native FD argument, перенос и очистку владельца с GC между
операциями и полное освобождение объектов. Контроль emuinit-clear удаляет
очистку исходного FD и обязан дойти до ошибки проверки refcount.
Файловый write и diagnostic text остаются управляемыми границами fixture.

Это ещё не checked lowering полного variadic-вызова из init/loadmod:
план нужно связать с module-owned call sites и их frame Types, binder,
результатом и исполнением FRAME/MCALL. Checked frontier полного emuinit
остаётся 2/4; AWM не запущен. Нельзя добавлять обычные packed Sys_fprint
или Sys_sprint в native64 registry вместо typed gateway. 32-битный код,
формат файлов и admission не менялись.
