# Series0109: font factory ownership and complete-matrix source closure

Font.open and Font.build now capture their result cell and input owners before
opening a font. They construct and retain the new managed font/name/display and
native display lease before publishing the result and retiring the old owner.
Results may overlap input cells or have byte alignment. Empty managed names stay
H instead of attempting D2H(H). Missing default subfonts refuse without a null
dereference. The default font description uses bounded formatting.

A private drawownedfont constructor uses the actual protected heap allocation
boundary. If managed allocation fails after a native font/cache lease was opened,
it calls font_close and preserves the allocation diagnostic. Native font parsing,
cache lookup/insertion and device services remain C; no JIT replacement is added.
Managed Draw registration remains refused before descriptor staging on native64.

## Regression and acceptance scope

The owning-resource fixture now executes seven real generated-Frame64 factories:
84 scenarios per model (the previous40 plus40 font alias/failure combinations and
four empty-name/missing-input cases). It uses actual Heap/Type/refcount and mapped
retirement code, native Image/Screen release and the production font factories,
constructor and destructor. Font-open/build/cache/format/String decoding services
are bounded adapters. This is not a font-cache algorithm, collector, arbitrary
callback-error or guest managed CALL/RET proof. Two added source controls omit font
rollback or name retention; each must stop at semantic exit10, never a signal,
sanitizer/compiler failure or timeout. The original ten controls remain required.

Local O3 nonrecovering UBSan passes WORD32 and WORD32-file with22230 predicates
per model, real references restored and both allocation ledgers empty. Complete
Draw compilation and existing scalar340506 predicates/model pass. The GUI map
fixture also passes1072 cases for each WORD64/WORD32 model. Hosted exact-source
ARM64/x64 ASan acceptance and the new complete three-target matrix are pending.

The preceding complete workflow41700048688 FAILED on all three jobs, rather than
proving the whole series0108. All actual logs identify the same test-source link
failure: gui-type-maps extracted drawmodinit without its newly factored private
drawmaketypes dependency. This series includes that actual helper, without a stub,
and runs the complete GUI map gate in the short Draw-owner ARM64/x64 job too.
The prior focused owner ASan success remains valid within its stated scope.

## Readiness and peer agreement

Bounded Frame64 C-consumer coverage grows65/89 to67/89. It is not67 registered
exports or a complete managed program. ARM64/x64 native fast paths remain166/173
(95.95%), RV64M103/173 (59.54%); all173 dispatch entries include C support. These
counts do not measure Android/AWM completion. Restricted scalar-call programs
already build/run; full managed/imported Dis64 admission, real collector/resource
integration and Android device validation remain before a full AWM APK claim.
No reliable completion date or new APK is claimed.

Read incoming FLASHVM-0107 again: its MOVMP/root metadata is not imported. Shared
wire/feature/ABI IDs,43/173 default admission, immutable named dis64-calls launch
and ordinary WORD32 LEA are unchanged. FlashVM's independent typed-call validator
is still planned; that is distinct from Inferno's restricted executor. No peer
implementation acceptance is inferred. Further exchange uses the two journals.
