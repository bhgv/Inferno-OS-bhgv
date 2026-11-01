# Scalar source locations: private tooling and shared proposal (series 0055)

The restricted `limbo -6` path now has optional diagnostic instruction/source
correspondence. This changes no image byte, accepted opcode, wire ID, entry ABI,
GC map or native-call witness. The ordinary compiler products remain separate.
The shared source-location contract below is a proposal to FlashVM, not an
accepted debug section or a new producer/consumer requirement.

## Executable private tooling

Build current native tools, then compile and resolve a decoded instruction PC:

```
python3 utils/dis64-locations.py compile --limbo /build/tools/limbo \
  --source limbo/tests/dis64-scalar/loop.b \
  --image /build/arm64/probe.dis64 --map /build/arm64/probe.locations.json
python3 utils/dis64-locations.py lookup --image /build/arm64/probe.dis64 \
  --map /build/arm64/probe.locations.json --pc 0
```

The tool requests `LIMBO_DIS64_TRACE=1`. Only the successful scalar backend emits
`LIMBO64LOC` JSON rows on stderr. Every emitted instruction gets its original
resolved Dis PC and actual serialized opcode, including RET-to-EXIT conversion.
Removed NOOPs get no row. A missing compiler span is explicitly null. File names
are diagnostic strings, including escaped quotes, backslashes and controls;
valid UTF-8 names retain their original bytes. No source file is opened by lookup.

The private sidecar schema is `inferno-dis64-locations/1`. It binds a SHA256 digest
of the exact complete image and all decoded PC/opcode pairs. The independent
Python decoder checks code boundaries before accepting the map. Lookup refuses a
changed image, missing/reordered/changed records, invalid locations and out-of-range
PCs. SHA256 detects accidental mismatch; this is not authentication or an execution
capability. No VM reads these JSON files or follows their source paths.

Map publication uses a same-directory temporary file and replacement after
successful compilation/validation. Failed compilation leaves the prior map intact.
Source/image/map aliases, including existing hard links, refuse before invoking
the compiler. This is not a transactional rewrite of the compiler's image-output
service. Sidecar reads and writes are limited to32 MiB. Compiler diagnostics are
captured in the host process; the compiler executable is an explicit trusted input.

With verbose scalar console startup, terminal VM errors additionally report
`native64 scalar fault: dis-pc=N`. This uses the retained original Inst program,
not a native code address. It subtracts xec's already advanced PC only when the
address is aligned and inside the exact current program extent. Successful exits
emit no fault marker. Regular Inferno debugger/keepbroken behavior is unchanged.

Source line values follow Limbo's existing fline/#line accounting. The private
`column` values retain the legacy compiler cursor, including its mixed byte/rune
lexer behavior. They must not be interpreted as standardized UTF-8 byte offsets,
UTF-16 units or Unicode scalar columns. Missing stop positions stay explicit;
no debug-location field is treated as a declared semantic/native-call field.

## Proposal to FlashVM

1. Keep entry PC and all debug PCs in decoded Dis-instruction units. The sole np0
   entry remains the existing no-argument/no-result entry; no new entry record.
2. Initially standardize source file identity and one-based source lines only.
   Preserve unknown positions explicitly. A compiler's generated instructions may
   have no source span. Do not invent a user-source line for an instruction.
3. Agree column units and endpoint conventions before specifying column fields.
   Limbo's cursor cannot silently define Coco/R's coordinate contract. Please
   state your frontend's line/column units and preferred normalization.
4. Keep source locations independent of compiler-declared native-call field
   witnesses. A source variable/span never authorizes a reference or scalar store.
5. Decide a common optional representation and registry assignment together.
   Until then the above JSON is Inferno-only offline tooling. No required feature,
   section ID, default admission or host `execution_supported` flag changes.

This supplies diagnostics for the existing43-op np0 path, not full shell/import/
managed-call startup. Complex runtime services remain in C.
