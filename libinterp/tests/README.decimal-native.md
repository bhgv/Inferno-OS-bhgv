# Short decimal native paths and adjacent string validation

This checkpoint was based on experiment cc9c319, including the adjacent LP64
number/big-constant/Styx changes (3c6f5dc) and compiler-local byte descriptors
(cc9c319). The existing compiler-local descriptor acceptance is retained;
its indentation now follows the surrounding runtime source.

AMD64 and ARM64 now classify CVTCW and CVTCL as LeafDecimal. They parse ASCII
strings with an optional leading '+' or '-' and at most nine decimal digits.
Empty strings, sign-only strings and language nil produce zero. Accumulation
is exact within signed 32-bit range; CVTCL and legacy WORD64 sign-extend the
same result. Whitespace, trailing text, embedded NULs, longer numbers and Rune
representations decline to the existing C parser before any store. C-null
owners also decline, preserving the VM's error path.

The ASCII terminator must already be zero. string2c ordinarily writes that
terminator; declining a nonzero terminator preserves that observable mutation
in C rather than silently skipping it. All input bytes are captured before
the output store, including exact owner-slot aliases and output inside the
source object. There are no helper calls, allocations, reference changes,
cache writes, frame/PC changes or FP instructions in these native paths.
Only caller-saved registers are used. Existing 64-unit staging/RX publication
limits remain unchanged; AMD64 short success branches are range-checked.

The production C string span entry c2string now rejects H for a nonempty
buffer before pointer arithmetic or scanning. Empty H spans retain the same
behavior as empty C-null spans. Sys UTF rejects nonempty H backing storage
before decoding, encoding or writing any result. Tests also cover C-null/H
local type pointers and pointer-bearing byte-sized local descriptors, with
exact error identities and unchanged outputs. These checks complement the
new structural acceptance of compiler-local pointer-free byte descriptors.

Validation commands:

```
python3 libinterp/tests/decimal-native.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/decimal-native.py
python3 libinterp/tests/decimal-jit.py
SYS_UTF_CASE=bounds CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-utf.py
STRING_SLOT_CASE=vm CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/module-byte-descriptors.py
python3 libinterp/tests/slice-tokenize-suite.py --core
python3 libinterp/tests/jit-coverage.py
```

The native fixture runs 159744 cases per WORD model through actual published
AMD64 code and actual cvtcw/cvtcl/string2c. It compares complete operand and
source images, source/destination alignments, exact aliases, interior output,
nil/null owners and nonterminated fallbacks in all four FP rounding modes
with preexisting exception flags. Rune helpers are assertion adapters and are
not claimed as a native Rune execution test.

The Unicorn fixture independently models full images above 4 GiB, both WORD
models, both output widths and all byte values in the scanned digit position.
It checks malformed lengths, unterminated strings, exact aliases, interior
outputs, write-free fallback, FP control/status and staged capacity rejection.
It executes the actual staged AMD64/ARM64 instructions, not a translated parser.
The complete byte matrix passes 485792 checks across both architectures.
The runtime publication matrix now contains 77 actual Dis instructions and
checks allocation/protection failures, retries, rejection, ownership and unmap.

The original host number and big-constant drivers and the compiler-local byte
matrix are now part of the 75-driver core suite. The number driver additionally
passes at -O3 under UBSan. Styx source and its new test were reviewed; its Limbo
runtime command was not executed in this host-only checkpoint. Replaying the
parent string/UTF sources fails the added H-span tests (UBSan pointer wrap in
c2string and invalid access in UTF).

The full string-return matrix under ASan fails after the sandbox's
unsupported nonlocal-jump stack handling: after the missing-/proc warning, ASan's
internal reporting/formatting path reports a stack underflow. The complete
matrix passes UBSan, and the VM-string/UTF subset passes ASan+UBSan. Replaying the parent C sources reproduces the same ASan internal
stack-underflow diagnostic. This is not presented as a passing full ASan
string-return run; complete sanitizer acceptance remains unresolved here.

Coverage is now AMD64 130/43 and ARM64 130/43, counted as guarded native paths
versus interpreter-only active instructions. RV64+M remains 103/70, RV64+Zmmul
90/83 and RV64I 87/86. There are 173 active instructions. The full native 64-bit
VM/frame/GC ABI port and physical ARM64/RV64 acceptance are still separate.
