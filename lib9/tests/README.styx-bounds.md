# Styx message size and frame boundaries

Run `python3 lib9/tests/styx-bounds.py` from the repository root.

Each production codec (lib9, libkern, emu/port/styx.c) runs 6,288 cases.
The fixture compiles real encoding, sizing and decoding bodies with the shared
protocol declarations; the emulator copy runs independently, without stubbing
its codec functions.

Coverage includes all supported request/reply types, independently calculated
wire sizes, every serialized field, unaligned buffers, walk counts 0/1/16,
string lengths 0 through 65,535, nil strings, exact output capacities and short
output buffers. Each supported message is truncated at every byte position;
invalid declared sizes must not cause string rewriting in a following frame.
Valid frames retain their existing acceptance of trailing buffered bytes.
A deterministic 4,096-frame mixed-byte corpus and hostile 32-bit data counts
exercise malformed inputs under sanitizer instrumentation.

Independent invalid-input families cover unsigned message-size overflow,
oversized walk arrays, 65,536/65,537-byte strings, missing nonempty payloads and
zero-byte nil payloads. Sizing opaque data remains possible before assigning
its storage. Tread/Rwrite counts remain unrestricted by inline-payload checks.
The exact UINT32_MAX total size is accepted by the sizing function; the next
byte is rejected without trying to allocate a giant message.

Strict UBSan is the default. To add ASan:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 lib9/tests/styx-bounds.py
```

LeakSanitizer process inspection is disabled; other instrumentation remains on.
`STYX_M2S_SOURCE`, `STYX_S2M_SOURCE` and `STYX_EMU_SOURCE` select individual
alternate production bodies. `STYX_CASES=valid|encode|decode|empty` isolates
families; with `encode`, `STYX_ENCODE=overflow|walk|string|payload` narrows them
further. Original decoder bodies fail the frame-isolation assertions; original
encoder bodies independently fail the overflow, array, string and empty-payload
regressions. This tests codecs, not network transport, filesystem scheduling,
a native kernel boot or a complete target ABI.
