# Native dial paths and reply boundaries

`python3 emu/port/tests/dial-paths.py` compiles the complete real
`emu/port/dial.c` and `os/port/dial.c`, replacing only their platform includes
with native I/O adapters. It runs both implementations separately under ASan
and UBSan with `NDEBUG`. Explicit predicates remain active in every build.
The fixture does not allocate; its LSan hook avoids unavailable `/proc`
inspection without disabling address or undefined-behavior instrumentation.

The production changes cover:

- Reject dial strings that cannot fit instead of silently dialing a truncated
  destination; stop rooted protocol searches at the beginning of the buffer.
- Use the clone portion of relative connection-server replies rather than the
  translated destination when constructing a network path.
- Reject clone paths without a slash and release the opened control descriptor.
- Keep all five native read results in `long`, validate their complete width
  before buffer indexing, and release descriptors on oversized replies.
- Reject missing-slash or oversized listen directories before opening a path.
- Bound announce-directory formatting and account for the prefix before
  accepting a returned conversation number.

The matrix checks parsing, rooted and relative translations, descriptor
transfer/cleanup, preserved failure outputs, oversized native counts through
`LONG_MAX`, and combinations of listen-prefix and reply lengths. Each of four
builds passes 16,322 predicates (65,288 total). This is a count of predicates,
not independent network operations or opcode coverage.

Selectors `--case parse|translate|count|listen|announce|clone`,
`--port emu|os`, and `--profile address|undefined` isolate failures.
For example:

```
python3 emu/port/tests/dial-paths.py --revision 9ee638b --case translate --port emu --profile undefined
```

All 24 combinations of those six families and four builds fail against
`9ee638b`, after successful compilation, with an active predicate or sanitizer
diagnostic. To compile old sources, the driver normalizes only the old private
parser's void return signature and adds a successful return value; it does not
change its parsing behavior. The current sources require no normalization.

The adapters provide deterministic kernel return values; this is not a live
network, scheduler-unwind, boot, ARM64-hardware, or complete native-ABI test.

## Complete command writes / Полная запись управляющих команд

The `write` family injects negative, zero, positive-short and oversized native
results into connection-server requests, clone connect commands and announce.
Only an exact count is success. A short control write is not retried as a tail:
these devices interpret each write as a separate command. Existing negative
native errors are preserved; nonnegative mismatches receive an explicit error.
Every opened descriptor is closed on failure and no data descriptor is opened
or control ownership published. The new family fails against 275bc64 before
this correction. Both complete production port bodies run under ASan and UBSan.

Семейство `write` подставляет отрицательный, нулевой, короткий положительный и
завышенный результат записи запросов cs, connect и announce. Успешна только
полная запись. Хвост команды отдельно не дописывается: устройство разбирает каждую
запись как самостоятельную команду. Отрицательная нативная ошибка сохраняется,
для остальных несовпадений задаётся явная ошибка. При отказе закрываются все
открытые дескрипторы; data не открывается, владение control не публикуется.
Тест падает на 275bc64 до исправления. Полные тела обоих портов проверяются
ASan/UBSan; реальные сетевые соединения fixture не создаёт.
