# Original Type-ordinal construction golden

Existing Dis64 v1.0/ABI1, canonical five sections, no new IDs or public permission.
Decode reordered-types42.hex with bytes.fromhex; manifest pins exact SHA256/length.
Root entry0 uses frame ordinal1 (112 bytes); its int32 helper entry7 uses frame0
(88 bytes). FRAME explicitly selects ordinal0; root result tail is byte104.
Both descriptors are aligned8 with zero maps. Result42. This detects accidental
normalization of frame indices during actual Module/Type construction.

Run python3 utils/tests/dis64-call-construct.py alongside the unchanged39 peer
inputs. The actual load64 constructor is executed, all tracked allocation refusals
are injected, source bytes are poisoned after success, actual C/RX runs/unloads
continue, and four strict source controls verify ownership/mode/lowering.
The constructor remains unpublished and is not readmod/file CALL admission.
