# Настоящая unquoted из emuinit на native64

`utils/tests/native64-emuinit-unquoted.py` извлекает тело `unquoted` прямо из
`appl/cmd/emuinit.b`, без изменения исходника, и добавляет только экспортный
wrapper `parse`. Обе функции проходят checked LOAD lowering и независимую
сверку с classic Dis. Тело исполняется в owning native module через настоящие
MFRAME/MCALL, String/List handlers и C/JIT closure.

Добавлены прямые IINDC, ISLICEC и IADDC с проверкой ширины, адресации и точного
разделения scalar/root в frame/data maps. Рекурсия разрешена только для
self-call в LOAD-профиле: точная идентичность frame/аргумента/result, inherited
result и непосредственно следующий RET. Mutual recursion, обычный non-tail
self-call и старые профили остаются закрыты. Это обычный рекурсивный CALL,
а не оптимизация расхода стека.

Проверены 360 вариантов C/JIT исполнения: nil, пустые строки, пробелы,
кавычки, удвоенная кавычка, незакрытая кавычка с реальным рекурсивным вызовом,
Unicode и встроенный NUL. Отдельно проверены 352 отказа выделения памяти при
исполнении и все 14/15 позиций конструктора C/JIT: rollback, REG, roots,
модули и types. Пятнадцать испорченных metadata отвергаются до allocation.
Активное удаление проверки root map или повреждение borrowed recursive
result pointer обнаруживается до небезопасного вызова. ASan/UBSan с NDEBUG
и статический host build с Android headers проходят эти проверки.
Последний не является выполнением ARM64 Bionic; целевые запуски оставлены CI.

Checked preflight настоящего emuinit теперь принимает unquoted; classic bytes
всех 16 проверенных startup-исходников не изменились. Полный emuinit и AWM
ещё не исполняются: init блокируется getenv, getenv — ref Sys->FD/nil,
loadmod — вариадическим Sys->sprint. File admission полного emuinit/AWM этим
изменением не расширен. Цель остаётся emuinit → /dis/wm/awm.dis → init(nil,args),
затем настоящее окно, ввод и JIT на Android ARM64. 32-битный ABI сохранён.

Запуск: `python3 utils/tests/native64-emuinit-unquoted.py --limbo /path/to/limbo`.
