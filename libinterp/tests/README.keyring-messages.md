# Keyring message and key parsing regressions

`keyring-messages.py` extracts the production byte-array validator, framed-stream
and record I/O functions, and real emulator `kprotect`/`krethrow` implementations.
It uses the actual generated builtin frames and Heap/Array/String declarations.
Kernel I/O and VM allocation are controlled fixtures, not live network or GC.

```sh
python3 libinterp/tests/keyring-messages.py
python3 libinterp/tests/keyring-keys.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  ASAN_OPTIONS=detect_leaks=0 python3 libinterp/tests/keyring-messages.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  ASAN_OPTIONS=detect_leaks=0 python3 libinterp/tests/keyring-keys.py
```

Message checks cover:

- Complete and partial reads/writes, zero progress, native errors, every header
  byte mutation, truncated bodies, decimal count boundaries and empty errors.
- Record transport markers, truncation at 4095 payload bytes, bounded diagnostics,
  and single-write semantics. Stream short writes can be continued; record short
  writes must fail without emitting a second record.
- Native-buffer cleanup when VM result or diagnostic construction throws, using
  real Proc error frames. The old single `getmsg` result stays owned until a new
  value is constructed; ordinary I/O failure still returns nil.
- Counts and byte-array metadata checked before narrowing native lengths, using
  the host WORD layout and a 32-bit WORD model. Counts above 32 bits are tested
  when the host model can represent them. These are not full 32-bit target runs.

`keyring-keys.py` runs the production base64 decoder, libmp import/export and
allocation functions, cryptographic key destructors, and all nine RSA/DSA/ElGamal
key/signature parsers. It compares actual decoded values and each field in its
real structure, including distinct DSA p/q. All missing-field, malformed-field and
injected import-failure positions must release partial allocations. Success
advances to the next field; failure leaves a deterministic original cursor.
Padded and unpadded base64 are supported. Invalid alphabet, misplaced padding,
empty fields and inputs exceeding the existing 768-byte decoded scratch capacity
are rejected instead of accepting zero or silently truncated integers.

The integer import failure injection returns nil before calling libmp; it does
not simulate libmp's process-fatal allocator failure path. The tests do not
validate mathematical key parameters or perform an authentication handshake.

Original-function substitution reproduces the record hangup buffer corruption,
acceptance of malformed stream headers, a native receive-buffer leak on VM
allocation failure, and the DSA duplicated-field/cursor regression. The generic
`lib9` base64 decoder keeps its tolerant public behavior; strict validation is
local to the serialized cryptographic integer parser.

## Кратко по-русски

Исправления защищают обмен сообщениями Keyring от повреждения буфера ошибки,
неверных заголовков, неполных записей и утечек при исключениях VM. Пустое сообщение
об ошибке сохраняет свой тип. Проверки размеров выполняются до сужения длины.

Разбор ключей RSA/DSA/ElGamal прекращается на первом неверном поле и освобождает
частичный объект. DSA больше не читает `p` повторно вместо `q`. Слишком длинное
base64-число отвергается целиком. Числовые параметры ключей на криптографическую
корректность эти проверки не проверяют; сетевое рукопожатие не моделируется.
