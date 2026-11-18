# Sys.fstat: проверенный смешанный результат native64

В неизменённом appl/cmd/emuinit.b getenv теперь проходит Sys.fstat. Следующий
первый отказ — PC112, INEWA для array of byte. Полные getenv, emuinit и AWM
ещё не исполняются. Путь остаётся emuinit → /dis/wm/awm.dis → init(nil,args),
затем настоящее окно/ввод/JIT Android ARM64.

Добавлен частный checked-IR контракт 88 только для Sys.fstat с настоящим
hash 0xda4499c2 и native frame 72/map {0,128}. Наследуемая передача aggregate
результата и общие tuple-вызовы остаются закрыты. Контракт не сериализуется
и не расширяет file admission.

| Часть результата | Native offset | Размер | Роль |
|---|---:|---:|---|
| status | 0 | 4 | WORD |
| padding | 4 | 4 | scalar |
| name, uid, gid, muid | 8, 16, 24, 32 | 4 × 8 | String roots |
| qid.path | 40 | 8 | big |
| qid.vers/qtype | 48, 52 | 2 × 4 | int |
| mode/atime/mtime | 56, 60, 64 | 3 × 4 | int |
| padding | 68 | 4 | scalar |
| length | 72 | 8 | big |
| dtype/dev | 80, 84 | 2 × 4 | int |

Compiler проверяет Sys/FD/Dir declarations и все 14 листьев результата.
Перед переводом инструкций exact fstat result groups получают единую native
раскладку. Это нужно настоящему getenv: отдельные (ok,d) locals прежде
размещали status по native92, Dir по native96, теряя tuple padding. Новый
план выравнивает группу, сохраняет prefix и сдвигает последующие locals;
повторная обработка той же группы идемпотентна. Legacy offsets, descriptors
и classic Dis не меняются. Изменяется только private native plan. Формальные
аргументы не могут перекрываться результатом; весь план ограничен 1 MiB и
учитывается в существующем budget.

Constructor до allocation проверяет полные 88 байт, все четыре root bits,
scalar bits, код LEA, imported signature/frame и запрет inherited/local
контракта. Перед публикацией borrowed result address runtime binder повторно
проверяет диапазон, map роли всех байтов и принадлежность pending frame
живому стеку. Ни адрес результата, ни padding не становятся GC roots.

Новый gate native64-emuinit-fstat.py использует настоящее module/sys.m и
свежий -H Sys header. Пять полных Limbo функций проходят независимую сверку с
classic Dis: LOAD, status, Dir.length, повторный вызов и shifted scalar prefix.
Реальные MFRAME/MCALL/RET и настоящий Sys_fstat/unpackdir/retstr исполняются
через C/JIT. Подменён только native filesystem transport kdirfstat; builtin
регистрация в fixture задаётся явно. Это не запуск полного Sys init/emuinit
или реального файлового ввода-вывода на Android.

Проверены 2016 C/JIT случаев: все поля, длины выше 2^32, Unicode Strings,
пустые/прежние/общие владельцы четырёх строк, native failure и каждый отказ
Dir/четырёх String allocations. Проверяются padding/canaries, status при
частичной публикации и полное освобождение modules/Types/String roots.
Constructor: все 24/25 allocation failures C/JIT с rollback и неизменным REG.
19 повреждённых metadata отвергаются до allocation. Runtime контроль
удалённого четвёртого root останавливается до builtin в C/JIT. Активное
удаление constructor root check или runtime map check обнаруживается своим
семантическим предикатом. IR unit проверяет repacking/idempotence и восемь
неверных namespace/type/leaf/span вариантов; отключение repacking выявляется.

Восстановлена и консольная flow mutation после переноса helper в
include/dis64/pilot/operands.h: shadow headers копируются вместе, 256 CFG
oracles и удаление intersection проходят. Свежий amd64 classic-mk build и
paired console boot проходят. Эта починка не меняет production flow policy.

Валидация: новый gate прошёл ASan/UBSan с -O1/NDEBUG и отдельную статическую
host-сборку с Android headers/free_ seam. Последняя не является выполнением
ARM64 Bionic. Целевые C/JIT прогоны добавлены в CI amd64/arm64/riscv64/Bionic.
Повторно прошли pairs, raises, primary, concat-args, imports, calls,
unquoted, FD/nil и compiler multi/external units. Checked preflight всех
16 startup sources сохраняет classic bytes. 32-битный ABI не изменён.
