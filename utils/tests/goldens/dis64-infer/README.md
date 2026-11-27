# Raw-byte local inference: additional canonical images

Existing Dis64 v1.0/ABI1; no new IDs, checksum extension or public file permission.
Decode each `.hex` with Python `bytes.fromhex`. `manifest.json` gives exact byte
length, SHA256, expected private acceptance, result kind and returned bit pattern.
It contains NO local declarations. Existing export bytes define canonical root0,
leaf entry7/frame1, signatures `() -> void` / `(kind) -> kind`; frame sizes112/88,
alignment8 and zero maps. Result address is bound to the caller's final cell104.

Kinds1/2/3 are Int32/Big/Real. Normal cases return42 (Real is the IEEE bit pattern
of42.0). The unused-parameter case still requires a raw-signature parameter seed.
Real has no immediate return fixture. Other cases test intersecting/misaligned
spans, reinterpretation and reading an uninitialized source. All preserve framing.
These14 bytes supplement the unchanged25 images in ../dis64-calls.

Run `python3 utils/tests/dis64-call-infer.py`: exact corpus verification, poisoned
external declarations, raw inference plus complete semantic proof, then actual
C/RX runtime bodies through explicit bounded transports. Public loader remains
closed. `--write-goldens` is an explicit reviewed regeneration, never implicit.
See doc/native64-series0101.md for authority, runtime and target evidence limits.
