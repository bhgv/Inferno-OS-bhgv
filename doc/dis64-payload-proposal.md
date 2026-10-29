# Dis64: следующий обмен по payloads

Дата: 2026-10-29. **Предложение**, не принятая часть ABI1. Accepted envelope и
registry уже реализованы отдельно. Эти поля нельзя включать в compiler/runtime
до явного ответа FlashVM; здесь нет executable target или новых opcode IDs.

## Module metadata — конкретный кандидат

Один module payload, BE u32 поля в следующем порядке:

1. name_string_id — non-nil pool string; правила путей/import identities отдельно.
2. profile — кандидат0 library,1 executable (внутреннее поле, требует согласия).
3. data_bytes — размер runtime module data.
4. При data_bytes!=0: data_type_id, descriptor точно покрывает data_bytes.
5. Только для executable: entry_pc и entry_frame_type_id.

Library не имеет entry и автоматически не запускается при импорте. Entry
валидируется против decoded instruction_count и Frame64 descriptor. Header
profile/entry не надо дублировать в новой специальной section. Предлагаю exact
payload length по выбранному profile, без игнорируемых хвостов.

## Types, strings, code — предложения к уточнению

- Types: count:u32; IDs — порядковые индексы. Для каждого: object_bytes:u32,
  alignment:u32, затем полная map8 ceil(object_bytes/64). Общие проверки уже
  реализованы в dis-layout.h. Предлагаю пока structural descriptor, без нового
  type-kind ID; назначение entry frame/module data задаёт referencing field.
  Но Frame links/result address/temps нельзя считать roots: точные descriptor/
  safepoint правила остаются обязательным отдельным решением для исполнения.
- Strings: count:u32, затем count non-nil records rune_count:u32 + BE Rune32.
  Nil остаётся отдельным initializer, не специальным строковым count и не
  неявным pool index. Полный payload должен расходоваться точно. Pool indices
  могут ссылаться на одинаковое содержание; string case unique/order проверяются
  по содержимому. Прошу явное согласие на pool-level count/concatenation.
- Code: instruction_count:u32, затем поток инструкций. Предлагаю сохранить
  existing byte opcode/address modes и compact signed30 operand encoding там,
  где семантика ABI1 явно определена. Декодируется ровно instruction_count
  инструкций, конец совпадает с концом code payload. Нужна согласованная
  таблица roles для адресов ref64 и control-table references.
- Для CASE/CASEL/CASEC/GOTO предлагаю table reference как nonnegative signed30
  immediate table_id вместо offset в mutable module data. Runtime декодирует
  его в ссылку на отдельную typed immutable table. Это кандидат operand-role,
  не разрешение переосмыслить старые файлы под classic/NC64 discriminator.

## Что нужно от FlashVM

Прошу принять/уточнить module profile/fields, types layout и pool concatenation,
а также дать точный вариант code operand roles, imports/exports signatures и
initializers/relocations, соответствующий собственному Coco/R→FIR backend.
Inferno подстраивается к общему расширяемому контракту; bootstrap frontend не
заменяет основной compiler path FlashVM. Импорты не сводим к имени и старому
32-bit hash без ABI/structural interface проверки.

После согласования нужен один настоящий module golden, затем интерпретатор,
реальные C calls/ref-result GC/unwind и JIT. Opaque envelope с метками M/C/T
не считается таким модулем. Signed-module cryptographic format в эту серию
не входит; Tk/security/crypto и исключённые JIT backends не меняются.
