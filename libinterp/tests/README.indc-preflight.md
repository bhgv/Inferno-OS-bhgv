Ownership and concurrent upstream work
--------------------------------------

The production repair is already in b6c160b (which also covers INSC). This
follow-up contributes independent standalone regressions and expands the private
layout/capacity sweep to the existing production limit. It does not replace the
concurrent interpreter/emitter implementation. The original independent repair
attempt was not published after the expected branch head changed.

INDC operand and String-header preflight
=======================================

C INDC and the AMD64/ARM64 guarded leaves check all three decoded operand
slots before reading either source or publishing a destination. Source slots
hold a native String pointer and a VM WORD index; the destination holds one VM
WORD. Native pointers and WORD32/WORD64 remain independent widths. Null, H and
wrapping slots decline/error before external slot reads.

After capturing the String owner, the complete len/max metadata prefix is
bounded before any field read. C preserves unaligned decoded operand support. Nil/H String values retain exNilref; invalid
metadata raises exHeap before bounds evaluation. Valid headers retain signed
representation checks, logical index bounds and selected ASCII/Rune payload
span validation. Only a successful character read writes the destination.

Both native leaves preserve the existing fixed 128-unit private code capacity.
Small test buffers now use that production capacity; every insufficient capacity
still rejects without publishing bytes. Native declines leave VM/heap images
unchanged and let the interpreter report its ordinary exception. Opcode
availability remains AMD64/ARM64 164/173; this repairs input coverage, not a new
opcode. Arithmetic span guards do not prove arbitrary native pointers refer to
live allocated objects.

Validation
----------

- indc-preflight.py: 1920 cases per WORD model; decoded slots, wrapping headers,
  nil/H values, invalid representations/capacities/indices, byte-aligned slots,
  exact error identities and unchanged surrounding data. ASan/UBSan/NDEBUG passes.
- indc-preflight-jit.py: 1286 AMD64 and 1272 ARM64 capacity/read-hook/image checks
  over both WORD widths and three metadata layouts, including a 128-byte offset.
- Six parent replays fail independently: C slots/header and each native emitter's
  slots/header. INDC_CASE, INDC_ARCHES and INDC_PROBES select focused replays;
  INDC_PREFLIGHT_SOURCE and EMITTER_DIR select committed parent sources.
- Existing C string-cell spans: 7260 checks per WORD model. The reduced-address
  oracle now accounts for earlier invalid-header errors; decoded slots retain
  native arithmetic rather than inheriting the reduced String address model.
- Existing instruction string-cell suite: 13120 AMD64/ARM64 span/image/write
  cases; layout suite: 1053150 capacity/canary checks per architecture.
- Production C/AMD64 RX string-store suite: 253952 cases per WORD model.

ARM64 evidence is instruction emulation, not physical hardware execution or a
full Inferno boot. The new tests are registered in the owning checkpoint suite.

Кратко по-русски
----------------

Проверка INDC сначала ограничивает все слоты декодированных операндов, затем
полный заголовок строки. Ошибочный вход не читает запрещённые области и не
публикует результат. Исправлены настоящий C-интерпретатор и оба JIT: x64/ARM64.
Старые реализации отдельно воспроизводят ошибки слотов и заголовков. Форматы,
ABI и лимит private code не меняются; эмуляция ARM64 не является проверкой железа.
