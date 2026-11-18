# Program-owned Sys call sites: FRAME/MCALL и C/JIT

Добавлены `Sys_setprogram64` и отдельный native trampoline
`Sys_programcall64`. Они связывают существующие typed Sys gateways с точными
original-Inst PC доверенного caller-модуля. Обычный `$Sys` registry не получил
packed variadic-входов; файл не может выбрать этот режим флагом.

Installer проверяет caller/callee Type identities и полные prefix/tail maps,
соседние LEA/MCALL, размеры и root role result cell, mapped operand модуля,
ordinal и отсутствие повторных PC. Рецепты, slots и ссылки на Types копируются
в существующую module-owned таблицу. Снимки LEA/MCALL сохраняются для проверки
неизменности кода. До полного успеха hook не публикуется. Отказ выделения памяти
освобождает уже приобретённые Types/slots и оставляет Module неизменённым.

`native64typedlea8step` делегирует program-owned binder перед обычным binder.
На точном LEA PC проверяются снимки инструкций и реальный native target:
правильного frame Type недостаточно, entry должен быть `Sys_programcall64`.
Затем существующий Sys binder публикует восемь байт result address. Остальные
инструкции идут обычным C/JIT путём. Trampoline использует MCALL-retained
caller Module и return PC, а не глобальный номер текущего рецепта. Он повторно
проверяет owner, target Type/callback и снимки кода, после чего запускает
module-owned typed gateway. Ошибка producer сохраняет diagnostic при передаче
в обычный VM error/unwind. Busy таблица не освобождается во время output;
retirement снимает hook перед освобождением Type owners.

API рассчитан на доверенный compiler/builder и неизменные original Insts.
Он не проверяет все аргументные записи и CFG полного тела, не читает wire
call table и не аутентифицирует произвольные C pointers. Проверка source-плана
остаётся отдельной обязанностью. Сохраняется ограничение frame gateway:
caller/callee должны находиться в одном текущем stack extent.

Проверки:

* `native64-arguments.py --formatter` выполняет три реальных source-рецепта
  emuinit через FRAME, MOVP, очистку FD, настоящий local binder, LEA и MCALL,
  затем настоящий formatter/GC и Sys gateways. Девять исполнений на вариант
  охватывают три GC phases. Есть семь metadata и шесть live identity отказов,
  rollback installer, копирование рецептов, busy output и полное освобождение.
* `native64-sys-program.py` исполняет эти call sequences в полном native VM
  и leaf JIT: 72 случая C/JIT, quanta 1/3/1000, старый snapshot инструкций,
  чужой native callback и allocation failure producer. Проверяются точный
  результат/его ширина, guards, diagnostic и полный scheduler/module/Type
  teardown. Здесь producer управляемый: настоящий formatter проверяется
  предыдущим gate. Компилируется также полный production runt.c с formatter.
* Активные удаления проверок contract и return PC должны провалить первый
  gate; удаление target check позволяет дойти до запрещённого C callback и
  обязано провалить второй gate до нормального исполнения.

Оба gate проходят ASan/UBSan. Полная amd64-консоль и pair boot, owning module
C/JIT и старые getenv/registration gates проверяются отдельно. В CI новый
VM gate включён для amd64/arm64/riscv64 и Android ARM64 Bionic; локальный host
прогон не означает запуск на Android устройстве.

Это исполнение call sequences с заранее подготовленным source bank.
Целые `init`/`loadmod` по-прежнему не принимаются checked compiler, и AWM ещё
не запущен. Следующий этап — compiler lowering variadic FRAME/MCALL и установка
этих program sites из owning constructor полного source-derived модуля.
Classic bytes, 32-битный ABI и file admission не изменены.
