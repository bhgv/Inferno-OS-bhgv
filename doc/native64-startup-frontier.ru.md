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

Следующий этап: [настоящий emuinit boot](native64-emuinit-boot.ru.md): полный исходный startup в выбранном Linux профиле, реальные Sys/formatter/file reader и checked файловый Command. Arg и настоящий AWM ещё не допускаются.

Следующий этап: [настоящий Arg](native64-arg.ru.md), 8/8 checked bodies и 324 реальных C/JIT процесса emuinit с нормальным разбором argv. AWM/Draw/Tk и Android window ещё впереди.

Следующий этап: [форматирование координат WM](native64-coordinate-format.ru.md): неизменённые awm.r2s/wmclient.r2s, typed numeric tails и inherited Sys result; AWM 2/20, wmclient 5/21. Полный графический startup ещё не допускается.

Следующий этап: [поля Draw.Context](native64-context-fields.ru.md): checked AWM init проходит PC51 и nil test wm, первый отказ теперь PC61 — Rect argument вызова reshape. Это ещё не runtime acceptance полного AWM.

Следующий этап: [Point/Rect в imported arguments](native64-geometry-args.ru.md): AWM init проходит reshape PC61, следующий отказ PC72 — Window.image. Реальные Limbo-to-Limbo C/JIT вызовы проверяются отдельным gate; полного AWM окна ещё нет.

Следующий этап: [Window roots и список startinput](native64-window-startup.ru.md): AWM init проходит PC72/84; следующий checked отказ PC93 — makescreen. Полного AWM окна ещё нет.

Следующий этап: [Draw payload roots и Image.display](native64-draw-fields.ru.md): makescreen проходит Display.color на PC1222; следующий отказ PC1227 — global result binding Screen.allocate. Init остаётся на PC93.

Следующий этап: [makescreen и MP root results](native64-makescreen.ru.md): весь makescreen допускается (23 инструкции, 3 Draw calls); init проходит PC93 и останавливается на PC97 — triple-channel result Wmsrv.init.

Следующий этап: [тройка каналов](native64-channel-triples.ru.md): AWM init проходит Wmsrv.init PC97 и останавливается на Draw.Context allocation PC100. Provider Wmsrv.init — на managed channel allocation PC27.

Следующий этап: [Context allocation/copy](native64-context-new.ru.md): AWM init проходит PC100 и доходит до PC116 (fatal -> kill -> computed Sys.open path).

Следующий этап: [String preparation и temporary clear](native64-string-prep.ru.md): настоящие kill/fatal принимаются целиком; AWM init доходит до PC120 INEWCP. Полный init и UI пока не запущены.

Следующий этап: [primitive channel allocation](native64-channel-new.ru.md): AWM init проходит PC120 и останавливается на spawn setup PC127/ISPAWN PC131. Wmsrv mapped channel PC27 остаётся отдельным барьером.

Следующий этап: [mapped tuple channels](native64-mapped-channels.ru.md): Wmsrv.init проходит PC27–29 и доходит до spawn setup PC30. AWM init остаётся на spawn setup PC127/131.

Следующий этап: [checked local spawn](native64-spawn.ru.md): spawn выполняется в C/JIT для полностью проверенных void bodies. AWM/Wmsrv init sites остаются PC127/30 с причиной multi-callee; дальше блокируют command PC1392 и wm PC40.

Следующий этап: [borrowed List prefixes](native64-list-prefix.ru.md): awm.command проходит Sh.run preparation PC1392 и доходит до ISEND PC1401. Spawned handlers пока не принимаются целиком; Wmsrv wm остаётся PC40.

Следующий этап: [primitive channel SEND/RECV](native64-channel-transfer.ru.md): awm.command проходит ISEND PC1401 и доходит до ICONSW PC1404 (fd list). Проверен реальный scheduler/wakeup, но command и GUI целиком пока не принимаются.

Следующий этап: [списки 32-битных int](native64-integer-lists.ru.md).
AWM command проходит построение fd list для Sys.pctl на PC1404;
следующий checked отказ — PC1457 IEXIT. Полного графического запуска ещё нет.

