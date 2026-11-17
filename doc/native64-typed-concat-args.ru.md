# Конкатенация String в аргументе импортного вызова

В emuinit.getenv выражение `sys->open("#e/"+v, Sys->OREAD)` компилируется
как MFRAME, IADDC прямо в String-поле pending frame, MOVW mode, LEA result,
MCALL. Раньше checked external parser принимал только MOV-инструкции
аргументов и отказывал на этом вызове с external-argument.

Теперь только выбранный checked LOAD путь принимает IADDC для фиксированного
импортного String-аргумента. Оба источника обязаны иметь точный String type
и прямой FP/MP root; middle offset должен помещаться в 16 бит. Проверяются
ordinal, prepared frame, класс формального аргумента и classic destination.
Сохраняются запреты входа ветвления в setup island и обычные result contracts.
Static Tany/nil в raw IR не заменяет String type; динамический String со
значением nil проходит обычную семантику addstring.

Native builder проверяет обе полные 8-байтные ячейки источников и mapped
String-поле импортного кадра. До выделений module validator повторно проверяет
caller FP/MP maps, принадлежность инструкции импортному setup island,
callee frame geometry и destination map. Materializer переносит middle offset
в reg и публикует результат через pending frame в FP40. Применяются настоящие
addc/addstring/stringdup и обычное освобождение mapped кадра; сырого копирования
managed указателей или нового wire opcode не добавлено.

Это не общий допуск String arithmetic во всех checked функциях, не computed
local-call argument, не вариадический вызов и не расширение файловых профилей.
Scalar/external-only профиль по-прежнему отклоняет такой source. Публичный ABI,
wire IDs и 32-битные пути не меняются.

## Проверка

```sh
python3 utils/tests/native64-typed-concat-args.py --limbo /path/to/new/limbo
python3 utils/tests/dis64-typed-concat-args.py
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 utils/tests/native64-typed-concat-args.py --limbo /path/to/new/limbo
```

Контрольный Paths.open имеет тот же String/int argument prefix, что Sys.open,
но возвращает String. Сам Sys.open и Sys->FD этим gate не выполняются.
Четыре consumer-функции проверяют fresh/forwarded return, обе очередности
FP/MP и FP/FP. Проверяются ASCII, Rune, embedded NUL, nil/empty, алиасы,
повторная замена результата, ошибка provider, OOM первого/второго concat и
полное освобождение owners, Types, MP и Modlink refs. Матрица C/JIT:
4032 случая, кванты 1/3/1000 и рост стека.

Compiler unit проверяет десять неверных operand/type/frame вариантов,
профильную границу и активное отключение exact String class guards.
Независимый classic oracle проверяет источники, middle, destination, opcode,
import hashes и exports; четыре corrupt reports обязаны отвергаться.
Двенадцать metadata refusals происходят без выделений; все constructor
allocation failures проверяют rollback и неизменность REG. Активные controls
отдельно отключают source map, destination map и портят перенос middle offset;
семантический observer останавливает последний до чтения неверного указателя.
C consumer дополнительно наблюдает String/scalar аргументы перед MCALL;
все C/JIT комбинации проверяют полный результат и ошибку provider.

Gate concat-args включён двадцать первым в target CI и full port driver.
На настоящем исходнике getenv первая причина отказа сдвигается с MFRAME
Sys.open (PC96) к следующему сравнению ref Sys->FD с nil (PC101,
load-nil-roles). Это ещё не выполнение полного getenv или AWM.

Полный gate проходит локально x64 ASan+UBSan/NDEBUG с detect_leaks=0:
4032 случая, три native omission controls, exact-class compiler control,
23 C / 24 JIT constructor allocation refusals. Та же матрица и controls
проходят в static host-сборке с Android header/allocator namespace. Это
host evidence, а не выполнение на ARM64/Bionic. Прежние external, String MP
imports и raises gates с controls, свежая console/paired boot проходят.
