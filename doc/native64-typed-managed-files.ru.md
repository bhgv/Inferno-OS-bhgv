# Checked LOAD → String/List file trial

Доверенный checked-IR consumer теперь загружает и вызывает файловый модуль
существующего `-64m` trial: `string → list of string` и обратное направление.
Это следующий шаг после файлового Command/Wm.init. Он соединяет managed
результаты, native import Types и настоящий namespace/admission reader.

Используются прежние `0xface0010`, String/List tags и однолистовые тела
провайдера. Новых wire IDs, flags и публичного managed ABI нет. Произвольные
файловые тела, включая полный AWM, этим изменением не допускаются.

## Установка и загрузка

`Native64TypedImportRequest.managed=1` явно выбирается доверенным native
builder. Ноль сохраняет scalar/Command-маршрут. Installer сверяет имена и
порядок с ldt, независимые classic `.m` hashes с ldt и generated String.m
hash для соответствующего направления. Импортный Type имеет ровно 72 байта,
np=2, map `{0,0x80}` и аргумент в ячейке 64. Все проверки предшествуют
allocation. Owner копирует request/pool bytes и хранит отдельную 64-битную
маску выбора reader; сами bytes не выбирают файловый профиль.

`read64managedlibrary` разделяет с scalar reader namespace I/O, stat/read
bounds, release/acquire, signing/admission, dontcompile и cleanup. Новый
`prepare64managedlibrarylink` сначала проверяет весь файл существующим
managed proof, затем сопоставляет запросы с экспортами по module/method и
структурной форме. Проверяется и неиспользуемое тело. Из файла строятся
независимые Types, Inst и ext; linkmod создаёт свежий экземпляр. Входной
Module ref, staging lease и pin имеют отдельные пути освобождения при ошибке.
Файловый модуль не имеет entry и автоматически не вызывается при LOAD.

## Проверки

```sh
python3 utils/tests/native64-typed-managed-files.py --limbo /path/to/limbo
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-managed-files.py --limbo /path/to/limbo
```

Два независимых Limbo-провайдера Box/Other меняют имена и порядок экспорта.
Пять полных checked consumer-тел — open, round, split, join, clear — получают
геометрию и инструкции из compiler report с независимым classic Dis oracle.
Request records берутся из отдельно скомпилированного `-64l` consumer.
Classic hashes сверяются с generated `.m`/String.m. Входные buffers запроса,
пула и hashes портятся после установки owner.

4320 C/JIT запусков проверяют round-trip, String/List результаты, ASCII/NUL,
Rune с supplementary code point, nil String, nil List, рост стека, кванты
1/3/1000, две свежие загрузки и девять I/O/admission/pin/dontcompile/no-call
режимов. Используются реальные cons/head/free-list и mapped frame cleanup.
Проверяются точные ссылки, pins, MP isolation, охранные bytes, read/close/
admission/constructor counters и полное освобождение на успехе и ошибке.

13 preallocation отказов проверяют selector, hash, frame/root map, ldt и
callback; allocation rollback сохраняет REG. Прямая конструкция проверяет
каждую allocation (12 C / 13 JIT), отдельную ошибку pin, запрос одной функции,
повреждённое неиспользуемое тело и точную module identity. Активные контроли
удаляют canonical hash guard, root-map guard и publication selector.
Default profile и disabled service не выполняют namespace/constructor work.
Полный новый gate проходит локально ASan+UBSan/NDEBUG с detect_leaks=0.
Это проверка ограниченного стенда, а не sanitizer-прогон полного GUI VM.

Gate включён четырнадцатым в x64/ARM64/RV64/Android ARM64 Bionic matrix и
full port driver; budget typed jobs увеличен с 35 до 40 минут для нового gate.
Локально проверяются x64 и свежая полная console/paired
boot; результаты других архитектур следует брать из CI конкретного commit.
Старые scalar и Command gates сохраняются. 32-битный код не изменён.

## Bionic и дальнейшая работа

В CI предыдущего коммита ARM64 и RV64 typed jobs прошли; отдельная Android
GUI-сборка и classic32 также прошли. Android typed job теперь успешно
линкует converter, но обнаружил, что `--wrap=free` в статическом Bionic
перехватывает libc FILE buffers. Как в существующем dis64-command.py,
файловые gates теперь оставляют libc free отдельно, а VM allocations
проверяют строгим `free_`; `__real_free` разрешается через linker defsym.
Это изменение стенда, не ослабление VM allocation ledger. Реальный Bionic
повторный запуск подтверждается отдельно.

Далее нужны расширение допуска source/файлового consumer и поддержка
оставшихся тел реального графа emuinit/sh/AWM. Полный графический AWM пока
не исполнялся. Совместимый публичный managed ABI остаётся отдельной работой
с FlashVM; текущие частные trial-формы его не назначают.

Следующий участок: [cons/head/tail внутри checked потребителя](native64-typed-lists.ru.md).
