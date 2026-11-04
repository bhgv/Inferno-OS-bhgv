# Series0117: exact font-number fields and replay of committed font descriptions

This series removes native-long-dependent conversion from the bitmap font text
parser, checks the existing font corpus, and makes its ownership/numeric tests
active under optimized NDEBUG builds. It changes no shared Dis64 wire layout,
JIT opcode or managed Draw registration/admission policy.

## Complete C-format numbers before narrowing

buildfont previously accepted partial tokens through strtol and assigned optional
subfont offsets to int without a range check. On a 64-bit host a representable
long outside Int32 could therefore narrow to another glyph offset; out-of-range
input could behave differently on another C data model. Partial height/ascent or
range tokens could also be mistaken for following fields. The diagnostic's old
pointer-difference/int format mismatch is removed by reporting the invalid field.

A bounded-magnitude parser now recognizes complete decimal/octal/hex tokens,
including signs, before publishing an Int32 value. It distinguishes a filename
from a complete but overflowing numeric token. Overflow cannot grow the unsigned
accumulator; no locale, errno or native-long width determines the result. Height/
ascent keep their existing positive short-field limits. Range values must be
nonnegative and ordered, now correctly allowing the inclusive Unicode endpoint
U+10FFFF. An overflowing optional offset followed by a filename refuses the font.
Existing signed Int32 offset compatibility is retained; this is not a new glyph
position or pixel-rendering proof.

Separators use the six ASCII whitespace bytes, so CRLF descriptions work without
embedding CR into subfont names. Numeric-prefix filenames retain their bytes.
A final token is treated as a filename even when entirely numeric, including
trailing whitespace. This resolves the previous last-token ambiguity that could
create an empty filename. Nil descriptions/names refuse before dereference.
Each partial-allocation refusal still retires previously constructed ranges,
cache arrays, copied names and the Font owner.

## Independent corpus and active ownership checks

font-oracles.py uses Python integers and an independent C-format token grammar:
515 direct lexical cases and318 whole descriptions include Int32 endpoints,
overflow, incomplete tokens, CRLF, numeric filenames, Unicode endpoints and late
numeric failures after an earlier owned range. Returned parsed fields/names are
compared directly, not through an implementation-generated acceptance label.

The test additionally decodes all224 committed .font descriptions independently
and compares all4,959 complete min/max/offset/name records against actual C output.
Every description admits with identical expected fields. Its mutable C input is
separately owned; no subfont bitmap is opened or rendered by this corpus check.
Existing allocation failure at every construction boundary, short dimensions,
32767/32768 subfont-count limits and file-length refusal/descriptor-close checks
remain enabled.

The old fixture assert calls are replaced by explicit active conditions, so
-DNDEBUG does not erase their work. Four controlled omissions remove range,
separator, Unicode endpoint or optional-offset checks. Each must stop at the
specific numeric-status or font-admission predicate; a resource allocation failure,
compiler error, signal, sanitizer report or timeout is not semantic detection.
Unexpectedly admitted fonts are released before the controlled stop. The old
fixture-only LSan-disable hook is removed; normal hosted leak detection stays
available in addition to explicit allocation counts.

Local O3 nonrecovering UBSan with platform aliases passes229,454 active predicates
and all four controls. Existing caller prototypes pass syntax/type checks; the
actual libdraw archive rebuilds with classic mk. A new short native ARM64/x64
ASan+UBSan job and the full target pipeline run the normal test plus all controls.
The test supports target compiler/runner/include selection and checks that labelled
ARM64/x64/RV64 compilers actually match their native target macros. Exact-source
hosted acceptance is pending at publication.

Native file/render/subfont release callbacks remain bounded services; the parser
allocates actual Font/Cachefont structures, not VM Heap headers. No managed
Font.build CALL, renderer, GC, opened-subfont cache lifetime, Tk change or new
Android/AWM acceptance is inferred. Bounded Draw C-consumer count remains84/89;
this series strengthens the native parser used by existing consumers.

## Prior receipts and coordination

Previous d58c1f1 native ARM64 mix/attach job126284123985 in41741580067 passes;
actual log read:38,447 predicates/model,18 attach plus10 composed colormix scenarios
and all23 controls. Its x64 counterpart126284123959 was still running when checked.
Primitive jobs126284123994 ARM64 and126284123811 x64 pass with actual logs read:
3,000 cases/2,209,253 predicates/model and seven controls. Architecture-dependent
predicate totals are recorded as observed, not silently replaced by local counts.
No previous-source receipt is acceptance of this new font parser.

Re-fetched experiment at d58c1f1: no newer incoming reply after FLASHVM-0117.
The independent typed-role/initialization/result/LEA checkpoint remains agreed and
pending on the same immutable corpus before owning execution. Existing structural
25 identities/23 passes/two malformed outer do not establish the private21 semantic
outcomes, managed execution or root/code lifetime. Default43/173, ordinary WORD32
LEA, immutable named -X dis64-calls -d launch/cache discrimination and shared
wire/feature/ABI IDs remain unchanged. Coordination stays in the journals.

Linux ARM64 console build/execution already has target evidence. Managed imports,
roots/collector/resource integration and Android/AWM startup/graphics remain open.
Fast native dispatch counts stay ARM64/x64 166/173(95.95%), RV64M103/173(59.54%),
with C support for all173 dispatch entries. These are not whole-port readiness
percentages; no unsupported APK completion date is assigned.
