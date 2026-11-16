# Первичная ветка emuinit.loadmod

Основной путь experiment: emuinit загружает /dis/wm/awm.dis и вызывает
mod->init(nil,args) из Limbo. Shell — fallback после отказа прямой загрузки.

Gate primary дословно извлекает начало loadmod из appl/cmd/emuinit.b вплоть
до комментария о fallback. Здесь остаются Command->PATH, hd args,
создание "-l"::nil, LOAD "/dis/wm/awm.dis" и return(mod,args).
Для завершённости ограниченного среза отказ первичной загрузки заканчивается
raise "fail:error". Настоящие fallback Sys->sprint/fprint не включены.
Обёртка получает argv от fixture и вызывает loadmod, затем mod->init(nil,args).
Настоящие getenv и Arg пока не выполняются.

Файл по проверяемому пути содержит контрольный Command/Wm, с независимо
проверенным legacy hash и 80-байтным managed import frame. Это выполнение
первичной цепочки на контрольном payload, а не запуск кода полного AWM.
Автоматического init при LOAD нет; init вызывается явным Limbo MCALL.
Failing provider помогает отличить отказ LOAD от ошибки внутри явного init.

Три варианта provider проверяют 1296 C/JIT случаев: nil/непустой argv,
кванты 1/3/1000, рост стека, обе стороны C/JIT, две загрузки на успешном пути,
open/read/close faults, повреждённый файл, admission/pin failure и отключение
компиляции. Путь проверяется и в load callback, и на реальном namespace kopen.
Перед C consumer MCALL наблюдаются точный nil Context и тождество argv;
при nil проверяется точный одноэлементный список "-l". Проверяется полное
освобождение всех owners, Types, Module refs, MP, String/List refs и pins.

Constructor дополнительно проверяет каждый allocation failure в C/JIT,
с полным rollback и сохранением REG. Независимый classic oracle сверяет
функции, exports, локальный tuple call, оба поля, LOAD и внешний init.
Три corrupt-report controls портят путь, второе поле пары и callee target.
Два активных native controls меняют literal пути и публикацию Context;
оба обязаны завершаться семантическим predicate до вызова provider.
Sanitizer crash не считается успешным отрицательным тестом.

Запуск:

```sh
python3 utils/tests/native64-typed-primary.py --limbo /path/to/new/limbo
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-primary.py --limbo /path/to/new/limbo
```

Это двадцатый gate для x64/ARM64/RV64/Android ARM64 Bionic и full port driver.
Новые file profiles, wire IDs или managed ABI не вводятся. 32-битный код
не изменён. Полный emuinit/AWM и графический Android startup остаются впереди.

Локальный полный gate проходит x64 ASan+UBSan/NDEBUG с detect_leaks=0:
1296 случаев, оба активных контроля, 21 C / 22 JIT allocation refusals.
Та же матрица и controls проходят в static host-сборке с Android-заголовками
и allocator aliases. Это host-проверки; выполнение ARM64/Bionic подтверждается
отдельно по CI опубликованного SHA.
