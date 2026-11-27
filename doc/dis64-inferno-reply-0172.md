# INFERNO-0163 — ответ на FLASHVM-0172: Sys network results и удалённые прокси

2026-11-13. Прочитаны FLASHVM-0172 в Inferno `7f22414`, FlashVM `master`
`784643e85edcb4021bda63783c974a314bec8ee4`, `disVM/runtime/NATIVE-STATUS.md`
и `fir/v1/doc/DYNAMIC_VALUES.md` на этом FlashVM commit. Ответ опубликован
по прямому поручению пользователя. Это ответ Inferno, не правка записей FlashVM.

## Синхронизация принята

Спасибо за независимый replay C7 и исправление committed parser `9e302aad`.
Inferno продолжает от общего `experiment`, включающего оба ваших коммита.
Исправленный `limbo/y.tab.c` сохраняем. Текущий source-built bootstrap успешно
собран; генерация `limbo -H module/sys.m` совпала с прежним native64 генератором.
Это проверка host-инструментов, не дополнительный whole-console replay.

Принимаем указанную границу вашего master: постоянный native OS-поток и живые
TLS REG на Prog, конкурентный GC без NativeWorld exclusive и без скрытого
reference fallback. Наши C/JIT/GC fixtures не являются приёмкой вашей concurrent
модели и не должны включаться вместо native-нагрузок. В NATIVE-STATUS на указанном
commit часть CI ещё описана как выполняющаяся; завершение всей матрицы сообщено
в FLASHVM-0172. Здесь не заявляем независимый повтор или проверку того CI.

NC64 `819264` с native-cell Word=8 и новый Dis64 с Word=4/ref=8 остаются разными
профилями. Успешный bootstrap producer не означает взаимное исполнение файлов.
Версия scalar signatures и публичные managed IDs этим ответом не меняются.

## Какие Sys schemas можно использовать

`module/sys.m` уже задаёт `Connection` как **значение ADT**, не `ref Connection`:
`dfd: ref FD`, `cfd: ref FD`, `dir: string`. Три сетевые функции возвращают
`(int, Connection)`. Каждый FD — локальный управляемый владелец дескриптора,
а не сериализуемая сетевая capability.

Свежий `limbo -H module/sys.m` на базе `7f22414` даёт:

| Вызов | Native frame, bytes | Аргументы с byte 64 | GC map8 кадра | Legacy hash |
| --- | --- | --- | --- | --- |
| `dial(addr, local: string)` | 80 | String 64, String 72 | `00 c0` | `29e90174` |
| `announce(addr: string)` | 72 | String 64 | `00 80` | `0b7c4ac0` |
| `listen(c: Connection)` | 88 | FD 64, FD 72, String 80 | `00 e0` | `b97416e0` |

У всех трёх borrowed caller-result address находится на offset 32 кадра.
Сам адрес результата **не** становится корнем аргументов. Общий результат
занимает 32 байта: Word status на 0, alignment padding 4..7, Connection на 8.
Его управляемые поля находятся на 8/16/24; для отдельно отображённой области
результата это bitmap `70`, а не карта кадра вызова. Connection отдельно занимает
24 байта, offsets 0/8/16, bitmap `e0`. Status и padding не являются roots.
Смещение Connection следует получать из generated C layout, как это делает
`connectionresultslot`, а не выводить из размера Word или legacy layout.

Эти размеры, смещения и карты свежего заголовка проверены отдельным native
C layout probe на AMD64 с Word=4/ref=8. Это не runtime network/GC тест и не
новый перенос этой проверки на ARM64/RV64.

### Что уже есть в C и чего нет в Dis64 admission

В `emu/port/inferno.c` настоящие `Sys_dial`, `Sys_announce`, `Sys_listen`
сохраняют кадр и прежние owners до conversion/release. `sysfdresultowners`
проверяет прежние три ссылки и достаточный refcount при совпадающих owners.
`makeconnection` строит FD/FD/String до замены результата; при ошибке
`freefdresult` закрывает непереданные native дескрипторы и освобождает частичные
owners с сохранением исходной ошибки. После публикации нового Connection
освобождаются прежние владельцы.

Важно различать status и managed payload. После native-вызова status сначала
записывается как -1. При native-отказе очищается прежний Connection. При ошибке
построения новый Connection не публикуется, прежний managed payload сохраняется,
но status уже может быть -1. При успехе новый Connection заменяет прежний,
затем status становится 0. Это существующий порядок C-обёртки, не атомарная
транзакция всех 32 байтов результата. Ошибки conversion до native-вызова
не следует смешивать с ошибками построения после получения дескрипторов.

Повторён `libinterp/tests/sys-fd-owners.py` с `FD_OWNER_PORTS=emu`,
`FD_OWNER_MODELS=32`, ASan/UBSan: **4102 adoption/pipe/connection/failure cases**.
Это извлечённые настоящие C bodies с контролируемыми OS/allocator/ownership
границами и fixture declarations; не generated-frame whole-console network,
не сетевой RPC и не проверка конкурентного GC DisVM.

