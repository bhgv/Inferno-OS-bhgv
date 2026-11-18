# Диагностический барьер startup-функций

AWM preflight теперь отдельно проверяет checked LOAD diagnostics для всех
16 исходных startup-модулей. На текущем compiler это 380 функций, из которых
37 принимаются как отдельные checked IR bodies. Это не допуск файла к загрузке
и не утверждение о запуске этих функций или полного AWM.

Для каждого исходника выполняются две ordinary-компиляции с `-i`: без
LIMBO_DIS64_* и с typed/external/load diagnostics. Их classic Dis обязан
совпадать побайтово. Результат сохраняет SHA classic bytes, причину отказа,
PC, opcode и операнды, независимо прочитанные из classic Dis. Для FRAME
показываются также четыре следующих инструкции подготовки вызова.
Отказ layout/signature до обхода тела не приписывается случайной инструкции.

Одинаковые имена методов разных ADT, например top/get/put в wmsrv, допустимы:
идентичность записи задаётся парой name/first. Они не теряются при построении
словаря по одному имени. Проверка принимает как доказательство только
нормальное завершение reporter, валидный JSON и неизменённые classic bytes;
sanitizer/crash не считается обычным отказом функции.

Основной путь остаётся `emuinit → /dis/wm/awm.dis → mod->init(nil,args)`.
Текущие первые барьеры полного emuinit:

| Функция | PC | Причина | Контекст |
|---|---:|---|---|
| init | 7 | multi-callee | FRAME вызова getenv |
| loadmod | 69 | multi-call | FRAME вариадического Sys->sprint в fallback |
| getenv | 96 | external-argument | MFRAME Sys->open; следующий IADDC пишет String в pending argument |
| unquoted | 134 | unsupported-opcode | IINDC — индексирование строки |

FRAME/MFRAME в таблице обозначают место отказа разбора вызова, а не общий
запрет этих операторов. Более поздние барьеры могут стать видны после
устранения первого. Например, unquoted рекурсивен; отчёт не утверждает,
что одно добавление IINDC сделает функцию исполнимой.

Полный preflight сохраняет прежние static LOAD hashes и четыре file-profile
проверки, добавляя checked_frontier в каждую запись JSON:

```sh
python3 utils/tests/awm-native64-preflight.py --limbo /path/to/limbo --report startup.json
python3 utils/tests/awm-native64-preflight.py --limbo /path/to/limbo --checked-only --report frontier.json
```

Вторая команда предназначена для быстрого анализа checked IR: static imports
и четыре file profiles в этом режиме не проверяются. Обе команды явно
помечают отсутствие runtime acceptance. 32-битный формат и ABI не меняются.


Следующий этап: [computed String arguments](native64-typed-concat-args.ru.md).
После его compiler lowering первый барьер getenv перемещается к PC101:
сравнение ref Sys->FD с nil (load-nil-roles). Таблица выше сохраняет снимок
предыдущего compiler; актуальные координаты следует получать preflight.

Следующий проверенный участок: [неизменённая unquoted](native64-emuinit-unquoted.ru.md).
Теперь её полное тело принимается checked compiler и исполняется в native
module через C/JIT, включая self-tail recursion. Это не запуск полного emuinit.

Следующий участок: [прямое сравнение Sys.FD с nil](native64-emuinit-fd.ru.md).
getenv теперь проходит PC101 и останавливается на PC104: Sys.fstat возвращает
aggregate (int, Dir). Полный emuinit/AWM всё ещё закрыт.

Следующий участок: [Sys.fstat result и caller tuple layout](native64-emuinit-fstat.ru.md).
getenv теперь проходит PC104 и останавливается на PC112 (INEWA byte array).
Полный emuinit/AWM пока не исполняется; classic bytes сохранены.

Следующий участок: [owned byte arrays](native64-emuinit-bytes.ru.md).
getenv проходит INEWA PC112; первый отказ теперь PC113, вычисленный `len buf`
в аргументах Sys.read. Полный startup пока не исполняется.

Следующий проверенный этап: [полные getenv и unquoted](native64-emuinit-getenv.ru.md), C/JIT с настоящими телами Sys open/fstat/read и управляемым файловым транспортом. Полный startup ещё не запущен.

Следующий этап: [реальные variadic-планы и boot Sys.fstat](native64-emuinit-varargs.ru.md). fstat теперь зарегистрирован в обычном выбранном boot-профиле; все три variadic source-плана проверяются и воспроизводятся через typed gateways. Полные init/loadmod пока не исполняются.

Следующий проверенный этап: [program-owned FRAME/MCALL и C/JIT](native64-sys-program.ru.md). Мост связывает exact original PCs с typed binder/trampoline; полные init/loadmod пока не исполняются.

Следующий этап: [полный неизменённый emuinit](native64-emuinit-full.ru.md): 4/4 checked bodies, owning C/JIT и Limbo-вызов `Command.init(nil,args)` с управляемыми сервисами. Это ещё не загрузка настоящего AWM.
