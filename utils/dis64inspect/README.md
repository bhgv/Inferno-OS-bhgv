# Dis64 envelope inspector

Build with `make` in this directory, or compile `dis64inspect.c` as C99 with
`-Wno-unused-function -I../../include`. The utility uses the shared accepted ABI1 envelope codec.

```
dis64inspect input.dis64
dis64inspect input.dis64 --rewrite output.dis64
```

The JSON result explicitly reports structural envelope acceptance only:
`envelope_only:true`, `execution_supported:false`. Module/code/type/import
payloads are opaque; this is not a VM, compiler target or NC64 converter.
The rewrite option writes the requested output path and copies canonical
opaque payloads byte-for-byte. It does not create an executable program.

Input is bounded to 64 MiB, 1024 extensions and 4096 sections. No mandatory VM
extensions are implemented by the utility; it rejects nonempty feature manifests.
The reusable header accepts an explicit read-only feature-support callback.

Registry and complete scope: `doc/dis64-envelope.md` at the repository root.
Tests: `python3 libinterp/tests/dis-envelope.py` from the repository root.

## Explicit scalar semantic check

```
dis64inspect pilot42.dis64 --pilot
dis64inspect pilot42.dis64 --pilot --rewrite checked.dis64
```

The opt-in mode uses `dis64pilotcheck` for exact module metadata, complete type
maps, string IDs, scalar code roles, entry/frame/data references and count-zero
initializers. JSON adds `scalar_pilot:true` and checked numeric metadata, with
`envelope_only:false` and **`execution_supported:false`**. It does not execute
MOVW or EXIT, allocate a VM module or resolve imports. The supported subset and
limits are documented in `doc/dis64-pilot.md`. Invalid semantic input leaves an
existing rewrite destination untouched; later filesystem write failure is not
an atomic transaction. Default mode still accepts opaque envelope payloads.

The header-only codec closure includes unused writer/layout entry points;
`-Wno-unused-function` applies only to those static helper definitions, while
other compiler warnings remain enabled. Regression: `libinterp/tests/dis64-inspect.py`.
