# Scalar Dis64 cross-reader and live-local layout review

This reverse contribution from FlashVM is an explicit interoperability gate for
Inferno's restricted Limbo `-6` backend. It does not change the file profile or
default execution policy. The test generates 64 layouts containing zero through
seven Big locals crossed with zero through seven WORD locals. Locals remain live
across a WORD expression that uses compiler scratch cells. The program checks
both those locals and the expression result.

Supply a real source-built compiler and external semantic inspector:

```
python3 utils/tests/dis64-external-reader.py \
    --limbo /build/tools/limbo \
    --other-limbo /build/flashvm/limbo-bootstrap \
    --reader /build/flashvm/dis64inspect
```

`--other-limbo` is optional and requires byte-identical complete output from the
second compiler. Every source also compiles to ordinary Dis before and after
scalar compilation; those ordinary products must be identical. The reader must
report an admitted scalar pilot, closed general execution, zero entry PC and a
reserved entry frame, then rewrite every byte unchanged. Independently located
frame maps are mutated to introduce a managed root; all 64 must be refused and
the rewrite destination must remain unchanged. This is an invalid-image control,
not evidence that arbitrary mapped programs are supported.

Optionally supply a complete experimental emulator and its root:

```
python3 utils/tests/dis64-external-reader.py \
    --limbo /build/tools/limbo --reader /build/flashvm/dis64inspect \
    --root /build/native64 --emu /build/native64/emu/Linux/o.emu-g
```

`--runner` can select an explicit target executor. Each of the 64 programs runs
with JIT disabled and enabled. A deliberately wrong expected answer adds two
real divide-error cases. All 130 OS processes require the matching completion
marker and the late empty-scheduler marker after retirement. A timeout, an early
diagnostic alone or a successful host compile cannot satisfy this oracle.

Executed on native Linux x64 using two independently built compiler products,
the complete classic-mk emulator and FlashVM's built inspector. This is not
physical ARM64/RV64 execution, a full shell, managed transport or file CALL/MCALL
admission. The compiler's scratch tail preserves overlapping scratch byte ranges;
these fixtures exercise layout changes without inferring semantic fields from a
zero GC map.
