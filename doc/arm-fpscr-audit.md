# Linux/Android ARM FPSCR

The ARM lib9 interface now uses VFP FPSCR fields instead of x87 masks:
rounding is bits 23:22, five IEEE trap enables are bits 12:8, and accumulated
exception flags are bits 4:0. Plus/minus infinity rounding encodings are mapped
to ARM's order, which differs from the old x87 definitions. VFP has no global
single/double precision selector, so the obsolete precision masks are zero.

`setfcr` merges only rounding and the five enables; `setfsr` merges only the five
accumulated flags. Neither setter can overwrite the other's fields. Both also
preserve NZCV, QC, IDC/IDE, FZ, DN, AHP and unrepresented native fields. The lib9
getters expose only fields represented by the Math API. Input-denormal status
is not falsely reported as IEEE underflow.

`include/arm-fp.h` selects hardware register access from the compiler's
`__ARM_FP` target and retains `USEVFP=1` for older compilers. Explicit
`USEVFP=0` or `ARM_FPSCR_VFP=0` disables it. The no-hardware getters return zero
instead of leaving an undefined return register; setters are no-ops. This does
not add software floating-point environment emulation.

Android now passes the same `ARMFPFLAGS` to C and assembler. Linux provides the
same optional variable; its default retains the cross-toolchain target defaults.
All target FP options/overrides must be shared by C and assembly. ARM lib9 entry
points support A32 and Thumb code with the appropriate symbol annotation; the
JIT itself still emits A32. Android's manual size_t/ptrdiff_t/NULL definitions
were replaced with standard stddef.h, which permits the host regression bridge
without changing the ARM32 native type widths.

## Reference

Arm ARMv7-A/R architecture manual, FPSCR register description:
<https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e>.

Cortex-A9 FPU manual, rounding/control fields and implementation restrictions:
<https://documentation-service.arm.com/static/5f037bddcafe527e86f5c293>.

Trap-enable availability is implementation-dependent. Writing the right bits
does not make hardware trapping available on implementations where the enables
read as zero. OS signal delivery is not established by these changes.

## Checks

```sh
PYTHONPATH=/path/to/arm-test-deps python3 libinterp/tests/arm-fpscr.py
```

The script needs Python Keystone, Capstone, Unicorn and a host C compiler. It
compiles the actual Linux/Android FPcontrol C sources with their actual headers.
It preprocesses the actual lib9 assembly for eight target/override cases and
both A32/Thumb, assembles with Keystone and executes with Unicorn. Setters'
proposed VMSR values are inspected before Unicorn applies its hardware mask.

All five enable combinations and four rounding modes are checked through the
Math C bridge using an independent native-register model. Separate integrated
checks run Math -> actual ARM lib9 assembly -> VFP division and verify directed
rounding, inexact, invalid, division by zero, overflow and underflow. Random
register values exercise preservation of unrelated fields. The current suite
performs 39970 checks. Its no-VFP cases verify defined getters and no-op setters.

Unicorn implements trap enables as read-as-zero; the model checks their mapping,
not actual trap delivery. There is no ARM SDK cross-link, OS signal test or
physical-device run in this harness. The emulator ARM FPsave/FPrestore functions now save/restore the complete FPSCR
in the first aligned uint32 of FPU, retaining the 28-byte context footprint.
The no-VFP save writes a defined zero and restore is a no-op. The suite switches
two contexts repeatedly, checks buffer canaries and verifies independent Math
rounding/status followed by VFP arithmetic in each. Host assertions compile the
actual Linux/Android FPU layouts. FP register values continue to obey the C ABI;
the JIT does not keep floating operands live across helper/scheduler calls.

Real scheduler/GC/signal interaction still requires an emulator integration run.
A separate issue remains in newprog: copying a parent's stored FPU context can
miss changes made in its current scheduling quantum before spawn.
