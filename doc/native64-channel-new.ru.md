# Native64: создание primitive channels из checked Limbo

Checked LOAD lowering принимает INEWCB/INEWCW/INEWCL/INEWCF/INEWCP:
byte, int/fix, big, real и один mapped pointer payload. Для INEWCP
проверяется исходный тип payload: String, Ref, Chan, Array, List или Module.
Raw и mapped aggregate payload пока остаются закрытыми.

Ёмкость — signed Word из FP/MP, immediate либо отсутствие middle operand.
Выход — mapped FP/MP cell или проверенный borrowed caller result address.
Lowering проверяет nominal channel type и соответствие opcode payload.
Owning module до allocation проверяет границы/alignment, scalar capacity map,
root destination map и отсутствие aggregate result contract для borrowed
выхода. Здесь нет новых serialized records и новых private annotations.

C interpreter использует production newc/channelargs/cnewc, JIT — production
cnewcscalar либо обычный VM fallback. Buffer allocation и его root-cell
initialization используют production heaparray/initarray; freechan/freearray
освобождают очереди, payloads и descriptor retains. Classic wire bytes и
32-bit paths сохраняются.

## Проверки

`utils/tests/native64-channel-new.py` компилирует 13 полных Limbo functions.
Независимый classic oracle обнаруживает 78 повреждений opcode/mode/operands/
result records и подтверждает одинаковые classic bytes. Отдельный source
boundary допускает все шесть pointer payload classes и отвергает raw/mapped
aggregate constructors.

1938 C/JIT случаев с quanta 1/3/1000 включают:

- пять builtin payloads и точные Type/mover identities;
- FP, MP, constant и zero capacities, signed int limits и отрицательные размеры;
- прямой caller result, FP temporary, global result и повторное создание;
- ошибки Channel allocation, обеих очередей и buffer allocation;
- сохранение старого результата, инициализацию всех pointer buffer cells;
- уничтожение настоящего String payload при освобождении buffer;
- полное освобождение frames/modules/channels/buffers и всех Type retains.

24 metadata refusals проверяются до allocation с неизменными owner/REG.
Проверены все 35/36 constructor allocation failures. Active controls меняют
материализованную ёмкость и удаляют scalar/root-map guard ёмкости; каждый
обязан нарушить соответствующий semantic predicate.

Gate включён в native64-port.py и CI Linux amd64/arm64/riscv64, Android ARM64
Bionic. Локальное исполнение относится к amd64; channel send/receive и Android
UI этим gate не проверяются.

## Следующий барьер

Неизменённый AWM init проходит INEWCP на PC120. Первый отказ теперь PC127:
IFRAME для ISPAWN на PC131. Проверка startup modules сохраняет bytes всех
16 classic artifacts. Полный AWM init и event loop ещё не запущены.

Wmsrv.init по-прежнему упирается в INEWCMP на PC27: его составной mapped
payload требует переноса descriptor с полной layout/map proof. Primitive
channel allocation не означает готовность серверного Wmsrv.

Локально прошли UBSan и ASan/UBSan с обоими active controls
(`ASAN_OPTIONS=detect_leaks=0`; fixture отдельно проверяет полное освобождение).
Регрессии: 318 Context cases с тремя controls, 400 channel-triple cases с
четырьмя controls, полный emuinit/Arg и 324 настоящих C/JIT boot-процесса.
Чистая amd64 console и paired boot также прошли.

Продолжение: [mapped tuple channels](native64-mapped-channels.ru.md)
открывает двух- и трёхкорневые tuple payload и переносит Wmsrv.init с PC27
на spawn setup PC30. Mixed payload остаётся закрытым.
