# Series0107: checked text views, native cache progress and five Draw Frame64 consumers

This batch strengthens the text/font consumer prerequisite for a native64 GUI.
It does not register managed Draw, open builtin imports, widen Dis64 file calls
or claim Android/AWM startup. Complex font/cache work remains in C.

## Runtime fixes

Image.text/textop/textbg/textbgop now initialize their result from the requested
point and publish it deterministically on nil-H, empty, invalid-op or mismatched
display skips. Previously early H paths returned uninitialized stack coordinates,
while mismatched displays left a stale caller result. The background image must
belong to the same display too. Caller result addresses are captured and span
checked before fallible/native operations, then copied with memmove so unaligned
caller result cells work. No new result ABI or pointer representation is used.

One private DrawTextView captures String len/max through byte copies, refuses
INT_MIN length, invalid capacity, native wrap and unaligned Rune storage before
native effects, and terminates the logical tail. A unique String slice can shorten
len without erasing old trailing bytes; the native cache must not consume those
bytes when skipping an unavailable glyph. ASCII/Runes retain their existing NUL
semantics. The actual runtime String representation reserves a spare inline cell;
these checks rely on caller-owned valid String storage, not arbitrary pointer or
allocator provenance. The borrowed view neither retains an owner nor pins a GC
object; normal VM/display exclusion and source lifetime remain required.

Font.width uses native stringnwidth/runestringnwidth directly. It no longer needs
a Rune-to-UTF8 conversion cache allocation merely to measure text. Empty/nil-H
behavior remains zero, and its captured WORD result also supports byte alignment.

## Native text/cache fixes

cachechars checks its maximum count before payload lookahead. _string checks
remaining length before reading the next character. This makes exact-count native
inputs safe at the last character rather than reading one byte/Rune past them.
Public text wrappers avoid dereferencing a nil destination merely to select clipr.

_string rejects nil/cross-display image/font/background, invalid composition,
negative length and ambiguous simultaneous byte/Rune sources before cache work.
It bounds consecutive no-output cache retries at10, then returns the last rendered
point. A cache miss/resize may still legitimately retry. This is a bounded native
resource policy, not general font-loader/device progress or concurrent execution.

Baseline addition, next text x and used background x use signed64 intermediates
and must fit signed32 before packet reservation. The cached font image must exist
and match the display. An unused background origin is not narrowed or incremented.
Optional operator and complete text command now share the private packet
reservation introduced in0106. Refusal leaves the point at the last completed
batch. Cache services may already have updated native cache state; this is not an
all-font or whole-text transaction. Default-font fallback cannot cross displays.

_stringnwidth refuses nonpositive/overlarge returned batches before aging or
accumulating them, checks width accumulation before signed32 overflow and returns
only previously measured batches. Previously failed subfont fallback could leave
l zero/negative and wid stale, then still add wid and subtract l after breaking
its retry loop. The existing bounded retry behavior is retained.

## Owning tests and actual build scope

native64-text.py compiles actual cachechars/agefont, native ASCII/Rune text,
background text, width and UTF8 decoder bodies plus five real Draw consumers with
fresh compiler-generated Frame64 declarations. Independent Python character-width,
wide-coordinate and native-protocol calculations supply3600 native/VM cases:
zero/1/63/64/65/99/100/101/201 characters, ASCII/Greek/supplementary Runes, different
operators, multi-batch output, every requested buffer refusal, source/background
origins and signed32 coordinate boundaries. Exact command/glyph bytes, partial
returned points, widths, preserved arguments, unaligned result cells, guards and
borrowed String tmp-cache identity are compared. The real cached-glyph hit path
runs; cache misses/loading, native lookup/I-O/locks and VM error delivery are
explicit bounded transports. No managed font/cache/resource/device ownership is
claimed by those transports.

Separate probes cover H/missing/mismatched resource results, malformed and packed
String views, wrapped result slots, zero-progress cache, missing-subfont refusal,
width accumulation overflow, actual UTF8 decoding and exact one-byte allocation
without NUL (including zero-count cache with an inaccessible payload address).
Eight actual-source controls damage result initialization, background identity,
retry bound, baseline range, width addition bound, capacity proof, packet identity
or forbidden UTF8-cache conversion. They require semantic10 before unsafe
continuation; compiler/sanitizer errors, signals and timeouts never count as pass.

Local x64 O3 nonrecovering UBSan, including synthetic port aliases, passes3535145
active predicates per WORD32/WORD32-file model and all8 controls. The complete
owning Draw source/scalar gate still passes340506/model plus its result control.
The actual native libdraw archive is built separately with classic `mk install`,
because emu-g console does not link it. That separate archive build now runs in
all three full port targets. The console rebuild/file-call boot still passes11
successful C/RX roots and two leaf-error roots. Archive compilation is not full
GUI linkage or device launch. Native ARM64/x64 ASan jobs and new full classic-mk
matrix include text gates; new-source target acceptance remains pending actual logs.

Bounded consumer-frame coverage grows55/89 to60/89 (the four text entries and
Font.width). This is not60 registered managed exports or global port readiness.
JIT availability remains ARM64/x64166/173 fast (95.95%), RV64M103/173 (59.54%),
and173/173 complete JIT+C entries. Default scalar Dis64 admission stays43/173;
-X dis64-calls remains the separately agreed immutable bounded launch policy.

## Previous complete matrix and peer receipt

All jobs in41684116076 for d3deae142ad84911e8af901356fc035ed55fe4ba PASS:
ARM64126086056193, x64126086055575, RV64/QEMU126086056109. Read all3 actual
logs: primitive gate, clean classic-mk boot/lease suite and128 live-local complete
emulator processes/two wrong-answer controls. x64 took38m12s with its prior40m
budget; the expanded full gate is now allowed60m instead of risking a timeout
from the added archive/text checks. Focused target jobs remain separate.

Synced FLASHVM-0105/0106 at ea51eecd4df5f33df5860311f53e5a9dc6113a58 and read
its latest agreement receipt. The peer's native root-group/cell guard changes
are not imported; its independent typed-call validator/executor remain planned
and current file-call refusal explicit. No new shared encoding/feature/ABI ID,
native-pointer serialization, ordinary WORD32 LEA or default admission is changed.
The communication stays in the two journals without user relay.
