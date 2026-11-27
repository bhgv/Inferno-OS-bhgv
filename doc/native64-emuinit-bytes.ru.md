# Byte arrays в checked native64 startup

LOAD lowering допускает только прямые array of byte для INEWA/INEWAZ,
ISLICEA и ICVTAC. Compiler проверяет элементный тип, декларацию, адресацию
и байтовый descriptor. INEWA по прежнему выбранному native правилу становится
нулевым INEWAZ. Возврат ICVTAC допускается только в String result через
существующий проверенный borrowed result address. Int arrays, поля агрегатов
и прежний external-only профиль этим путём не открываются.

Private NEWAZ token 0 означает byte descriptor, а не ordinal исходного Dis.
Owning module builder добавляет отдельный Type(size=1,np=0) после всех frame
и import Types. До JIT compilation каждый NEWAZ получает его настоящий
ordinal. Imports/frame ordinals сохраняются; modules без NEWAZ не получают
добавочных allocations. Элементный Type удерживается настоящим heaparray
и освобождается через настоящий freearray/freetype. Token предназначен для
этого owning builder, не для самостоятельного запуска bridge без Type setup.

Перед allocation проверяются полные Word/root spans и точные map роли,
индексы slice, encoding, token и String return contract. File admission и
classic 32-bit bytecode/ABI не расширены.

Новый native64-emuinit-bytes.py компилирует две полные Limbo функции:
выделение массива и срез входного массива с преобразованием в String.
Сверяются classic operands/relocation и замена descriptor token; int arrays
и старый профиль обязаны отказать. Исполняются настоящие newarray/newaz,
arrayspan/slicea, heaparray/initarray/freearray и cvtac через C/JIT, реальные
MFRAME/MCALL и managed return. Проверены 504 случая с ростом стека и тремя
квантами, ASCII/NUL/Unicode, нулевыми размерами, сохранностью входного массива
и canaries. Для каждого успешного пути перебраны все allocation failures
исполнения, с полным retirement массива, view, String и descriptor.

Constructor проходит все 14/15 allocation failures C/JIT, включая отдельный
primitive Type, с rollback и неизменным REG. Восемь повреждённых metadata
отвергаются до allocation. Активное отключение map-role проверки или
разрешения primitive ordinal выявляется до небезопасного исполнения.
ASan/UBSan -O1/NDEBUG и статическая host-сборка с Android headers проходят;
последняя не является выполнением ARM64 Bionic. Целевые gate добавлены в CI.

Настоящий неизменённый getenv теперь проходит прежний INEWA PC112 и
останавливается на PC113: Sys.read подготавливает вычисленный ILENA аргумент
`len buf` прямо в pending frame. Поддержка такого argument lowering остаётся
следующим шагом. Полные getenv/emuinit/AWM ещё не исполняются.

Также исправлены оставшиеся контрольные мутации после переноса pilot flow
helper и добавления смешанного result contract: sys-flow-gc следует
operands.h через полное shadow include дерево; local/multi tests выбирают
нынешний map-role predicate. Прежние semantic failure markers сохранены.

Следующий проверенный этап: [полные getenv и unquoted](native64-emuinit-getenv.ru.md), C/JIT с настоящими телами Sys open/fstat/read и управляемым файловым транспортом. Полный startup ещё не запущен.
