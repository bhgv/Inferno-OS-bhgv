# Unicode scalar literals in Regex / Unicode-литералы в Regex

Regex's private syntax/FSM tags previously began at `1<<16`, and its end-of-input
sentinel had the same value. Those integers are valid Unicode scalars U+10000
through U+1000B. A literal U+10000 was rejected as end of input; other literals
could be interpreted as internal operators instead of characters.

The tags now begin at `1<<21`, outside the valid Unicode scalar range. The parser
sentinel uses the tag base, avoiding an independently maintained magic number.
The public compile/execute signatures and capture representation are unchanged.
Clients using internal tags should be rebuilt together with the module. Arena
images are internal VM objects, not a portable serialized regexp format.

`regex-unicode.b` uses the actual separately compiled Regex module. It covers
ASCII/BMP representatives, all twelve previous tag collisions, an emoji and
U+10FFFF. Each rune is checked as a literal, escaped literal, repeated capture,
character class and alternative. Returned offsets must count runes rather than
UTF-8 bytes. Invalid syntax remains invalid. The matrix has 85 checks per run.

An optional hosted integration driver uses the companion FlashVM NC64 compiler
and DisVM. From an Inferno checkout beside FlashVM:

```sh
TMPDIR="$PWD/../flashVM/build/tmp" python3 appl/lib/tests/regex-unicode.py \
  --compiler ../flashVM/build/runtime/bootstrap/limbo-bootstrap \
  --runtime ../flashVM/build/runtime/disvm
```

It compiles the library and regression into a private directory, then executes
JIT/interpreter modes at quanta 1 and 2048. It rejects FAIL output even if the VM
entry returns normally. No host regexp engine substitutes for Regex. Compiler
and runtime must have matching NC64 ABI; this does not establish classic/NC64
interchange or a physical ARM64 host run. The library remains ordinary Limbo.
`--library` and `--includes` allow replay against unmodified preceding sources.
The preceding library/interface fail at U+10000 with an active compile-result
check; the corrected pair passes all four runs.

## По-русски

Внутренние теги синтаксиса и автомата Regex начинались с `1<<16`, совпадая с
допустимыми символами U+10000…U+1000B. U+10000 ошибочно принимался за конец ввода;
остальные могли интерпретироваться как операции автомата. База тегов перенесена
на `1<<21`, за пределы диапазона Unicode. Маркер конца ввода использует эту базу,
без второго независимого числового значения.

Публичные сигнатуры compile/execute и массивы границ захватов не меняются.
Клиентов, обращающихся к внутренним тегам, следует пересобрать вместе с модулем.
Объект Arena не является переносимым форматом сохранения регулярных выражений.

Регрессия проверяет 17 символов в пяти формах — 85 случаев за запуск: литерал,
экранирование, повтор с захватом, класс и альтернатива. Включены все 12 прежних
коллизий, emoji и U+10FFFF. Смещения проверяются в рунах, а не байтах UTF-8.
Синтаксические ошибки продолжают отклоняться.

Команда выше запускает настоящую библиотеку на совместимой паре NC64-компилятора
и DisVM в двух режимах и с двумя квантами. Это проверка библиотеки на VM, а не
новый синтаксис regexp-срезов в компиляторе, не отдельный regexp-JIT и не проверка
на физическом ARM64. Неизменённые предыдущие исходники воспроизводят отказ на
U+10000 через `--library` и `--includes`.