Следующий этап: [завершение процесса](native64-process-exit.ru.md).
Весь настоящий awm.command принимается. AWM init проходит spawn PC127/131
и доходит до PC136: computed String argument локального fatal.
Полного графического запуска ещё нет; Wmsrv wm остаётся на PC40.

Следующий этап: [computed String local argument](native64-local-concat.ru.md).
AWM init проходит fatal PC136 и доходит до PC140 IMOVW (typed-span),
чтения win.ctl при подготовке ALT. Весь command остаётся accepted.
Графического запуска пока нет.

Следующий этап: [блокирующий ALT](native64-alt.ru.md): простые int/String
guards работают в C/JIT с настоящими очередями. AWM init проходит PC140
Window.ctl и доходит до PC142 Wmcontext.ctl; command остаётся accepted.
Полного графического event loop ещё нет.

Следующий этап: [Wmcontext channels и ref ALT](native64-wmcontext-alt.ru.md).
AWM init проходит ctl/kbd/ptr и PC149 reference payload, останавливается
на PC151: tuple (ref Client, chan of string) из join. Command остаётся accepted;
полного графического event loop ещё нет.

Следующий этап: [пара ref/channel в ALT](native64-alt-pairs.ru.md).
Настоящий awm.init проходит адрес пары из join на PC151 и останавливается
на PC153: tuple из req. Command остаётся accepted. Это продвижение typed
frontier; полный AWM event loop и Android UI ещё не запущены.

Следующий этап: [request tuple в ALT](native64-alt-requests.ru.md).
Настоящий awm.init проходит тройку из req на PC153 и доходит до PC154,
alt-field-channel для wmrectIO.write через Sys.FileIO. Command остаётся
accepted; FileIO channel fields и mixed payloads — следующий участок.
Полный AWM/Android UI пока не запущен.

Следующий этап: [Sys.FileIO и mixed ALT payloads](native64-fileio-alt.ru.md).
Настоящий awm.init проходит оба FileIO guard’а и полную подготовку внешнего
ALT с IGOTO. Следующий отказ — PC168 indirect-or-reserved-operand внутри
обработки события: z.ctl <-= "exit" через Client.ctl. Command остаётся
accepted; графический AWM ещё не запущен.

Следующий этап: [поля и каналы Client](native64-client-channels.ru.md).
Настоящий awm.init проходит z.ctl SEND и чтение z.znext (PC168/169).
Следующий отказ — PC178 context-nil-roles: сравнение win.image/screen.image.
Command остаётся accepted; графический AWM ещё не запущен.

Следующий этап: [field reference identity](native64-field-reference.ru.md).
Настоящий awm.init проходит win.image/screen.image на PC178. Следующий отказ —
PC180 multi-callee: reshaped пока отказывается на PC716, IMOVM локального Rect
из Image.r. Command остаётся accepted; графический AWM ещё не запущен.

Следующий этап: [локальная Image geometry](native64-local-image-geometry.ru.md).
Настоящий reshaped проходит PC716 и доходит до PC762 load-nil-roles для списка
z.wins. Пока этот callee не принят целиком, awm.init остаётся на PC180
multi-callee. Command остаётся accepted; графический AWM ещё не запущен.

Следующий этап: [списки и поля Wmsrv.Window](native64-window-list.ru.md).
Настоящий reshaped проходит PC762–764 (nil/hd/w.img=nil) и доходит до PC765:
image-geometry-owner для передачи w.r в геометрический вызов. awm.init пока
остаётся на PC180 multi-callee; Command accepted, графический AWM не запущен.

Следующий этап: [аргументы геометрии Wmsrv.Window](native64-window-geometry-args.ru.md).
Настоящий reshaped проходит подготовку w.r и oldr.min для Rect.subpt.
На PC765 остаётся external-result-kind: нужен возврат Draw.Rect из импорта.
awm.init остаётся на PC180 multi-callee. Command accepted;
графический AWM и Android UI пока не запущены.
