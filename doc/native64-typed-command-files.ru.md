# Checked LOAD → файловый Command/Wm.init

`install64typedimports` теперь принимает не только scalar-запросы, но и
существующую фиксированную форму `Command.init(Context, list of string)`.
Она нужна для того же Limbo-вызова, которым emuinit и shell запускают AWM.
Потребитель загружает файл через настоящий namespace/admission reader,
сохраняет модуль в MP и явно вызывает `init` из Limbo.

Это расширение частного installer на уже существующий Command trial.
Управляемый ABI для произвольных функций не назначается: используется прежняя
`0xface0186`-форма, без новых wire IDs, flags или изменения файлового формата.
Сама реализация AWM пока не допускается целиком и не исполняется.

## Контракт

Для scalar-записей сохраняются прежние проверки и нулевая карта прототипа.
Command-запись проверяется существующими `dis64clientbindingscheck` и
`dis64clientbindingget`: один импорт, фиксированные `Command`/`init` в пуле,
void-result, два аргумента и точная существующая trial-signature.
Hash внутри payload должен совпадать с независимым `.m`-hash и native ldt.
Как и раньше, hash служит согласованности доверенных native-метаданных,
а файловую authority даёт полная проверка файла.

Импортный Type должен иметь ровно 80 байт, np=2 и карту `{0,0xc0}`:
Context в ячейке 64, list of String в 72, никаких других root-ячеек.
Проверка происходит до allocation и публикации owner. Проверенные запросы
по-прежнему копируются одной allocation; входные buffers после установки
не нужны. Runtime reader, signing/admission policy и Command/Wm alias-matcher
используются без изменения их файлового допуска.

## Исполнение и проверка

```sh
python3 utils/tests/native64-typed-command-files.py --limbo /path/to/limbo
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-command-files.py --limbo /path/to/limbo
```

Три полных checked Limbo-тела `open`, `run`, `clear` компилируются отдельно
от провайдера. `run` делает два `p->init(ctxt, argv)` и меняет MP-счётчик.
Native-геометрия и hash сверяются с настоящим `limbo -H -M Command` для
интерфейса из module/sh.m. Existing request bytes берутся из независимо
скомпилированного `-64c`-пробника. Координаты и imports проверяет независимый
classic Dis oracle. Сохраняются witnesses из appl/cmd/emuinit.b и sh.b/sh.y.

Провайдеры — конечные исходные реализации `Command` и `Wm`, скомпилированные
в файлы `-64b`. Они читают argv, делают head/tail/cons, копируют и очищают
локальные ссылки. Третий `Wm.init` всегда делит на ноль. Он успешно
загружается без вызова и даёт ожидаемую ошибку при явном source-вызове.
Это проверка отсутствия автоматического init при LOAD.

Всего 3888 C/JIT запусков (3×1296) варьируют оба режима, рост стека,
кванты 1/3/1000, argv с ASCII/NUL и Rune, nil-список, nil и non-nil Context,
две свежие загрузки и девять режимов I/O/admission/pin/dontcompile/no-init.
Nonnull Context-макет имеет три картированных владельца; поля Draw не
используются как реальные GUI-объекты. Input buffers запросов и hashes
портятся после установки.

Проверяются явный вызов, MP-счётчики, охранные байты, pin transfer,
числа read/close/admission/constructor, исходный MP, точные ссылки в успешных
сценариях и полное освобождение при ошибках. Используются настоящие cons,
consp, headp, tail, lenl, lenc и freelist. Это стенд владения и refcount;
отдельный полный VM-прогон проверяет обычные Command launchers.

17 отказов installer до выделения проверяют root-карту, extent, np,
hash/payload consistency, trial version/count и callback. Отказ единственной
allocation сохраняет REG и owner. Активные контроли удаляют payload-hash
проверку, Command root-карту и retain String-head при cons: каждый обязан
дать свой семантический отказ без sanitizer-ошибки. Полный новый gate
проходит ASan+UBSan/NDEBUG, detect_leaks=0.

Старый scalar file gate (1152 запусков и его контроли), default/disabled
границы, свежая полная консоль/paired boot и 54 full-VM C/JIT launchers
(243 raw checks) проходят. Старые 32-битные пути сохранены.

## CI и Bionic

Предыдущий двенадцатый gate прошёл на x64, ARM64 и RV64. Android остановился
при линковке стенда: не хватало `inferno_strtoull`, на который Android lib9.h
переименовывает стандартное имя. Общий файловый unit теперь включает
существующую lib9/strtoull.c под Bionic. Локальная проверка именно этой
приватной линковки проходит с прежним scalar pipeline:

```sh
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all -DTEST_PRIVATE_UNSIGNED_CONVERTER -Dstrtoull=inferno_strtoull' \
  python3 utils/tests/native64-typed-files.py --limbo /path/to/limbo
```

Это host-проверка alias, не замена реального Android-прогона. Результат новой
Android-матрицы подтверждается отдельно. Command file gate включён
тринадцатым для x64/ARM64/RV64/Android ARM64/Bionic и в full port driver.
Лимит job повышен с 25 до 35 минут: прежний x64 job занял 23m55s.

Остаются общий managed-import ABI, другие файловые managed-провайдеры,
допуск потребителя из файла вместо доверенного builder и неподдержанные
тела полного графа emuinit/sh/AWM. Реальный графический AWM не запущен.
