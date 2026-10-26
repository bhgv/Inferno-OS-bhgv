# Decimal conversion preflight

AMD64/ARM64 CVTCW, CVTCL and CVTCF validate complete source and destination slots
before loading a String or executing a floating conversion. Present String
owners must have a nonwrapping inline-header span before len/max loads. H keeps
its existing zero result. Numeric syntax, magnitude limits, negative zero,
termination checks and fallback behavior remain covered by the functional matrix.

C conversion operators validate both slots before string2c can terminate ASCII
or prepare a Rune UTF-8 cache. Invalid destination slots therefore cannot mutate
the source before raising exHeap. string2c checks the complete header, including
the cache-pointer field, before its first metadata read. Nil/H owner semantics
are unchanged. Header-wrap errors precede length-dependent errors, because the
length cannot safely be read first. The reduced UTF-8 oracle reflects that order.

These arithmetic guards do not establish that arbitrary pointers name mapped
allocations. Tests preserve the distinction between independent WORD models and
native host pointers. Full Inferno boot and physical ARM64 remain separate.

The extra preflight uses the backend's existing 128-unit private buffer. Decimal
instruction/layout fixtures now use that actual capacity and retain partial
publication/canary checks. Production LeafMax and the layout ABI are unchanged.

Validation:

* `decimal-native.py`: 2,543,616 C/AMD64 RX functional cases across WORD32/64,
  plus 276 preflight cases. The complete test passes ASan/UBSan/NDEBUG. Invalid
  sources/destinations, wrapping owners, ASCII termination and Rune cache work
  preserve full source/slot/REG images, FP status and errno on rejection.
* `DECIMAL_VM_PARSER=1 DECIMAL_VM_BIG_PARSER=1 DECIMAL_ACCEPTED_ONLY=1`
  with `decimal-native.py`: 2,202,624 comparisons against actual lib9 parsers on
  the natively accepted input subset, plus the same preflight cases. Unrestricted
  fallback semantics are not equated with the host libc oracle.
* `python3 -O decimal-jit.py`: 915,555 AMD64/ARM64 parser/alias/layout/FP and
  canary checks. Read hooks forbid String reads in the added invalid slot/owner
  cases; write hooks detect even same-value publication. Probe environment
  variables select destination or owner failures independently.
* `decimal-layouts.py`: 2,106,300 sizing/publication/canary probes across both
  production emitters, including distant fields and insufficient capacities.
* `utf8-view-spans-c.py`: 840 termination/cache/address/preparation cases with
  real backing storage and reduced arithmetic addresses.

Previous emitters independently fail the destination-ordering and wrapping-owner
probes on AMD64 and ARM64. Previous C conversion/string2c bodies independently
fail decoded-slot and owner-header probes. New checks use explicit conditions
and remain active under NDEBUG and optimized Python.

Guarded native availability stays 164/173 for AMD64/ARM64. This is improved
validation of existing opcode paths, not an increase in fully native commands.


Adjacent decimal-span driver capacity
------------------------------------

The independently registered decimal-spans-jit.py also uses production's existing
128 four-byte-unit capacity. Its previous 64-unit buffer rejects a valid expanded
ARM64 path after the slot/header preflight changes, before executing its tests.
The driver now verifies NULL-output sizing and every smaller caller capacity for
zero publication with canaries, then executes the unchanged complete span/image/
write/FP matrix. Production LeafMax, emitted code and VM ABI are unchanged.
