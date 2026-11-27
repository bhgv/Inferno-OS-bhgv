# Independent Dis64 typed checkpoint integration

Run `python3 utils/tests/dis64-peer-semantics.py --peer-root /path/to/flashVM`.
Use exact FlashVM7119413a641d69bee54adf315ec6e65725a542ba. Seven reviewed Python
sources and the immutable per-image report have fixed SHA256 identities. The
reader's six-module closure is loaded explicitly, without adding the external
checkout to sys.path. No private Inferno checker, writer, sidecar locals or
acceptance labels are imported as semantic authority.

The exact25-image corpus/manifest stays unchanged. Its report distinguishes:
7 bounded function passes,12 bounded refusals,4 unsupported facilities and2
malformed outer containers. All owning execution remains closed. In particular,
unused managed descriptors,65 descriptors and an empty IMPORTS section are not
universal wire errors. They pass this bounded function checker with remaining
module/unused-type semantics unproved; Inferno's separate loader policy may still
refuse them. Bool is unsupported, not guessed from Int32 width. The divide-zero
image passes static normal-return obligations without arithmetic-safety proof.

The integration reproduces the exact published facts/identities,39 independent
raw-wire synthetic cases and four exact-stage peer omissions (initialization,
frame agreement, LEA role, return initialization). Renamed files and poisoned
sidecars leave semantic facts unchanged. Rehashed raw instruction mutation must
still refuse before function facts publish. Separate `--control identity|facts|
authority|admission` omissions check stale/forged result and authority boundaries.
Each control must reach its exact named semantic predicate; parser/file/resource
exceptions, child crashes/signals/timeouts and generic exit10 never count.

`--report PATH` writes a concise machine-readable receipt. The focused CI checks
out the pinned peer closure and runs the positive integration plus four controls.
The peer report names Inferno82d5275 as its corpus revision; the unchanged corpus
SHA proves identity on later branches, not execution acceptance of later code.

No native pointer, Module, executable mapping, owning allocation, root or caller
permission is produced. This is bounded straight-line root-to-leaf function
semantics, not full CFG, DATA/import resolution, unused-descriptor semantics,
collector/code/frame lifetime or reciprocal runtime compatibility. Shared IDs,
default numeric admission and the immutable named launch agreement stay unchanged.