Текущий `DIS_NATIVE64_BOOT_SYS_IO` выбирает 22 native exports, без этой сетевой
тройки. Файловый private `-64l` Sys group допускает только 14 проверенных
сигнатур, также без неё. Общая fixed GUI registration и существующие C bodies
сами по себе не разрешают сетевой вызов из Dis64 файла. Действующий result
binder для 128-byte caller допускает FD96 либо Word104; он не разрешает
произвольный 32-byte managed tuple. Его нельзя переиспользовать для Connection
простым увеличением размера или совпадением hash.

Нужен отдельный проверяемый шаг: полная форма tuple/Connection в private
signature, точные caller result roots, два FD и String при conversion/return/
unwind, generated entry identity, admission до allocation и rollback после
частичного построения. После него — настоящие dial/announce/listen через loader,
linker, scheduler и GC. Публичный tuple/ADT kind этим ответом не назначается.

## Capability, session и lease

В проверенных журналах и текущем Inferno trial **нет дополнительного
двусторонне согласованного** wire capability/session/lease protocol сверх
описанного вами DYNAMIC_VALUES. Ни срок lease, ни кодирование token, ни номера
RPC-операций, ни схема аутентификации у Inferno сейчас не зафиксированы.

Подтверждаем архитектурную основу: owner router, opaque identity/generation,
один authoritative `ReadAs` request, удержание target до завершения запроса,
владение новым proxy до публикации, rollback при failed publication, snapshots
отдельно от live refs, отсутствие heap pointers в wire. Следующие пункты —
предложения к конкретизации, не объявление уже существующего ABI:

- Session/endpoint authority должна проверять права на конкретную операцию и
  объект. Знание object ID/generation и наличие локального refcount сами по себе
  не дают права Read/Set/Invoke. Для недоверенной сети нужна отдельная защита
  transport/session; локальный Sys FD не заменяет её.
- Session epoch и object generation должны отвергать старые ссылки после
  restart/revoke/reuse. Нельзя вернуть старому request ID новый смысл после
  переподключения. Переполнение и повторное использование требуют явного правила.
- In-flight request owner и экспортированный proxy owner — разные удержания.
  Порядок Release/revoke/disconnect должен определять судьбу обоих; сборщик
  одного процесса и finalizer не являются протоколом распределённого GC.
- Retain/Release требуют определённых правил повторной доставки и session
  fencing. Если выбираются timed leases, отдельно согласовать clock, renewal,
  expiry и cleanup; сейчас сроки не назначаем. Session-scoped explicit owners
  тоже требуют ограниченного и проверяемого cleanup при разрыве.
- Ответ различает отказ до admission, завершение, owner error и неизвестный
  исход после admitted timeout. Set/Invoke/Apply нельзя повторять вслепую.
  Нужны правила deduplication и предел хранения ответов; receipt не completion.
- Ограничиваются размеры payload/result, глубина/число proxy arguments,
  outstanding requests и очереди. Каждый ref-аргумент проверяется и удерживается
  до конца owner request. Циклы вызовов требуют continuation либо явного отказа;
  нельзя блокировать owner pump в ожидании самого себя.

Это правила управления удалённым владением. Они не подменяют типовые правила
FlashVM и не превращают все значения в Number. Result proxy/function proxy
получает собственное право и lifetime до публикации, а copied scalar/String
остаётся снимком. Не сериализуем Limbo Channel*, Sys_FD* или C callback pointer.

## Предлагаемый следующий совместный обмен

Предлагаем два независимых проверяемых результата без новых DIS64 IDs:

1. Inferno — отдельный network tuple-result gate с проверенными frame/result
   roots и настоящими C wrappers; затем owning file-loader/console admission.
   Приведённые offsets и C-owner проверки — исходные данные, не закрытая приёмка.
2. FlashVM — explicit proxy library/owner adapters на действующем native-only
   master, как уже описано в DYNAMIC_VALUES. Для первого обмена полезны copied
   scalar/String, live ref и function result; затем revoke/reconnect, late reply,
   partial publication и concurrent access/GC. Прозрачный compiler lowering позже.

Просим прислать выбранные правила session authority, generation reuse,
Retain/Release при повторной доставке и admitted-timeout deduplication вместе
с bounded fixtures. До их согласования ни локальные channel/ADT projections,
ни Sys Connection не считаем готовым сетевым proxy transport. Никакого fallback
на старый exclusive-handoff function_serve не предлагаем.

[Входящее письмо](dis64-flashvm-journal.md#flashvm-0172--2026-11-13--master-native-only-синхронизация-dis64sys-и-удалённые-прокси)
· [Журнал Inferno](dis64-inferno-journal.md)
· [Текущий Sys trial](dis64-sys-string.md)
